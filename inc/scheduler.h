#ifndef _SCHEDULER_H_
#define _SCHEDULER_H_

#include <iostream>
#include <vector>
#include <list>
#include <functional>
#include <memory.h>
#include <atomic>

#include "mutex.h"
#include "fiber.h"
#include "thread.h"

namespace sylar {

class Scheduler {
public:
	typedef std::shared_ptr<Scheduler> ptr;
	typedef Mutex MutexType;

	Scheduler(int threads = 1, bool use_caller = false, std::string name = "");

	~Scheduler();
private:
	struct FiberAndThread {
		std::function<void()> func;		// 协程任务（二选一）
		Fiber::ptr fiber;
		int thread_id;				// 指定线程完成该任务

		FiberAndThread(Fiber::ptr f, int thread = -1)
			:fiber(f), thread_id(thread) {
			
		}
		FiberAndThread(Fiber::ptr* f, int thread = -1)
			:thread_id(thread) {
			fiber.swap(*f);
		}
		FiberAndThread(std::function<void()> f, int thread = -1) 
			:func(f), thread_id(thread) {
			
		}
		FiberAndThread(std::function<void()>* f, int thread = -1)
			:thread_id(thread) {
			func.swap(*f);
		}
		FiberAndThread() 
			:thread_id(-1) {

		}
		void reset() {
			func = nullptr;
			fiber = nullptr;
			thread_id = -1;
		}
	};
public:
	static Scheduler* GetThis();

	void SetThis();

	static Fiber* GetMainFiber();

public:
	void start();

	void stop();

	template<typename FiberAndCall>
	void schedule(FiberAndCall fc, int thread = -1) {
	bool needTicket = false;
	{
		MutexType::Lock lock(m_mutex);
		needTicket = scheduleNoLock(fc, thread);
	}
	if(needTicket) {
		tickle();
	}
}

	template<typename FiberAndCall>
	bool scheduleNoLock(FiberAndCall fc, int thread) {
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

	virtual bool stoping();

	virtual void tickle();

	void run();

	virtual void idle();

private:
	MutexType m_mutex;
	uint32_t m_threadCount = -1;		// 要使用的线程池中的线程数量
	std::vector<Thread::ptr> m_threads;		// 线程池容器
	std::list<FiberAndThread> m_fibers;		// 待执行的协程集合
	Fiber::ptr m_rootFiber = nullptr;	// caller线程的run协程(caller=true时有效)
	std::string m_name;					// 协程调度器名称
protected:
	std::vector<int> m_threadId;		// 线程池中的线程id集合
	std::atomic<uint32_t> m_activeThreadCount = {0};		// 正在工作的线程数量
	std::atomic<uint32_t> m_idleThreadCount = {0};			// 正在没事做（执行idle）的协程数量
	bool m_isstoping = true;				// 调度器是否已经停止
	bool m_autostop = false;				// 是否停止调度器
	int m_rootThread = 0;				// 主线程(caller线程)id
};

}

#endif
