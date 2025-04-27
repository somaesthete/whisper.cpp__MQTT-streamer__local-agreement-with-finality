#include "overlapIndex.hpp"
#include <algorithm>
#include <cctype>
#include <locale>

// Helper function to clean and compare strings
bool cleanAndCompare(const std::string& left, const std::string& right) {
    auto cleanString = [](const std::string& str) {
        std::string cleaned;
        std::remove_copy_if(str.begin(), str.end(), std::back_inserter(cleaned),
                            [](char c) { return std::ispunct(c); });
        std::transform(cleaned.begin(), cleaned.end(), cleaned.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        return cleaned;
    };
    return cleanString(left) == cleanString(right);
}

// Function to calculate the overlap index
int overlapIndex(const std::vector<std::string>& prompt, const std::vector<std::string>& transcription) {
    int maxOverlap = std::min(prompt.size(), transcription.size());
    int bestMatch = 0;
    int bestMatchIndex = 0;

    for (int i = 1; i <= maxOverlap; ++i) {
        int thisMatches = 0;
        for (int j = 0; j < i; ++j) {
            if (cleanAndCompare(prompt[prompt.size() - i + j], transcription[j])) {
                ++thisMatches;
            }
        }
        if (thisMatches > bestMatch) {
            bestMatch = thisMatches;
            bestMatchIndex = i;
        }
    }

    return bestMatchIndex;
}