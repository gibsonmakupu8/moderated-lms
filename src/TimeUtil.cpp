#include "lms/TimeUtil.hpp"

#include <cstdio>
#include <ctime>

#include "lms/Common.hpp"

namespace lms {
namespace {

// Howard Hinnant's public-domain civil-calendar algorithms.
long long daysFromCivil(long long y, unsigned m, unsigned d) {
    y -= m <= 2;
    const long long era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(y - era * 400);
    const unsigned doy = (153 * (m > 2 ? m - 3 : m + 9) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + static_cast<long long>(doe) - 719468;
}

void civilFromDays(long long z, long long& y, unsigned& m, unsigned& d) {
    z += 719468;
    const long long era = (z >= 0 ? z : z - 146096) / 146097;
    const unsigned doe = static_cast<unsigned>(z - era * 146097);
    const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    y = static_cast<long long>(yoe) + era * 400;
    const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const unsigned mp = (5 * doy + 2) / 153;
    d = doy - (153 * mp + 2) / 5 + 1;
    m = mp < 10 ? mp + 3 : mp - 9;
    y += (m <= 2);
}

bool isLeap(int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }

int daysInMonth(int y, int m) {
    static const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    return (m == 2 && isLeap(y)) ? 29 : days[m - 1];
}

}  // namespace

long long parseTimestamp(const std::string& text) {
    int y = 0, mo = 0, d = 0, h = 0, mi = 0, s = 0;
    char tail = 0;
    int n = std::sscanf(text.c_str(), "%4d-%2d-%2d %2d:%2d:%2d%c", &y, &mo, &d, &h, &mi, &s, &tail);
    if (n != 6) {
        s = 0;
        n = std::sscanf(text.c_str(), "%4d-%2d-%2d %2d:%2d%c", &y, &mo, &d, &h, &mi, &tail);
        if (n != 5) {
            throw ValidationError("Date/time must look like YYYY-MM-DD HH:MM (for example 2026-11-20 16:00).");
        }
    }
    if (y < 2000 || y > 2100 || mo < 1 || mo > 12 || d < 1 || d > daysInMonth(y, mo) || h < 0 ||
        h > 23 || mi < 0 || mi > 59 || s < 0 || s > 59) {
        throw ValidationError("That date/time does not exist: " + text);
    }
    return daysFromCivil(y, static_cast<unsigned>(mo), static_cast<unsigned>(d)) * 86400LL + h * 3600LL +
           mi * 60LL + s;
}

std::string formatTimestamp(long long seconds) {
    long long days = seconds / 86400;
    long long rem = seconds % 86400;
    if (rem < 0) {
        rem += 86400;
        --days;
    }
    long long y = 0;
    unsigned m = 0, d = 0;
    civilFromDays(days, y, m, d);
    char buf[40];
    std::snprintf(buf, sizeof buf, "%04lld-%02u-%02u %02lld:%02lld:%02lld", y, m, d, rem / 3600,
                  (rem % 3600) / 60, rem % 60);
    return buf;
}

long long SystemClock::nowSeconds() const {
    std::time_t t = std::time(nullptr);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &t);
#else
    localtime_r(&t, &local);
#endif
    return daysFromCivil(local.tm_year + 1900, static_cast<unsigned>(local.tm_mon + 1),
                         static_cast<unsigned>(local.tm_mday)) * 86400LL +
           local.tm_hour * 3600LL + local.tm_min * 60LL + local.tm_sec;
}

}  // namespace lms
