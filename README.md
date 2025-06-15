# Cache Simulator with MESI Protocol

A multi-core cache coherence simulator implementing the MESI (Modified, Exclusive, Shared, Invalid) protocol with a shared bus architecture.

## 📋 Project Overview

This project simulates a 4-core processor system with individual L1 caches connected via a shared bus. The simulator implements the MESI cache coherence protocol to maintain data consistency across all caches and provides detailed performance analysis through various cache configuration parameters.

## 👥 Authors

- **Amber Agarwal** (2023CS50625)
- **Anoop Singh** (2023CS10459)

**Supervisor:** Prof. Rijurekha Sen  
**Institution:** IIT Delhi, Department of Computer Science and Engineering

## 🏗️ Architecture

### Core Components

- **Cache**: Individual cache instances for each processor core
- **Bus**: Shared communication medium for cache coherence
- **MESI Protocol**: Cache coherence protocol implementation
- **Statistics**: Performance monitoring and analysis

### Key Features

- 4-core simulation with individual L1 caches
- MESI cache coherence protocol
- Shared bus with priority-based arbitration
- LRU (Least Recently Used) replacement policy
- Comprehensive performance statistics
- Support for various cache configurations

## 🔧 Implementation Details

### Cache Structure
- **Tag Array**: Stores tag, MESI state, and timestamp for each cache line
- **Data Array**: Placeholder for actual data blocks
- **Associativity**: Configurable set-associative cache
- **Replacement Policy**: LRU-based eviction

### Bus Protocol
- **Single shared bus** for all cache-to-cache and cache-to-memory transfers
- **Priority-based arbitration**: Lower core ID gets higher priority
- **Transaction types**: Data transfer, invalidation signals, coherence messages

### MESI States
- **Modified (M)**: Cache line is dirty and exclusive to this cache
- **Exclusive (E)**: Cache line is clean and exclusive to this cache
- **Shared (S)**: Cache line may exist in multiple caches
- **Invalid (I)**: Cache line is not valid

## 📊 Performance Analysis

The simulator provides comprehensive analysis by varying:

1. **Number of Cache Sets**: Impact of cache size on hit rates
2. **Cache Associativity**: Effect of conflict resolution capabilities
3. **Block Size (Offset Bits)**: Influence of spatial locality
4. **Fixed Cache Size Trade-offs**: Sets vs. Associativity optimization

### Key Metrics

- **Execution Cycles**: Total cycles for instruction completion
- **Idle Cycles**: Cycles spent waiting for bus access
- **Cache Hits/Misses**: Cache performance indicators
- **Bus Traffic**: Total data transferred via bus
- **Write-backs**: Modified blocks written to memory
- **Invalidations**: Cache coherence operations

## 🧪 Test Cases

### Included Test Applications

- **App3**: Multiple cores writing to same location (write-back scenarios)
- **App4**: Single-set cache stress test with invalidations
- **App5**: Each core reads different blocks (sequential execution)
- **App6**: Single core operation (isolated execution)
- **App7/App8**: False sharing analysis and comparison
- **App9**: Same location reads (cache-to-cache transfers)
- **App10**: Same location writes (coherence overhead)

### False Sharing Analysis

The simulator includes specific test cases demonstrating false sharing effects:
- **App7**: Causes false sharing with same cache line accesses
- **App8**: Avoids false sharing with different cache line accesses
- Performance difference: ~300 cycles due to unnecessary invalidations

## 🚀 Usage

### Prerequisites
- C++ compiler with C++11 support
- Make utility (optional)

### Compilation
```bash
make
```

### Running the Simulator(To run the test cases given in traces folder use the run.sh bash script)
```bash
/run.sh
```

### Configuration Parameters
- Cache size (number of sets)
- Associativity level
- Block size (offset bits)
- Memory access latency
- Bus transfer latency

## 📈 Results and Observations

### Key Findings

1. **Core Priority Impact**: Core 3 consistently shows highest execution cycles due to lowest bus priority
2. **Deterministic Behavior**: Fixed conflict resolution ensures reproducible results
3. **Cache Configuration Trade-offs**: Optimal performance depends on workload characteristics
4. **False Sharing Penalty**: Significant performance degradation (~300 cycles) when present

### Performance Trends

- **Increasing Sets**: Generally reduces execution cycles through better hit rates
- **Higher Associativity**: Improves conflict resolution but may increase access latency
- **Larger Blocks**: Benefits sequential access patterns but may increase miss penalty
- **Fixed Size Trade-off**: Sweet spot exists between sets and associativity

## 📁 File Structure

```
COL-216-ASSIGNMENT-3/
├── outputs/                    # Output files for self made traces
├── traces/                 # Test trace files (App3-App10)
├── cache.cpp/               # Main code
├── Col_216_Assignment_3.pdf # Detailed project report
└── README.md              # This file
```

## 📊 Performance Graphs

The simulator generates performance analysis graphs showing:
- Execution cycles vs. number of sets
- Performance impact of associativity changes
- Block size optimization effects
- Fixed cache size configuration trade-offs

## 🔍 Bus Arbitration Rules

1. **Priority Order**: Core 0 > Core 1 > Core 2 > Core 3
2. **Conflict Resolution**: Lower ID core gets bus access first
3. **State Transitions**: Sender updates immediately, receiver waits for data
4. **Write-back Policy**: Modified blocks must be written back before eviction

## 📝 Statistics Tracked

- **Read/Write Operations**: Total memory access counts
- **Cache Hits/Misses**: Performance indicators per core
- **Bus Transactions**: Total bus usage statistics
- **Idle Cycles**: Waiting time due to bus contention
- **Data Traffic**: Total bytes transferred via bus
- **Invalidations**: Cache coherence operation counts

## 🎯 Educational Value

This simulator serves as an excellent educational tool for understanding:
- Cache coherence protocols (MESI)
- Multi-core system design challenges
- Performance impact of cache configurations
- False sharing and its mitigation
- Bus-based shared memory architectures

## 📄 Documentation

For detailed implementation details, assumptions, and comprehensive analysis, refer to the complete project report: `Col_216_Assignment_3.pdf`

## 🤝 Contributing

This project was developed as part of an academic assignment. For questions or suggestions, please refer to the original authors.

## 📜 License

This project is developed for educational purposes as part of coursework at IIT Delhi.
