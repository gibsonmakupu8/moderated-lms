#include "lms/Validation.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>

#include "lms/Common.hpp"

namespace lms {

std::string trim(const std::string& text) {
    std::size_t begin = 0, end = text.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(text[begin]))) ++begin;
    while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1]))) --end;
    return text.substr(begin, end - begin);
}

std::string toLower(const std::string& text) {
    std::string out = text;
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

bool containsIgnoreCase(const std::string& haystack, const std::string& needle) {
    return toLower(haystack).find(toLower(needle)) != std::string::npos;
}

std::string requireText(const std::string& text, const std::string& fieldName, std::size_t maxLength) {
    std::string value = trim(text);
    if (value.empty()) throw ValidationError(fieldName + " is required.");
    if (value.size() > maxLength) {
        throw ValidationError(fieldName + " must be at most " + std::to_string(maxLength) + " characters.");
    }
    return value;
}

std::string requireUsername(const std::string& username) {
    std::string value = trim(username);
    if (value.size() < 3 || value.size() > 30) throw ValidationError("Username must be 3 to 30 characters long.");
    for (unsigned char c : value) {
        if (!(std::isalnum(c) || c == '.' || c == '_' || c == '-')) {
            throw ValidationError("Username may contain only letters, digits, '.', '_' and '-'.");
        }
    }
    return value;
}

void requirePasswordPolicy(const std::string& password) {
    bool letter = false, digit = false;
    for (unsigned char c : password) {
        letter = letter || std::isalpha(c);
        digit = digit || std::isdigit(c);
    }
    if (password.size() < 8 || password.size() > 128 || !letter || !digit) {
        throw ValidationError("Password must be 8-128 characters and contain at least one letter and one digit.");
    }
}

double requireMark(double value, double maximum, const std::string& fieldName) {
    if (!std::isfinite(value)) throw ValidationError(fieldName + " must be a number.");
    if (value < 0 || value > maximum) {
        throw ValidationError(fieldName + " must be between 0 and " + formatMark(maximum) + ".");
    }
    return std::round(value * 100.0) / 100.0;
}

std::string formatMark(double value) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.2f", value);
    return buf;
}

}  // namespace lms
