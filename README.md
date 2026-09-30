# Memory Management Simulator — Paging & Page Replacement in C

A trace-driven virtual memory simulator written in C that replicates how an OS handles virtual-to-physical address translation, page fault detection, and page replacement. Built as a low-level systems programming project to understand the internals of paging, dirty-page write-back, and reference-count-based eviction policies.

---

## Table of Contents

- [Overview](#overview)
- [How It Works](#how-it-works)
- [Page Replacement Policy](#page-replacement-policy)
- [Project Structure](#project-structure)
- [Build Instructions](#build-instructions)
- [Usage](#usage)
- [Sample Input](#sample-input)
- [Sample Output](#sample-output)
- [Bugs Found & Fixed](#bugs-found--fixed)
- [What I Learned](#what-i-learned)

---

## Overview

This simulator reads a memory access trace file — a list of (address, R/W) pairs representing what a real CPU would generate — and replays it against a configurable virtual memory system. It tracks:

- Which pages are currently resident in physical frames
- When a page fault occurs and which page gets evicted
- Whether the evicted page was dirty (modified) and needs to be written to disk
- Final statistics: total accesses, page faults, and disk writes

---

## How It Works

```
Virtual Address  →  Page Number + Offset  →  Frame Lookup  →  Hit or Fault
```

Every memory address is split into two parts:

| Part | Formula | Meaning |
|---|---|---|
| Page Number | `address / page_size` | Which virtual page this address belongs to |
| Offset | `address % page_size` | Position within that page (unchanged across translation) |

The simulator maintains a fixed set of physical frames. On each access:

1. **Hit** — Page already in a frame → increment its reference count (max 10), mark dirty if write
2. **Compulsory miss** — An empty frame is available → load the page, print `Page NULL replaced by Page Y`
3. **Page fault** — All frames full → run the replacement policy, evict a victim, load the new page

---

## Page Replacement Policy

This simulator uses a **modified FIFO** policy gated by reference counts:

**Reference Count Rules:**
- Every page is loaded with a reference count of **3**
- After every **4 memory accesses**, all resident pages are aged down by 1 (minimum 0)
- Each subsequent access to an already-resident page bumps its count up by 1 (maximum 10)
- Only pages with reference count **0** are eligible for eviction

**Eviction Process:**
1. Among all frames with `ref_count == 0`, pick the one loaded **earliest** (FIFO order)
2. If no frame has `ref_count == 0`, reduce every frame's count by 1 and repeat
3. If the evicted frame was last written to (`mode == 'w'`), flush it to disk (dirty write-back)

This policy gives recently loaded or frequently accessed pages a chance to stay in memory, while still maintaining a deterministic FIFO order among equally aged pages.

---

## Project Structure

```
.
├── mem.c          # Main simulator — argument parsing, trace loading, simulation loop
├── header.h       # Struct definition for the memory configuration (M)
├── tracefile      # Sample memory access trace (see format below)
└── README.md
```

---

## Build Instructions

**Requirements:** GCC, GNU Make, Linux/Unix environment

```bash
# Compile manually
gcc -o prog04 mem.c -lm

# Or using the makefile (if included)
make -f prog04.makefile
```

---

## Usage

```bash
./prog04 <VirtualMemory> <PageSize> <PhysicalMemory> <tracefile> <d|n>
```

| Argument | Description |
|---|---|
| `VirtualMemory` | Exponent: virtual address space = 2^VirtualMemory |
| `PageSize` | Exponent: page size in bytes = 2^PageSize |
| `PhysicalMemory` | Number of physical frames (given directly, not as a power of 2) |
| `tracefile` | Path to the memory access trace file |
| `d` / `n` | Debug mode on (`d`) or off (`n`) |

**Example from spec:**
```bash
./prog04 20 10 17 tracefile d
```
This sets virtual memory = 2²⁰ bytes, page size = 2¹⁰ = 1024 bytes, 17 physical frames, reads from `tracefile`, and prints debug output.

---

## Sample Input

Each line in the trace file is an address followed by `r` (read) or `w` (write):

```
0 r
1024 w
0 r
2048 r
1024 w
3072 r
```

One or more blank lines at the end of the file are expected and handled correctly.

---

## Sample Output

**Debug mode on (`d`):**
```
Page NULL replaced by Page 0
Page NULL replaced by Page 1
Page 0 replaced by Page 2
Page 0 was not dirty
Page 1 replaced by Page 3
Page 1 was dirty

Number of Memory Accesses: 8
Number of Memory Accesses that resulted in Page Faults: 2
Number of Pages written to the disk: 1
```

**Debug mode off (`n`):**
```
Number of Memory Accesses: 8
Number of Memory Accesses that resulted in Page Faults: 2
Number of Pages written to the disk: 1
```

> **Note:** Page faults count only cases where a resident page was replaced. Compulsory misses (loading into an empty frame) are not counted. Dirty pages still in memory at program end are not counted in disk writes.

---

## Bugs Found & Fixed

Working through this project involved finding and fixing several real bugs in the original implementation. Documenting them here because they're worth knowing about:

### 1. Off-by-one heap overflow in trace reading
**Original:**
```c
while(fscanf(fp, "%ld %c", &addr[i++], &r_w[j++]) != EOF);
```
`i++` and `j++` fire as argument side effects, even on the final failed EOF read — writing one slot past the allocated array.

**Fixed:**
```c
while(fscanf(fp, "%ld %c", &addr[n], &r_w[n]) == 2) { n++; }
```
Only increment after confirming a successful read.

---

### 2. Hardcoded frame count ignored the command-line argument
**Original:**
```c
if(j == 4) j = -1;
```
The frame cycle was hardcoded to 5 regardless of `phy_mem`. Any other frame count would cause out-of-bounds access or silently leave frames unused.

**Fixed:**
```c
if(j == m.phy_mem - 1) j = -1;
```

---

### 3. No page hit detection — every access was treated as a fault
The original loop evicted and replaced a page unconditionally on every access. A correct simulator must first check whether the incoming page is already resident in any frame before deciding to evict.

**Fixed:** Added a scan of all frames before the eviction path.

---

### 4. Negative array index — out-of-bounds read
**Original:**
```c
ref_count[j-1]  // when j == 0, this reads ref_count[-1]
```
Undefined behaviour. Happened to not crash on the test machine due to heap layout luck.

---

### 5. No NULL check on fopen
**Original:** `fopen()` result used directly with no check.
A missing trace file caused a segfault instead of a clear error message.

**Fixed:**
```c
if(fp == NULL) {
    printf("ERROR: could not open trace file %s\n", v[4]);
    return 1;
}
```

---

### 6. debug_op argument was parsed but never used
The original code stored `v[5]` into `m.mode` but then printed debug output unconditionally. The `d`/`n` flag now actually controls whether debug lines are printed.

---

## What I Learned

- **C gives you zero guardrails.** Argument evaluation order, array bounds, NULL pointers — every assumption has to be explicit.
- **Bugs that don't crash are worse than segfaults.** A wrong index producing plausible-looking output is far harder to catch than a clean crash.
- **Test at the boundary values.** The hardcoded-5 bug only revealed itself when the frame count changed from the one value used during development.
- **Understanding the OS through code.** Implementing page replacement from scratch made concepts like thrashing, working sets, and dirty-page write-back concrete rather than abstract.

---

## Author

Kisha Soya
