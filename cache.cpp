#include <iostream>
#include <vector>
#include <tuple>
#include <fstream>
#include <sstream>
#include <iomanip>

using namespace std;

enum  CacheState { M, E, S, I };
enum  operation {R,W};
enum miss_or_hit {HIT,MISS};

using CacheLineMeta = tuple<int, CacheState, int>;

struct Bits {
    int tag_bits;
    int index_bits;
    int offset_bits;
};

class Cache {
public:
    vector<vector<CacheLineMeta>> tag_array;
    vector<vector<vector<int>>> data_array;
    int num_sets;
    int assosciativity;
    int offset_bits;
    int blocksize_in_bytes;
    // Constructor
    Cache(int num_sets, int num_ways, int line_size_bytes) {
        tag_array.resize(num_sets, vector<CacheLineMeta>(num_ways, { -1, CacheState::I, -1 }));
        data_array.resize(num_sets, vector<vector<int>>(num_ways, vector<int>(line_size_bytes, 0)));
        num_sets = num_sets;
        assosciativity = num_ways;
        offset_bits = line_size_bytes;
        blocksize_in_bytes = (1<<line_size_bytes);
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
        
        // TODO: Parse the address into tag, index, and offset bits
        
        // Placeholder return - replace with actual parsed values
        return {0, 0, 0};
    }
    
};
