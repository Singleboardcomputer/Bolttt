#!/bin/bash

set -e

echo "======================================"
echo "LVI-DMA Fuzzer - Quick Start Script"
echo "======================================"
echo ""

if [[ $EUID -ne 0 ]]; then
   echo "ERROR: This script must be run as root (for CPU pinning and timing access)"
   echo "Usage: sudo ./run-fuzzer.sh"
   exit 1
fi

CPU_VENDOR=$(lscpu | grep "Vendor ID" | awk '{print $3}')

if [[ "$CPU_VENDOR" != "GenuineIntel" ]]; then
    echo "WARNING: This system is running $CPU_VENDOR, not Intel!"
    echo "LVI-DMA attacks target Intel-specific microarchitecture."
    echo ""
    echo "This fuzzer will compile but may not function correctly."
    echo "For accurate results, deploy on an Intel GCP instance."
    echo ""
    read -p "Continue anyway? (y/N) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        exit 1
    fi
fi

echo "[*] Checking CPU features..."
REQUIRED_FEATURES=("rdtscp" "clflush" "ht")
for feature in "${REQUIRED_FEATURES[@]}"; do
    if grep -q "$feature" /proc/cpuinfo; then
        echo "    [✓] $feature"
    else
        echo "    [✗] $feature - MISSING"
        echo "ERROR: Required CPU feature not available"
        exit 1
    fi
done
echo ""

echo "[*] Building fuzzer..."
make clean > /dev/null 2>&1
make -j$(nproc) || {
    echo "ERROR: Build failed"
    exit 1
}
echo "[+] Build complete"
echo ""

TARGET_BINARY="${1:-/usr/bin/ls}"

if [[ ! -f "$TARGET_BINARY" ]]; then
    echo "ERROR: Target binary not found: $TARGET_BINARY"
    echo "Usage: sudo ./run-fuzzer.sh [TARGET_BINARY]"
    exit 1
fi

echo "[*] Running gadget scan on: $TARGET_BINARY"
./lvi-dma-fuzzer -s -b "$TARGET_BINARY" || {
    echo "ERROR: Gadget scan failed"
    exit 1
}
echo ""

read -p "Proceed with fuzzing campaign? (y/N) " -n 1 -r
echo
if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    echo "Aborted by user"
    exit 0
fi

OUTPUT_FILE="lvi-dma-$(date +%Y%m%d-%H%M%S).csv"

echo ""
echo "[*] Starting fuzzing campaign..."
echo "    Target: $TARGET_BINARY"
echo "    Output: $OUTPUT_FILE"
echo ""

./lvi-dma-fuzzer \
    -b "$TARGET_BINARY" \
    -c 3 \
    -i 10000 \
    -r 5000 \
    -t 50 \
    -o "$OUTPUT_FILE" \
    -v

echo ""
echo "======================================"
echo "Fuzzing campaign complete!"
echo "Results: $OUTPUT_FILE"
echo "======================================"
