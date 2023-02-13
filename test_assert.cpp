#include "sylar.h"

sylar::Logger::ptr t_logger = SYLAR_LOG_NAME("system");

void func_1_fiber() {
	SYLAR_LOG_INFO(t_logger) << "run in fiber begin";
	sylar::Fiber::YieldToHold_2();
	SYLAR_LOG_INFO(t_logger) << "run in fiber end";
	sylar::Fiber::YieldToHold_2();
}

void func_1() {
	SYLAR_LOG_INFO(t_logger) << "main begin 1";
	{
		sylar::Fiber::GetThis();
		SYLAR_LOG_INFO(t_logger) << "main begin";
		sylar::Fiber::ptr fiber(new sylar::Fiber(func_1_fiber, 0, true));
		fiber->call();
		SYLAR_LOG_INFO(t_logger) << "main after swapIn";
		fiber->call();
		SYLAR_LOG_INFO(t_logger) << "main after end";
		fiber->call();
	}
	SYLAR_LOG_INFO(t_logger) << "main after end 2";
}

int main(void) {
	sylar::Thread::SetName("FiberTest");
	std::vector<sylar::Thread::ptr> vec;
	for(size_t i = 0; i < 1; ++i) {
		vec.push_back(sylar::Thread::ptr(new sylar::Thread(func_1, "test_" + std::to_string(i))));
	}
	for(auto i : vec) {
		i->join();
	}

	return 0;
}
