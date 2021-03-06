#pragma once

#include "rs-core/common.hpp"
#include "rs-core/meta.hpp"
#include "unicorn/string.hpp"
#include <array>
#include <ostream>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace RS {

    // Needed early

    template <typename InputRange>
    std::string join(const InputRange& range, std::string_view delim = {}, bool term = false) {
        std::string result;
        for (auto& s: range) {
            result += make_view(s);
            result += delim;
        }
        if (! term && ! result.empty() && ! delim.empty())
            result.resize(result.size() - delim.size());
        return result;
    }

    // Case conversion functions

    class CaseWords:
    public LessThanComparable<CaseWords> {
    public:
        using iterator = Strings::const_iterator;
        using value_type = Ustring;
        CaseWords() = default;
        CaseWords(Uview text);
        CaseWords(const Ustring& text): CaseWords(Uview(text)) {}
        CaseWords(const char* text): CaseWords(Uview(text)) {}
        const Ustring& operator[](size_t i) const noexcept { return list_[i]; }
        CaseWords& operator+=(const CaseWords& c);
        iterator begin() const noexcept { return list_.begin(); }
        iterator end() const noexcept { return list_.end(); }
        bool empty() const noexcept { return list_.empty(); }
        size_t hash() const noexcept { return hash_range(list_); }
        void push_front(Uview text);
        void push_back(Uview text);
        size_t size() const noexcept { return list_.size(); }
        Ustring str(Uview format = "f ") const;
        Ustring camel_case() const { return str("c"); }
        Ustring fold_case() const { return str("f "); }
        Ustring initials() const { return str("i"); }
        Ustring kebab_case() const { return str("l-"); }
        Ustring lower_case() const { return str("l "); }
        Ustring macro_case() const { return str("u_"); }
        Ustring pascal_case() const { return str("t"); }
        Ustring sentence_case() const { return str("s "); }
        Ustring snake_case() const { return str("l_"); }
        Ustring title_case() const { return str("t "); }
        Ustring upper_case() const { return str("u "); }
        void swap(CaseWords& c) noexcept { list_.swap(c.list_); }
        friend bool operator==(const CaseWords& a, const CaseWords& b) noexcept { return a.list_ == b.list_; }
        friend bool operator<(const CaseWords& a, const CaseWords& b) noexcept { return a.list_ < b.list_; }
    private:
        Strings list_;
    };

    inline CaseWords operator+(const CaseWords& a, const CaseWords& b) { auto c = a; c += b; return c; }
    inline std::ostream& operator<<(std::ostream& out, const CaseWords& c) { return out << c.str(); }
    inline void swap(CaseWords& a, CaseWords& b) noexcept { a.swap(b); }

    // Character functions

    template <typename T> constexpr T char_to(char c) noexcept { return T(uint8_t(c)); }

    // String property functions

    bool ascii_icase_equal(std::string_view lhs, std::string_view rhs) noexcept;
    bool ascii_icase_less(std::string_view lhs, std::string_view rhs) noexcept;
    bool starts_with(std::string_view str, std::string_view prefix) noexcept;
    bool ends_with(std::string_view str, std::string_view suffix) noexcept;
    bool string_is_ascii(std::string_view str) noexcept;

    // String manipulation functions

    namespace RS_Detail {

        template <typename T, typename... Args>
        inline void catstr_helper(std::string& s, const T& t, const Args&... args) {
            if constexpr (std::is_same_v<T, char>)
                s += t;
            else if constexpr (std::is_arithmetic_v<T>)
                s += std::to_string(t);
            else if constexpr (Meta::has_plus_assign_operator<std::string, T>)
                s += t;
            else if constexpr (std::is_constructible_v<std::string, T>)
                s += std::string(t);
            else
                static_assert(dependent_false<T>);
            if constexpr (sizeof...(Args) > 0)
                catstr_helper(s, args...);
        }

    }

    void add_lf(std::string& str);
    std::string add_prefix(std::string_view s, std::string_view prefix);
    std::string add_suffix(std::string_view s, std::string_view suffix);
    std::string drop_prefix(std::string_view s, std::string_view prefix);
    std::string drop_suffix(std::string_view s, std::string_view suffix);
    Ustring indent(Uview str, size_t depth);
    std::string linearize(std::string_view str);
    std::string pad_left(std::string_view str, size_t len, char pad = ' ');
    std::string pad_right(std::string_view str, size_t len, char pad = ' ');
    std::pair<std::string_view, std::string_view> partition_at(std::string_view str, std::string_view delim);
    std::pair<std::string_view, std::string_view> partition_by(std::string_view str, std::string_view delims = ascii_whitespace);
    std::string repeat(std::string_view s, size_t n, std::string_view delim = {});
    std::string replace(std::string_view s, std::string_view target, std::string_view subst, size_t n = npos);
    Strings splitv(std::string_view src, std::string_view delim = ascii_whitespace);
    Strings splitv_lines(std::string_view src);
    std::string trim(std::string_view str, std::string_view chars = ascii_whitespace);
    std::string trim_left(std::string_view str, std::string_view chars = ascii_whitespace);
    std::string trim_right(std::string_view str, std::string_view chars = ascii_whitespace);

    template <typename... Args>
    std::string catstr(const Args&... args) {
        std::string s;
        if constexpr (sizeof...(Args) > 0)
            RS_Detail::catstr_helper(s, args...);
        return s;
    }

    template <typename C>
    void null_term(std::basic_string<C>& str) noexcept {
        auto p = str.find(C(0));
        if (p != npos)
            str.resize(p);
    }

    template <typename OutputIterator>
    void split(std::string_view src, OutputIterator dst, std::string_view delim = ascii_whitespace) {
        if (src.empty())
            return;
        if (delim.empty()) {
            *dst = std::string(src);
            return;
        }
        size_t i = 0, j = 0, size = src.size();
        while (i < size) {
            j = src.find_first_of(delim, i);
            if (j == npos) {
                *dst = std::string(src.substr(i, npos));
                break;
            }
            if (j > i) {
                *dst = std::string(src.substr(i, j - i));
                ++dst;
            }
            i = src.find_first_not_of(delim, j);
        }
    }

    template <typename OutputIterator>
    void split_lines(std::string_view src, OutputIterator dst) {
        if (src.empty())
            return;
        size_t i = 0, j = 0, size = src.size();
        while (i < size) {
            j = src.find('\n', i);
            if (j == npos) {
                *dst = std::string(src.substr(i, npos));
                break;
            } else if (j - i > 0 && src[j - 1] == '\r') {
                *dst = std::string(src.substr(i, j - i - 1));
            } else {
                *dst = std::string(src.substr(i, j - i));
            }
            ++dst;
            i = j + 1;
        }
    }

    // String formatting functions

    Ustring hexdump(const void* ptr, size_t n, size_t block = 0);
    Ustring hexdump(std::string_view str, size_t block = 0);
    inline Ustring tf(bool b) { return b ? "true" : "false"; }
    inline Ustring yn(bool b) { return b ? "yes" : "no"; }

    template <typename T>
    Ustring expand_integer(T t, char delim = '\'') {
        Ustring s = dec(t);
        size_t a = size_t(s[0] == '-'), b = s.size();
        while (b - a > 3) {
            b -= 3;
            s.insert(b, 1, delim);
        }
        return s;
    }

    template <typename T>
    Ustring expand_hex(T t, char delim = '\'') {
        Ustring s = hex(t);
        size_t a = size_t(s[0] == '-'), b = s.size();
        while (b - a > 4) {
            b -= 4;
            s.insert(b, 1, delim);
        }
        s.insert(a, "0x");
        return s;
    }

    template <typename Range>
    Strings to_strings(const Range& r) {
        Strings s;
        for (auto& x: r)
            s.push_back(to_str(x));
        return s;
    }

    template <size_t N>
    Ustring hex(const std::array<uint8_t, N>& bytes) {
        using namespace RS_Detail;
        Ustring s;
        for (auto b: bytes)
            append_hex_byte(b, s);
        return s;
    }

    template <typename... Args>
    Ustring fmt(Uview pattern, const Args&... args) {
        Strings argstr{to_str(args)...};
        Ustring result;
        size_t i = 0, psize = pattern.size();
        while (i < psize) {
            size_t j = pattern.find('$', i);
            if (j == npos)
                j = psize;
            result.append(pattern.data() + i, j - i);
            i = j;
            while (pattern[i] == '$') {
                ++i;
                j = i;
                if (pattern[j] == '{')
                    ++j;
                size_t k = j;
                while (ascii_isdigit(pattern[k]))
                    ++k;
                if (k == j || (pattern[i] == '{' && pattern[k] != '}')) {
                    result += pattern[i++];
                    continue;
                }
                size_t a = std::stoul(Ustring(pattern.substr(j, k - j)));
                if (a > 0 && a <= argstr.size())
                    result += argstr[a - 1];
                if (pattern[k] == '}')
                    ++k;
                i = k;
            }
        }
        return result;
    }

    // HTML/XML tags

    class Tag {
    public:
        Tag() = default;
        Tag(std::ostream& out, std::string_view element);
        ~Tag() noexcept;
        Tag(const Tag&) = delete;
        Tag(Tag&& t) noexcept;
        Tag& operator=(const Tag&) = delete;
        Tag& operator=(Tag&& t) noexcept;
    private:
        std::string end;
        std::ostream* os = nullptr;
    };

    template <typename T>
    void tagged(std::ostream& out, std::string_view element, const T& t) {
        Tag html(out, element);
        out << t;
    }

    template <typename... Args>
    void tagged(std::ostream& out, std::string_view element, const Args&... args) {
        Tag html(out, element);
        tagged(out, args...);
    }

    // String literals

    namespace Literals {

        Strings operator""_csv(const char* p, size_t n);
        Ustring operator""_doc(const char* p, size_t n);
        Strings operator""_qw(const char* p, size_t n);

    }

}

RS_DEFINE_STD_HASH(RS::CaseWords);
