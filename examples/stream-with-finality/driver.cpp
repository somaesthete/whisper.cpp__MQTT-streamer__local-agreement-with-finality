#include "driver.hpp"
#include "localConsensusByN.hpp"
#include "overlapIndex.hpp"
#include <algorithm>
#include <iostream>
#include <sstream>

using namespace std;

driver::driver(int BUFFER_LEN, int LOCAL_AGREEMENT_N, int PROMPT_LEN)
    : BUFFER_LEN(BUFFER_LEN), LOCAL_AGREEMENT_N(LOCAL_AGREEMENT_N), PROMPT_LEN(PROMPT_LEN), lines_read(0) {
    for (int i = 0; i < BUFFER_LEN; ++i) {
        ctxBuffer.emplace_back();
    }
}

tuple<vector<string>, deque<vector<string>>, vector<string>> driver::drive(const string& line) {
    // Tokenize and insert the line into the buffer
    vector<string> lineSanitizedTokens;
    istringstream iss(line);
    for (string token; iss >> token;) {
        lineSanitizedTokens.push_back(token);
    }
    if (ctxBuffer.size() == BUFFER_LEN) {
        ctxBuffer.pop_front();  // Remove from the front for LIFO behavior
    }
    ctxBuffer.push_back(lineSanitizedTokens);

    // Prepare prompt
    vector<string> prompt;
    if (committed_tokens.size() > PROMPT_LEN) {
        prompt.assign(committed_tokens.end() - PROMPT_LEN, committed_tokens.end());
    } else {
        prompt = committed_tokens;
    }

    // Get new tokens using the driver step function
    vector<string> newTokens = fnDriverStep(prompt, ctxBuffer, LOCAL_AGREEMENT_N);
    committed_tokens.insert(committed_tokens.end(), newTokens.begin(), newTokens.end());

    return {newTokens, ctxBuffer, committed_tokens};
}

vector<string> driver::fnDriverStep(const vector<string>& prompt, const deque<vector<string>>& buffer, int LOCAL_AGREEMENT_N) {
    vector<vector<string>> candidateBuffer;
    for (const auto& transcription : buffer) {
        candidateBuffer.push_back(vector<string>(transcription.begin() + overlapIndex(prompt, transcription), transcription.end()));
    }

    return localConsensusByN(candidateBuffer, LOCAL_AGREEMENT_N);
}