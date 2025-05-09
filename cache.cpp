#include <iostream>
#include <vector>
#include <tuple>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <bitset>
#include <getopt.h>
#include <climits>

using namespace std;

enum CacheState { I,M, E, S };
enum operation {R, W};
enum miss_or_hit {HIT, MISS};

using CacheLineMeta = tuple<int, CacheState, int>;

struct Statistics {
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
    int data_traffic_in_bytes = 0;
};

struct Bits {
    int tag_bits;
    int index_bits;
    int offset_bits;
};

struct Bus {
    bool busy = false;
    int cycle_remaining = 0;
    int source_cache = -1;
    int target_cache = -1;
    string address;
    bool invalidation = false;
    CacheState set_state;

    int transactions=0;
    int BusRd = 0;
    int BusRdX = 0;
    int BusInv = 0;
    int traffic=0;

    // Extend Bus struct to support multi-step transactions
    enum TransactionType { NONE, CACHE_TO_CACHE, WRITE_BACK };
    TransactionType transaction_type = NONE;
    int pending_writeback_cache = -1; // which cache needs to write back after transfer
};

class Cache {
public:
    vector<vector<CacheLineMeta>> tag_array;
    vector<vector<vector<int>>> data_array;
    Statistics stats;
    int num_sets;
    int set_bits;
    int associativity;
    int offset_bits;
    int blocksize_in_bytes;
    bool stall_flag = false;
    int current_instruction_number = 0;
    bool is_active = true;
    int waiting_time = 0;
    int cache_id = -1;
    vector<pair<operation, string>> trace_data;

    // Constructor
    Cache(int set_bits, int num_ways, int cache_line_bits, string filepath, int id) {
        num_sets = (1 << set_bits);
        this->set_bits = set_bits;
        associativity = num_ways;
        offset_bits = cache_line_bits;
        blocksize_in_bytes = (1 << cache_line_bits);
        cache_id = id;
        tag_array.resize(num_sets, vector<CacheLineMeta>(num_ways, {-1, CacheState::I, -1}));
        data_array.resize(num_sets, vector<vector<int>>(num_ways, vector<int>(blocksize_in_bytes, 0)));
        process_trace_file(filepath, trace_data);
    }

    // Helper: pretty-print the state
    static string state_to_string(CacheState state) {
        switch (state) {
            case CacheState::M: return "M";
            case CacheState::E: return "E";
            case CacheState::S: return "S";
            case CacheState::I: return "I";
            default: return "?";
        }
    }

    // Print tag array (for debugging)
    void print_tag_array() {
        cout << "Tag Array for Cache " << cache_id << ":\n";
        for (int set = 0; set < tag_array.size(); ++set) {
            cout << "Set " << set << ": ";
            for (auto& [tag, state, ts] : tag_array[set]) {
                cout << "[Tag: " << tag << ", State: " << state_to_string(state) << ", Time: " << ts << "] ";
            }
            cout << '\n';
        }
    }

    void process_trace_file(string file_path, vector<pair<operation, string>>& trace_data) {
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
            hex_address = hex_address.substr(2); // Remove "0x" prefix
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
        
        struct Bits bits;
        bits.offset_bits = addr & ((1 << offset_bits) - 1); // Extract offset bits
        addr >>= offset_bits; // Shift right by offset bits
        bits.index_bits = addr & ((1 << set_bits) - 1); // Extract index bits
        addr >>= set_bits; // Shift right by index bits
        bits.tag_bits = addr; // Remaining bits are tag bits
        return bits;
    }

    // Update the timestamp for a cache line (LRU policy)
    void update_timestamp(int index, int way, int current_time) {
        auto& [tag, state, ts] = tag_array[index][way];
        ts = current_time;
    }

    // Find the way containing a specific tag in a set, or return -1 if not found
    int find_way(int index, int tag) {
        for (int i = 0; i < associativity; i++) {
            auto& [stored_tag, state, ts] = tag_array[index][i];
            if (stored_tag == tag && state != CacheState::I) {
                return i;
            }
        }
        return -1;
    }

    // Find a way to replace (either empty or LRU)
    int find_replacement_way(int index) {
        for (int i = 0; i < associativity; i++) {
            auto& [stored_tag, state, ts] = tag_array[index][i];
            if (state == CacheState::I) {
                return i;
            }
        }

        int replace_idx = 0;
        int min_ts = INT_MAX;
        
        for (int i = 0; i < associativity; i++) {
            auto& [stored_tag, state, ts] = tag_array[index][i];
            if (ts < min_ts) {
                min_ts = ts;
                replace_idx = i;
            }
        }
        
        return replace_idx;
    }

    void read_hit(const string& address, Bus& bus) {
        bus.invalidation = false;
        Bits bits = parse(address);
        int index = bits.index_bits;
        int tag = bits.tag_bits;
        int way = find_way(index, tag);
        
        if (way != -1) {
            update_timestamp(index, way, current_instruction_number);
        }else {
            cerr << "Error: Way not found in read_hit" << endl;
            return;
        }
        
        stats.instructions++;
        stats.reads++;
        stats.execution_cycles++;
    }

    void write_hit(const string& address, Bus& bus, vector<Cache*>& caches) {
        
        
        Bits bits = parse(address);
        int index = bits.index_bits;
        int tag = bits.tag_bits;
        int way = find_way(index, tag);
        
        if (way == -1) {
            cerr << "Error: Way not found in write_hit" << endl;
            return;
        }
        
        auto& [stored_tag, state, ts] = tag_array[index][way];
        
        update_timestamp(index, way, current_instruction_number);
        
        if (state == CacheState::M || state == CacheState::E) {
            tag_array[index][way] = make_tuple(tag, CacheState::M, current_instruction_number);
            stats.instructions++;
            stats.writes++;
            stats.execution_cycles++;
            bus.invalidation = false;
            is_active = true;
            waiting_time = 0;
            stall_flag = false;
            return;

        }else if (state == CacheState::S) {
            if (bus.busy) {
                stats.idle_cycles++;
                stall_flag = true;
                return;
            }
            bus.busy = true;
            bus.cycle_remaining = 1; 
            stall_flag = true;
            is_active = false;
            waiting_time=1;
            bus.target_cache = cache_id;
            bus.address = address;
            bus.invalidation = true;
            bus.BusInv++;
            for (auto& cache : caches) {
                if (cache->cache_id != cache_id) {
                    int other_way = cache->find_way(index, tag);
                    if (other_way != -1) {
                        auto& [other_stored_tag, other_state, other_ts] = cache->tag_array[index][other_way];                 
                        cache->tag_array[index][other_way] = make_tuple(other_stored_tag, CacheState::I, other_ts);
                    }
                }
            }
            stats.writes++;
            stats.bus_invalidations++;
            stats.execution_cycles++;
        }else{
            cerr << "Error: Invalid state in write_hit" << endl;
            return;
        }
    }

    void read_miss(const string& address, Bus& bus, vector<Cache*>& caches) {
        
        if (bus.busy) {
            stall_flag = true;
            stats.idle_cycles++;
            return;
        }
        
        bus.BusRd++;
        bus.invalidation = false;
        Bits bits = parse(address);
        int index = bits.index_bits;
        int tag = bits.tag_bits;
        
        // Check if another cache has this data
        bool shared = false;
        int source_cache = -1;
        bool writing_back = false;
        for (int i = 0; i < caches.size(); i++) {
            if (i == cache_id) continue;
            
            Cache* other_cache = caches[i];
            int other_way = other_cache->find_way(index, tag);
            
            if (other_way != -1) {
                auto& [stored_tag, state, ts] = other_cache->tag_array[index][other_way];
                if(state == CacheState::I) {
                    continue; // Invalid state, skip
                }
                shared = true;
                source_cache = i;
                if (state == CacheState::M) {
                    writing_back = true;
                }
                other_cache->tag_array[index][other_way] = make_tuple(stored_tag, CacheState::S, ts);
            }
        }
        
        // Start bus transaction
        bus.busy = true;
        bus.target_cache = cache_id;
        bus.address = address;
        bus.invalidation = false;
        
        // Set this cache as waiting
        is_active = false;
        bus.set_state = CacheState::S;
        if (writing_back) {
            // First, do cache-to-cache transfer
            bus.cycle_remaining = blocksize_in_bytes/2;
            waiting_time = blocksize_in_bytes/2;
            bus.traffic += blocksize_in_bytes;
            
            caches[source_cache]->stats.data_traffic_in_bytes += blocksize_in_bytes;
            // Set up for second transaction (write-back)
            bus.transaction_type = Bus::CACHE_TO_CACHE;
            bus.pending_writeback_cache = source_cache;
        } else if (shared) {
            bus.cycle_remaining = blocksize_in_bytes/2; // 1 cycle for read
            waiting_time = blocksize_in_bytes/2;
            bus.traffic += blocksize_in_bytes;
            
            caches[source_cache]->stats.data_traffic_in_bytes += blocksize_in_bytes;
        } else {
            bus.set_state = CacheState::E;
            bus.cycle_remaining = 100;
            waiting_time = 100;
            bus.traffic += blocksize_in_bytes;
            
        }
        stats.data_traffic_in_bytes += blocksize_in_bytes;
        stats.execution_cycles++;
        stats.cache_misses++;
        stall_flag = true;
        stats.reads++;
    }

    void write_miss(const string& address, Bus& bus, vector<Cache*>& caches) {
        
        if (bus.busy) {
            stall_flag = true;
            stats.idle_cycles++;
            return;
        }
        
        bus.invalidation = false;
        Bits bits = parse(address);
        int index = bits.index_bits;
        int tag = bits.tag_bits;
        
        bool writing_back = false;
        bool invalidated = false;
        bus.BusRdX++;
        for (int i = 0; i < caches.size(); i++) {
            if (i == cache_id) continue;
            
            Cache* other_cache = caches[i];
            int other_way = other_cache->find_way(index, tag);
            
            if (other_way != -1) {
                auto& [stored_tag, state, ts] = other_cache->tag_array[index][other_way];
                if (state == CacheState::I) {
                    continue; // Invalid state, skip
                }
                invalidated = true;
                if (state == CacheState::M) {
                    writing_back = true;
                }
                other_cache->tag_array[index][other_way] = make_tuple(stored_tag, CacheState::I, ts);
            }
        }
        
        if(invalidated){
            stats.bus_invalidations++;
        }
        // Start bus transaction
        bus.busy = true;
        bus.target_cache = cache_id;
        bus.address = address;
        
        // Set this cache as waiting
        is_active = false;
        bus.set_state = CacheState::M;
        if(writing_back) {
            bus.BusRdX++;
            bus.cycle_remaining = 200; // 2 cycles for write
            waiting_time = 200;
            bus.traffic += 2 * blocksize_in_bytes;
            caches[bus.target_cache]->stats.data_traffic_in_bytes +=  blocksize_in_bytes;
            caches[bus.target_cache]->stats.write_back++;
        } else{
            bus.cycle_remaining = 100;
            waiting_time = 100;
            bus.traffic += blocksize_in_bytes;
        }
        
        stats.data_traffic_in_bytes += blocksize_in_bytes;
        stats.cache_misses++;
        stats.execution_cycles++;

        stall_flag=true;
        stats.writes++;
    }

    void handle_bus_transaction_completion(Bus& bus, vector<Cache*>& caches) {
        Bits bits = parse(bus.address);
        int index = bits.index_bits;
        int tag = bits.tag_bits;

        bus.busy = false;
        bus.cycle_remaining = 0;
        is_active = true;
        stall_flag = false;
        waiting_time = 0;
        // Complete the state transition for write_hit on Shared state
        if (bus.invalidation) {
            int way = find_way(index, tag);
            if (way != -1) {
                auto& [stored_tag, state, ts] = tag_array[index][way];
                if (state == CacheState::S) {
                    tag_array[index][way] = make_tuple(tag, CacheState::M, current_instruction_number);
                    current_instruction_number++;
                    
                    return;
                }else{
                    cerr << "Error: state should have been S when issuing invalidate" << endl;
                    return;
                }
            }else{
                cerr << "Error: Way not found in handle_bus_transaction_completion" << endl;
                return;
            }
        }
        bus.target_cache=-1;
        
        int replace_way = find_replacement_way(index);
        auto& [old_tag, old_state, old_ts] = tag_array[index][replace_way];
        
        if (old_state == CacheState::M) {
            stats.write_back++;
            stats.data_traffic_in_bytes += blocksize_in_bytes;
            bus.traffic += blocksize_in_bytes;
            bus.cycle_remaining = 100;
            bus.address = bus.address;
            bus.invalidation = false;
            waiting_time = 100;
            is_active = false;
            stall_flag = true;
            bus.target_cache = cache_id;
            bus.busy=true;
            stats.cache_evictions++;
            tag_array[index][replace_way] = make_tuple(tag, CacheState::I, current_instruction_number);            
            return;
        }
        
        if(old_state != CacheState::I){
            stats.cache_evictions++;
        }
        
        tag_array[index][replace_way] = make_tuple(tag, bus.set_state, current_instruction_number);


        if (bus.transaction_type == Bus::CACHE_TO_CACHE) {
            // Now schedule the write-back
            bus.busy = true;
            bus.transaction_type = Bus::WRITE_BACK;
            bus.target_cache = -1;
            bus.cycle_remaining = 100; // Write-back to memory
            bus.traffic += blocksize_in_bytes;
            caches[bus.pending_writeback_cache]->stats.data_traffic_in_bytes += blocksize_in_bytes;
            caches[bus.pending_writeback_cache]->stats.write_back++;
            // Keep bus busy for write-back
            
        } else if (bus.transaction_type == Bus::WRITE_BACK) {
            cerr<<"it should not have any target cache"<<endl;
        }
        // stats.execution_cycles++;
        current_instruction_number++;
        stats.instructions++;

    }

};

int main(int argc, char* argv[]) {
    string tracefile = "default_trace.txt"; // Default trace file
    int s = 6;                             // Default set index bits
    int E = 2;                             // Default associativity
    int b = 5;                             // Default block bits
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

    Cache cache0(s, E, b, trace_path_0, 0);
    Cache cache1(s, E, b, trace_path_1, 1);
    Cache cache2(s, E, b, trace_path_2, 2);
    Cache cache3(s, E, b, trace_path_3, 3);
    
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
                if (bus.target_cache >= 0) {
                    // cout<<"cache "<<bus.target_cache<<"did execution in cycle"<<cycle<<"has instruction"<<caches[bus.target_cache]->stats.execution_cycles<<endl;
                    caches[bus.target_cache]->handle_bus_transaction_completion(bus, caches);
                }else{
                    bus.busy = false;
                    bus.cycle_remaining = 0;
                    bus.target_cache = -1;
                    bus.address = "";
                    bus.invalidation = false;
                    bus.transaction_type=Bus::NONE;
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
                            // cout<<"Cache " << cache->cache_id << " read hit in cycle " << cycle << endl;
                            cache->read_hit(address, bus );
                        } else { 
                            // cout<<"Cache " << cache->cache_id << " write hit in cycle " << cycle << endl;
                            cache->write_hit(address, bus, caches);
                        }
                    } else {
                        if (op == operation::R) {
                            // cout<<"Cache " << cache->cache_id << " read miss in cycle " << cycle << endl;
                            cache->read_miss(address, bus, caches);
                        } else { 
                            // cout<<"Cache " << cache->cache_id << " write miss in cycle " << cycle << endl;
                            cache->write_miss(address, bus, caches);
                        }
                    }
                    
                    if (!cache->stall_flag) {
                        cache->current_instruction_number++;
                    }
                } else {
                    cache->stats.execution_cycles++;
                    // cout<<"cache " << cache->cache_id << "did execution in " << cycle << "and has "<<cache->stats.execution_cycles<<" instructions"<<endl;
                    cache->waiting_time--;
                    if (cache->waiting_time <= 0) {
                        cache->is_active = true;
                        cache->waiting_time = 0;
                    }
                }
            }
        }
    }
    
    // Output statistics to file if requested
    ofstream outfile;
    if (outfilename != "default_output.txt") {
        outfile.open(outfilename);
        if (!outfile.is_open()) {
            cerr << "Error: Could not open output file " << outfilename << endl;
        }
    }
    

    cout << "==================== SIMULATION PARAMETERS ======================" << endl;
    cout << "01. Trace Prefix:                      " << tracefile << endl;
    cout << "02. Set Index Bits:                    " << s << endl;
    cout << "03. Associativity:                     " << E << endl;
    cout << "04. Block Bits:                        " << b << endl;
    cout << "05. Block Size (Bytes):                " << (1 << b) << endl;
    cout << "06. Number of Sets:                    " << (1 << s) << endl;
    cout << "07. Cache Size (KB per core):          " << ((1 << s) * E * (1 << b)) / 1024 << " KB" << endl;
    cout << "08. MESI Protocol:                     Enabled" << endl;
    cout << "09. Write Policy:                      Write-back, Write-allocate" << endl;
    cout << "10. Replacement Policy:                LRU" << endl;
    cout << "11. Bus:                               Central snooping bus" << endl;
   cout<<endl;
    cout << "==================== SIMULATION STATISTICS =====================" << endl;
    
    cout << "******** Program execution completed ******** in " << cycle << "cycles ********"<< endl;
    // Print statistics
    for (int i = 0; i < 4; i++) {
        Cache* cache = caches[i];
        float miss_rate = 0.0;
        cache->stats.instructions = cache->stats.reads+cache->stats.writes;
        if(cache->stats.execution_cycles){
            cache->stats.execution_cycles++;
        }
        if (cache->stats.reads + cache->stats.writes > 0) {
            miss_rate = (cache->stats.cache_misses * 100.0) / (cache->stats.reads + cache->stats.writes);
        }
        
        cout << "============ Simulation results (Cache " << i << ") ============" << endl;
        cout << "01. number of instructions:            " << cache->stats.instructions << endl;
        cout << "02. number of reads:                   " << cache->stats.reads << endl;
        cout << "03. number of writes:                  " << cache->stats.writes << endl;
        cout << "04. number of execution cycles:        " << cache->stats.execution_cycles << endl;
        cout << "05. number of idle cycles:             " << cache->stats.idle_cycles << endl;
        cout << "06. number of cache misses:            " << cache->stats.cache_misses << endl;
        cout << "07. cache miss rate:                   " << fixed << setprecision(2) << miss_rate << '%' << endl;
        cout << "08. number of cache evictions:         " << cache->stats.cache_evictions << endl;
        cout << "09. number of write backs:             " << cache->stats.write_back << endl;
        cout << "10. number of invalidations:           " << cache->stats.bus_invalidations << endl;
        cout << "11. data traffic in bytes:             " << cache->stats.data_traffic_in_bytes << endl;  
        cout << "12. total cycles                       " << cache->stats.execution_cycles+cache->stats.idle_cycles << endl;
        // Write to output file if open
        if (outfile.is_open()) {
            // eventually write to the file
            outfile << "Cache " << i << " Statistics:" << endl;
            outfile << "01. number of instructions:            " << cache->stats.instructions << endl;
            outfile << "02. number of reads:                   " << cache->stats.reads << endl;
            outfile << "03. number of writes:                  " << cache->stats.writes << endl;
            outfile << "04. number of execution cycles:        " << cache->stats.execution_cycles << endl;
            outfile << "05. number of idle cycles:             " << cache->stats.idle_cycles << endl;
            outfile << "06. number of cache misses:            " << cache->stats.cache_misses << endl;
            outfile << "07. cache miss rate:                   " << fixed << setprecision(2) << miss_rate << '%' << endl;
            outfile << "08. number of cache evictions:         " << cache->stats.cache_evictions << endl;
            outfile << "09. number of write backs:             " << cache->stats.write_back << endl;
            outfile << "10. number of invalidations:           " << cache->stats.bus_invalidations << endl;
            outfile << "11. data traffic in bytes:             " << cache->stats.data_traffic_in_bytes << endl;
            outfile << "12. total cycles                       " << cache->stats.execution_cycles+cache->stats.idle_cycles << endl;
            
        }
    }
    
    cout << "==================== BUS STATISTICS =============================" << endl;
    cout << "01. number of transactions:            " << bus.BusRd + bus.BusRdX + bus.BusInv << endl;
    cout << "02. data traffic in bytes:             " << bus.traffic << endl;


    if (outfile.is_open()) {
        outfile.close();
    }
    
    return 0;
}