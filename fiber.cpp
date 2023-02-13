#include "fiber.h"
#include <iostream>
#include <atomic>

#include "log.h"
#include "config.h"
#include "macro.h"
#include "scheduler.h"

namespace sylar {

static Logger::ptr s_logger = SYLAR_LOG_NAME("system");

static std::atomic<uint64_t> s_fiber_id(0);
static std::atomic<uint64_t> s_fiber_count(0);

static thread_local Fiber* t_fiber = nullptr;
static thread_local std::shared_ptr<Fiber> t_threadfiber = nullptr;

static ConfigVar<uint32_t>::ptr g_fiber_stack_size = Config::lookup<uint32_t>("fiber.stack_size", 1024 * 128, "fiber stack size");

class MallocStackAllocator {
public:
	static void* Alloc(size_t size) {
		return malloc(size);
	}

	static void Dealloc(void* stack) {
		return free(stack);
	}
};

using StackAllocator = MallocStackAllocator;

Fiber::Fiber() {
	m_state = EXEC;
	SetThis(this);
	if(getcontext(&m_ctx)) {
		SYLAR_ASSERT2(false, "getcontext");
	}
	
	++ s_fiber_count;

	SYLAR_LOG_INFO(s_logger) << "Fiber::Fiber main begin...";
}

Fiber::Fiber(std::function<void()> cb, uint32_t stacksize, bool use_caller)
	:m_id(++s_fiber_id),
	 m_cb(cb) {
	++s_fiber_count;
	m_stacksize = (stacksize != 0 ? stacksize : g_fiber_stack_size->getValue());

	
	// SYLAR_LOG_DEBUG(SYLAR_LOG_ROOT()) << "test_1";
	if(getcontext(&m_ctx)) {
		SYLAR_ASSERT2(false, "getcontext");
	}

	m_stack = StackAllocator::Alloc(m_stacksize);

	m_ctx.uc_link = nullptr;
	m_ctx.uc_stack.ss_sp = m_stack;
	m_ctx.uc_stack.ss_size = m_stacksize;

	if(use_caller) {
		makecontext(&m_ctx, &Fiber::CallerMainFunc, 0);
	} else {
		makecontext(&m_ctx, &Fiber::MainFunc, 0);
	}
	
	SYLAR_LOG_INFO(s_logger) << "Fiber::Fiber id = " << m_id;
}

Fiber::~Fiber() {
	--s_fiber_count;
	if(m_stack) {
		SYLAR_ASSERT(m_state == TERM
				   || m_state == INIT
				   || m_state == EXCEPT);
		StackAllocator::Dealloc(m_stack);
	} else {
		SYLAR_ASSERT(m_state == EXEC);
		SYLAR_ASSERT(!m_cb);

		Fiber* f = t_fiber;
		if(f == this) {
			SetThis(nullptr);
		}
	}

	SYLAR_LOG_INFO(s_logger) << "Fiber::~Fiber id = " << m_id << "\ttotal fibers: " << s_fiber_count;
}

void Fiber::swapIn() {
	SetThis(this);
	SYLAR_ASSERT(m_state != EXEC);
	m_state = EXEC;

	if(swapcontext(&Scheduler::GetMainFiber()->m_ctx, &m_ctx)) {
		SYLAR_ASSERT2(false, "swapcontext");
	}
}

void Fiber::swapOut() {
	SetThis(Scheduler::GetMainFiber());
	
	if(swapcontext(&m_ctx, &Scheduler::GetMainFiber()->m_ctx)) {
		SYLAR_ASSERT2(false, "swpacontext");
	}
}

void Fiber::call() {
	SetThis(this);
	m_state = EXEC;
	if(swapcontext(&t_threadfiber->m_ctx, &m_ctx)) {
		SYLAR_ASSERT2(false, "swapcontext");
	}
}

void Fiber::back() {
	SetThis(t_threadfiber.get());

	if(swapcontext(&m_ctx, &t_threadfiber->m_ctx)) {
		SYLAR_ASSERT2(false, "swapcontext");
	}
}

void Fiber::SetThis(Fiber* f) { t_fiber = f; }

Fiber::ptr Fiber::GetThis() { 
	if(t_fiber) {
		return t_fiber->shared_from_this(); 
	}
	Fiber::ptr f(new Fiber());
	// SYLAR_ASSERT(t_fiber == f.get());
	t_threadfiber = f;
	return t_fiber->shared_from_this();
}

uint64_t Fiber::GetTotalfibers() { return s_fiber_count;}

uint64_t Fiber::GetFiberid() {
	if(t_fiber) {
		return t_fiber->getId();
	}
	return 0;
}

void Fiber::YeildToReady_2() {
	Fiber::ptr cur = GetThis();
	SYLAR_ASSERT(cur->m_state == EXEC);
	cur->m_state = READY;
	cur->back();
}


void Fiber::YeildToReady() {
	Fiber::ptr cur = GetThis();
	SYLAR_ASSERT(cur->m_state == EXEC);
	cur->m_state = READY;
	cur->swapOut();
}

void Fiber::YieldToHold_2() {
	Fiber::ptr cur = GetThis();
	SYLAR_ASSERT(cur->m_state == EXEC);

	// m_state = HOLD;
	cur->back();
}


void Fiber::YieldToHold() {
	Fiber::ptr cur = GetThis();
	SYLAR_ASSERT(cur->m_state == EXEC);

	// m_state = HOLD;
	cur->swapOut();
}

void Fiber::MainFunc() {
	Fiber::ptr cur = GetThis();
	SYLAR_ASSERT(cur);
	try{
		cur->m_cb();
		cur->m_cb = nullptr;
		cur->m_state = TERM;
	} catch(std::exception& ex) {
		SYLAR_LOG_ERROR(s_logger) << "Fiber MainFunc Exception: " << ex.what() << "fiber id = " << cur->getId() << std::endl << BacktraceToString();
		cur->m_state = EXCEPT;
	} catch(...) {
		SYLAR_LOG_ERROR(s_logger) << "Fiber MainFunc Exception " << "fiber id = " << cur->getId() << std::endl << BacktraceToString();
		cur->m_state = EXCEPT;
	}
	Fiber* tmp_f = cur.get();
	cur.reset();
	tmp_f->swapOut();

	SYLAR_LOG_INFO(s_logger) << "never run to here!";
}

void Fiber::CallerMainFunc() {
	Fiber::ptr cur = GetThis();
	SYLAR_ASSERT(cur);

	try {
		cur->m_cb();
		cur->m_cb = nullptr;
		cur->m_state = TERM;
	} catch(std::exception& ex) {
		SYLAR_LOG_ERROR(s_logger) << "Fiber MainFunc Exception: " << ex.what() << "fiber id = " << cur->getId() << std::endl << BacktraceToString();
		cur->m_state = EXCEPT;
	} catch(...) {
		SYLAR_LOG_ERROR(s_logger) << "Fiber MainFunc Exception " << "fiber id = " << cur->getId() << std::endl << BacktraceToString();
		cur->m_state = EXCEPT;
	}
	Fiber* tmp_f = cur.get();
	cur.reset();
	tmp_f->back();

	SYLAR_LOG_INFO(s_logger) << "nover run to here!";
}

}
