// Console input/output helpers (Presentation layer). All prompts re-ask on invalid input.
#pragma once

#include <iosfwd>
#include <optional>
#include <string>
#include <vector>

namespace lms {

// Thrown when standard input ends (Ctrl+D / Ctrl+Z or piped input finished). Deliberately NOT a
// std::exception so that error handlers for ordinary failures never swallow it.
struct InputClosed {};

class Console {
public:
    Console(std::istream& in, std::ostream& out, bool interactive);

    std::string line(const std::string& prompt);                 // trimmed, may be empty
    std::string requiredLine(const std::string& prompt);         // re-asks until non-empty
    long long integer(const std::string& prompt, long long min, long long max);
    double number(const std::string& prompt);
    bool yesNo(const std::string& prompt);
    std::string password(const std::string& prompt);             // not echoed on a terminal
    // Shows numbered options and returns the chosen number (0 = last "back/exit" option).
    int menu(const std::string& title, const std::vector<std::string>& options);

    void say(const std::string& text = "");
    void heading(const std::string& text);
    void error(const std::string& text);
    static std::string pad(const std::string& text, std::size_t width);

private:
    std::istream& in_;
    std::ostream& out_;
    bool interactive_;
};

}  // namespace lms
