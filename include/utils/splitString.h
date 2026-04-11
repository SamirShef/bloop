#pragma once
#include <sstream>
#include <string>
#include <vector>

namespace bloop {

inline std::vector<std::string>
splitString(const std::string &str, char separator) {
    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string token;
    while (std::getline(ss, token, separator)) {
        tokens.push_back(token);
    }
    return tokens;
}

}