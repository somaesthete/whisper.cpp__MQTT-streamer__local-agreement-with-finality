#include "../overlapIndex.hpp"
#include <iostream>
#include <vector>
#include <string>
#include <cassert>

using namespace std;

void test_overlapIndex() {
    
    vector<string> prompt;
    vector<string> transcription;
    int result_idx;


    // test 0
    prompt = {"Hello", "and..."};
    transcription = {"Hello", "and", "we", "are", "cooking"};
    result_idx = overlapIndex(prompt, transcription);

    int expected_idx = 2;

    cout << "test 0" << endl;
    cout << "======" << endl << result_idx << " : " <<  expected_idx << endl << endl;
    assert(result_idx == expected_idx && "Test failed. No match."); // TODO: these are not tripping for some reason?


    // test 1
    prompt = {"We're", "transcribing", "with", "a", "100", "millisecond", "latency,"};
    transcription = {"transcribing", "with", "a", "100ms", "latency", "and", "we're", "going", "to", "implement", "local", "consensus."};
    result_idx = overlapIndex(prompt, transcription);

    expected_idx = 6;
    cout << "test 1" << endl;
    cout << "======" << endl << result_idx << " : " << expected_idx << endl;
    assert(result_idx == expected_idx && "Test failed. No match."); // TODO: these are not tripping for some reason?
}

int main() {
    test_overlapIndex();
    cout << "\nAll tests passed: Output indexes match expected values." << endl;
    return 0;
}