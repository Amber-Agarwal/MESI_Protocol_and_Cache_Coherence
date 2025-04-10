#include <iostream>
#include <vector>
#include <tuple>
#include <fstream>
#include <sstream>
#include <iomanip>

using namespace std;

enum  CacheState { M, E, S, I };
enum  operation {R,W};

using CacheLineMeta = tuple<int, CacheState, int>;

class Cache {
public:
    vector<vector<CacheLineMeta>> tag_array;

    vector<vector<vector<int>>> data_array;

    // Constructor
    Cache(int num_sets, int num_ways, int line_size_bytes) {
        tag_array.resize(num_sets, vector<CacheLineMeta>(num_ways, { -1, CacheState::I, -1 }));

        data_array.resize(num_sets, vector<vector<int>>(num_ways, vector<int>(line_size_bytes, 0)));
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

    operation hit_or_miss (struct )





    


};
