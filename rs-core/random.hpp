#pragma once

#include "rs-core/common.hpp"
#include "rs-core/float.hpp"
#include "rs-core/string.hpp"
#include "rs-core/vector.hpp"
#include <algorithm>
#include <cmath>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <map>
#include <random>
#include <set>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

#ifdef _XOPEN_SOURCE
    #include <fcntl.h>
    #include <unistd.h>
#else
    #include <windows.h>
    #include <wincrypt.h>
#endif

RS_LDLIB(msvc: advapi32);

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
    RangeValue<ForwardRange> random_choice(RNG& rng, const ForwardRange& range) {
        using T = RangeValue<ForwardRange>;
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
    std::vector<RangeValue<ForwardRange>> random_sample(RNG& rng, const ForwardRange& range, size_t k) {
        using T = RangeValue<ForwardRange>;
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

        inline double UniformIntegerProperties::pdf(int x) const noexcept {
            if (x >= lo && x <= hi)
                return 1.0 / (hi - lo + 1);
            else
                return 0;
        }

        inline double UniformIntegerProperties::cdf(int x) const noexcept {
            if (x < lo)
                return 0;
            else if (x < hi)
                return double(x - lo + 1) / (hi - lo + 1);
            else
                return 1;
        }

        inline double UniformIntegerProperties::ccdf(int x) const noexcept {
            if (x <= lo)
                return 1;
            else if (x <= hi)
                return double(hi - x + 1) / (hi - lo + 1);
            else
                return 0;
        }

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

        inline double BinomialDistributionProperties::pdf(int x) const noexcept {
            if (x >= 0 && x <= tests)
                return xbinomial(tests, x) * pow(prob, x) * pow(1 - prob, tests - x);
            else
                return 0;
        }

        inline double BinomialDistributionProperties::cdf(int x) const noexcept {
            if (x < 0)
                return 0;
            else if (x >= tests)
                return 1;
            double c = 0;
            for (int y = 0; y <= x; ++y)
                c += pdf(y);
            return c;
        }

        inline double BinomialDistributionProperties::ccdf(int x) const noexcept {
            if (x <= 0)
                return 1;
            else if (x > tests)
                return 0;
            double c = 0;
            for (int y = tests; y >= x; --y)
                c += pdf(y);
            return c;
        }

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

        inline double DiceProperties::pdf(int x) const noexcept {
            if (x < num || x > max())
                return 0;
            double s = 1, t = 0;
            for (int i = 0, j = x - 1; i < num; ++i, j -= fac, s = - s)
                t += s * xbinomial(j, num - 1) * xbinomial(num, i);
            return t * pow(double(fac), - num);
        }

        inline double DiceProperties::ccdf(int x) const noexcept {
            if (x <= num)
                return 1;
            if (x > max())
                return 0;
            double s = 1, t = 0;
            for (int i = 0, j = num * (fac + 1) - x; i < num; ++i, j -= fac, s = - s)
                t += s * xbinomial(j, num) * xbinomial(num, i);
            return t * pow(double(fac), - num);
        }

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

        inline double UniformRealProperties::sd() const noexcept {
            static const double inv_sqrt12 = sqrt(1.0 / 12.0);
            return inv_sqrt12 * (hi - lo);
        }

        inline double UniformRealProperties::pdf(double x) const noexcept {
            if (x > lo && x < hi)
                return 1 / (hi - lo);
            else
                return 0;
        }

        inline double UniformRealProperties::cdf(double x) const noexcept {
            if (x <= lo)
                return 0;
            else if (x < hi)
                return (x - lo) / (hi - lo);
            else
                return 1;
        }

        inline double UniformRealProperties::ccdf(double x) const noexcept {
            if (x <= lo)
                return 1;
            else if (x < hi)
                return (hi - x) / (hi - lo);
            else
                return 0;
        }

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

        inline double NormalDistributionProperties::quantile(double p) const noexcept {
            if (p < 0.5)
                return xm - xs * cquantile_z(p);
            else if (p == 0.5)
                return xm;
            else
                return xm + xs * cquantile_z(1 - p);
        }

        inline double NormalDistributionProperties::cquantile(double q) const noexcept {
            if (q < 0.5)
                return xm + xs * cquantile_z(q);
            else if (q == 0.5)
                return xm;
            else
                return xm - xs * cquantile_z(1 - q);
        }

        inline double NormalDistributionProperties::pdf_z(double z) const noexcept {
            return exp(- z * z / 2) / sqrt2pi_d;
        }

        inline double NormalDistributionProperties::cdf_z(double z) const noexcept {
            return (erf(z / sqrt2_d) + 1) / 2;
        }

        inline double NormalDistributionProperties::cquantile_z(double q) const noexcept {
            // Beasley-Springer approximation
            // For |z|<3.75, absolute error <1e-6, relative error <2.5e-7
            // For |z|<7.5, absolute error <5e-4, relative error <5e-5
            // This will always be called with 0<q<1/2
            static constexpr double threshold = 0.08;
            static constexpr double a1 = 2.321213;
            static constexpr double a2 = 4.850141;
            static constexpr double a3 = -2.297965;
            static constexpr double a4 = -2.787189;
            static constexpr double b1 = 1.637068;
            static constexpr double b2 = 3.543889;
            static constexpr double c1 = -25.44106;
            static constexpr double c2 = 41.3912;
            static constexpr double c3 = -18.615;
            static constexpr double c4 = 2.506628;
            static constexpr double d1 = 3.130829;
            static constexpr double d2 = -21.06224;
            static constexpr double d3 = 23.08337;
            static constexpr double d4 = -8.473511;
            if (q < threshold) {
                double r = sqrt(- log(q));
                return (((a1 * r + a2) * r + a3) * r + a4) / ((b1 * r + b2) * r + 1);
            } else {
                double q1 = 0.5 - q;
                double r = q1 * q1;
                return q1 * (((c1 * r + c2) * r + c3) * r + c4) / ((((d1 * r + d2) * r + d3) * r + d4) * r + 1);
            }
        }

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
        template <typename Range> explicit WeightedChoice(const Range& pairs) { append(pairs); }
        template <typename RNG> T operator()(RNG& rng) const;
        void add(const T& t, F f);
        void append(std::initializer_list<std::pair<T, F>> pairs);
        template <typename Range> void append(const Range& pairs);
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
        template <typename Range>
        void WeightedChoice<T, F>::append(const Range& pairs) {
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

    template <typename Rng2, typename Rng1>
    Rng2 seed_from(Rng1& src) {
        typename Rng1::result_type random_data[Rng2::state_size];
        std::generate(std::begin(random_data), std::end(random_data), std::ref(src));
        std::seed_seq seq(std::begin(random_data), std::end(random_data));
        return Rng2(seq);
    }

    template <typename Rng1, typename Rng2>
    void seed_from(Rng1& src, Rng2& dst) {
        typename Rng1::result_type random_data[Rng2::state_size];
        std::generate(std::begin(random_data), std::end(random_data), std::ref(src));
        std::seed_seq seq(std::begin(random_data), std::end(random_data));
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

    namespace RS_Detail {

        struct LoremGenerator {
            void operator()(Xoroshiro& rng, Ustring& dst, size_t bytes, bool paras) const {
                static constexpr const char* classic[] = {
                    "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. ",
                    "Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. ",
                    "Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. ",
                    "Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum. ",
                };
                static constexpr size_t n_lines = sizeof(classic) / sizeof(classic[0]);
                if (bytes == 0)
                    return;
                dst.reserve(bytes + 20);
                for (size_t i = 0; i < n_lines && dst.size() <= bytes; ++i)
                    dst += classic[i];
                if (paras)
                    dst.replace(dst.size() - 1, 1, "\n\n");
                while (dst.size() <= bytes) {
                    size_t n_para = paras ? rng() % 7 + 1 : npos;
                    for (size_t i = 0; i < n_para && dst.size() <= bytes; ++i)
                        dst += classic[rng() % n_lines];
                    if (paras)
                        dst.replace(dst.size() - 1, 1, "\n\n");
                }
                size_t cut = dst.find_first_of("\n .,", bytes);
                if (cut != npos)
                    dst.resize(cut);
                while (! ascii_isalpha(dst.back()))
                    dst.pop_back();
                dst += '.';
                if (paras)
                    dst += '\n';
            }
        };

    }

    inline Ustring lorem_ipsum(uint64_t seed, size_t bytes, bool paras = true) {
        RS_Detail::LoremGenerator gen;
        Xoroshiro rng(seed, 0x05b6b84c03ae03d2ull);
        Ustring text;
        gen(rng, text, bytes, paras);
        return text;
    }

}
