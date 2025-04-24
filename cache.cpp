#include <iostream>
#include <vector>
#include <tuple>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <bitset>
#include <getopt.h>

using namespace std;

enum  CacheState { M, E, S, I };
enum  operation {R,W};
enum miss_or_hit {HIT,MISS};

using CacheLineMeta = tuple<int, CacheState, int>;

struct Statictics {
    int instructions = 0;
    int reads = 0;
    int writes = 0;
    int execution_cycles = 0;
    int idle_cycles = 0;
    int cache_misses = 0;
    float cache_miss_rate = 0.0;
    int cache_evictions = 0;
    int write_back = 0;
    int bus_invalidations = 0;
    int bus_write_back = 0;
    int read_misses = 0;
    int cache_to_cache_transfers = 0;
    int memory_transactions = 0;
    int interventions = 0;
    int flushes = 0;
};
struct Bits {
    int tag_bits;
    int index_bits;
    int offset_bits;
};


class Cache {
public:
    vector<vector<CacheLineMeta>> tag_array;
    vector<vector<vector<int>>> data_array;
    public: Statictics stats;
    int num_sets;
    int set_bits;
    int assosciativity;
    int offset_bits;
    int blocksize_in_bytes;
    bool stall_flag = false;
    int current_instruction_number = 0;
    bool is_active = true;
    int waiting_time = 0;
    vector<pair<operation, string>> trace_data;
    // Constructor
    Cache(int set_bits, int num_ways, int cache_line_bits,string filepath) {
        num_sets = (1<<set_bits);
        this->set_bits = set_bits;
        assosciativity = num_ways;
        offset_bits = cache_line_bits;
        blocksize_in_bytes = (1<<cache_line_bits);
        tag_array.resize(num_sets, vector<CacheLineMeta>(num_ways, { -1, CacheState::I, -1 }));
        data_array.resize(num_sets, vector<vector<int>>(num_ways, vector<int>(cache_line_bits, 0)));
        process_trace_file(filepath, trace_data);
    }

    // Optional: pretty-print the state (helper)
    static string state_to_string(CacheState state) {
        switch (state) {
            case CacheState::M: return "M";
            case CacheState::E: return "E";
            case CacheState::S: return "S";
            case CacheState::I: return "I";
            default: return "?";
        }
    }

    // Example method: print tag array
    void print_tag_array() {
        cout << "Tag Array:\n";
        for (int set = 0; set < tag_array.size(); ++set) {
            cout << "Set " << set << ": ";
            for (auto& [tag, state, ts] : tag_array[set]) {
                cout << "[Tag: " << tag << ", State: " << state_to_string(state) << ", Time: " << ts << "] ";
            }
            cout << '\n';
        }
    }

    void process_trace_file(string file_path, vector<pair<operation, string>> &trace_data) {
        ifstream file(file_path);
        if (!file.is_open()) {
            cerr << "Error: Could not open file " << file_path << endl;
            return;
        }
    
        string line;
        while (getline(file, line)) {
            stringstream ss(line);
            char op_char;
            string hex_address;

            ss >> op_char >> hex_address;
    
            operation op = (op_char == 'R') ? operation::R : operation::W;
    
            if (hex_address.length() < 8) {
                hex_address = string(8 - hex_address.length(), '0') + hex_address;
            }
    
            trace_data.emplace_back(op, hex_address);
        }
    
        file.close();

    }

    miss_or_hit hit_or_miss(struct Bits cache_bits) {
        int index = cache_bits.index_bits;
        int tag = cache_bits.tag_bits;

        if (index >= num_sets) {
            cerr << "Error: Index out of bounds." << endl;
            return miss_or_hit::MISS;
        }

        for (auto& [stored_tag, state, ts] : tag_array[index]) {
            if (stored_tag == tag && state != CacheState::I) {
                return miss_or_hit::HIT;
            }
        }

        return miss_or_hit::MISS;
    }

    struct Bits parse(string address) {
        // Convert hexadecimal string to integer
        unsigned long addr = stoul(address, nullptr, 16);
        // Print address in binary format
        
        struct Bits bits;
        bits.offset_bits = addr & ((1 << offset_bits) - 1);
        addr >>= offset_bits;
        cout<<set_bits<<endl;
        bits.index_bits = addr & ((1 << set_bits) - 1);
        addr >>= set_bits;
        bits.tag_bits = addr;
        return bits;
    }

    void read_hit(const string& address, Bus& bus) {
        stats.reads++;
        stats.execution_cycles++;
    }

    void write_hit(const string& address, Bus& bus, vector<Cache*>& caches) {
        Bits bits = parse(address);
        int index = bits.index_bits;
        int tag = bits.tag_bits;
        
        for (int i = 0; i < assosciativity; i++) {
            auto& [stored_tag, state, ts] = tag_array[index][i];
            if (stored_tag == tag) {
                if (state == CacheState::M || state == CacheState::E) {
                    // Already exclusive or modified - just update state to M
                    tag_array[index][i] = make_tuple(tag, CacheState::M, current_instruction_number);
                    stats.writes++;
                    stats.execution_cycles++;
                } else if (state == CacheState::S) {
                    // Shared state, need to invalidate other caches
                    if (bus.busy) {
                        stall_flag = true;
                        return;
                    }
                    
                    // Broadcast invalidate
                    bus.busy = true;
                    bus.cycle_remaining = 2; // 2 cycles for bus transaction
                    bus.source_cache = -1;  // Identify this cache in the array
                    bus.address = address;
                    bus.invalidation = true;
                    
                    // Set this cache as waiting
                    is_active = false;
                    waiting_time = 2;
                    
                    stats.writes++;
                    stats.execution_cycles++;
                    
                    // Will transition to M when bus operation completes
                }
                return;
            }
        }
    }

    void read_miss(const string& address, Bus& bus, vector<Cache*>& caches) {
        if (bus.busy) {
            stall_flag = true;
            return;
        }
        
        Bits bits = parse(address);
        int index = bits.index_bits;
        int tag = bits.tag_bits;
        
        // Check if another cache has this data
        bool shared = false;
        int source_cache = -1;
        
        for (int i = 0; i < caches.size(); i++) {
            Cache* other_cache = caches[i];
            if (other_cache == this) continue;
            
            for (auto& [stored_tag, state, ts] : other_cache->tag_array[index]) {
                if (stored_tag == tag && (state == CacheState::M || state == CacheState::E || state == CacheState::S)) {
                    shared = true;
                    if (state == CacheState::M || state == CacheState::E) {
                        source_cache = i;
                    }
                    break;
                }
            }
        }
        
        // Start bus transaction
        bus.busy = true;
        bus.cycle_remaining = 100; // 100 cycles for memory access by default
        bus.source_cache = source_cache;
        bus.target_cache = -1; // This cache
        bus.address = address;
        bus.shared_line = shared;
        
        // Set this cache as waiting
        is_active = false;
        waiting_time = (source_cache >= 0) ? 2 : 100; // 2 cycles if from cache, 100 if from memory
        
        stats.reads++;
        stats.cache_misses++;
    }

    void write_miss(const string& address, Bus& bus, vector<Cache*>& caches) {
        if (bus.busy) {
            stall_flag = true;
            return;
        }
        
        Bits bits = parse(address);
        int index = bits.index_bits;
        int tag = bits.tag_bits;
        
        // Start bus transaction
        bus.busy = true;
        bus.cycle_remaining = 100; // 100 cycles for memory access
        bus.source_cache = -1;
        bus.target_cache = -1; // This cache
        bus.address = address;
        bus.invalidation = true;
        
        // Set this cache as waiting
        is_active = false;
        waiting_time = 100;
        
        stats.writes++;
        stats.cache_misses++;
    }

    void handle_bus_transaction_completion(Bus& bus, vector<Cache*>& caches) {
        Bits bits = parse(bus.address);
        int index = bits.index_bits;
        int tag = bits.tag_bits;
        
        // Find a free line or evict LRU
        int replace_idx = -1;
        int min_ts = INT_MAX;
        
        for (int i = 0; i < assosciativity; i++) {
            auto& [stored_tag, state, ts] = tag_array[index][i];
            if (state == CacheState::I) {
                replace_idx = i;
                break;
            }
            if (ts < min_ts) {
                min_ts = ts;
                replace_idx = i;
            }
        }
        
        auto& [old_tag, old_state, old_ts] = tag_array[index][replace_idx];
        
        // Handle eviction if necessary
        if (old_state == CacheState::M) {
            stats.write_back++;
            stats.cache_evictions++;
        } else if (old_state != CacheState::I) {
            stats.cache_evictions++;
        }
        
        // Update the cache line based on the operation
        if (bus.invalidation) {
            // Write miss/hit that caused invalidation - set to M
            tag_array[index][replace_idx] = make_tuple(tag, CacheState::M, current_instruction_number);
        } else {
            // Read miss
            if (bus.shared_line) {
                // Data is in other caches, set to S
                tag_array[index][replace_idx] = make_tuple(tag, CacheState::S, current_instruction_number);
            } else {
                // Data is not in other caches, set to E
                tag_array[index][replace_idx] = make_tuple(tag, CacheState::E, current_instruction_number);
            }
        }
    }

    void handle_snoop(const string& address, Bus& bus, vector<Cache*>& caches) {
        Bits bits = parse(address);
        int index = bits.index_bits;
        int tag = bits.tag_bits;
        
        for (int i = 0; i < assosciativity; i++) {
            auto& [stored_tag, state, ts] = tag_array[index][i];
            if (stored_tag == tag && state != CacheState::I) {
                if (bus.invalidation) {
                    // Invalidation request
                    if (state == CacheState::M) {
                        // Need to write back modified data
                        stats.bus_write_back++;
                    }
                    tag_array[index][i] = make_tuple(stored_tag, CacheState::I, ts);
                    stats.bus_invalidations++;
                } else {
                    // Read request
                    if (state == CacheState::M) {
                        // Need to provide data and change to S
                        tag_array[index][i] = make_tuple(stored_tag, CacheState::S, ts);
                        bus.shared_line = true;
                    } else if (state == CacheState::E) {
                        // Need to provide data and change to S
                        tag_array[index][i] = make_tuple(stored_tag, CacheState::S, ts);
                        bus.shared_line = true;
                    } else if (state == CacheState::S) {
                        // Already shared, just set shared line
                        bus.shared_line = true;
                    }
                }
                return;
            }
        }
    }
};
  
struct Bus {
    bool busy = false;
    int cycle_remaining = 0;
    int source_cache = -1;
    int target_cache = -1;
    string address;
    bool shared_line = false;
    bool invalidation = false;
};

int main(int argc, char* argv[]) {
    string tracefile = "default_trace.txt"; // Default trace file
    int s = 6;                             // Default set index bits
    int E = 4;                             // Default associativity
    int b = 6;                             // Default block bits
    string outfilename = "default_output.txt"; // Default output file

    int opt;
    while ((opt = getopt(argc, argv, "t:s:E:b:o:h")) != -1) {
        switch (opt) {
            case 't':
                tracefile = optarg;
                break;
            case 's':
                s = stoi(optarg);
                break;
            case 'E':
                E = stoi(optarg);
                break;
            case 'b':
                b = stoi(optarg);
                break;
            case 'o':
                outfilename = optarg;
                break;
            case 'h':
                cout << "Usage: " << argv[0] << " -t <tracefile> -s <s> -E <E> -b <b> -o <outfilename> -h" << endl;
                cout << "  -t <tracefile>: name of parallel application (e.g. app1) whose 4 traces are to be used in simulation" << endl;
                cout << "  -s <s>: number of set index bits (number of sets in the cache = S = 2^s)" << endl;
                cout << "  -E <E>: associativity (number of cache lines per set)" << endl;
                cout << "  -b <b>: number of block bits (block size = B = 2^b)" << endl;
                cout << "  -o <outfilename>: logs output in file for plotting etc." << endl;
                cout << "  -h: prints this help" << endl;
                return 0;
            default:
                cerr << "Usage: " << argv[0] << " -t <tracefile> -s <s> -E <E> -b <b> -o <outfilename> -h" << endl;
                return 1;
        }
    }

    

    // Construct the trace file paths based on the tracefile name
    string trace_path_0 = "traces/" + tracefile + "_proc0.trace";
    string trace_path_1 = "traces/" + tracefile + "_proc1.trace";
    string trace_path_2 = "traces/" + tracefile + "_proc2.trace";
    string trace_path_3 = "traces/" + tracefile + "_proc3.trace";

    Cache cache0(s,E,b,trace_path_0);
    Cache cache1(s,E,b,trace_path_1);
    Cache cache2(s,E,b,trace_path_2);
    Cache cache3(s,E,b,trace_path_3);
    
    vector<Cache*> caches = {&cache0, &cache1, &cache2, &cache3};
    Bus bus;
    int cycle = 0;
    bool all_done = false;
    
    while (!all_done) {
        cycle++;
        all_done = true;
        
        // Process bus
        if (bus.busy) {
            bus.cycle_remaining--;
            if (bus.cycle_remaining <= 0) {
                // Bus transaction complete
                bus.busy = false;
                
                // If this was a read or write miss, update the target cache
                if (bus.target_cache >= 0) {
                    caches[bus.target_cache]->handle_bus_transaction_completion(bus, caches);
                }
            }
        }
        
        // Process each cache
        for (int i = 0; i < 4; i++) {
            Cache* cache = caches[i];
            
            // Check if any trace operations remain
            if (cache->current_instruction_number < cache->trace_data.size()) {
                all_done = false;
                
                if (cache->is_active) {
                    auto [op, address] = cache->trace_data[cache->current_instruction_number];
                    Bits bits = cache->parse(address);
                    miss_or_hit result = cache->hit_or_miss(bits);
                    
                    cache->stall_flag = false;
                    
                    if (result == miss_or_hit::HIT) {
                        if (op == operation::R) {
                            cache->read_hit(address, bus);
                        } else { // operation::W
                            cache->write_hit(address, bus, caches);
                        }
                    } else { // MISS
                        if (op == operation::R) {
                            cache->read_miss(address, bus, caches);
                        } else { // operation::W
                            cache->write_miss(address, bus, caches);
                        }
                    }
                    
                    if (!cache->stall_flag) {
                        cache->current_instruction_number++;
                    }
                } else {
                    cache->stats.idle_cycles++;
                    cache->waiting_time--;
                    if (cache->waiting_time <= 0) {
                        cache->is_active = true;
                        cache->waiting_time = 0;
                    }
                }
            }
            
            // Process snooping for each cache
            if (!bus.busy) continue;
            
            if (i != bus.target_cache && i != bus.source_cache) {
                cache->handle_snoop(bus.address, bus, caches);
            }
        }
        
        // Check for termination condition (e.g., max cycles or all traces processed)
        if (cycle > 1000000) { // Safety limit
            cout << "Reached maximum cycle limit" << endl;
            break;
        }
    }
    
    // Print statistics
    for (int i = 0; i < 4; i++) {
        Cache* cache = caches[i];
        cout << "============ Simulation results (Cache " << i << ") ============" << endl;
        cout << "01. number of reads:                " << cache->stats.reads << endl;
        cout << "02. number of read misses:          " << cache->stats.cache_misses - (cache->stats.writes - cache->stats.read_misses) << endl;
        cout << "03. number of writes:               " << cache->stats.writes << endl;
        cout << "04. number of write misses:         " << cache->stats.writes - cache->stats.read_misses << endl;
        cout << "05. total miss rate:                " << fixed << setprecision(2) 
             << (cache->stats.cache_misses * 100.0) / (cache->stats.reads + cache->stats.writes) << '%' << endl;
        cout << "06. number of write backs:          " << cache->stats.write_back << endl;
        cout << "07. number of cache to cache transfers: " << cache->stats.cache_to_cache_transfers << endl;
        cout << "08. number of memory transactions:      " << cache->stats.memory_transactions << endl;
        cout << "09. number of interventions:            " << cache->stats.interventions << endl;
        cout << "10. number of invalidations:            " << cache->stats.bus_invalidations << endl;
        cout << "11. number of flushes:                  " << cache->stats.flushes << endl;
    }
    
    return 0;
}




