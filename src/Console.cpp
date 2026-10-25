#include "lms/Console.hpp"

#include <cstdlib>
#include <iostream>
#include <limits>

#include "lms/Validation.hpp"

#ifdef _WIN32
#include <windows.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

namespace lms {

Console::Console(std::istream& in, std::ostream& out, bool interactive) : in_(in), out_(out), interactive_(interactive) {}

std::string Console::line(const std::string& prompt) {
    out_ << prompt << std::flush;
    std::string text;
    if (!std::getline(in_, text)) throw InputClosed{};
    return trim(text);
}

std::string Console::requiredLine(const std::string& prompt) {
    for (;;) {
        std::string text = line(prompt);
        if (!text.empty()) return text;
        error("This field cannot be empty.");
    }
}

long long Console::integer(const std::string& prompt, long long min, long long max) {
    for (;;) {
        std::string text = line(prompt);
        char* end = nullptr;
        errno = 0;
        long long value = std::strtoll(text.c_str(), &end, 10);
        if (!text.empty() && end && *end == '\0' && errno == 0 && value >= min && value <= max) return value;
        error("Please enter a whole number from " + std::to_string(min) + " to " + std::to_string(max) + ".");
    }
}

double Console::number(const std::string& prompt) {
    for (;;) {
        std::string text = line(prompt);
        char* end = nullptr;
        errno = 0;
        double value = std::strtod(text.c_str(), &end);
        if (!text.empty() && end && *end == '\0' && errno == 0 && value == value) return value;
        error("Please enter a number, for example 72.5.");
    }
}

bool Console::yesNo(const std::string& prompt) {
    for (;;) {
        std::string text = toLower(line(prompt + " (y/n): "));
        if (text == "y" || text == "yes") return true;
        if (text == "n" || text == "no") return false;
        error("Please answer y or n.");
    }
}

std::string Console::password(const std::string& prompt) {
    out_ << prompt << std::flush;
    std::string text;
#ifdef _WIN32
    HANDLE handle = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode = 0;
    const bool tty = interactive_ && GetConsoleMode(handle, &mode);
    if (tty) SetConsoleMode(handle, mode & ~ENABLE_ECHO_INPUT);
    bool ok = static_cast<bool>(std::getline(in_, text));
    if (tty) {
        SetConsoleMode(handle, mode);
        out_ << "\n";
    }
#else
    termios saved{};
    const bool tty = interactive_ && isatty(STDIN_FILENO) && tcgetattr(STDIN_FILENO, &saved) == 0;
    if (tty) {
        termios quiet = saved;
        quiet.c_lflag &= static_cast<tcflag_t>(~ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &quiet);
    }
    bool ok = static_cast<bool>(std::getline(in_, text));
    if (tty) {
        tcsetattr(STDIN_FILENO, TCSANOW, &saved);
        out_ << "\n";
    }
#endif
    if (!ok) throw InputClosed{};
    if (!text.empty() && text.back() == '\r') text.pop_back();
    return text;  // not trimmed: spaces may be part of a password
}

int Console::menu(const std::string& title, const std::vector<std::string>& options) {
    heading(title);
    for (std::size_t i = 0; i < options.size(); ++i) {
        const bool last = i + 1 == options.size();
        out_ << "  " << (last ? 0 : static_cast<int>(i) + 1) << ") " << options[i] << "\n";
    }
    return static_cast<int>(integer("Choose: ", 0, static_cast<long long>(options.size()) - 1));
}

void Console::say(const std::string& text) { out_ << text << "\n"; }
void Console::heading(const std::string& text) { out_ << "\n=== " << text << " ===\n"; }
void Console::error(const std::string& text) { out_ << "! " << text << "\n"; }

std::string Console::pad(const std::string& text, std::size_t width) {
    if (text.size() >= width) return text.substr(0, width);
    return text + std::string(width - text.size(), ' ');
}

}  // namespace lms
