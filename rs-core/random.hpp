#pragma once

#include "rs-core/common.hpp"
#include "rs-core/float.hpp"
#include "rs-core/int128.hpp"
#include "rs-core/meta.hpp"
#include "rs-core/string.hpp"
#include "rs-core/vector.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <map>
#include <random>
#include <set>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

#ifdef _MSC_VER
    #pragma warning(push)
    #pragma warning(disable: 4723) // potential divide by 0
#endif

namespace RS {

    // LCG functions

    constexpr uint32_t lcg32(uint32_t x) noexcept { return uint32_t(32310901ul * x + 850757001ul); }
    constexpr uint64_t lcg64(uint64_t x) noexcept { return uint64_t(3935559000370003845ull * x + 8831144850135198739ull); }

    class Lcg32:
    public EqualityComparable<Lcg32> {
    public:
        using result_type = uint32_t;
        constexpr Lcg32() noexcept: x(0) {}
        explicit constexpr Lcg32(uint32_t s) noexcept: x(s) {}
        constexpr uint32_t operator()() noexcept { x = lcg32(x); return x; }
        constexpr bool operator==(const Lcg32& rhs) const noexcept { return x == rhs.x; }
        constexpr void seed(uint32_t s) noexcept { x = s; }
        static constexpr uint32_t min() noexcept { return 0; }
        static constexpr uint32_t max() noexcept { return ~ uint32_t(0); }
    private:
        uint32_t x;
    };

    class Lcg64:
    public EqualityComparable<Lcg64> {
    public:
        using result_type = uint64_t;
        constexpr Lcg64() noexcept: x(0) {}
        explicit constexpr Lcg64(uint64_t s) noexcept: x(s) {}
        uint64_t constexpr operator()() noexcept { x = lcg64(x); return x; }
        bool constexpr operator==(const Lcg64& rhs) const noexcept { return x == rhs.x; }
        void constexpr seed(uint64_t s) noexcept { x = s; }
        static constexpr uint64_t min() noexcept { return 0; }
        static constexpr uint64_t max() noexcept { return ~ uint64_t(0); }
    private:
        uint64_t x;
    };

    // ISAAC generators

    class Isaac32 {
    public:
        using result_type = uint32_t;
        Isaac32() noexcept { seed(nullptr, 0); }
        explicit Isaac32(uint32_t s) noexcept { seed(&s, 1); }
        Isaac32(const uint32_t* sptr, size_t len) noexcept { seed(sptr, len); }
        Isaac32(std::initializer_list<uint32_t> s) noexcept { seed(s); }
        uint32_t operator()() noexcept;
        void seed() noexcept { seed(nullptr, 0); }
        void seed(uint32_t s) noexcept { seed(&s, 1); }
        void seed(const uint32_t* sptr, size_t len) noexcept;
        void seed(std::initializer_list<uint32_t> s) noexcept;
        static constexpr uint32_t min() noexcept { return 0; }
        static constexpr uint32_t max() noexcept { return ~ uint32_t(0); }
    private:
        uint32_t res[256];
        uint32_t mem[256];
        uint32_t a, b, c, index;
        void next_block() noexcept;
    };

    class Isaac64 {
    public:
        using result_type = uint64_t;
        Isaac64() noexcept { seed(nullptr, 0); }
        explicit Isaac64(uint64_t s) noexcept { seed(&s, 1); }
        Isaac64(const uint64_t* sptr, size_t len) noexcept { seed(sptr, len); }
        Isaac64(std::initializer_list<uint64_t> s) noexcept { seed(s); }
        uint64_t operator()() noexcept;
        void seed() noexcept { seed(nullptr, 0); }
        void seed(uint64_t s) noexcept { seed(&s, 1); }
        void seed(const uint64_t* sptr, size_t len) noexcept;
        void seed(std::initializer_list<uint64_t> s) noexcept;
        static constexpr uint64_t min() noexcept { return 0; }
        static constexpr uint64_t max() noexcept { return ~ uint64_t(0); }
    private:
        uint64_t res[256];
        uint64_t mem[256];
        uint64_t a, b, c, index;
        void next_block() noexcept;
    };

    // PCG generators

    class Pcg32 {
    public:
        using result_type = uint32_t;
        Pcg32() noexcept;
        explicit Pcg32(uint64_t s) noexcept { seed(s); }
        uint32_t operator()() noexcept;
        void advance(int64_t offset) noexcept;
        void seed(uint64_t s) noexcept;
        static constexpr uint32_t min() noexcept { return 0; }
        static constexpr uint32_t max() noexcept { return ~ uint32_t(0); }
    private:
        uint64_t state;
    };

    class Pcg64 {
    public:
        using result_type = uint64_t;
        Pcg64() noexcept;
        explicit Pcg64(Uint128 s) noexcept { seed(s); }
        explicit Pcg64(uint64_t hi, uint64_t lo) noexcept { seed(hi, lo); }
        uint64_t operator()() noexcept;
        void advance(int64_t offset) noexcept;
        void seed(Uint128 s) noexcept;
        void seed(uint64_t hi, uint64_t lo) noexcept { seed(Uint128{hi, lo}); }
        static constexpr uint64_t min() noexcept { return 0; }
        static constexpr uint64_t max() noexcept { return ~ uint64_t(0); }
    private:
        Uint128 state;
    };

    // Xoroshiro generator

    class Xoroshiro:
    public EqualityComparable<Xoroshiro> {
    public:
        using result_type = uint64_t;
        Xoroshiro() noexcept { seed(0); }
        explicit Xoroshiro(uint64_t s) noexcept { seed(s); }
        Xoroshiro(uint64_t s1, uint64_t s2) noexcept { seed(s1, s2); }
        uint64_t operator()() noexcept {
            y ^= x;
            x = rotl(x, 55) ^ y ^ (y << 14);
            y = rotl(y, 36);
            return x + y;
        }
        bool operator==(const Xoroshiro& rhs) const noexcept { return x == rhs.x && y == rhs.y; }
        void seed(uint64_t s) noexcept {
            x = s + 0x9e3779b97f4a7c15ull;
            x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ull;
            x = (x ^ (x >> 27)) * 0x94d049bb133111ebull;
            x ^= (x >> 31);
            y = s;
        }
        void seed(uint64_t s1, uint64_t s2) noexcept { x = s1; y = s2; }
        static constexpr uint64_t min() noexcept { return 0; }
        static constexpr uint64_t max() noexcept { return ~ uint64_t(0); }
    private:
        uint64_t x, y;
    };

    // Basic random distributions

    template <typename T, typename RNG>
    T random_integer(RNG& rng, T min, T max) {
        static_assert(std::is_integral<T>::value);
        // We need an unsigned integer type big enough for both the RNG and
        // output ranges.
        using rng_type = typename RNG::result_type;
        using out_type = std::make_unsigned_t<T>;
        using larger_type = std::conditional_t<(sizeof(out_type) > sizeof(rng_type)), out_type, rng_type>;
        using work_type = std::conditional_t<(sizeof(larger_type) > sizeof(unsigned)), larger_type, unsigned>;
        // Compare the input range (max-min of the values generated by the
        // RNG) with the output range (max-min of the possible results).
        if (min > max)
            std::swap(min, max);
        work_type rng_min = work_type(rng.min());
        work_type rng_range = work_type(rng.max()) - rng_min;
        work_type out_range = work_type(out_type(max) - out_type(min));
        work_type result = 0;
        if (out_range < rng_range) {
            // The RNG range is larger than the output range. Divide the
            // output of the RNG by the rounded down quotient of the ranges.
            // If one range is not an exact multiple of the other, this may
            // yield a value too large; try again.
            work_type ratio = (rng_range - out_range) / (out_range + 1) + 1;
            work_type limit = ratio * (out_range + 1) - 1;
            do result = work_type(rng() - rng_min);
                while (result > limit);
            result /= ratio;
        } else if (out_range == rng_range) {
            // The trivial case where the two ranges are equal.
            result = work_type(rng() - rng_min);
        } else {
            // The output range is larger than the RNG range. Split the output
            // range into a quotient and remainder modulo the RNG range +1;
            // call this function recursively for the quotient, then call the
            // RNG directly for the remainder. Try again if the result is too
            // large.
            work_type high = 0, low = 0;
            work_type ratio = (out_range - rng_range) / (rng_range + 1);
            do {
                high = random_integer(rng, work_type(0), ratio) * (rng_range + 1);
                low = work_type(rng() - rng_min);
            } while (low > out_range - high);
            result = high + low;
        }
        return min + T(result);
    }

    template <typename T, typename RNG>
    T random_integer(RNG& rng, T t) {
        static_assert(std::is_integral<T>::value);
        if (t <= T(1))
            return T(0);
        else
            return random_integer(rng, T(0), t - T(1));
    }

    template <typename T, typename RNG>
    T random_dice(RNG& rng, T n = T(1), T faces = T(6)) {
        static_assert(std::is_integral<T>::value);
        if (n < T(1) || faces < T(1))
            return T(0);
        T sum = T(0);
        for (T i = T(0); i < n; ++i)
            sum += random_integer(rng, T(1), faces);
        return sum;
    }

    template <typename T, typename RNG>
    T random_triangle_integer(RNG& rng, T hi, T lo) {
        static_assert(std::is_integral<T>::value);
        if (hi == lo)
            return hi;
        T d = hi > lo ? hi - lo : lo - hi;
        T x = random_integer(rng, (d + T(1)) * (d + T(2)) / T(2));
        T y = (int_sqrt(T(8) * x + T(1)) - T(1)) / T(2);
        return hi > lo ? lo + y : lo - y;
    }

    template <typename T, typename RNG>
    T random_real(RNG& rng, T a = T(1), T b = T(0)) {
        static_assert(std::is_floating_point<T>::value);
        return a + (b - a) * (T(rng() - rng.min()) / (T(rng.max() - rng.min()) + T(1)));
    }

    template <typename T, typename RNG>
    T random_normal(RNG& rng) {
        static_assert(std::is_floating_point<T>::value);
        T u1 = random_real<T>(rng);
        T u2 = random_real<T>(rng);
        return std::sqrt(T(-2) * std::log(u1)) * std::cos(T(2) * pi_c<T> * u2);
    }

    template <typename T, typename RNG>
    T random_normal(RNG& rng, T m, T s) {
        static_assert(std::is_floating_point<T>::value);
        return m + s * random_normal<T>(rng);
    }

    template <typename RNG>
    bool random_bool(RNG& rng) {
        using R = typename RNG::result_type;
        R min = rng.min();
        R range = rng.max() - min;
        R cutoff = min + range / R(2);
        // Test for odd range is reversed because range is one less than the number of values
        bool odd = range % R(1) == R(0);
        R x = R();
        do x = rng();
            while (odd && x == min);
        return x > cutoff;
    }

    template <typename RNG>
    bool random_bool(RNG& rng, double p) {
        return random_real<double>(rng) <= p;
    }

    template <typename RNG, typename T>
    bool random_bool(RNG& rng, T num, T den) {
        static_assert(std::is_integral<T>::value);
        return random_integer(rng, den) < num;
    }

    template <typename ForwardRange, typename RNG>
    Meta::RangeValue<ForwardRange> random_choice(RNG& rng, const ForwardRange& range) {
        using T = Meta::RangeValue<ForwardRange>;
        using std::begin;
        using std::end;
        auto i = begin(range), j = end(range);
        if (i == j)
            return T();
        size_t n = std::distance(i, j);
        std::advance(i, random_integer(rng, n));
        return *i;
    }

    template <typename T, typename RNG>
    T random_choice(RNG& rng, std::initializer_list<T> list) {
        if (list.size() == 0)
            return T();
        else
            return list.begin()[random_integer(rng, list.size())];
    }

    template <typename ForwardRange, typename RNG>
    std::vector<Meta::RangeValue<ForwardRange>> random_sample(RNG& rng, const ForwardRange& range, size_t k) {
        using T = Meta::RangeValue<ForwardRange>;
        using std::begin;
        using std::end;
        auto b = begin(range), e = end(range);
        size_t n = std::distance(b, e);
        if (k > n)
            throw std::length_error("Sample size requested is larger than the population");
        auto p = b;
        std::advance(p, k);
        std::vector<T> sample(b, p);
        for (size_t i = k; i < n; ++i, ++p) {
            auto j = random_integer(rng, i);
            if (j < k)
                sample[j] = *p;
        }
        return sample;
    }

    // Random distribution properties

    class UniformIntegerProperties {
    public:
        explicit UniformIntegerProperties(int n) noexcept: UniformIntegerProperties(0, std::max(n, 1) - 1) {}
        explicit UniformIntegerProperties(int a, int b) noexcept: lo(std::min(a, b)), hi(std::max(a, b)) {}
        int min() const noexcept { return lo; }
        int max() const noexcept { return hi; }
        double mean() const noexcept { return (double(lo) + double(hi)) / 2; }
        double sd() const noexcept { return sqrt(variance()); }
        double variance() const noexcept { double r = hi - lo + 1; return (r * r - 1) / 12; }
        double pdf(int x) const noexcept;
        double cdf(int x) const noexcept;
        double ccdf(int x) const noexcept;
    private:
        int lo, hi;
    };

    class BinomialDistributionProperties {
    public:
        BinomialDistributionProperties(int t, double p) noexcept: tests(t), prob(p) {}
        int t() const noexcept { return tests; }
        double p() const noexcept { return prob; }
        int min() const noexcept { return 0; }
        int max() const noexcept { return tests; }
        double mean() const noexcept { return t() * p(); }
        double sd() const noexcept { return sqrt(variance()); }
        double variance() const noexcept { return t() * p() * (1 - p()); }
        double pdf(int x) const noexcept;
        double cdf(int x) const noexcept;
        double ccdf(int x) const noexcept;
    private:
        int tests;
        double prob;
    };

    class DiceProperties {
    public:
        DiceProperties() = default;
        explicit DiceProperties(int number, int faces = 6) noexcept: num(number), fac(faces) {}
        int number() const noexcept { return num; }
        int faces() const noexcept { return fac; }
        int min() const noexcept { return num; }
        int max() const noexcept { return num * fac; }
        double mean() const noexcept { return num * (double(fac) + 1) / 2; }
        double sd() const noexcept { return sqrt(variance()); }
        double variance() const noexcept { double f = fac; return num * (f * f - 1) / 12; }
        double pdf(int x) const noexcept;
        double cdf(int x) const noexcept { return ccdf(num * (fac + 1) - x); }
        double ccdf(int x) const noexcept;
    private:
        int num = 1, fac = 6;
    };

    class UniformRealProperties {
    public:
        UniformRealProperties() = default;
        explicit UniformRealProperties(double a, double b = 0) noexcept: lo(std::min(a, b)), hi(std::max(a, b)) {}
        double min() const noexcept { return lo; }
        double max() const noexcept { return hi; }
        double mean() const noexcept { return (lo + hi) / 2; }
        double sd() const noexcept;
        double variance() const noexcept { double r = hi - lo; return r * r / 12; }
        double pdf(double x) const noexcept;
        double cdf(double x) const noexcept;
        double ccdf(double x) const noexcept;
        double quantile(double p) const noexcept { return lo + p * (hi - lo); }
        double cquantile(double q) const noexcept { return hi - q * (hi - lo); }
    private:
        double lo = 0, hi = 1;
    };

    class NormalDistributionProperties {
    public:
        NormalDistributionProperties() = default;
        explicit NormalDistributionProperties(double m, double s) noexcept: xm(m), xs(std::abs(s)) {}
        double min() const noexcept { return {}; }
        double max() const noexcept { return {}; }
        double mean() const noexcept { return xm; }
        double sd() const noexcept { return xs; }
        double variance() const noexcept { return xs * xs; }
        double pdf(double x) const noexcept { return pdf_z((x - xm) / xs); }
        double cdf(double x) const noexcept { return cdf_z((x - xm) / xs); }
        double ccdf(double x) const noexcept { return cdf_z((xm - x) / xs); }
        double quantile(double p) const noexcept;
        double cquantile(double q) const noexcept;
    private:
        double xm = 0, xs = 1;
        double pdf_z(double z) const noexcept;
        double cdf_z(double z) const noexcept;
        double cquantile_z(double q) const noexcept;
    };

    // Spatial distributions

    template <typename T, size_t N>
    class RandomVector {
    public:
        using result_type = Vector<T, N>;
        using scalar_type = T;
        static constexpr size_t dim = N;
        RandomVector(): vec(T(1)) {}
        explicit RandomVector(T t): vec(t) {}
        explicit RandomVector(const Vector<T, N>& v): vec(v) {}
        template <typename RNG> Vector<T, N> operator()(RNG& rng) const;
        Vector<T, N> scale() const { return vec; }
    private:
        result_type vec;
    };

        template <typename T, size_t N>
        template <typename RNG>
        Vector<T, N> RandomVector<T, N>::operator()(RNG& rng) const {
            Vector<T, N> v;
            for (size_t i = 0; i < N; ++i)
                v[i] = random_real(rng, T(0), vec[i]);
            return v;
        }

    template <typename T, size_t N>
    class SymmetricRandomVector {
    public:
        using result_type = Vector<T, N>;
        using scalar_type = T;
        static constexpr size_t dim = N;
        SymmetricRandomVector(): vec(T(1)) {}
        explicit SymmetricRandomVector(T t): vec(t) {}
        explicit SymmetricRandomVector(const Vector<T, N>& v): vec(v) {}
        template <typename RNG> Vector<T, N> operator()(RNG& rng) const;
        Vector<T, N> scale() const { return vec; }
    private:
        result_type vec;
    };

        template <typename T, size_t N>
        template <typename RNG>
        Vector<T, N> SymmetricRandomVector<T, N>::operator()(RNG& rng) const {
            Vector<T, N> v;
            for (size_t i = 0; i < N; ++i)
                v[i] = random_real(rng, - vec[i], vec[i]);
            return v;
        }

    namespace RS_Detail {

        template <typename T, size_t N>
        struct RandomInSphere {
            template <typename RNG> Vector<T, N> generate(RNG& rng) const {
                Vector<T, N> v;
                do std::generate(v.begin(), v.end(), [&] { return random_real<T>(rng, -1, 1); });
                    while (v.r2() > T(1));
                return v;
            }
        };

        template <typename T>
        struct RandomInSphere<T, 2> {
            template <typename RNG> Vector<T, 2> generate(RNG& rng) const {
                using std::sin;
                using std::cos;
                Vector<T, 2> v;
                do {
                    T phi = random_real<T>(rng, 0, 2 * pi_c<T>);
                    T r = random_real<T>(rng) + random_real<T>(rng);
                    if (r > T(1))
                        r = T(2) - r;
                    v = {r * cos(phi), r * sin(phi)};
                } while (v.r2() > T(1));
                return v;
            }
        };

        template <typename T, size_t N>
        struct RandomOnSphere {
            template <typename RNG> Vector<T, N> generate(RNG& rng) const {
                Vector<T, N> v;
                do v = RandomInSphere<T, N>().generate(rng);
                    while (v == Vector<T, N>());
                return v.dir();
            }
        };

        template <typename T>
        struct RandomOnSphere<T, 2> {
            template <typename RNG> Vector<T, 2> generate(RNG& rng) const {
                using std::cos;
                using std::sin;
                T phi = random_real<T>(rng, 0, 2 * pi_c<T>);
                return Vector<T, 2>(cos(phi), sin(phi));
            }
        };

        template <typename T>
        struct RandomOnSphere<T, 3> {
            template <typename RNG> Vector<T, 3> generate(RNG& rng) const {
                using std::cos;
                using std::sin;
                using std::sqrt;
                T phi = random_real<T>(rng, 0, 2 * pi_c<T>);
                T z = random_real<T>(rng, -1, 1);
                T r = sqrt(T(1) - z * z);
                T x = r * cos(phi);
                T y = r * sin(phi);
                return {x, y, z};
            }
        };

    }

    template <typename T, size_t N>
    class RandomInSphere:
    private RS_Detail::RandomInSphere<T, N> {
    public:
        using result_type = Vector<T, N>;
        using scalar_type = T;
        static constexpr size_t dim = N;
        RandomInSphere(): rad{T(1)} {}
        explicit RandomInSphere(T r): rad{std::fabs(r)} {}
        template <typename RNG> Vector<T, N> operator()(RNG& rng) const { return rad * this->generate(rng); }
        T radius() const noexcept { return rad; }
    private:
        T rad;
    };

    template <typename T, size_t N>
    class RandomOnSphere:
    private RS_Detail::RandomOnSphere<T, N> {
    public:
        using result_type = Vector<T, N>;
        using scalar_type = T;
        static constexpr size_t dim = N;
        RandomOnSphere(): rad{T(1)} {}
        explicit RandomOnSphere(T r): rad{std::fabs(r)} {}
        template <typename RNG> Vector<T, N> operator()(RNG& rng) const { return rad * this->generate(rng); }
        T radius() const noexcept { return rad; }
    private:
        T rad;
    };

    // Unique distribution

    template <typename T>
    struct UniqueDistribution {
    public:
        using distribution_type = T;
        using result_type = decltype(std::declval<T>()(std::declval<std::minstd_rand&>()));
        explicit UniqueDistribution(T& t): base(&t), log() {}
        template <typename RNG> result_type operator()(RNG& rng);
        void clear() noexcept { log.clear(); }
        bool empty() const noexcept { return log.empty(); }
        result_type min() const { return base->min(); }
        result_type max() const { return base->max(); }
        size_t size() const noexcept { return log.size(); }
    private:
        T* base;
        std::set<result_type> log;
    };

        template <typename T>
        template <typename RNG>
        typename UniqueDistribution<T>::result_type UniqueDistribution<T>::operator()(RNG& rng) {
            result_type x;
            do x = (*base)(rng);
                while (log.count(x));
            log.insert(x);
            return x;
        }

    template <typename T> inline UniqueDistribution<T> unique_distribution(T& t) { return UniqueDistribution<T>(t); }

    // Weighted choice distribution

    namespace RS_Detail {

        template <typename F, bool Int = std::is_integral<F>::value>
        struct WeightRange;

        template <typename F>
        struct WeightRange<F, true> {
            template <typename RNG> F operator()(RNG& rng, F range) const { return random_integer(rng, range); }
        };

        template <typename F>
        struct WeightRange<F, false> {
            template <typename RNG> F operator()(RNG& rng, F range) const { return random_real(rng, range); }
        };

    }

    template <typename T, typename F = double>
    class WeightedChoice {
    public:
        using frequency_type = F;
        using result_type = T;
        WeightedChoice() = default;
        WeightedChoice(std::initializer_list<std::pair<T, F>> pairs) { append(pairs); }
        template <typename InputRange> explicit WeightedChoice(const InputRange& pairs) { append(pairs); }
        template <typename RNG> T operator()(RNG& rng) const;
        void add(const T& t, F f);
        void append(std::initializer_list<std::pair<T, F>> pairs);
        template <typename InputRange> void append(const InputRange& pairs);
        bool empty() const noexcept { return total() <= F(0); }
    private:
        static_assert(std::is_arithmetic<F>::value);
        using weight_range = RS_Detail::WeightRange<F>;
        std::map<F, T> table;
        F total() const { return table.empty() ? F() : std::prev(table.end())->first; }
    };

        template <typename T, typename F>
        template <typename RNG>
        T WeightedChoice<T, F>::operator()(RNG& rng) const {
            if (empty())
                return T();
            else
                return table.upper_bound(weight_range()(rng, total()))->second;
        }

        template <typename T, typename F>
        void WeightedChoice<T, F>::add(const T& t, F f) {
            if (f > F(0))
                table.insert(table.end(), {total() + f, t});
        }

        template <typename T, typename F>
        void WeightedChoice<T, F>::append(std::initializer_list<std::pair<T, F>> pairs) {
            std::vector<std::pair<T, F>> vfs{pairs};
            auto sum = total();
            for (auto& vf: vfs) {
                if (vf.second > F(0)) {
                    sum += vf.second;
                    table.insert(table.end(), {sum, vf.first});
                }
            }
        }

        template <typename T, typename F>
        template <typename InputRange>
        void WeightedChoice<T, F>::append(const InputRange& pairs) {
            auto sum = total();
            for (auto& p: pairs) {
                if (p.second > F(0)) {
                    sum += p.second;
                    table.insert(table.end(), {sum, p.first});
                }
            }
        }

    // Other random algorithms

    template <typename RNG>
    void random_bytes(RNG& rng, void* ptr, size_t n) {
        static constexpr auto uint64_max = ~ uint64_t(0);
        if (! ptr || ! n)
            return;
        auto range_m1 = uint64_t(std::min(uintmax_t(RNG::max()) - uintmax_t(RNG::min()), uintmax_t(uint64_max)));
        size_t block;
        if (range_m1 == uint64_max)
            block = 8;
        else
            block = (ilog2p1(range_m1) - 1) / 8;
        auto bp = static_cast<uint8_t*>(ptr);
        if (block) {
            while (n >= block) {
                auto x = rng() - RNG::min();
                memcpy(bp, &x, block);
                bp += block;
                n -= block;
            }
            if (n) {
                auto x = rng() - RNG::min();
                memcpy(bp, &x, n);
            }
        } else {
            for (size_t i = 0; i < n; ++i)
                bp[i] = uint8_t(random_integer(rng, 256));
        }
    }

    template <typename RNG1, typename RNG2>
    void seed_from(RNG1& src, RNG2& dst) {
        std::array<uint32_t, RNG2::state_size> array;
        std::generate_n(array.data(), array.size(), std::ref(src));
        std::seed_seq seq(array.begin(), array.end());
        dst.seed(seq);
    }

    template <typename RNG, typename RandomAccessIterator>
    void shuffle(RNG& rng, RandomAccessIterator i, RandomAccessIterator j) {
        size_t n = std::distance(i, j);
        for (size_t a = 0; a + 1 < n; ++a) {
            size_t b = random_integer(rng, a, n - 1);
            if (a != b)
                std::swap(i[a], i[b]);
        }
    }

    template <typename RNG, typename RandomAccessRange>
    void shuffle(RNG& rng, RandomAccessRange& range) {
        using std::begin;
        using std::end;
        shuffle(begin(range), end(range), rng);
    }

    // Text generators

    Ustring lorem_ipsum(uint64_t seed, size_t bytes, bool paras = true);

}

#ifdef _MSC_VER
    #pragma warning(pop)
#endif
