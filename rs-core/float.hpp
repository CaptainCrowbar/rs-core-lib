#pragma once

#include "rs-core/common.hpp"
#include "rs-core/meta.hpp"
#include "rs-core/vector.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <deque>
#include <functional>
#include <numeric>
#include <type_traits>
#include <utility>
#include <vector>

namespace RS {

    // Arithmetic constants

    #define RS_DEFINE_CONSTANT(name, value) \
        constexpr long double name##_ld = static_cast<long double>(value); \
        constexpr double name##_d = double(name##_ld); \
        constexpr double name = name##_d; \
        constexpr float name##_f = float(name##_ld); \
        template <typename T> constexpr T name##_c = T(name##_ld);

    #define RS_DEFINE_CONSTANT_2(name, symbol, value) \
        constexpr long double name##_ld = static_cast<long double>(value); \
        constexpr long double symbol##_ld = name##_ld; \
        constexpr double name##_d = double(name##_ld); \
        constexpr double symbol##_d = double(symbol##_ld); \
        constexpr double name = name##_d; \
        constexpr double symbol = symbol##_d; \
        constexpr float name##_f = float(name##_ld); \
        constexpr float symbol##_f = float(symbol##_ld); \
        template <typename T> constexpr T name##_c = T(name##_ld); \
        template <typename T> constexpr T symbol##_c = T(symbol##_ld);

    // Mathematical constants

    RS_DEFINE_CONSTANT(e,                2.71828'18284'59045'23536'02874'71352'66249'77572'47093'69996l);
    RS_DEFINE_CONSTANT(ln2,              0.69314'71805'59945'30941'72321'21458'17656'80755'00134'36026l);
    RS_DEFINE_CONSTANT(ln10,             2.30258'50929'94045'68401'79914'54684'36420'76011'01488'62877l);
    RS_DEFINE_CONSTANT(log2e,            1.44269'50408'88963'40735'99246'81001'89213'74266'45954'15299l);
    RS_DEFINE_CONSTANT(log10e,           0.43429'44819'03251'82765'11289'18916'60508'22943'97005'80367l);
    RS_DEFINE_CONSTANT(pi,               3.14159'26535'89793'23846'26433'83279'50288'41971'69399'37511l);
    RS_DEFINE_CONSTANT(pi_over_2,        1.57079'63267'94896'61923'13216'91639'75144'20985'84699'68755l);
    RS_DEFINE_CONSTANT(pi_over_4,        0.78539'81633'97448'30961'56608'45819'87572'10492'92349'84378l);
    RS_DEFINE_CONSTANT(inv_pi,           0.31830'98861'83790'67153'77675'26745'02872'40689'19291'48091l);
    RS_DEFINE_CONSTANT(inv_sqrtpi,       0.56418'95835'47756'28694'80794'51560'77258'58440'50629'32900l);
    RS_DEFINE_CONSTANT(two_over_pi,      0.63661'97723'67581'34307'55350'53490'05744'81378'38582'96183l);
    RS_DEFINE_CONSTANT(two_over_sqrtpi,  1.12837'91670'95512'57389'61589'03121'54517'16881'01258'65800l);
    RS_DEFINE_CONSTANT(sqrt2,            1.41421'35623'73095'04880'16887'24209'69807'85696'71875'37695l);
    RS_DEFINE_CONSTANT(sqrt3,            1.73205'08075'68877'29352'74463'41505'87236'69428'05253'81038l);
    RS_DEFINE_CONSTANT(inv_sqrt2,        0.70710'67811'86547'52440'08443'62104'84903'92848'35937'68847l);
    RS_DEFINE_CONSTANT(inv_sqrt3,        0.57735'02691'89625'76450'91487'80501'95745'56476'01751'27013l);
    RS_DEFINE_CONSTANT(egamma,           0.57721'56649'01532'86060'65120'90082'40243'10421'59335'93992l);
    RS_DEFINE_CONSTANT(phi,              1.61803'39887'49894'84820'45868'34365'63811'77203'09179'80576l);

    // Conversion factors

    RS_DEFINE_CONSTANT(arcsec,         pi_ld / (180 * 3600));      // rad
    RS_DEFINE_CONSTANT(arcmin,         pi_ld / (180 * 60));        // rad
    RS_DEFINE_CONSTANT(degree,         pi_ld / 180);               // rad
    RS_DEFINE_CONSTANT(inch,           0.0254l);                   // m
    RS_DEFINE_CONSTANT(foot,           0.3048l);                   // m
    RS_DEFINE_CONSTANT(yard,           0.9144l);                   // m
    RS_DEFINE_CONSTANT(mile,           1609.344l);                 // m
    RS_DEFINE_CONSTANT(nautical_mile,  1852);                      // m
    RS_DEFINE_CONSTANT(ounce,          0.028'349'523'125l);        // kg
    RS_DEFINE_CONSTANT(pound,          0.453'592'37l);             // kg
    RS_DEFINE_CONSTANT(short_ton,      907.184'74l);               // kg
    RS_DEFINE_CONSTANT(long_ton,       1016.046'908'8l);           // kg
    RS_DEFINE_CONSTANT(pound_force,    4.448'221'615'260'5l);      // N
    RS_DEFINE_CONSTANT(erg,            1e-7l);                     // J
    RS_DEFINE_CONSTANT(foot_pound,     1.355'817'948'331'400'4l);  // J
    RS_DEFINE_CONSTANT(calorie,        4.184l);                    // J
    RS_DEFINE_CONSTANT(kilocalorie,    4184);                      // J
    RS_DEFINE_CONSTANT(ton_tnt,        4.184e9l);                  // J
    RS_DEFINE_CONSTANT(horsepower,     745.699'871'582'270'22l);   // W
    RS_DEFINE_CONSTANT(mmHg,           133.322'387'415l);          // Pa
    RS_DEFINE_CONSTANT(atmosphere,     10'1325);                   // Pa
    RS_DEFINE_CONSTANT(zero_celsius,   273.15l);                   // K

    // Arithmetic functions

    namespace RS_Detail {

        template <typename T2, typename T1, char Mode, bool FromFloat = std::is_floating_point<T1>::value>
        struct Round;

        template <typename T2, typename T1, char Mode>
        struct Round<T2, T1, Mode, true> {
            T2 operator()(T1 value) const noexcept {
                using std::ceil;
                using std::floor;
                T1 t = Mode == '<' ? floor(value) : Mode == '>' ? ceil(value) : floor(value + T1(1) / T1(2));
                return T2(t);
            }
        };

        template <typename T2, typename T1, char Mode>
        struct Round<T2, T1, Mode, false> {
            T2 operator()(T1 value) const noexcept {
                return T2(value);
            }
        };

    }

    template <typename T>
    constexpr T c_pow(T x, int y) noexcept {
        bool neg = y < 0;
        if (neg)
            y = - y;
        T z = T(1);
        while (y > 0) {
            if (y % 2)
                z *= x;
            x *= x;
            y /= 2;
        }
        if (neg)
            z = T(1) / z;
        return z;
    }

    template <typename T> constexpr T degrees(T rad) noexcept { return rad * (T(180) / pi_c<T>); }
    template <typename T> constexpr T radians(T deg) noexcept { return deg * (pi_c<T> / T(180)); }
    template <typename T1, typename T2> constexpr T2 interpolate(T1 x1, T2 y1, T1 x2, T2 y2, T1 x) noexcept
        { return x1 == x2 ? (y1 + y2) * (T1(1) / T1(2)) : y1 == y2 ? y1 : y1 + (y2 - y1) * ((x - x1) / (x2 - x1)); }
    template <typename T2, typename T1> T2 iceil(T1 value) noexcept { return RS_Detail::Round<T2, T1, '>'>()(value); }
    template <typename T2, typename T1> T2 ifloor(T1 value) noexcept { return RS_Detail::Round<T2, T1, '<'>()(value); }
    template <typename T2, typename T1> T2 iround(T1 value) noexcept { return RS_Detail::Round<T2, T1, '='>()(value); }
    template <typename T> T logistic(T x) noexcept { return T(1) / (T(1) + std::exp(- x)); }
    template <typename T> T logistic2(T x) noexcept { return T(2) * logistic(x) - T(1); }
    template <typename T> T inverse_logistic(T x) noexcept { return - std::log(T(1) / x - T(1)); }
    template <typename T> T inverse_logistic2(T x) noexcept { return inverse_logistic((x + T(1)) / T(2)); }

    template <typename T>
    T round_to_digits(T x, int prec) noexcept {
        static_assert(std::is_floating_point_v<T>);
        if (x == 0)
            return 0;
        prec = std::max(prec, 1);
        T y = std::abs(x);
        T scale = std::floor(std::log10(y)) - prec + 1;
        T factor = std::pow(T(10), scale);
        y = factor * std::floor(y / factor + T(0.5));
        if (x < 0)
            y = - y;
        return y;
    }

    // Arithmetic literals

    namespace Literals {

        constexpr float operator""_degf(long double x) noexcept { return float(radians(x)); }
        constexpr float operator""_degf(unsigned long long x) noexcept { return float(radians(static_cast<long double>(x))); }
        constexpr double operator""_deg(long double x) noexcept { return double(radians(x)); }
        constexpr double operator""_deg(unsigned long long x) noexcept { return double(radians(static_cast<long double>(x))); }
        constexpr long double operator""_degl(long double x) noexcept { return radians(x); }
        constexpr long double operator""_degl(unsigned long long x) noexcept { return radians(static_cast<long double>(x)); }

    }

    // Precision sum

    template <typename T>
    class PrecisionSum {
    public:
        using value_type = T;
        void operator()(T t) {
            using std::abs;
            size_t i = 0;
            for (T p: partials) {
                if (abs(t) < abs(p))
                    std::swap(t, p);
                T sum = t + p;
                p -= sum - t;
                t = sum;
                if (p != T())
                    partials[i++] = p;
            }
            partials.erase(partials.begin() + i, partials.end());
            partials.push_back(t);
        }
        operator T() const {
            return std::accumulate(partials.begin(), partials.end(), T());
        }
        void clear() noexcept { partials.clear(); }
    private:
        static_assert(std::is_floating_point<T>::value);
        std::vector<T> partials;
    };

    template <typename SinglePassRange>
    Meta::RangeValue<SinglePassRange> precision_sum(const SinglePassRange& range) {
        PrecisionSum<Meta::RangeValue<SinglePassRange>> sum;
        for (auto x: range)
            sum(x);
        return sum;
    }

    // Numerical integration

    namespace RS_Detail {

        template <typename T>
        class IntegralIterator:
        public ForwardIterator<IntegralIterator<T>, const T> {
        public:
            IntegralIterator() = default;
            template <typename F> IntegralIterator(T x1, T x2, size_t k, F f):
                function(f), start_x(x1), delta_x((x2 - x1) / k), prev_y(f(x1)) { ++*this; }
            explicit IntegralIterator(size_t k): index(k + 1) {}
            const T& operator*() const noexcept { return area_element; }
            IntegralIterator& operator++() {
                ++index;
                T x = start_x + delta_x * index;
                T y = function(x);
                area_element = delta_x * (prev_y + y) / 2;
                prev_y = y;
                return *this;
            }
            bool operator==(const IntegralIterator& rhs) const noexcept { return index == rhs.index; }
        private:
            std::function<T(T)> function;
            T start_x = 0;
            T delta_x = 0;
            T prev_y = 0;
            T area_element = 0;
            size_t index = 0;
        };

        template <typename T, size_t N>
        class VolumeIterator:
        public ForwardIterator<VolumeIterator<T, N>, const T> {
        public:
            using vector_type = Vector<T, N>;
            VolumeIterator(): done(true) {}
            template <typename F> VolumeIterator(vector_type x1, vector_type x2, size_t k, F f):
                function(f), start_x(x1), delta_x((x2 - x1) / k), n_edge(k),
                volume_factor(std::ldexp(product_of(delta_x), - int(N))), volume_element(get_volume()) {}
            const T& operator*() const noexcept { return volume_element; }
            VolumeIterator& operator++() { next_index(); volume_element = get_volume(); return *this; }
            bool operator==(const VolumeIterator& rhs) const noexcept { return done == rhs.done && (done || index == rhs.index); }
        private:
            static constexpr size_t points = size_t(1) << N;
            std::function<T(vector_type)> function;
            std::deque<std::pair<vector_type, T>> function_cache;
            vector_type start_x;
            vector_type delta_x;
            Vector<size_t, N> index;
            size_t n_edge = 0;
            T volume_factor = 0;
            T volume_element = 0;
            bool done = false;
            void next_index() {
                size_t i = 0;
                while (i < N) {
                    if (++index[i] < n_edge)
                        break;
                    index[i++] = 0;
                }
                done = i == N;
            }
            T get_value(vector_type x) {
                static constexpr size_t cache_size = 2 * points;
                for (auto& xy: function_cache)
                    if (xy.first == x)
                        return xy.second;
                T y = function(x);
                function_cache.push_back({x, y});
                if (function_cache.size() > cache_size)
                    function_cache.pop_front();
                return y;
            }
            T get_volume() {
                auto corner = start_x + delta_x * index;
                std::array<T, points> y;
                for (size_t i = 0; i < points; ++i) {
                    auto x = corner;
                    for (size_t j = 0; j < N; ++j)
                        x[j] += delta_x[j] * ((i >> j) & 1);
                    y[i] = get_value(x);
                }
                return volume_factor * precision_sum(y);
            }
        };

    }

    template <typename T, typename F>
    T line_integral(T x1, T x2, size_t k, F f) {
        using namespace RS_Detail;
        static_assert(std::is_floating_point<T>::value);
        static_assert(std::is_convertible<F, std::function<T(T)>>::value);
        IntegralIterator<T> i(x1, x2, k, f), j(k);
        return precision_sum(irange(i, j));
    }

    template <typename T, size_t N, typename F>
    T volume_integral(Vector<T, N> x1, Vector<T, N> x2, size_t k, F f) {
        using namespace RS_Detail;
        static_assert(std::is_floating_point<T>::value);
        static_assert(std::is_convertible<F, std::function<T(Vector<T, N>)>>::value);
        VolumeIterator<T, N> i(x1, x2, k, f), j;
        return precision_sum(irange(i, j));
    }

    // Root finding

    namespace RS_Detail {

        template <typename T>
        T default_epsilon() noexcept {
            static_assert(std::is_floating_point<T>::value);
            return std::pow(T(10), - std::min(T(sizeof(T)), T(8)));
        }

    }

    template <typename T>
    struct Bisection {
        static_assert(std::is_floating_point<T>::value);
        T epsilon = RS_Detail::default_epsilon<T>();
        size_t limit = 100;
        size_t count = 0;
        T error = {};
        template <typename F> T operator()(F f, T a, T b) {
            using std::abs;
            T x1 = a, x2 = b, x3 = a, y1 = f(a), y2 = f(b), y3 = 0;
            count = 0;
            error = abs(y1);
            if (error <= epsilon || limit == 0)
                return a;
            error = abs(y2);
            if (error <= epsilon)
                return b;
            for (; count < limit; ++count) {
                x3 = (x1 + x2) / 2;
                y3 = f(x3);
                error = abs(y3);
                if (error <= epsilon)
                    break;
                if ((y1 > 0) != (y3 > 0)) {
                    x2 = x3;
                    y2 = y3;
                } else {
                    x1 = x3;
                    y1 = y3;
                }
            }
            return x3;
        }
        template <typename F> T operator()(F f, T a = {}) { return (*this)(f, a, a); }
    };

    template <typename T>
    struct FalsePosition {
        static_assert(std::is_floating_point<T>::value);
        T epsilon = RS_Detail::default_epsilon<T>();
        size_t limit = 100;
        size_t count = 0;
        T error = {};
        template <typename F> T operator()(F f, T a, T b) {
            using std::abs;
            T x1 = a, x2 = b, x3 = a, y1 = f(a), y2 = f(b), y3 = 0;
            count = 0;
            error = abs(y1);
            if (error <= epsilon || limit == 0)
                return a;
            error = abs(y2);
            if (error <= epsilon)
                return b;
            for (; count < limit; ++count) {
                x3 = (x1 * y2 - x2 * y1) / (y2 - y1);
                y3 = f(x3);
                error = abs(y3);
                if (error <= epsilon)
                    break;
                if ((y1 > 0) != (y3 > 0)) {
                    x2 = x3;
                    y2 = y3;
                } else {
                    x1 = x3;
                    y1 = y3;
                }
            }
            return x3;
        }
        template <typename F> T operator()(F f, T a = {}) { return (*this)(f, a, a); }
    };

    template <typename T>
    struct NewtonRaphson {
        static_assert(std::is_floating_point<T>::value);
        T epsilon = RS_Detail::default_epsilon<T>();
        size_t limit = 100;
        size_t count = 0;
        T error = {};
        template <typename F, typename DF> T operator()(F f, DF df, T a = {}) {
            using std::abs;
            T x = a, y = f(a);
            error = abs(y);
            for (count = 0; count < limit; ++count) {
                if (error <= epsilon)
                    break;
                T dy = df(x);
                if (dy != 0)
                    x -= y / dy;
                else
                    x += 100 * epsilon;
                y = f(x);
                error = abs(y);
            }
            return x;
        }
    };

    template <typename T>
    struct PseudoNewtonRaphson {
        static_assert(std::is_floating_point<T>::value);
        T epsilon = RS_Detail::default_epsilon<T>();
        T delta = RS_Detail::default_epsilon<T>();
        size_t limit = 100;
        size_t count = 0;
        T error = {};
        template <typename F> T operator()(F f, T a = {}) {
            using std::abs;
            T x = a, y = f(a);
            error = abs(y);
            for (count = 0; count < limit; ++count) {
                if (error <= epsilon)
                    break;
                T dy = (f(x + delta) - y) / delta;
                if (dy != 0)
                    x -= y / dy;
                else
                    x += 100 * epsilon;
                y = f(x);
                error = abs(y);
            }
            return x;
        }
    };

}
