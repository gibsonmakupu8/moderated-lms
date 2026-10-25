// Wall-clock time helpers. Timestamps are stored as text "YYYY-MM-DD HH:MM:SS" (school local time)
// so they sort correctly in SQLite. Differences are computed on wall-clock seconds, which keeps the
// late-penalty rule independent of the computer's time-zone database.
#pragma once

#include <string>

namespace lms {

// Parses "YYYY-MM-DD HH:MM" or "YYYY-MM-DD HH:MM:SS" into seconds since 1970-01-01 00:00:00 (wall clock).
// Throws ValidationError for malformed or impossible dates.
long long parseTimestamp(const std::string& text);

// Formats wall-clock seconds as "YYYY-MM-DD HH:MM:SS".
std::string formatTimestamp(long long seconds);

// Abstraction over "now" so deadline and penalty rules are testable (Dependency Inversion).
class IClock {
public:
    virtual ~IClock() = default;
    virtual long long nowSeconds() const = 0;
    std::string nowString() const { return formatTimestamp(nowSeconds()); }
};

class SystemClock : public IClock {
public:
    long long nowSeconds() const override;
};

// Controllable clock used by the automated tests.
class FixedClock : public IClock {
public:
    explicit FixedClock(long long seconds) : seconds_(seconds) {}
    explicit FixedClock(const std::string& timestamp) : seconds_(parseTimestamp(timestamp)) {}
    long long nowSeconds() const override { return seconds_; }
    void set(const std::string& timestamp) { seconds_ = parseTimestamp(timestamp); }
    void advance(long long seconds) { seconds_ += seconds; }

private:
    long long seconds_;
};

}  // namespace lms
