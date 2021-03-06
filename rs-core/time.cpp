#include "rs-core/time.hpp"
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <stdexcept>
#include <thread>

using namespace std::chrono;
using namespace std::literals;

namespace RS {

    // System specific time and date conversions

    timespec timepoint_to_timespec(const system_clock::time_point& tp) noexcept {
        return duration_to_timespec(tp - system_clock::time_point());
    }

    timeval timepoint_to_timeval(const system_clock::time_point& tp) noexcept {
        return duration_to_timeval(tp - system_clock::time_point());
    }

    system_clock::time_point timespec_to_timepoint(const timespec& ts) noexcept {
        system_clock::duration d;
        timespec_to_duration(ts, d);
        return system_clock::time_point() + d;
    }

    system_clock::time_point timeval_to_timepoint(const timeval& tv) noexcept {
        system_clock::duration d;
        timeval_to_duration(tv, d);
        return system_clock::time_point() + d;
    }

    #ifdef _WIN32

        system_clock::time_point filetime_to_timepoint(const FILETIME& ft) noexcept {
            static constexpr int64_t filetime_freq = 10'000'000;        // FILETIME ticks (100 ns) per second
            static constexpr int64_t windows_epoch = 11'644'473'600ll;  // Windows epoch (1601) to Unix epoch (1970) in seconds
            int64_t ticks = (int64_t(ft.dwHighDateTime) << 32) + int64_t(ft.dwLowDateTime);
            int64_t sec = ticks / filetime_freq - windows_epoch;
            int64_t nsec = 100ll * (ticks % filetime_freq);
            return system_clock::from_time_t(time_t(sec)) + duration_cast<system_clock::duration>(nanoseconds(nsec));
        }

        FILETIME timepoint_to_filetime(const system_clock::time_point& tp) noexcept {
            static constexpr uint64_t filetime_freq = 10'000'000;        // FILETIME ticks (100 ns) per second
            static constexpr uint64_t windows_epoch = 11'644'473'600ll;  // Windows epoch (1601) to Unix epoch (1970) in seconds
            auto unix_time = tp - system_clock::from_time_t(0);
            uint64_t nsec = duration_cast<nanoseconds>(unix_time).count();
            uint64_t ticks = nsec / 100ll + filetime_freq * windows_epoch;
            return {uint32_t(ticks & 0xfffffffful), uint32_t(ticks >> 32)};
        }

    #endif

    // Time and date formatting

    DateFormat::DateFormat(Uview format, uint32_t flags) {
        size_t i = 0, n = format.size();
        while (i < n) {
            if (ascii_isalnum(format[i])) {
                char uc = ascii_toupper(format[i]);
                size_t j = i + 1;
                while (j < n && ascii_toupper(format[j]) == uc)
                    ++j;
                Uview code = format.substr(i, j - i);
                if (code == "MMM"sv)
                    expanded_ += "m3";
                else if (code == "Mmm"sv)
                    expanded_ += "m4";
                else if (code == "mmm"sv)
                    expanded_ += "m5";
                else if (code == "WWW"sv)
                    expanded_ += "w3";
                else if (code == "Www"sv)
                    expanded_ += "w4";
                else if (code == "www"sv)
                    expanded_ += "w5";
                else if (code == "yy"sv || code == "yyyy"sv
                        || code == "m"sv || code == "mm"sv
                        || code == "d"sv || code == "dd"sv
                        || code == "H"sv || code == "HH"sv
                        || code == "M"sv || code == "MM"sv
                        || code == "S"sv || code == "SS"sv
                        || code[0] == 's')
                    expanded_ += std::string{format[i], char('0' + j - i)};
                else
                    throw std::invalid_argument("Invalid date format: " + quote(format));
                i = j;
            } else if (format[i] == '+' && n - i >= 5 && Uview(format.data() + i, 5) == "+ZZZZ"sv) {
                expanded_ += "Z5";
                i += 5;
            } else {
                expanded_ += format[i++];
                expanded_ += '=';
            }
        }
        flags_ = flags;
    }

    Ustring DateFormat::operator()(system_clock::time_point tp, uint32_t flags) const {
        static constexpr Uview month_name[] = {
            "jan", "feb", "mar", "apr", "may", "jun",
            "jul", "aug", "sep", "oct", "nov", "dec",
        };
        static constexpr Uview weekday_name[] = {
            "sun", "mon", "tue", "wed", "thu", "fri", "sat",
        };
        static const auto fraction_str = [] (double f, char c) {
            int d = c - '0';
            double x = std::floor(f * std::pow(10.0, double(d)));
            return dec(int(x), d);
        };
        static const auto zone_offset = [] (std::tm& tm) {
            std::string buf(10, '\0');
            size_t n = std::strftime(buf.data(), buf.size(), "%z", &tm);
            buf.resize(n);
            return buf;
        };
        if (flags == 0)
            flags = flags_;
        std::time_t tt = system_clock::to_time_t(tp);
        auto diff = tp - system_clock::from_time_t(tt);
        if (diff.count() < 0) {
            --tt;
            diff += 1s;
        }
        double fraction = duration_cast<Dseconds>(diff).count();
        std::tm tm;
        if (flags & local_zone)
            tm = *std::localtime(&tt);
        else
            tm = *std::gmtime(&tt);
        std::string result;
        for (size_t i = 0, n = expanded_.size(); i < n; i += 2) {
            Uview code(expanded_.data() + i, 2);
            if (code[1] == '=')
                result += code[0];
            else if (code == "y2"sv)
                result += dec(rem(tm.tm_year, 100), 2);
            else if (code == "y4"sv)
                result += dec(tm.tm_year + 1900);
            else if (code == "m1"sv)
                result += dec(tm.tm_mon + 1);
            else if (code == "m2"sv)
                result += dec(tm.tm_mon + 1, 2);
            else if (code == "m3"sv)
                result += ascii_uppercase(month_name[tm.tm_mon]);
            else if (code == "m4"sv)
                result += ascii_titlecase(month_name[tm.tm_mon]);
            else if (code == "m5"sv)
                result += month_name[tm.tm_mon];
            else if (code == "d1"sv)
                result += dec(tm.tm_mday);
            else if (code == "d2"sv)
                result += dec(tm.tm_mday, 2);
            else if (code == "w3"sv)
                result += ascii_uppercase(weekday_name[tm.tm_wday]);
            else if (code == "w4"sv)
                result += ascii_titlecase(weekday_name[tm.tm_wday]);
            else if (code == "w5"sv)
                result += weekday_name[tm.tm_wday];
            else if (code == "H1"sv)
                result += dec(tm.tm_hour);
            else if (code == "H2"sv)
                result += dec(tm.tm_hour, 2);
            else if (code == "M1"sv)
                result += dec(tm.tm_min);
            else if (code == "M2"sv)
                result += dec(tm.tm_min, 2);
            else if (code == "S1"sv)
                result += dec(tm.tm_sec);
            else if (code == "S2"sv)
                result += dec(tm.tm_sec, 2);
            else if (code[0] == 's')
                result += fraction_str(fraction, code[1]);
            else if (code == "Z5"sv)
                result += flags & local_zone ? zone_offset(tm) : "+0000";
        }
        return result;
    }

    // Time and date parsing

    namespace {

        void date_skip_punct(const char*& ptr, const char* end) {
            while (ptr != end && (ascii_ispunct(*ptr) || ascii_isspace(*ptr)))
                ++ptr;
        };

        bool date_read_number(const char*& ptr, const char* end, int& result) {
            date_skip_punct(ptr, end);
            auto next = std::find_if_not(ptr, end, ascii_isdigit);
            if (next == ptr)
                return false;
            Ustring sub(ptr, next);
            char* dummy = nullptr;
            result = int(std::strtoul(sub.data(), &dummy, 10));
            ptr = next;
            return true;
        };

        bool date_read_number(const char*& ptr, const char* end, double& result) {
            date_skip_punct(ptr, end);
            if (ptr == end || ! ascii_isdigit(*ptr))
                return false;
            auto next = std::find_if_not(ptr, end, ascii_isdigit);
            if (next != end && *next == '.')
                next = std::find_if_not(next + 1, end, ascii_isdigit);
            Ustring sub(ptr, next);
            char* dummy = nullptr;
            result = std::strtod(sub.data(), &dummy);
            ptr = next;
            return true;
        };

        bool date_read_month(const char*& ptr, const char* end, int& result) {
            date_skip_punct(ptr, end);
            if (ptr == end)
                return false;
            else if (ascii_isdigit(*ptr))
                return date_read_number(ptr, end, result);
            else if (end - ptr < 3 || ! ascii_isalpha(*ptr))
                return false;
            Ustring mon(ptr, 3);
            mon = ascii_lowercase(mon);
            if (mon == "jan")       result = 1;
            else if (mon == "feb")  result = 2;
            else if (mon == "mar")  result = 3;
            else if (mon == "apr")  result = 4;
            else if (mon == "may")  result = 5;
            else if (mon == "jun")  result = 6;
            else if (mon == "jul")  result = 7;
            else if (mon == "aug")  result = 8;
            else if (mon == "sep")  result = 9;
            else if (mon == "oct")  result = 10;
            else if (mon == "nov")  result = 11;
            else if (mon == "dec")  result = 12;
            else                    return false;
            ptr = std::find_if_not(ptr + 3, end, ascii_isalpha);
            return true;
        };

    }

    namespace RS_Detail {

        double parse_time_as_seconds(Uview str) {
            static constexpr double jyear = 31'557'600;
            Ustring nstr = replace(str, " "s, ""s);
            auto ptr = nstr.data(), end = ptr + nstr.size();
            char sign = '+';
            if (ptr != end && (*ptr == '+' || *ptr == '-'))
                sign = *ptr++;
            if (ptr == end)
                throw std::invalid_argument("Invalid time: " + quote(str));
            double count = 0, seconds = 0;
            char* next = nullptr;
            Ustring unit;
            while (ptr != end) {
                if (! ascii_isdigit(*ptr))
                    throw std::invalid_argument("Invalid time: " + quote(str));
                count = std::strtod(ptr, &next);
                unit.clear();
                while (next != end && ascii_isalpha(*next))
                    unit += *next++;
                if (starts_with(unit, "Yy"s))       count *= jyear * 1.0e24;
                else if (starts_with(unit, "Zy"s))  count *= jyear * 1.0e21;
                else if (starts_with(unit, "Ey"s))  count *= jyear * 1.0e18;
                else if (starts_with(unit, "Py"s))  count *= jyear * 1.0e15;
                else if (starts_with(unit, "Ty"s))  count *= jyear * 1.0e12;
                else if (starts_with(unit, "Gy"s))  count *= jyear * 1.0e9;
                else if (starts_with(unit, "My"s))  count *= jyear * 1.0e6;
                else if (starts_with(unit, "ky"s))  count *= jyear * 1.0e3;
                else if (starts_with(unit, "ms"s))  count *= 1.0e-3;
                else if (starts_with(unit, "us"s))  count *= 1.0e-6;
                else if (starts_with(unit, "µs"s))  count *= 1.0e-6;
                else if (starts_with(unit, "ns"s))  count *= 1.0e-9;
                else if (starts_with(unit, "ps"s))  count *= 1.0e-12;
                else if (starts_with(unit, "fs"s))  count *= 1.0e-15;
                else if (starts_with(unit, "as"s))  count *= 1.0e-18;
                else if (starts_with(unit, "zs"s))  count *= 1.0e-21;
                else if (starts_with(unit, "ys"s))  count *= 1.0e-24;
                else if (starts_with(unit, "y"s))   count *= jyear;
                else if (starts_with(unit, "d"s))   count *= 86400.0;
                else if (starts_with(unit, "h"s))   count *= 3600.0;
                else if (starts_with(unit, "m"s))   count *= 60.0;
                else if (starts_with(unit, "s"s))   {}
                else                                throw std::invalid_argument("Invalid time: " + quote(str));
                seconds += count;
                ptr = next;
            }
            if (sign == '-')
                seconds = - seconds;
            return seconds;
        }

    }

    system_clock::time_point parse_date(Uview str, uint32_t flags) {
        uint32_t order = flags & (ymd_order | dmy_order | mdy_order);
        uint32_t zone = flags & (utc_zone | local_zone);
        if (popcount(order) > 1 || popcount(zone) > 1 || flags - order - zone)
            throw std::invalid_argument("Invalid date flags: 0x" + hex(flags, 1));
        int year = 0, month = 0, day = 0, hour = 0, min = 0;
        double sec = 0;
        auto ptr = str.data(), end = ptr + str.size();
        bool ok = true;
        switch (order) {
            case dmy_order:  ok = date_read_number(ptr, end, day) && date_read_month(ptr, end, month) && date_read_number(ptr, end, year); break;
            case mdy_order:  ok = date_read_month(ptr, end, month) && date_read_number(ptr, end, day) && date_read_number(ptr, end, year); break;
            default:         ok = date_read_number(ptr, end, year) && date_read_month(ptr, end, month) && date_read_number(ptr, end, day); break;
        }
        if (! ok)
            throw std::invalid_argument("Invalid date: " + quote(str));
        date_skip_punct(ptr, end);
        if (ptr != end && (*ptr == 'T' || *ptr == 't'))
            ++ptr;
        date_read_number(ptr, end, hour);
        date_read_number(ptr, end, min);
        date_read_number(ptr, end, sec);
        return make_date(year, month, day, hour, min, sec, zone);
    }

    // Class PollWait

    void PollWait::wait() {
        duration delta = min_wait;
        while (! poll()) {
            std::this_thread::sleep_for(delta);
            delta = std::min(2 * delta, max_wait);
        }
    }

    bool PollWait::do_wait_until(time_point t) {
        duration delta = min_wait;
        while (! poll()) {
            duration remain = t - clock_type::now();
            if (remain <= duration())
                return false;
            std::this_thread::sleep_for(std::min(delta, remain));
            delta = std::min(2 * delta, max_wait);
        }
        return true;
    }

    // Class Stopwatch

    Stopwatch::Stopwatch(Uview name, int precision) noexcept {
        try {
            prefix = Ustring(name) + " : ";
            prec = precision;
            start = ReliableClock::now();
        }
        catch (...) {}
    }

    Stopwatch::~Stopwatch() noexcept {
        try {
            auto t = ReliableClock::now() - start;
            logx(prefix + format_time(t, prec));
        }
        catch (...) {}
    }

}
