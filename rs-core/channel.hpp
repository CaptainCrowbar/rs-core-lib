#pragma once

#include "rs-core/common.hpp"
#include "rs-core/optional.hpp"
#include "rs-core/time.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <exception>
#include <functional>
#include <map>
#include <mutex>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>

namespace RS {

    // Forward declarations

    class BufferChannel;
    class Channel;
    class Dispatch;
    class EventChannel;
    class FalseChannel;
    template <typename T> class GeneratorChannel;
    template <typename T> class MessageChannel;
    template <typename T> class QueueChannel;
    class StreamChannel;
    class ThrottleChannel;
    class TimerChannel;
    class TrueChannel;
    template <typename T> class ValueChannel;

    // Channel base class

    class Channel:
    public Wait {
    public:
        virtual ~Channel() noexcept;
        Channel(const Channel&) = delete;
        Channel(Channel&& c) = default;
        Channel& operator=(const Channel&) = delete;
        Channel& operator=(Channel&& c) noexcept;
        virtual void close() noexcept = 0;
        virtual bool is_closed() const noexcept = 0;
        virtual bool is_async() const noexcept { return true; }
    protected:
        Channel() = default;
    };

    // Intermediate base classes

    class EventChannel:
    public Channel {
    public:
        using callback = std::function<void()>;
    protected:
        EventChannel() = default;
    };

    template <typename T>
    class MessageChannel:
    public Channel {
    public:
        using callback = std::function<void(const T&)>;
        using value_type = T;
        virtual bool read(T& t) = 0;
        Optional<T> read_opt();
    protected:
        MessageChannel() = default;
    };

        template <typename T>
        Optional<T> MessageChannel<T>::read_opt() {
            T t;
            if (read(t))
                return std::move(t);
            else
                return {};
        }

    class StreamChannel:
    public Channel {
    public:
        using callback = std::function<void(std::string&)>;
        static constexpr size_t default_buffer = 16384;
        virtual size_t read(void* dst, size_t maxlen) = 0;
        size_t buffer() const noexcept { return bytes; }
        std::string read_all();
        std::string read_str();
        size_t read_to(std::string& dst);
        void set_buffer(size_t n) noexcept { bytes = n; }
    protected:
        StreamChannel() = default;
    private:
        size_t bytes = default_buffer;
    };

    // Concrete channel classes

    class TrueChannel:
    public EventChannel {
    public:
        RS_NO_COPY_MOVE(TrueChannel);
        TrueChannel() = default;
        virtual void close() noexcept { open = false; }
        virtual bool is_closed() const noexcept { return ! open; }
        virtual bool is_shared() const noexcept { return true; }
    protected:
        virtual bool do_wait_for(duration /*t*/) { return true; }
    private:
        std::atomic<bool> open{true};
    };

    class FalseChannel:
    public EventChannel {
    public:
        RS_NO_COPY_MOVE(FalseChannel);
        FalseChannel() = default;
        virtual void close() noexcept;
        virtual bool is_closed() const noexcept { return ! open; }
        virtual bool is_shared() const noexcept { return true; }
    protected:
        virtual bool do_wait_for(duration t);
    private:
        mutable std::mutex mutex;
        std::condition_variable cv;
        bool open = true;
    };

    class TimerChannel:
    public EventChannel {
    public:
        RS_NO_COPY_MOVE(TimerChannel);
        template <typename R, typename P> explicit TimerChannel(std::chrono::duration<R, P> t) noexcept;
        virtual void close() noexcept;
        virtual bool is_closed() const noexcept { return ! open; }
        virtual bool is_shared() const noexcept { return true; }
        void flush() noexcept;
        duration interval() const noexcept { return delta; }
        auto next() const noexcept { return next_tick; }
    protected:
        virtual bool do_wait_for(duration t);
    private:
        using clock_type = ReliableClock;
        using time_point = clock_type::time_point;
        mutable std::mutex mutex;
        std::condition_variable cv;
        time_point next_tick;
        duration delta;
        bool open = true;
    };

        template <typename R, typename P>
        TimerChannel::TimerChannel(std::chrono::duration<R, P> t) noexcept {
            using namespace std::chrono;
            delta = std::max(duration_cast<duration>(t), duration());
            next_tick = clock_type::now() + delta;
        }

    class ThrottleChannel:
    public EventChannel {
    public:
        RS_NO_COPY_MOVE(ThrottleChannel);
        template <typename R, typename P> explicit ThrottleChannel(std::chrono::duration<R, P> t) noexcept;
        virtual void close() noexcept;
        virtual bool is_closed() const noexcept { return ! open; }
        virtual bool is_shared() const noexcept { return true; }
        duration interval() const noexcept { return delta; }
    protected:
        virtual bool do_wait_for(duration t);
    private:
        using clock_type = ReliableClock;
        using time_point = clock_type::time_point;
        mutable std::mutex mutex;
        std::condition_variable cv;
        time_point next = time_point::min();
        duration delta;
        bool open = true;
    };

        template <typename R, typename P>
        ThrottleChannel::ThrottleChannel(std::chrono::duration<R, P> t) noexcept {
            using namespace std::chrono;
            delta = std::max(duration_cast<duration>(t), duration());
        }

    template <typename T>
    class GeneratorChannel:
    public MessageChannel<T> {
    public:
        RS_NO_COPY_MOVE(GeneratorChannel);
        using generator = std::function<T()>;
        template <typename F> explicit GeneratorChannel(F f): mutex(), gen(f) {}
        virtual void close() noexcept;
        virtual bool is_closed() const noexcept { return ! gen; }
        virtual bool read(T& t);
    protected:
        virtual bool do_wait_for(Wait::duration /*t*/) { return true; }
    private:
        std::mutex mutex;
        generator gen;
    };

        template <typename T>
        void GeneratorChannel<T>::close() noexcept {
            auto lock = make_lock(mutex);
            gen = nullptr;
        }

        template <typename T>
        bool GeneratorChannel<T>::read(T& t) {
            auto lock = make_lock(mutex);
            if (gen)
                t = gen();
            return bool(gen);
        }

    template <typename T>
    class QueueChannel:
    public MessageChannel<T> {
    public:
        RS_NO_COPY_MOVE(QueueChannel);
        QueueChannel() = default;
        virtual void close() noexcept;
        virtual bool is_closed() const noexcept { return ! open; }
        virtual bool is_shared() const noexcept { return true; }
        virtual bool read(T& t);
        void clear() noexcept;
        bool write(const T& t);
        bool write(T&& t);
    protected:
        virtual bool do_wait_for(Wait::duration t);
    private:
        std::mutex mutex;
        std::condition_variable cv;
        bool open = true;
        std::deque<T> queue;
    };

        template <typename T>
        void QueueChannel<T>::close() noexcept {
            auto lock = make_lock(mutex);
            open = false;
            cv.notify_all();
        }

        template <typename T>
        bool QueueChannel<T>::read(T& t) {
            auto lock = make_lock(mutex);
            if (! open || queue.empty())
                return false;
            t = queue.front();
            queue.pop_front();
            if (! queue.empty())
                cv.notify_all();
            return true;
        }

        template <typename T>
        void QueueChannel<T>::clear() noexcept {
            auto lock = make_lock(mutex);
            queue.clear();
        }

        template <typename T>
        bool QueueChannel<T>::write(const T& t) {
            auto lock = make_lock(mutex);
            if (! open)
                return false;
            queue.push_back(t);
            cv.notify_all();
            return true;
        }

        template <typename T>
        bool QueueChannel<T>::write(T&& t) {
            auto lock = make_lock(mutex);
            if (! open)
                return false;
            queue.push_back(std::move(t));
            cv.notify_all();
            return true;
        }

        template <typename T>
        bool QueueChannel<T>::do_wait_for(Wait::duration t) {
            auto lock = make_lock(mutex);
            if (open && queue.empty() && t > Wait::duration())
                cv.wait_for(lock, t, [&] { return ! open || ! queue.empty(); });
            return ! open || ! queue.empty();
        }

    template <typename T>
    class ValueChannel:
    public MessageChannel<T> {
    public:
        RS_NO_COPY_MOVE(ValueChannel);
        ValueChannel() = default;
        explicit ValueChannel(const T& t): value(t) {}
        virtual void close() noexcept;
        virtual bool is_closed() const noexcept { return status == -1; }
        virtual bool is_shared() const noexcept { return true; }
        virtual bool read(T& t);
        void clear() noexcept;
        bool write(const T& t);
        bool write(T&& t);
    protected:
        virtual bool do_wait_for(Wait::duration t);
    private:
        std::mutex mutex;
        std::condition_variable cv;
        T value;
        int status = 0; // +1 = new value, 0 = no change, -1 = closed
    };

        template <typename T>
        void ValueChannel<T>::close() noexcept {
            auto lock = make_lock(mutex);
            status = -1;
            cv.notify_all();
        }

        template <typename T>
        bool ValueChannel<T>::read(T& t) {
            auto lock = make_lock(mutex);
            if (status != 1)
                return false;
            t = value;
            status = 0;
            return true;
        }

        template <typename T>
        bool ValueChannel<T>::write(const T& t) {
            auto lock = make_lock(mutex);
            if (status == -1)
                return false;
            if (t == value)
                return true;
            value = t;
            status = 1;
            cv.notify_all();
            return true;
        }

        template <typename T>
        bool ValueChannel<T>::write(T&& t) {
            auto lock = make_lock(mutex);
            if (status == -1)
                return false;
            if (t == value)
                return true;
            value = std::move(t);
            status = 1;
            cv.notify_all();
            return true;
        }

        template <typename T>
        bool ValueChannel<T>::do_wait_for(Wait::duration t) {
            auto lock = make_lock(mutex);
            if (status == 0 && t > Wait::duration())
                cv.wait_for(lock, t, [&] { return status != 0; });
            return status;
        }

    class BufferChannel:
    public StreamChannel {
    public:
        RS_NO_COPY_MOVE(BufferChannel);
        BufferChannel() = default;
        virtual void close() noexcept;
        virtual bool is_closed() const noexcept { return ! open; }
        virtual size_t read(void* dst, size_t maxlen);
        void clear() noexcept;
        bool write(std::string_view src) { return write(src.data(), src.size()); }
        bool write(const void* src, size_t len);
    protected:
        virtual bool do_wait_for(duration t);
    private:
        std::mutex mutex;
        std::condition_variable cv;
        std::string buf;
        size_t ofs = 0;
        bool open = true;
    };

    // Dispatch controller class

    class Dispatch {
    public:
        enum class mode { async, sync };
        enum class reason { closed, empty, error };
        struct result {
            Channel* channel = nullptr;
            std::exception_ptr error;
            reason why = reason::empty;
        };
        template <typename F> static void add(EventChannel& c, mode m, F f);
        template <typename T, typename F> static void add(MessageChannel<T>& c, mode m, F f);
        template <typename F> static void add(StreamChannel& c, mode m, F f);
        static bool empty() noexcept { return tasks().empty(); }
        static result run() noexcept;
        static void stop() noexcept;
    private:
        RS_NO_INSTANCE(Dispatch);
        friend class Channel;
        using callback = std::function<void()>;
        struct task_info {
            Thread thread;
            callback call;
            mode kind;
            std::atomic<bool> done;
            std::exception_ptr error;
        };
        using task_map = std::map<Channel*, task_info>;
        static void do_add(Channel& c, mode m, callback f);
        static void do_drop(Channel& c) noexcept;
        static task_map& tasks() noexcept;
    };

        template <typename F>
        void Dispatch::add(EventChannel& c, mode m, F f) {
            callback call(f);
            if (! call)
                throw std::bad_function_call();
            do_add(c, m, call);
        }

        template <typename T, typename F>
        void Dispatch::add(MessageChannel<T>& c, mode m, F f) {
            typename MessageChannel<T>::callback chan_call(f);
            if (! chan_call)
                throw std::bad_function_call();
            auto disp_call = [&c,chan_call,t=T()] () mutable {
                if (c.read(t))
                    chan_call(t);
            };
            do_add(c, m, disp_call);
        }

        template <typename F>
        void Dispatch::add(StreamChannel& c, mode m, F f) {
            StreamChannel::callback chan_call(f);
            if (! chan_call)
                throw std::bad_function_call();
            auto disp_call = [&c,chan_call,s=std::string()] () mutable {
                if (c.read_to(s))
                    chan_call(s);
            };
            do_add(c, m, disp_call);
        }

    std::string to_str(Dispatch::mode m);
    std::string to_str(Dispatch::reason r);
    inline std::ostream& operator<<(std::ostream& out, Dispatch::mode m) { return out << to_str(m); }
    inline std::ostream& operator<<(std::ostream& out, Dispatch::reason r) { return out << to_str(r); }

}
