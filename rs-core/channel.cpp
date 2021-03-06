#include "rs-core/channel.hpp"
#include <stdexcept>
#include <thread>

namespace RS {

    // Class Channel

    Channel::~Channel() noexcept {
        Dispatch::do_drop(*this);
    }

    Channel& Channel::operator=(Channel&& c) noexcept {
        if (&c != this)
            Dispatch::do_drop(*this);
        return *this;
    }

    // Class StreamChannel

    std::string StreamChannel::read_all() {
        using namespace std::chrono;
        std::string s;
        for (;;) {
            if (! wait_for(seconds(1)))
                continue;
            if (is_closed())
                break;
            read_to(s);
        }
        return s;
    }

    std::string StreamChannel::read_str() {
        std::string s(bytes, '\0');
        size_t n = read(&s[0], bytes);
        s.resize(n);
        return s;
    }

    size_t StreamChannel::read_to(std::string& dst) {
        size_t pos = dst.size(), n = 0;
        auto guard = scope_exit([&] { dst.resize(pos + n); });
        dst.resize(pos + bytes);
        n = read(&dst[0] + pos, bytes);
        return n;
    }

    // Class FalseChannel

    void FalseChannel::close() noexcept {
        auto lock = make_lock(mutex);
        open = false;
        cv.notify_all();
    }

    bool FalseChannel::do_wait_for(duration t) {
        auto lock = make_lock(mutex);
        if (! open)
            return true;
        else if (t <= duration())
            return false;
        cv.wait_for(lock, t, [&] { return ! open; });
        return ! open;
    }

    // Class TimerChannel

    void TimerChannel::close() noexcept {
        auto lock = make_lock(mutex);
        open = false;
        cv.notify_all();
    }

    void TimerChannel::flush() noexcept {
        auto lock = make_lock(mutex);
        if (! open)
            return;
        auto now = clock_type::now();
        if (now < next_tick)
            return;
        auto skip = (now - next_tick).count() / delta.count();
        next_tick += delta * (skip + 1);
    }

    bool TimerChannel::do_wait_for(duration t) {
        using namespace std::chrono;
        auto lock = make_lock(mutex);
        if (! open)
            return true;
        auto now = clock_type::now();
        if (next_tick <= now) {
            next_tick += delta;
            return true;
        }
        if (t <= duration())
            return false;
        auto remaining = duration_cast<duration>(next_tick - now);
        if (t < remaining) {
            cv.wait_for(lock, t, [&] { return ! open; });
            return ! open;
        }
        cv.wait_for(lock, remaining, [&] { return ! open; });
        if (open)
            next_tick += delta;
        return true;
    }

    // Class ThrottleChannel

    void ThrottleChannel::close() noexcept {
        auto lock = make_lock(mutex);
        open = false;
        cv.notify_all();
    }

    bool ThrottleChannel::do_wait_for(duration t) {
        using namespace std::chrono;
        auto lock = make_lock(mutex);
        if (! open)
            return true;
        auto now = clock_type::now();
        if (next <= now) {
            next = now + delta;
            return true;
        }
        if (t <= duration())
            return false;
        auto remaining = duration_cast<duration>(next - now);
        if (t < remaining) {
            cv.wait_for(lock, t, [&] { return ! open; });
            return ! open;
        }
        cv.wait_for(lock, remaining, [&] { return ! open; });
        if (open)
            next = clock_type::now() + delta;
        return true;
    }

    // Class BufferChannel

    void BufferChannel::close() noexcept {
        auto lock = make_lock(mutex);
        buf.clear();
        ofs = 0;
        open = false;
        cv.notify_all();
    }

    size_t BufferChannel::read(void* dst, size_t maxlen) {
        if (! dst || ! maxlen)
            return 0;
        auto lock = make_lock(mutex);
        if (! open)
            return 0;
        size_t n = std::min(buf.size() - ofs, maxlen);
        memcpy(dst, buf.data() + ofs, n);
        ofs += n;
        if (ofs == buf.size()) {
            buf.clear();
            ofs = 0;
        } else if (2 * ofs >= buf.size()) {
            buf.erase(0, ofs);
            ofs = 0;
        }
        return n;
    }

    bool BufferChannel::write(const void* src, size_t len) {
        auto lock = make_lock(mutex);
        if (! open)
            return false;
        buf.append(static_cast<const char*>(src), len);
        if (ofs < buf.size())
            cv.notify_all();
        return true;
    }

    void BufferChannel::clear() noexcept {
        auto lock = make_lock(mutex);
        buf.clear();
        ofs = 0;
    }

    bool BufferChannel::do_wait_for(duration t) {
        auto lock = make_lock(mutex);
        if (open && ofs == buf.size() && t > duration())
            cv.wait_for(lock, t, [&] { return ! open || ofs < buf.size(); });
        return ! open || ofs < buf.size();
    }

    // Class Dispatch

    Dispatch::result Dispatch::run() noexcept {
        using namespace std::chrono;
        using namespace std::literals;
        static constexpr Channel::duration min_interval = 1us;
        static constexpr Channel::duration max_interval = 1ms;
        result rc;
        if (tasks().empty())
            return rc;
        auto guard = scope_exit([&] { if (rc.channel) do_drop(*rc.channel); });
        auto interval = min_interval;
        int waits = 0;
        for (;;) {
            int calls = 0;
            for (auto& t: tasks()) {
                rc.channel = t.first;
                if (t.second.kind == mode::sync) {
                    try {
                        if (t.first->poll()) {
                            if (t.first->is_closed()) {
                                rc.why = reason::closed;
                                return rc;
                            }
                            t.second.call();
                            ++calls;
                        }
                    }
                    catch (...) {
                        rc.error = std::current_exception();
                        rc.why = reason::error;
                        return rc;
                    }
                } else if (t.second.done) {
                    rc.error = t.second.error;
                    rc.why = rc.error ? reason::error : reason::closed;
                    return rc;
                }
            }
            if (calls == 0) {
                if (++waits == 1)
                    interval = min_interval;
                else
                    interval = std::min(2 * interval, max_interval);
                std::this_thread::sleep_for(interval);
            } else {
                waits = 0;
                std::this_thread::yield();
            }
        }
    }

    void Dispatch::stop() noexcept {
        for (auto& t: tasks())
            t.first->close();
        while (! tasks().empty())
            run();
    }

    void Dispatch::do_add(Channel& c, mode m, callback f) {
        using namespace std::chrono;
        if (! c.is_shared() && tasks().count(&c))
            throw std::invalid_argument("Channel is not shareable");
        if (m != mode::sync && m != mode::async)
            throw std::invalid_argument("Invalid dispatch mode");
        if (m == mode::async && ! c.is_async())
            throw std::invalid_argument("Invalid dispatch mode for channel");
        auto& task = tasks()[&c];
        task.call = f;
        task.kind = m;
        if (task.kind == mode::async) {
            auto payload = [&] () noexcept {
                try {
                    for (;;) {
                        if (! c.wait_for(seconds(1)))
                            continue;
                        if (c.is_closed())
                            break;
                        task.call();
                    }
                }
                catch (...) {
                    task.error = std::current_exception();
                }
                task.done = true;
            };
            task.thread = Thread(payload);
        }
    }

    void Dispatch::do_drop(Channel& c) noexcept {
        tasks().erase(&c);
    }

    Dispatch::task_map& Dispatch::tasks() noexcept {
        static task_map map;
        return map;
    }

    std::string to_str(Dispatch::mode m) {
        switch (m) {
            case Dispatch::mode::sync:   return "sync";
            case Dispatch::mode::async:  return "async";
            default:                     return std::to_string(int(m));
        }
    }

    std::string to_str(Dispatch::reason r) {
        switch (r) {
            case Dispatch::reason::empty:   return "empty";
            case Dispatch::reason::closed:  return "closed";
            case Dispatch::reason::error:   return "error";
            default:                        return std::to_string(int(r));
        }
    }

}
