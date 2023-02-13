#ifndef _FIBER_H_
#define _FIBER_H_

#include <ucontext.h>
#include <functional>
#include <memory>

#include "macro.h"

namespace sylar {

class Scheduler;

class Fiber : public std::enable_shared_from_this<Fiber> {
public:
	friend class Scheduler;
	typedef std::shared_ptr<Fiber> ptr;

	enum State {
		INIT,		// 初始化
		HOLD,		// 暂停
		EXEC,		// 运行
		TERM,		// 结束
		READY,		// 已就绪
		EXCEPT		// 异常
	};

private:
	Fiber();
public:
	Fiber(std::function<void()> cb, uint32_t stacksize = 0, bool use_caller = false);

	~Fiber();

	void reset(std::function<void()> cb){
		SYLAR_ASSERT(m_stack);
		SYLAR_ASSERT(m_state == TERM
					|| m_state == EXCEPT
					|| m_state == INIT);
		m_cb = cb;
		if(getcontext(&m_ctx)) {
			SYLAR_ASSERT2(false, "getcontext");
		}

		m_ctx.uc_link = nullptr;
		m_ctx.uc_stack.ss_sp = m_stack;
		m_ctx.uc_stack.ss_size = m_stacksize;

		makecontext(&m_ctx, &Fiber::MainFunc, 0);
		m_state = INIT;
	}
	
	uint64_t getId() const { return m_id; }

	void swapIn();

	void swapOut();

	void call();

	void back();

	State getState() { return m_state; }

public:
	static void SetThis(Fiber* f);

	static Fiber::ptr GetThis();

	static uint64_t GetTotalfibers();

	static uint64_t GetFiberid();

	static void YeildToReady(); 

	static void YieldToHold();

	static void YeildToReady_2(); 

	static void YieldToHold_2();

	static void MainFunc(); 

	static void CallerMainFunc();

	
private:
	std::function<void()> m_cb;
	uint64_t m_id = 0;
	uint32_t m_stacksize = 0;
	void* m_stack = nullptr;
	ucontext_t m_ctx;
	State m_state = INIT;
};

}


#endif
