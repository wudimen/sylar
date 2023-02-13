#include "timer.h"

#include "util.h"

namespace sylar {

bool Timer::resetTimer(uint64_t ms, bool fromNow) {
	if(m_ms == ms && !fromNow) {
		return false;
	}
	TimerManager::MutexType::Lock lock(m_manager->m_mutex);
	if(!m_func) {
		return false;
	}

	auto it = m_manager->m_timers.find(shared_from_this());
	if(it == m_manager->m_timers.end()) {
		return false;
	}
	m_manager->m_timers.erase(it);
	uint64_t start;
	if(fromNow) {
		start = sylar::GetCurrentMs();
	} else {
		start = m_next - m_ms;
	}
	m_ms = ms;
	m_next = start + m_ms;
	auto it2 = m_manager->m_timers.insert(shared_from_this()).first;
	if(it2 == m_manager->m_timers.begin()) {
		lock.unlock();
		m_manager->OnInsertAtFirst();
	}

	return true;
}

bool Timer::reflushTimer() {
	TimerManager::MutexType::Lock lock(m_manager->m_mutex);
	if(m_func){
		return false;
	}
	auto it = m_manager->m_timers.find(shared_from_this());
	if(it == m_manager->m_timers.end()) {
		return false;
	}
	m_manager->m_timers.erase(it);

	uint64_t now = sylar::GetCurrentMs();
	m_next = now + m_ms;
	m_manager->m_timers.insert(shared_from_this()); 	// 时间只可能增加，因此不需调用OnInsertAtFirst()
	return true;
}

bool Timer::cancel() {
	TimerManager::MutexType::Lock lock(m_manager->m_mutex);
	if(m_func) {
		m_func = nullptr;
		auto it = m_manager->m_timers.find(shared_from_this());
		m_manager->m_timers.erase(it);
		return true;
	}
	return false;
}

Timer::Timer(uint64_t ms, std::function<void()> func, bool recurring, TimerManager* manager)
	:m_ms(ms), m_func(func), m_recurring(recurring), m_manager(manager) {
	m_next = sylar::GetCurrentMs() + m_ms;
}

Timer::Timer(uint64_t next) {
	m_next = next;
}

bool Timer::Comparator::operator()(const Timer::ptr& l1, const Timer::ptr& l2) const {
	if(!l1 && !l2) {
		return false;
	}
	if(!l1) {
		return true;
	}
	if(!l2) {
		return false;
	}
	if(l1->m_next < l2->m_next) {
		return true;
	}
	if(l1->m_next > l2->m_next) {
		return false;
	}
	return l1.get() < l2.get();
}

TimerManager::TimerManager() {
}

TimerManager::~TimerManager() {
}


Timer::ptr TimerManager::addTimer(uint64_t ms, std::function<void()> func, bool recurring) {
	Timer::ptr timer(new Timer(ms, func, recurring, this));
	MutexType::Lock lock(m_mutex);
	auto it = m_timers.insert(timer).first;
	if(it == m_timers.begin()) {
		lock.unlock();
		OnInsertAtFirst();
	}
	return timer;
}

static void OnTimer(std::weak_ptr<void> cond, std::function<void()> func) {
	std::shared_ptr<void> tmp = cond.lock();
	if(tmp) {
		func();
	}
}

Timer::ptr TimerManager::addCondTimer(uint64_t ms, std::function<void()> func, std::weak_ptr<void> cond, bool recurring) {
	return addTimer(ms, std::bind(&OnTimer, cond, func), recurring);
}

uint64_t TimerManager::getNextTime() {
	MutexType::Lock lock(m_mutex);
	if(m_timers.empty()) {
		return ~0ull;
	}
	auto it = *m_timers.begin();
	uint64_t now = sylar::GetCurrentMs();
	if(now > it->m_next) {
		return 0;
	}
	return it->m_next - now;
}

void TimerManager::listPassedTimer(std::vector<std::function<void()> >& funcs) {
	uint64_t now = sylar::GetCurrentMs();
	std::vector<Timer::ptr> expired;

	Timer::ptr t(new Timer(now));

	auto it = m_timers.lower_bound(t);
	while(it != m_timers.end() && (*it)->m_next == now) ++it;

	expired.insert(expired.begin(), m_timers.begin(), it);
	m_timers.erase(m_timers.begin(), it);
	funcs.reserve(expired.size());
	for(auto& timer : expired) {
		funcs.push_back(timer->m_func);
		if(timer->m_recurring) {
			timer->m_next = now + timer->m_ms;
			m_timers.insert(timer);
		} else {
			timer->m_func = nullptr;
		}
	}
}


}
