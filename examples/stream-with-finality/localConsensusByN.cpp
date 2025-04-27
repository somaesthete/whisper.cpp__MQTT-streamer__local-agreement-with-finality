// This file contains the implementation of the localConsensusByN function which
// computes a consensus on tokens from a buffer of token lists based on a threshold n.
#include "localConsensusByN.hpp"
#include <unordered_map>
#include <algorithm>

/**
 * Computes a local consensus on tokens from a buffer of token lists.
 * 
 * @param buffer A vector of vectors containing strings representing token lists.
 * @param n An integer threshold for determining consensus.
 * @return A vector of strings representing the consensus tokens.
 */
std::vector<std::string> localConsensusByN(const std::vector<std::vector<std::string>>& buffer, int n) {
    std::vector<std::string> result;
    // Initialize an empty result vector to store consensus tokens.
    if (buffer.empty()) {
    // Return an empty result if the input buffer is empty.
        return result;
    }

    size_t max_length = 0;
    // Determine the maximum length of the token lists in the buffer.
    for (const auto& lst : buffer) {
        max_length = std::max(max_length, lst.size());
    }

    for (size_t i = 0; i < max_length; ++i) {
    // Iterate over each position in the token lists up to the maximum length.
        std::unordered_map<std::string, int> token_counts;
    // Create a map to count the occurrences of each token at the current position.
        for (const auto& inferenceTokens : buffer) {
    // Loop through each token list in the buffer.
            if (i >= inferenceTokens.size()) {
    // Skip if the current position is beyond the length of the token list.
                continue;
            }

            std::string token = inferenceTokens[i];
    // Convert the current token to lowercase for case-insensitive comparison.
            std::transform(token.begin(), token.end(), token.begin(), ::tolower);

            ++token_counts[token];
    // Increment the count for the current token.
        }

        for (const auto& [token, count] : token_counts) {
    // Check each token's count to see if it meets or exceeds the threshold n.
            if (count >= n) {
                result.push_back(token);
    // If the token meets the threshold, add it to the result and stop checking further.
                break;
            }
        }
    }

    return result;
    // Return the final vector containing the consensus tokens.
}