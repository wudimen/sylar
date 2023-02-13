#ifndef _TIMER_H_
#define _TIMER_H_

#include <memory>
#include <vector>
#include <functional>
#include <set>

#include "mutex.h"

namespace sylar {

class TimerManager;
class Timer : public std::enable_shared_from_this<Timer> {
friend class TimerManager;
public:
	typedef std::shared_ptr<Timer> ptr;
	typedef Mutex MutexType;

public:
	bool resetTimer(uint64_t ms, bool fromNow);

	bool reflushTimer();

	bool cancel();
protected:
	Timer(uint64_t ms, std::function<void()> func, bool recurring, TimerManager* manager);

	Timer(uint64_t next);

private:
	uint64_t m_ms;
	uint64_t m_next;
	bool m_recurring;
	std::function<void()> m_func;
	TimerManager* m_manager;
	MutexType m_mutex;
private:
	struct Comparator {
		bool operator() (const Timer::ptr& l1, const Timer::ptr& l2) const;
	};
};

class TimerManager {
friend class Timer;
public:
	typedef std::shared_ptr<TimerManager> ptr;
	typedef Mutex MutexType;

public:
	TimerManager();

	virtual ~TimerManager();

	Timer::ptr addTimer(uint64_t ms, std::function<void()> func, bool recurring = false);

	Timer::ptr addCondTimer(uint64_t ms, std::function<void()> func, std::weak_ptr<void> cond, bool recurring = false);

	uint64_t getNextTime();

	bool hasTimer() { return !m_timers.empty(); }

	void listPassedTimer(std::vector<std::function<void()> >& funcs);

protected:
	virtual void OnInsertAtFirst() = 0;

private:
	std::set<Timer::ptr, Timer::Comparator> m_timers;

	MutexType m_mutex;
};

}

#endif
