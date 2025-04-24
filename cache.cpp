#include <iostream>
#include <vector>
#include <tuple>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <bitset>

using namespace std;

enum  CacheState { M, E, S, I };
enum  operation {R,W};
enum miss_or_hit {HIT,MISS};

using CacheLineMeta = tuple<int, CacheState, int>;

struct Statictics {
    int instructions;
    int reads;
    int writes;
    int execution_cycles;
    int idle_cycles;
    int cache_misses;
    float cache_miss_rate;
    int cache_evictions;
    int write_back;
    int bus_invalidations;
    int bus_write_back;
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
    // Constructor
    Cache(int set_bits, int num_ways, int cache_line_bits) {
        num_sets = (1<<set_bits);
        this->set_bits = set_bits;
        assosciativity = num_ways;
        offset_bits = cache_line_bits;
        blocksize_in_bytes = (1<<cache_line_bits);
        tag_array.resize(num_sets, vector<CacheLineMeta>(num_ways, { -1, CacheState::I, -1 }));
        data_array.resize(num_sets, vector<vector<int>>(num_ways, vector<int>(cache_line_bits, 0)));
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

    void process_trace_file(const string& file_path, vector<pair<operation, string>>& trace_data) {
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
};
  


