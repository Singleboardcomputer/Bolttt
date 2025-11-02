#!/usr/bin/env bash
set -xeuo pipefail

# This script runs inside an emulated aarch64 Ubuntu container.
# It builds V8's d8 shell with ASAN + REPRL for Fuzzilli fuzzing.

export DEBIAN_FRONTEND=noninteractive

echo "[*] Installing dependencies..."
apt-get update
apt-get install -y --no-install-recommends \
  ca-certificates git python3 python3-pip curl wget gnupg2 unzip \
  build-essential ninja-build pkg-config gperf clang llvm \
  clang-format cmake subversion python3-venv locales sudo \
  libstdc++-12-dev libc6-dev

locale-gen en_US.UTF-8 || true
update-locale LANG=en_US.UTF-8 || true

# ---- Depot Tools ----
echo "[*] Setting up depot_tools..."
DEPOT_TOOLS=/work/depot_tools
if [ ! -d "$DEPOT_TOOLS" ]; then
  git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git "$DEPOT_TOOLS"
fi
export PATH="$DEPOT_TOOLS:$PATH"

# ---- Fetch V8 ----
echo "[*] Fetching V8..."
cd /work
if [ ! -d /work/v8 ]; then
  fetch v8 || git clone https://chromium.googlesource.com/v8/v8.git /work/v8
fi

# ---- Clone Fuzzilli ----
echo "[*] Cloning Fuzzilli..."
if [ ! -d /work/fuzzilli ]; then
  git clone https://github.com/googleprojectzero/fuzzilli.git /work/fuzzilli
fi

cd /work/v8
gclient sync --with_branch_heads --nohooks || true

# ---- Configure build ----
OUTDIR=out/fuzzbuild
mkdir -p "$OUTDIR"
NUMJOBS=$(nproc || echo 1)

# Fuzzilli + REPRL + ASAN flags
GN_ARGS="
  is_debug=false
  is_asan=true
  use_custom_libcxx=false
  v8_enable_verify_heap=false
  v8_enable_i18n_support=false
  v8_enable_gdbjit=false
  v8_enable_disassembler=false
  v8_monolithic=true
  is_component_build=false
  symbol_level=1
  v8_fuzzilli=true
  v8_use_external_startup_data=false
"

echo "[*] GN args:"
echo "$GN_ARGS"

# ---- Run Fuzzilli's fuzzbuild.sh ----
FUZZBUILD_SCRIPT=/work/fuzzilli/Targets/V8/fuzzbuild.sh
if [ ! -x "$FUZZBUILD_SCRIPT" ]; then
  echo "!! fuzzbuild.sh not found at $FUZZBUILD_SCRIPT"
  ls -al /work/fuzzilli/Targets/V8 || true
  exit 1
fi

bash "$FUZZBUILD_SCRIPT" "$NUMJOBS" 2>&1 | tee /work/fuzzbuild_build.log

# ---- Verify output ----
if [ ! -x "/work/v8/out/fuzzbuild/d8" ]; then
  echo "!! d8 not found in /work/v8/out/fuzzbuild"
  find /work/v8 -name d8 -type f -executable | head
  exit 2
fi

echo "[*] Built d8 successfully at /work/v8/out/fuzzbuild/d8"

# ---- Bundle d8 and runtime libs ----
BUNDLE=/work/v8_fuzzilli_bundle
rm -rf "$BUNDLE"
mkdir -p "$BUNDLE/bin" "$BUNDLE/runtime_libs"
cp -v /work/v8/out/fuzzbuild/d8 "$BUNDLE/bin/"

ldd /work/v8/out/fuzzbuild/d8 | awk 'NF==4 {print $3} NF==2 {print $1}' | while read -r lib; do
  [ -z "$lib" ] && continue
  case "$lib" in
    linux-vdso*|ld-linux*) continue;;
  esac
  if [ -f "$lib" ]; then
    mkdir -p "$BUNDLE/runtime_libs/$(dirname "$lib")"
    cp -v --parents "$lib" "$BUNDLE/runtime_libs/" || true
  fi
done

# ---- Wrapper for running ----
cat > "$BUNDLE/run_d8.sh" <<'EOF'
#!/usr/bin/env bash
HERE="$(cd "$(dirname "$0")" && pwd)"
export LD_LIBRARY_PATH="$HERE/runtime_libs:${LD_LIBRARY_PATH:-}"
export ASAN_OPTIONS="allocator_may_return_null=1:detect_leaks=0:abort_on_error=1:handle_segv=1:detect_odr_violation=0"
exec "$HERE/bin/d8" "$@"
EOF
chmod +x "$BUNDLE/run_d8.sh"

# ---- Tarball ----
TAR=/work/v8_d8_fuzzilli_aarch64_bundle.tar.gz
tar -C /work -czf "$TAR" "$(basename "$BUNDLE")"

echo "[+] Done! Created bundle:"
ls -lh "$TAR"
