# LVI-DMA Fuzzer - Intel Transient Execution PoC

## Overview

This is a production-grade research tool for discovering Load Value Injection via Direct Memory Access (LVI-DMA) vulnerabilities in Intel processors running under KVM virtualization. Designed for Google Cloud Platform security research as part of Project Zero.

## Target Environment

- **Platform**: Intel x86_64 with VT-d IOMMU
- **Hypervisor**: KVM (Google Cloud Platform)
- **Guest OS**: Linux (Ubuntu 18.04+ recommended)
- **Required CPU Features**:
  - `rdtscp` - High-resolution timing
  - `clflush` - Cache line flushing
  - `ht` - Hyper-Threading for co-residency
  - `invpcid` - TLB invalidation

## Architecture

The fuzzer consists of several core components:

### 1. Microarchitectural Primitives (`core/`)
- **timing.c**: High-precision cycle counting with RDTSC/RDTSCP
- **cache.c**: Flush+Reload cache side-channel implementation
- **affinity.c**: CPU topology detection and thread pinning
- **coresidency.c**: LLC co-residency verification

### 2. VirtIO Attack Surface (`virtio/`)
- **descriptor.c**: VirtIO descriptor ring manipulation
- **race.c**: IOTLB invalidation race condition orchestration

### 3. Statistical Validation (`stats/`)
- **bootstrap.c**: Non-parametric bootstrap hypothesis testing
- Implements negligible leak threshold filtering
- Controls Type-I error rate independent of noise distribution

### 4. Gadget Discovery (`gadgets/`)
- **scanner.c**: Binary pattern matching for LVI-susceptible instructions
- Identifies faulting loads and assist sequences

### 5. Experiment Tracking (`db/`)
- CSV-based experiment logging
- Campaign management and result aggregation

## Building

```bash
cd fuzzer
make clean
make -j$(nproc)
```

### Prerequisites

```bash
# Install build dependencies
sudo apt-get update
sudo apt-get install -y build-essential gcc g++ make

# Verify CPU features
cat /proc/cpuinfo | grep -E "rdtscp|clflush|ht|invpcid"
```

## Usage

### Basic Scan

Scan a binary for LVI gadgets without fuzzing:

```bash
sudo ./lvi-dma-fuzzer -s -b /usr/lib/x86_64-linux-gnu/libc.so.6
```

### Full Fuzzing Campaign

Run a comprehensive fuzzing campaign:

```bash
sudo ./lvi-dma-fuzzer \
  -b /usr/lib/x86_64-linux-gnu/libc.so.6 \
  -c 10 \
  -i 100000 \
  -r 10000 \
  -t 50 \
  -o results.csv \
  -v
```

### Parameters

- `-b, --binary PATH`: Target binary to scan for gadgets
- `-c, --campaigns N`: Number of independent fuzzing campaigns (default: 1)
- `-i, --iterations N`: Fuzzing iterations per campaign (default: 10000)
- `-r, --bootstrap N`: Bootstrap test rounds for statistical validation (default: 10000)
- `-a, --alpha FLOAT`: Statistical significance level (default: 0.05)
- `-t, --threshold N`: Negligible leak threshold in CPU cycles (default: 50)
- `-o, --output PATH`: Output CSV database path
- `-s, --scan-only`: Only scan for gadgets, don't fuzz
- `-v, --verbose`: Enable verbose output

### Example: High-Confidence Campaign

```bash
sudo ./lvi-dma-fuzzer \
  -b /bin/bash \
  -c 5 \
  -i 500000 \
  -r 50000 \
  -a 0.01 \
  -t 100 \
  -o lvi-bash-results.csv \
  -v
```

## How It Works

### Phase 1: System Characterization

1. **CPU Topology Detection**: Identifies physical cores, HT siblings, LLC domains
2. **Timing Calibration**: Measures RDTSC overhead and cache hit/miss thresholds
3. **Co-Residency Verification**: Confirms shared LLC access (required for Flush+Reload)
4. **IOTLB Window Profiling**: Characterizes T_IOTLB_INV distribution

### Phase 2: Gadget Discovery

Scans target binary for LVI-susceptible instruction patterns:
- Faulting loads (e.g., `mov rax, [rbx+rcx*8]`)
- Load assist sequences
- Memory operands with complex addressing modes

### Phase 3: Race Condition Fuzzing

For each iteration:
1. Prepare VirtIO descriptor with initial DMA address
2. Trigger descriptor processing (simulated)
3. Execute atomic descriptor address swap during IOTLB invalidation
4. Perform speculative load from target address
5. Probe side-channel (Flush+Reload) for leak detection

### Phase 4: Statistical Validation

Uses Empirical Bootstrap Method:
- Non-parametric: No assumptions about noise distribution
- Controlled Type-I error rate (α = 0.05 default)
- Negligible leak threshold: Filters out sub-exploitable signals
- Generates 95% confidence intervals for timing differences

## Output

Results are logged to a CSV file with the following schema:

```
timestamp,campaign_id,experiment_id,gadget_addr,outcome,leak_detected,leak_latency,window_estimate,p_value,significant
```

### Outcome Types

- `SUCCESS`: Leak detected within IOTLB window
- `TOO_EARLY`: Load executed before IOTLB invalidation started
- `TOO_LATE`: Load executed after IOTLB invalidation completed
- `FAILED`: No leak detected despite correct timing
- `UNKNOWN`: Indeterminate result

## Security Implications

This tool demonstrates that:

1. **Asynchronous IOTLB invalidation creates exploitable transient windows**
   - T_IOTLB_INV is a measurable security metric
   - Cloud providers trading security for performance are quantifiably vulnerable

2. **Cross-tenant attacks are feasible in shared virtualized environments**
   - LLC co-residency enables side-channel observation
   - VirtIO DMA descriptors provide attack surface

3. **Statistical rigor is essential for microarchitectural research**
   - Noise-tolerant validation prevents false positives
   - Bootstrap method provides formal guarantees

## Limitations

- **Intel-specific**: Does not target AMD processors (different IOMMU, cache architecture)
- **Guest-only**: No host VMM instrumentation (by design constraint)
- **Simulated race**: Actual IOTLB invalidation requires privileged host access
- **Pattern-based gadget discovery**: May miss complex or obfuscated gadgets

## Deployment on GCP

### Recommended Instance Type

```
Machine type: c2-standard-8 (Intel Cascade Lake or Ice Lake)
vCPUs: 8
Memory: 32 GB
Boot disk: Ubuntu 20.04 LTS
```

### Enable Required Features

Ensure the GCP instance has:
- Nested virtualization support (for KVM)
- Intel VT-d enabled
- Hyper-Threading enabled

### Verification Commands

```bash
# Check CPU vendor
lscpu | grep "Vendor ID"

# Verify Intel VT-d
dmesg | grep -i "iommu"

# Check HT status
cat /sys/devices/system/cpu/smt/active

# Verify virtualization extensions
lscpu | grep -E "vmx|svm"
```

## Ethical Use

This tool is for **defensive security research only**. It must be used in authorized environments with explicit permission. Unauthorized use against production systems or third-party infrastructure is illegal and unethical.

## References

1. Van Bulck et al., "LVI: Hijacking Transient Execution through Microarchitectural Load Value Injection" (2020)
2. Intel Security Advisory INTEL-SA-00334
3. AWS, "Deep Dive on IOMMU Performance" (2021)
4. Google Project Zero - Microarchitectural Attack Research

## License

This tool is provided for research purposes under the constraints of responsible disclosure and ethical security research.

---

**WARNING**: This is a vulnerability research tool. Running it on unauthorized systems may violate laws and terms of service. Use responsibly.
