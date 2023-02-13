#include "log.h"

void test_1() {
	sylar::Logger::ptr logger(new sylar::Logger());
	{
	sylar::LogAppender::ptr tmp_ap(new sylar::StdoutLogAppender(), [](sylar::StdoutLogAppender* l){ std::cout << "析构函数调用" << std::endl;});
	logger->addAppender(tmp_ap);
	}
	std::cout << "000" << std::endl;
	// logger->addAppender(sylar::LogAppender::ptr(new sylar::StdoutLogAppender(), [](sylar::StdoutLogAppender* l){ std::cout << "析构函数调用" << std::endl;}));
	logger->addAppender(sylar::LogAppender::ptr(new sylar::FileLogAppender("log_1.log")));

	
	sylar::LogEvent::ptr event(new sylar::LogEvent(sylar::LogLevel::DEBUG, 123));
	event->getSS() << "first log";

	logger->log(event);
}

void test_2() {
	sylar::LoggerManager::ptr logMgr = sylar::LoggerManager::getInstance();
	sylar::Logger::ptr logger = logMgr->getLogger("sys");
	logger->addAppender(sylar::LogAppender::ptr(new sylar::StdoutLogAppender()));
	//sylar::Logger::ptr logger = logMgr->getRoot();

	sylar::LogEvent::ptr event(new sylar::LogEvent(sylar::LogLevel::DEBUG, 123));
	event->getSS() << "first log";

	logger->log(event);
}

void test_3() {
	sylar::LoggerManager::ptr logMgr(new sylar::LoggerManager());
	sylar::Logger::ptr logger = logMgr->getLogger("sys");
	logger->addAppender(sylar::LogAppender::ptr(new sylar::StdoutLogAppender()));
	logger->addAppender(sylar::LogAppender::ptr(new sylar::FileLogAppender("log/log_1.log")));
	SYLAR_LOG_LEVEL(logger, sylar::LogLevel::DEBUG) << "yyyy";
	SYLAR_LOG_INFO(logger) << "info log";
	SYLAR_LOG_FATAL(logger) << "fatal log";
	SYLAR_LOG_FMT_DEBUG(logger, "debug fmt log");
}

void test_4() {
	
		#define XX(c, s) \
			std::cout << "case #@c: \
				m_items.push_back(LogFormatItem::ptr(new s())); \
				break;"
			XX(c, s);
		#undef XX
/*
		#define CC(c) #c
		char* x = CC(a);
		std::cout << std::endl << x << std::endl;
		#undef CC
*/
}

int main(void) {
	test_3();
	std::cout << "proc end" << std::endl;

	return 0;
}
