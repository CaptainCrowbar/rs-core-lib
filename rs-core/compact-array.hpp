#pragma once

#include "rs-core/common.hpp"
#include "rs-core/meta.hpp"
#include <algorithm>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace RS {

    namespace RS_Detail {

        #if (defined(__GNUC__) && ! defined(__clang__) && (__GNUC__ <= 6)) || (defined(_MSC_VER) && _MSC_VER <= 1900)

            template <typename ForwardIterator>
            void destroy(ForwardIterator i, ForwardIterator j) {
                using T = typename std::iterator_traits<ForwardIterator>::value_type;
                for (; i != j; ++i)
                    (*i).~T();
            }

            template <typename InputIterator, typename ForwardIterator>
            ForwardIterator uninitialized_move(InputIterator i, InputIterator j, ForwardIterator k) {
                using T = typename std::iterator_traits<ForwardIterator>::value_type;
                for (; i != j; ++i, ++k)
                    new (&*k) T(std::move(*i));
                return k;
            }

        #else

            using std::destroy;
            using std::uninitialized_move;

        #endif

        template <typename CT>
        class CompactArrayIterator:
        public RandomAccessIterator<CompactArrayIterator<CT>, CT> {
        public:
            CompactArrayIterator() = default;
            CompactArrayIterator(const CompactArrayIterator<std::remove_const_t<CT>>& i): ptr(i.ptr) {}
            explicit CompactArrayIterator(CT* p): ptr(p) {}
            CT& operator*() const noexcept { return *ptr; }
            CompactArrayIterator& operator+=(ptrdiff_t rhs) noexcept { ptr += rhs; return *this; }
            ptrdiff_t operator-(CompactArrayIterator rhs) const noexcept { return ptr - rhs.ptr; }
        private:
            template <typename CT2> friend class CompactArrayIterator;
            CT* ptr = nullptr;
        };

    }

    template <typename T, size_t N>
    class CompactArray:
    public LessThanComparable<CompactArray<T, N>> {
    public:
        using const_iterator = RS_Detail::CompactArrayIterator<const T>;
        using const_reference = const T&;
        using difference_type = ptrdiff_t;
        using iterator = RS_Detail::CompactArrayIterator<T>;
        using reference = T&;
        using size_type = size_t;
        using value_type = T;
        static constexpr size_t threshold = N;
        CompactArray() noexcept = default;
        explicit CompactArray(size_t n, const T& t = {});
        template <typename InputIterator> CompactArray(InputIterator i, InputIterator j, std::enable_if_t<is_iterator<InputIterator>>* = nullptr);
        CompactArray(std::initializer_list<T> list);
        ~CompactArray() noexcept { clear(); }
        CompactArray(const CompactArray& ca);
        CompactArray(CompactArray&& ca) noexcept;
        CompactArray& operator=(const CompactArray& ca);
        CompactArray& operator=(CompactArray&& ca) noexcept;
        CompactArray& operator=(std::initializer_list<T> list);
        T& operator[](size_t i) noexcept { return data()[i]; }
        const T& operator[](size_t i) const noexcept { return data()[i]; }
        T& at(size_t i) { check_index(i); return data()[i]; }
        const T& at(size_t i) const { check_index(i); return data()[i]; }
        iterator begin() noexcept { return iterator(data()); }
        const_iterator begin() const noexcept { return const_iterator(cdata()); }
        const_iterator cbegin() const noexcept { return const_iterator(cdata()); }
        iterator end() noexcept { return begin() + num; }
        const_iterator end() const noexcept { return begin() + num; }
        const_iterator cend() const noexcept { return cbegin() + num; }
        T* data() noexcept { return reinterpret_cast<T*>(local ? mem : pc.ptr); }
        const T* data() const noexcept { return cdata(); }
        const T* cdata() const noexcept { return reinterpret_cast<const T*>(local ? mem : pc.ptr); }
        size_t capacity() const noexcept { return local ? N : pc.cap; }
        void clear() noexcept;
        bool empty() const noexcept { return num == 0; }
        bool is_compact() const noexcept { return local; }
        void reserve(size_t n);
        void resize(size_t n, const T& t = {});
        void shrink_to_fit();
        size_t size() const noexcept { return num; }
        template <typename InputIterator> iterator append(InputIterator i, InputIterator j);
        template <typename InputRange> iterator append(const InputRange& r);
        template <typename InputRange> iterator append(InputRange&& r);
        template <typename... Args> void emplace_back(Args&&... args);
        template <typename... Args> iterator emplace(const_iterator i, Args&&... args);
        iterator insert(const_iterator i, const T& t);
        iterator insert(const_iterator i, T&& t);
        template <typename InputIterator> iterator insert(const_iterator i, InputIterator j, InputIterator k);
        void push_back(const T& t);
        void push_back(T&& t);
        void erase(const_iterator i) noexcept;
        void erase(const_iterator i, const_iterator j) noexcept;
        void pop_back() noexcept;
        size_t hash() const noexcept;
        void swap(CompactArray& ca) noexcept;
        friend bool operator==(const CompactArray& lhs, const CompactArray& rhs) noexcept
            { return lhs.size() == rhs.size() && std::equal(lhs.begin(), lhs.end(), rhs.begin()); }
        friend bool operator<(const CompactArray& lhs, const CompactArray& rhs) noexcept
            { return std::lexicographical_compare(lhs.begin(), lhs.end(), rhs.begin(), rhs.end()); }
    private:
        using raw_memory = typename std::aligned_union<1, T>::type;
        union {
            raw_memory mem[N];
            struct {
                raw_memory* ptr;
                size_t cap;
            } pc;
        };
        size_t num = 0;
        bool local = true;
        void check_index(size_t i) const { if (i >= num) throw std::out_of_range("Compact array index out of bounds"); }
    };

    template <typename T, size_t N>
    CompactArray<T, N>::CompactArray(size_t n, const T& t) {
        reserve(n);
        std::uninitialized_fill(begin(), begin() + n, t);
        num = n;
    }

    template <typename T, size_t N>
    template <typename InputIterator>
    CompactArray<T, N>::CompactArray(InputIterator i, InputIterator j, std::enable_if_t<is_iterator<InputIterator>>*) {
        using namespace RS_Detail;
        if (std::is_same<typename std::iterator_traits<InputIterator>::iterator_category, std::input_iterator_tag>::value) {
            for (; i != j; ++i)
                push_back(*i);
        } else {
            size_t n = std::distance(i, j);
            reserve(n);
            std::uninitialized_copy(i, j, begin());
            num = n;
        }
    }

    template <typename T, size_t N>
    CompactArray<T, N>::CompactArray(std::initializer_list<T> list) {
        reserve(list.size());
        std::uninitialized_copy(list.begin(), list.end(), begin());
        num = list.size();
    }

    template <typename T, size_t N>
    CompactArray<T, N>::CompactArray(const CompactArray& ca) {
        reserve(ca.size());
        std::uninitialized_copy(ca.begin(), ca.end(), begin());
        num = ca.num;
    }

    template <typename T, size_t N>
    CompactArray<T, N>::CompactArray(CompactArray&& ca) noexcept {
        using namespace RS_Detail;
        reserve(ca.size());
        uninitialized_move(ca.begin(), ca.end(), begin());
        num = ca.num;
        ca.clear();
    }

    template <typename T, size_t N>
    CompactArray<T, N>& CompactArray<T, N>::operator=(const CompactArray& ca) {
        CompactArray temp(ca);
        swap(temp);
        return *this;
    }

    template <typename T, size_t N>
    CompactArray<T, N>& CompactArray<T, N>::operator=(CompactArray&& ca) noexcept {
        CompactArray temp(std::move(ca));
        swap(temp);
        return *this;
    }

    template <typename T, size_t N>
    CompactArray<T, N>& CompactArray<T, N>::operator=(std::initializer_list<T> list) {
        CompactArray temp(list);
        swap(temp);
        return *this;
    }

    template <typename T, size_t N>
    void CompactArray<T, N>::clear() noexcept {
        using namespace RS_Detail;
        destroy(begin(), end());
        if (! local)
            delete[] pc.ptr;
        num = 0;
        local = true;
    }

    template <typename T, size_t N>
    void CompactArray<T, N>::reserve(size_t n) {
        using namespace RS_Detail;
        if (n <= num) {
            destroy(begin() + n, end());
            num = n;
            return;
        }
        if (n <= N)
            return;
        size_t new_cap = iceil2(n);
        if (new_cap <= capacity())
            return;
        auto new_ptr = new raw_memory[new_cap];
        auto new_t_ptr = reinterpret_cast<T*>(new_ptr);
        uninitialized_move(begin(), end(), new_t_ptr);
        destroy(begin(), end());
        if (! local)
            delete[] pc.ptr;
        pc.ptr = new_ptr;
        pc.cap = new_cap;
        local = false;
    }

    template <typename T, size_t N>
    void CompactArray<T, N>::resize(size_t n, const T& t) {
        reserve(n);
        if (n > num) {
            std::uninitialized_fill(begin() + num, begin() + n, t);
            num = n;
        }
    }

    template <typename T, size_t N>
    void CompactArray<T, N>::shrink_to_fit() {
        using namespace RS_Detail;
        if (local) {
            // Already at minimum capacity, nothing to do
        } else if (num <= N) {
            auto old_ptr = pc.ptr;
            auto old_t_ptr = reinterpret_cast<T*>(old_ptr);
            local = true;
            uninitialized_move(old_t_ptr, old_t_ptr + num, begin());
            destroy(old_t_ptr, old_t_ptr + num);
            delete[] old_ptr;
        } else {
            auto new_ptr = new raw_memory[num];
            auto new_t_ptr = reinterpret_cast<T*>(new_ptr);
            uninitialized_move(begin(), end(), new_t_ptr);
            destroy(begin(), end());
            delete[] pc.ptr;
            pc.ptr = new_ptr;
            pc.cap = num;
        }
    }

    template <typename T, size_t N>
    template <typename InputIterator>
    typename CompactArray<T, N>::iterator CompactArray<T, N>::append(InputIterator i, InputIterator j) {
        using namespace RS_Detail;
        size_t n_old = num;
        if (std::is_same<typename std::iterator_traits<InputIterator>::iterator_category, std::input_iterator_tag>::value) {
            for (; i != j; ++i)
                push_back(*i);
        } else {
            size_t n_new = std::distance(i, j);
            reserve(num + n_new);
            std::uninitialized_copy(i, j, begin() + n_old);
            num += n_new;
        }
        return begin() + n_old;
    }

    template <typename T, size_t N>
    template <typename InputRange>
    typename CompactArray<T, N>::iterator CompactArray<T, N>::append(const InputRange& r) {
        using std::begin;
        using std::end;
        return append(begin(r), end(r));
    }

    template <typename T, size_t N>
    template <typename InputRange>
    typename CompactArray<T, N>::iterator CompactArray<T, N>::append(InputRange&& r) {
        using namespace RS_Detail;
        using std::begin;
        using std::end;
        auto i = begin(r), j = end(r);
        size_t n_old = num;
        if (std::is_same<typename std::iterator_traits<decltype(i)>::iterator_category, std::input_iterator_tag>::value) {
            for (; i != j; ++i)
                push_back(*i);
        } else {
            size_t n_new = std::distance(i, j);
            reserve(num + n_new);
            uninitialized_move(i, j, this->begin() + n_old);
            num += n_new;
        }
        return this->begin() + n_old;
    }

    template <typename T, size_t N>
    template <typename... Args>
    void CompactArray<T, N>::emplace_back(Args&&... args) {
        reserve(num + 1);
        new (data() + num) T(args...);
        ++num;
    }

    template <typename T, size_t N>
    template <typename... Args>
    typename CompactArray<T, N>::iterator CompactArray<T, N>::emplace(const_iterator i, Args&&... args) {
        size_t pos = i - begin();
        reserve(num + 1);
        if (pos < num) {
            new (data() + num) T(std::move(cend()[-1]));
            std::move_backward(begin() + pos, end() - 1, end());
            begin()[pos].~T();
        }
        new (data() + pos) T(args...);
        ++num;
        return begin() + pos;
    }

    template <typename T, size_t N>
    typename CompactArray<T, N>::iterator CompactArray<T, N>::insert(const_iterator i, const T& t) {
        size_t pos = i - begin();
        reserve(num + 1);
        if (pos < num) {
            new (data() + num) T(std::move(cend()[-1]));
            std::move_backward(begin() + pos, end() - 1, end());
            begin()[pos] = t;
        } else {
            new (data() + pos) T(t);
        }
        ++num;
        return begin() + pos;
    }

    template <typename T, size_t N>
    typename CompactArray<T, N>::iterator CompactArray<T, N>::insert(const_iterator i, T&& t) {
        size_t pos = i - begin();
        reserve(num + 1);
        if (pos < num) {
            new (data() + num) T(std::move(cend()[-1]));
            std::move_backward(begin() + pos, end() - 1, end());
            begin()[pos] = std::move(t);
        } else {
            new (data() + pos) T(std::move(t));
        }
        ++num;
        return begin() + pos;
    }

    template <typename T, size_t N>
    template <typename InputIterator>
    typename CompactArray<T, N>::iterator CompactArray<T, N>::insert(const_iterator i, InputIterator j, InputIterator k) {
        using namespace RS_Detail;
        size_t n_before = i - begin(), n_after = num - n_before;
        if (std::is_same<typename std::iterator_traits<InputIterator>::iterator_category, std::input_iterator_tag>::value) {
            CompactArray temp(i, cend());
            erase(i, end());
            for (; j != k; ++j)
                push_back(*j);
            append(std::move(temp));
        } else {
            size_t n_inserted = std::distance(j, k);
            reserve(num + n_inserted);
            auto insert_at = begin() + n_before;
            if (n_inserted < n_after) {
                uninitialized_move(end() - n_inserted, end(), end());
                std::move_backward(insert_at, end() - n_inserted, end());
                std::copy(j, k, insert_at);
            } else {
                uninitialized_move(insert_at, end(), end() + n_inserted - n_after);
                auto mid = j;
                std::advance(mid, n_after);
                std::copy(j, mid, insert_at);
                std::uninitialized_copy(mid, k, end());
            }
            num += n_inserted;
        }
        return begin() + n_before;
    }

    template <typename T, size_t N>
    void CompactArray<T, N>::push_back(const T& t) {
        reserve(num + 1);
        new (data() + num) T(t);
        ++num;
    }

    template <typename T, size_t N>
    void CompactArray<T, N>::push_back(T&& t) {
        reserve(num + 1);
        new (data() + num) T(std::move(t));
        ++num;
    }

    template <typename T, size_t N>
    void CompactArray<T, N>::erase(const_iterator i) noexcept {
        auto mut = begin() + (i - begin());
        std::move(i + 1, cend(), mut);
        end()[-1].~T();
        --num;
    }

    template <typename T, size_t N>
    void CompactArray<T, N>::erase(const_iterator i, const_iterator j) noexcept {
        using namespace RS_Detail;
        size_t n_erase = j - i;
        auto mut = begin() + (i - begin());
        std::move(j, cend(), mut);
        destroy(end() - n_erase, end());
        num -= n_erase;
    }

    template <typename T, size_t N>
    void CompactArray<T, N>::pop_back() noexcept {
        end()[-1].~T();
        --num;
    }

    template <typename T, size_t N>
    size_t CompactArray<T, N>::hash() const noexcept {
        size_t h = 0;
        std::hash<T> ht;
        for (auto& t: *this)
            h = (h << 1) + ht(t);
        return h;
    }

    template <typename T, size_t N>
    void CompactArray<T, N>::swap(CompactArray& ca) noexcept {
        using namespace RS_Detail;
        if (local && ca.local) {
            size_t common = std::min(num, ca.num);
            std::swap_ranges(begin(), begin() + common, ca.begin());
            if (num > common) {
                uninitialized_move(begin() + common, end(), ca.begin() + common);
                destroy(begin() + common, end());
            } else if (ca.num > common) {
                uninitialized_move(ca.begin() + common, ca.end(), begin() + common);
                destroy(ca.begin() + common, ca.end());
            }
            std::swap(num, ca.num);
        } else if (local) {
            auto p = ca.pc.ptr;
            auto c = ca.pc.cap;
            auto ca_ptr = reinterpret_cast<T*>(ca.mem);
            uninitialized_move(begin(), end(), ca_ptr);
            destroy(begin(), end());
            std::swap(num, ca.num);
            std::swap(local, ca.local);
            pc.ptr = p;
            pc.cap = c;
        } else if (ca.local) {
            ca.swap(*this);
        } else {
            std::swap(pc.ptr, ca.pc.ptr);
            std::swap(pc.cap, ca.pc.cap);
            std::swap(num, ca.num);
        }
    }

    template <typename T, size_t N>
    void swap(CompactArray<T, N>& a, CompactArray<T, N>& b) noexcept {
        a.swap(b);
    }

}

namespace std {

    template <typename T, size_t N>
    struct hash<RS::CompactArray<T, N>> {
        using argument_type = RS::CompactArray<T, N>;
        using result_type = size_t;
        size_t operator()(RS::CompactArray<T, N> ca) const noexcept { return ca.hash(); }
    };

}