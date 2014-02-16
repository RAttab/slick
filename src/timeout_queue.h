/* timeout_queue.h                                 -*- C++ -*-
   Rémi Attab (remi.attab@gmail.com), 08 Feb 2014
   FreeBSD-style copyright and disclaimer apply

   Timeout management queue.
*/

#pragma once

namespace slick {


/******************************************************************************/
/* TIEMOUT QUEUE                                                              */
/******************************************************************************/

template<typename Key, typename Clock = lockless::Wall>
struct TimeoutQueue
{
    typedef Clock::ClockT ClockT;

    typedef std::function<void(Key)> TimeoutFn;
    TimeoutFn onTimeout;

    int fd() const { return timer.fd(); }
    void poll(int timeoutMs = 0);

    void set(const Key& key, ClockT deadline);
    void setTTL(const Key& key, ClockT ttl);
    void remove(const Key& key);

    ClockT deadline(const Key& key) const;

private:

    void updateTimer(ClockT now = clock())
    {
        ClockT next = queue.top().deadline;
        double delay = Clock::toSec(Clock::diff(now, next));
        timer.setDelay(delay);
    }

    struct Deadline
    {
        Key key;
        ClockT deadline;

        explicit Deadline(Key key = Key(), ClockT deadline = 0) :
            key(std::move(key)), deadline(deadline)
        {}

        bool operator< (const Deadline& other)
        {
            return deadline < other.deadline;
        }
    };

    Clock clock;
    Timer timer;
    std::priority_queue<Deadline> queue;
    std::map<Key, ClockT> keys;
};


template<typename Key, typename Clock>
void
TimeoutQueue<Key, Clock>::
poll(int timeoutMs)
{
    ClockT now = clock();

    std::vector<Key> triggered;

    while (!queue.empty()) {
        auto& item = queue.top();

        if (!keys.count(item.key)) continue;
        if (item.deadline > now) break;

        triggered.push_back(std::move(item.key));
        queue.pop();
    }

    if (!queue.empty()) updateTimer(now);

    for (Key& key : triggered)
        timeoutFn(std::move(key));
}


template<typename Key, typename Clock>
void
TimeoutQueue<Key, Clock>::
setTTL(const Key& key, ClockT ttl)
{
    set(key, clock() + ttl);
}


template<typename Key, typename Clock>
void
TimeoutQueue<Key, Clock>::
set(const Key& key, ClockT deadline)
{
    auto ret = keys.insert(std::make_pair(key, deadline));
    if (!ret.second) {
        auto& oldDeadline = ret.first->second;
        if (deadline == oldDeadline) return;
        oldDeadline = deadline;
    }

    bool update = deadline < queue.top().deadline;
    queue.emplace(key, deadline);
    if (update) updateTimer();
}


template<typename Key, typename Clock>
void
TimeoutQueue<Key, Clock>::
remove(const Key& key)
{
    keys.erase(key);
}


template<typename Key, typename Clock>
auto
TimeoutQueue<Key, Clock>::
deadline(const Key& key) const -> ClockT
{
    auto it = keys.find(key);
    assert(it != keys.end());
    return it->second.deadline;
}

} // slick
