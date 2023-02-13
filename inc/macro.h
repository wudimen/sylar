/*
	保存常用的宏
*/
#ifndef _MACRO_H_
#define _MACRO_H_

#include <assert.h>
#include <string.h>
#include "log.h"
#include "util.h"

// sylar::Logger::ptr g_logger = sylar::LoggerManager::getInstance()->getLogger("system");

#define SYLAR_ASSERT(val) \
	if(!(val)) { \
		SYLAR_LOG_ERROR(SYLAR_LOG_ROOT()) << "ASSERTION: " << #val << "\ntrace:\n" << sylar::BacktraceToString(100, 2, "    "); \
		assert(val); \
	}

#define SYLAR_ASSERT2(val, str) \
	if(!(val)) { \
		SYLAR_LOG_ERROR(SYLAR_LOG_ROOT()) << "ASSERTION: " << #val << "\n" << #str << "\ntrace:\n" << sylar::BacktraceToString(100, 2, "    "); \
		assert(val); \
	}


#endif
