#include <iostream>
#include <vector>
#include <tuple>

using namespace std;

enum class CacheState { M, E, S, I };

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
};
