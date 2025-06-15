Here’s a complete `README.md` file for your GitHub repository. It summarizes your project, structure, usage, and documentation clearly for any future reader or evaluator:

---

```markdown
# COL216 Assignment 3 - Cache Simulator

This repository contains the implementation and analysis for **Assignment 3** of the Computer Architecture course (COL216) at **IIT Delhi**, focusing on simulating a **4-core MESI-based cache-coherent multiprocessor system**.

## 📄 Project Overview

The simulator models:
- A **4-core processor** system.
- Each core has a private cache implementing the **MESI coherence protocol**.
- A **shared snooping bus** that arbitrates memory requests.
- Simulation is performed **cycle-by-cycle**, tracking execution time, stalls, and coherence events.

### Key Features:
- Support for configurable:
  - Cache sets
  - Associativity
  - Block offset size
- Implements:
  - Cache-to-cache transfers
  - Bus contention
  - Write-back and eviction logic
  - Detailed performance statistics

## 📁 Repository Structure

```

├── src/                        # Source files (cache, bus, simulation logic)
├── traces/                    # Provided test traces (App1 - App10)
├── outputs/                   # Corresponding output files
├── images/                    # Graphs and illustrations (used in report)
├── Col\_216\_Assignment\_3.pdf   # 📘 Full report (includes flowcharts, observations)
└── README.md                  # 📄 This file

````

## 📘 Report Summary

The full report is available as `Col_216_Assignment_3.pdf`, and includes:
- Detailed class and structure descriptions (`Cache`, `Bus`, `Statistics`)
- Execution rules and cache policies
- Graphical analysis:
  - Varying number of sets
  - Varying associativity
  - Varying block offset
  - Fixed cache size with different set/assoc combinations
- Observations on bus arbitration and performance bottlenecks
- Case study on **False Sharing**
- Per-trace behavior analysis (`App1` to `App10`)
- A comprehensive **simulation flowchart**

## 📊 Sample Results

Some interesting findings:
- **Core 3** consistently finishes last due to lowest bus priority.
- **False Sharing** incurs ~300 cycle penalty compared to no-sharing version.
- **App9** (read sharing) completes in only 149 cycles due to efficient cache-to-cache transfers.
- **App10** (write sharing) suffers from repeated writebacks, adding ~300 cycles.

## 🚀 Running the Simulator

### Prerequisites:
- C++ compiler (e.g., `g++`)
- C++11 or higher standard

### Compile and Run:
```bash
g++ -std=c++11 -o simulator main.cpp
./simulator <trace_file>
````

Example:

```bash
./simulator traces/app1.txt
```

### Output:

The simulator generates detailed performance metrics for each core:

* Execution cycles
* Idle cycles
* Cache hits and misses
* Bus traffic
* Writebacks and evictions
* Invalidations

## 📎 Notes

* The simulator uses **deterministic conflict resolution** → Same results across runs.
* MESI protocol ensures coherence even under concurrent writes or reads.
* LRU eviction and single-bus architecture model real-world multiprocessor constraints.

## 🧠 Authors

* **Amber Agarwal** (2023CS50625)
* **Anoop Singh** (2023CS10459)
  Under the guidance of **Prof. Rijurekha Sen**

## 🏛️ Institution

Department of Computer Science and Engineering
**Indian Institute of Technology, Delhi**

---

📄 For complete implementation details and analysis, refer to [Col\_216\_Assignment\_3.pdf](./Col_216_Assignment_3.pdf)

```
