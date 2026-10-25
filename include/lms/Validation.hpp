// Input validation shared by the services. Every function throws ValidationError with a message
// the UI can show directly to the user.
#pragma once

#include <string>

namespace lms {

std::string trim(const std::string& text);
std::string toLower(const std::string& text);
bool containsIgnoreCase(const std::string& haystack, const std::string& needle);

// Returns the trimmed text; throws when empty or longer than maxLength.
std::string requireText(const std::string& text, const std::string& fieldName, std::size_t maxLength);
// 3-30 characters: letters, digits, '.', '_' or '-'.
std::string requireUsername(const std::string& username);
// At least 8 characters, containing a letter and a digit.
void requirePasswordPolicy(const std::string& password);
// Finite number within [0, maximum], rounded to 2 decimal places.
double requireMark(double value, double maximum, const std::string& fieldName);
std::string formatMark(double value);  // "72.50"

}  // namespace lms
