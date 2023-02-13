#ifndef _UTIL_H_
#define _UTIL_H_

#include <vector>
#include <string>
#include <sstream>
#include <iostream>
#include <execinfo.h>
#include <sys/time.h>



namespace sylar {

uint32_t GetThreadId();

uint32_t GetFiberId();

void Backtrace(std::vector<std::string>& bt, int size = 64, int skip = 1);

std::string BacktraceToString(int size = 65, int skip = 3, const std::string prefix = "");

uint64_t GetCurrentMs();

uint64_t GetCurrentUs();

}
#endif
