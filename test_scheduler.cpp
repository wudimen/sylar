#include "sylar.h"

sylar::Logger::ptr t_logger = SYLAR_LOG_NAME("system");

void fiber_test() {
	SYLAR_LOG_INFO(t_logger) << "====fiber_test";
}

int main(void) {
//std::cout << "hhh" << std::endl;
	sylar::Fiber::ptr test_fiber(new sylar::Fiber(fiber_test));
	sylar::Fiber::ptr test_fiber_1(new sylar::Fiber(fiber_test));
	sylar::Scheduler s(3, true, "scheduler");
	SYLAR_LOG_INFO(t_logger) << "main start";
	s.start();
	SYLAR_LOG_INFO(t_logger) << "main schedule";
	s.schedule(fiber_test);
	// s.schedule(&test_fiber);
	SYLAR_LOG_INFO(t_logger) << "main middle";
	s.schedule(fiber_test);
	// s.schedule(&test_fiber_1);
	s.stop();
	SYLAR_LOG_INFO(t_logger) << "main end";


	return 0;
}
