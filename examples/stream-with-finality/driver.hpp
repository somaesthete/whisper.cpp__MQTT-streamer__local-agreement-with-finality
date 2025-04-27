#ifndef DRIVER_HPP
#define DRIVER_HPP

#include <string>
#include <vector>
#include <deque>

using namespace std;

class driver {
public:
    driver(int BUFFER_LEN = 4, int LOCAL_AGREEMENT_N = 2, int PROMPT_LEN = 100);
    tuple<vector<string>, deque<vector<string>>, vector<string>> drive(const string& line);

private:
    deque<vector<string>> ctxBuffer;
    int BUFFER_LEN;
    int LOCAL_AGREEMENT_N;
    int PROMPT_LEN;
    vector<string> committed_tokens;
    bool DEBUG;
    int lines_read;

    static vector<string> fnDriverStep(const vector<string>& prompt, const deque<vector<string>>& buffer, int LOCAL_AGREEMENT_N);
};

#endif // DRIVER_HPP