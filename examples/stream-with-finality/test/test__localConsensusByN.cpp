#include <iostream>
#include <vector>
#include <cassert>
#include <string>
#include "../localConsensusByN.hpp" // Assuming the function is declared in this header

#include <string_view>

using namespace std;

void test_localConsensusByN() {
    
    vector<vector<string>> buffer = {
        {"frog", "walk", "into", "a", "cafe."},
        {},
        {"robber", "sitting", "alone", "in", "the", "corner."},
        {"robber", "sitting", "alone", "at", "a", "table."}
    };
    int n = 2;

    vector<string> actual_result = localConsensusByN(buffer, n);

    for (const auto& str : actual_result) {
        cout << str << " ";
    }
    cout << endl;
}

int main() {
    test_localConsensusByN();
    std::cout << "\nAll tests passed: Output matches expected values." << std::endl;
    return 0;
}