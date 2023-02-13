#include "util.h"
#include <sys/syscall.h>

#include "log.h"
#include "thread.h"
#include "fiber.h"


sylar::Logger::ptr g_logger = sylar::LoggerManager::getInstance()->getLogger("system");

namespace sylar {

uint32_t GetThreadId() { return syscall(SYS_gettid); }

uint32_t GetFiberId() { return Fiber::GetFiberid(); }

void Backtrace(std::vector<std::string>& bt, int size, int skip) {
	void** path = (void**)malloc(sizeof(char*) * 64);
	int s = backtrace(path, size);
	char** str = backtrace_symbols(path, s);
	if(str == NULL) {
		SYLAR_LOG_ERROR(g_logger) << "backtrace_symbols error";
		free(path);
		return;
	}
	for(size_t i = skip; i < s; ++i) {
		bt.push_back(str[i]);
	}
	free(path);
	free(str);
}

std::string BacktraceToString(int size, int skip, const std::string prefix) {
	std::vector<std::string> vec;
	std::stringstream ss;
	Backtrace(vec, size, skip);
	for(auto i : vec) {
		ss << prefix << i << std::endl;
	}
	return ss.str();
}

uint64_t GetCurrentMs() {
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return tv.tv_sec * 1000ul + tv.tv_usec / 1000;
}

uint64_t GetCurrentUs() {
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return tv.tv_sec * 1000ul * 1000ul + tv.tv_usec;
}

}
