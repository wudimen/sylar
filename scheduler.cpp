#include "scheduler.h"
#include <iostream>

#include "log.h"
#include "hook.h"
#include "macro.h"
#include "util.h"

namespace sylar {

static Logger::ptr g_logger = SYLAR_LOG_NAME("system");

static thread_local Scheduler* t_scheduler = nullptr;		// 当前调度器
static thread_local Fiber* t_threadFiber = nullptr;			// 主线程的run协程
// 除了caller线程的run协程外，其他所有的协程都是使用back回到t_threadFiber协程(caller线程的run协程管理的协程的t_threadFiber是run协程，其余的是各自线程的主协程)；		run协程回到caller的主协程；

/*
	除了主线程的run协程用call/back外，其余协程都使用swapIn/swapOut;
*/

Scheduler::Scheduler(int threads, bool use_caller, std::string name)
	:m_name(name) {
	SYLAR_ASSERT(threads > 0);
	if(use_caller) {
		// t_threadFiber = Fiber::GetThis().get();
		Fiber::GetThis();

		threads--;

		SYLAR_ASSERT(GetThis() == nullptr);
		t_scheduler = this;
		
		m_rootFiber.reset(new Fiber(std::bind(&Scheduler::run, this), 0, true));
		Thread::SetName(m_name);

		t_threadFiber = m_rootFiber.get();
		m_rootThread = GetThreadId();
		m_threadId.push_back(m_rootThread);

	} else {
		m_rootThread = -1;
	}
	m_threadCount = threads;
}

Scheduler::~Scheduler() {
	SYLAR_ASSERT(m_isstoping == true);
	if(GetThis() == this) {
		t_scheduler = nullptr;
	}
}

Scheduler* Scheduler::GetThis() {
	return t_scheduler;
}

void Scheduler::SetThis() {
	t_scheduler = this;
}

Fiber* Scheduler::GetMainFiber() {
	return t_threadFiber;
}

void Scheduler::start() {
	MutexType::Lock lock(m_mutex);

	if(!m_isstoping) {		// 看是否已经初始化过了
		return;
	}
	m_isstoping = false;

	SYLAR_ASSERT(m_threads.empty());

	for(size_t i = 0; i < m_threadCount; ++i) {
		m_threads.push_back(Thread::ptr(new Thread(std::bind(&Scheduler::run, this), m_name + "_" + std::to_string(i))));
		m_threadId.push_back(m_threads[i]->getId());
	}
}

void Scheduler::stop() {
	m_autostop = true;

	if(m_rootFiber
		&& m_threadCount == 0
		&& (m_rootFiber->getState() == Fiber::TERM
			|| m_rootFiber->getState() == Fiber::INIT))  {
		SYLAR_LOG_INFO(g_logger) << this << " stoped";
		m_isstoping = true;

		if(stoping()) {
			return ;
		}
	}

	if(m_rootThread == -1) {
		SYLAR_ASSERT(GetThis() != this);
	} else {
		SYLAR_ASSERT(GetThis() == this);
	}

	m_isstoping = true;
	for(size_t i = 0; i < m_threadCount; ++i) {
		tickle();
	}

	if(m_rootFiber) {
		tickle();
	}
	
	if(m_rootFiber && !stoping()) {
		m_rootFiber->call();
	}
		

	std::vector<Thread::ptr> tmp_thr;
	{
		MutexType::Lock lock(m_mutex);
		tmp_thr.swap(m_threads);
	}
	for(auto& i : tmp_thr) {
		i->join();
	}
}

bool Scheduler::stoping() {
	MutexType::Lock lock(m_mutex);
// 	std::cout <<"--- m_isstoping: " << (m_isstoping ? "true" : "false" ) 
// 				<< "  m_autostop: " << (m_autostop ? "true" : "false" )
// 				<< "  m_fibers.empty(): " << (m_fibers.empty() ? "true" : "false" )
// 				<< "  m_activeThreadCount == 0: " << ((m_activeThreadCount == 0) ? "true" : "false" ) << "size: " << m_activeThreadCount
// 				<< std::endl;
	return m_isstoping 
			&& m_autostop 
			&& m_fibers.empty() 
			&& m_activeThreadCount == 0;
}

/*
// template<typename FiberAndCall>
void Scheduler::schedule(FiberAndCall fc, int thread) {
	bool needTicket = false;
	{
		MutexType::Lock lock(m_mutex);
		needTicket = scheduleNoLock(fc, thread);
	}
	if(needTicket) {
		tickle();
	}
}

// template<typename FiberAndCall>
bool Scheduler::scheduleNoLock(FiberAndCall fc, int thread) {
	bool needTicket = false;
	if(m_fibers.empty()) {
		needTicket = true;
	}
	Scheduler::FiberAndThread tmp_fb(fc, thread);
	if(tmp_fb.func|| tmp_fb.fiber) {
		m_fibers.push_back(tmp_fb);
	}

	return needTicket;
}
*/

void Scheduler::tickle() {
	SYLAR_LOG_DEBUG(g_logger) << "tickle";
}

void Scheduler::run() {
	set_hook_enable(true);
	SetThis();
	if(m_rootThread != GetThreadId()) {
		t_threadFiber = Fiber::GetThis().get();
	}
	
	FiberAndThread tmp_fb;			// 装要执行的任务
	Fiber::ptr tmp_fiber = nullptr;		// 装要执行的任务转换成的fiber
	Fiber::ptr idleFiber = Fiber::ptr(new Fiber(std::bind(&Scheduler::idle, this)));		// 要执行的任务中没有任务则执行idle函数，要执行的idle协程

	while(true) {
		// 取出可以在这个线程执行的任务
		tmp_fb.reset();
		bool needTicket = false;
		bool countAdded = false;
		{
			MutexType::Lock lock(m_mutex);
			for(auto i = m_fibers.begin(); i != m_fibers.end(); ++i) {
				if(i->thread_id != -1 && i->thread_id != GetThreadId()) {
					needTicket = true;
					continue;
				}
				SYLAR_ASSERT(i->fiber || i->func);
				if(i->fiber && i->fiber->getState() == Fiber::EXEC) {
					continue;
				}
				tmp_fb = *i;
				m_fibers.erase(i);
				++m_activeThreadCount;
				countAdded = true;
				// needTicket |= i != m_fibers.end();
				break;
			}
		}
		if(needTicket) {
			tickle();
		}
		// 根据任务类型执行该任务
		if(tmp_fb.func) {
			if(!tmp_fiber) {
				tmp_fiber.reset(new Fiber(tmp_fb.func));
			} else {
				tmp_fiber->reset(tmp_fb.func);
			}
			tmp_fb.reset();
	
			tmp_fiber->swapIn();
			--m_activeThreadCount;
	
			if(tmp_fiber->getState() == Fiber::READY) {
				schedule(tmp_fiber);
				tmp_fiber.reset();
			} else if(tmp_fiber->getState() == Fiber::TERM
							|| tmp_fiber->getState() == Fiber::EXCEPT) {
				tmp_fiber->reset(nullptr);
			} else {
				tmp_fiber->m_state = Fiber::HOLD;
				tmp_fiber.reset();
			}
		} else if(tmp_fb.fiber && (tmp_fb.fiber->getState() != Fiber::TERM
								&& tmp_fb.fiber->getState() != Fiber::EXCEPT)) {
			tmp_fb.fiber->swapIn();
			--m_activeThreadCount;
	
			if(tmp_fb.fiber->getState() == Fiber::READY) {
				schedule(tmp_fb.fiber);
			} else if(tmp_fb.fiber->getState() != Fiber::TERM
							&& tmp_fb.fiber->getState() != Fiber::EXCEPT) {
				tmp_fb.fiber->m_state = Fiber::HOLD;
			}
			tmp_fb.reset();
		} else {
			if(countAdded) {
				--m_activeThreadCount;
				continue;
			}

			if(idleFiber->getState() == Fiber::TERM) {
				SYLAR_LOG_DEBUG(g_logger) << "idleFiber termed";
				break;
			}

			++m_idleThreadCount;
			idleFiber->swapIn();
			--m_idleThreadCount;

			if(idleFiber->getState() != Fiber::TERM
							&& idleFiber->getState() != Fiber::EXCEPT) {
				idleFiber->m_state = Fiber::HOLD;
			}
		}

	}
}

void Scheduler::idle() {
	SYLAR_LOG_DEBUG(g_logger) << "idle";
	while(!stoping()) {
	 	Fiber::YieldToHold_2();
	}
}



}
