#include <time.h>

#include "log.h"

namespace sylar{

std::string LogLevel::toString(LogLevel::Level level) {
	switch(level) {
		#define XX(level) \
		case LogLevel::level: \
			return #level; \
			break;

		XX(DEBUG);
		XX(INFO);
		XX(WARM);
		XX(ERROR);
		XX(FATAL);

		#undef XX
		default:
			return "UNKNOW";
	}
	return "UNKNOW";
}

LogLevel::Level LogLevel::fromString(const std::string& str) {
	#define XX(str_1, LEVEL) \
	if(str == #str_1) { \
		return LogLevel::LEVEL; \
	}

	XX(DEBUG, DEBUG);
	XX(INFO, INFO);
	XX(WARM, WARM);
	XX(ERROR, ERROR);
	XX(FATAL, FATAL);

	XX(debug, DEBUG);
	XX(info, INFO);
	XX(warm, WARM);
	XX(error, ERROR);
	XX(fatal, FATAL);
	return  LogLevel::UNKNOW;

	#undef XX
}

class StringLogFormatItem : public LogFormatter::LogFormatItem {
public:
	StringLogFormatItem(const std::string str = "") : m_str(str) {
	}
	void format(std::ostream& ss, std::shared_ptr<Logger> logger, LogEvent::ptr event) override {
		ss << m_str;
	}
private:
	std::string m_str;
};

class FilenameLogFormatItem : public LogFormatter::LogFormatItem {
public:
	FilenameLogFormatItem() {};
	void format(std::ostream& ss, std::shared_ptr<Logger> logger, LogEvent::ptr event) override {
		ss << event->getFilename();
	}
};

class LineLogFormatItem : public LogFormatter::LogFormatItem {
public:
	LineLogFormatItem() {};
	void format(std::ostream& ss, std::shared_ptr<Logger> logger, LogEvent::ptr event) override {
		ss << event->getLine();
	}
};

class ElapseLogFormatItem : public LogFormatter::LogFormatItem {
public:
	ElapseLogFormatItem() {};
	void format(std::ostream& ss, std::shared_ptr<Logger> logger, LogEvent::ptr event) override {
		ss << event->getElapse();
	}
};

class FiberidLogFormatItem : public LogFormatter::LogFormatItem {
public:
	FiberidLogFormatItem() {};
	void format(std::ostream& ss, std::shared_ptr<Logger> logger, LogEvent::ptr event) override {
		ss << event->getFiberid();
	}
};

class TimeLogFormatItem : public LogFormatter::LogFormatItem {
public:
	TimeLogFormatItem(std::string format = "%Y-%m-%d %H:%M:%S")
		:m_format(format) {
		if(m_format.empty()) {
			m_format = "%Y-%m-%d %H:%M:%S";
		}
	}
	void format(std::ostream& ss, std::shared_ptr<Logger> logger, LogEvent::ptr event) override {
		struct tm tm;
		time_t time = event->getTime();
		localtime_r(&time, &tm);
		char buf[256] = "";
		strftime(buf, 256, m_format.c_str(), &tm);
		ss << buf;
	}

private:
	std::string m_format;
};

class ThreadnameLogFormatItem : public LogFormatter::LogFormatItem {
public:
	ThreadnameLogFormatItem() {};
	void format(std::ostream& ss, std::shared_ptr<Logger> logger, LogEvent::ptr event) override {
		ss << event->getThreadname();
	}
};

class LevelLogFormatItem : public LogFormatter::LogFormatItem {
public:
	LevelLogFormatItem() {};
	void format(std::ostream& ss, std::shared_ptr<Logger> logger, LogEvent::ptr event) override {
		ss << LogLevel::toString(event->getLevel());
	}
};

class TabLogFormatItem : public LogFormatter::LogFormatItem {
public:
	TabLogFormatItem() {};
	void format(std::ostream& ss, std::shared_ptr<Logger> logger, LogEvent::ptr event) override {
		ss << "\t";
	}
};

class ThreadidLogFormatItem : public LogFormatter::LogFormatItem {
public:
	ThreadidLogFormatItem() {};
	void format(std::ostream& ss, std::shared_ptr<Logger> logger, LogEvent::ptr event) override {
		ss << event->getThreadId();
	}
};

class NewlineLogFormatItem : public LogFormatter::LogFormatItem {
public:
	NewlineLogFormatItem() {};
	void format(std::ostream& ss, std::shared_ptr<Logger> logger, LogEvent::ptr event) override {
		ss << "\n";
	}
};

class SsLogFormatItem : public LogFormatter::LogFormatItem {
public:
	SsLogFormatItem() {};
	void format(std::ostream& ss, std::shared_ptr<Logger> logger, LogEvent::ptr event) override {
		ss << event->getSS().str();
	}
};

void LogFormatter::analyseFmt() {		//可以更牛逼！！！！！！！！
	for(size_t i = 0; i < m_fmt.size(); ++i) {
#define XX(c, s) \
		if(#c == m_fmt.substr(i, 1)) { \
			m_items.push_back(LogFormatItem::ptr(new s())); \
			continue; \
		}

		XX(l, LevelLogFormatItem);			// 日志等级
		XX(t, ThreadidLogFormatItem);		// 线程ID
		XX(T, TabLogFormatItem);			// tab
		XX(N, NewlineLogFormatItem);		// '\n'
		XX(s, SsLogFormatItem);				// 日志内容
		XX(F, FilenameLogFormatItem);		// 文件名
		XX(L, LineLogFormatItem);			// 行号
		XX(e, ElapseLogFormatItem);			// 运行时间
		XX(f, FiberidLogFormatItem);		// 协程ID
		XX(a, TimeLogFormatItem);			// 当前时间戳
		XX(n, ThreadnameLogFormatItem);		// 线程名称

#undef XX
		m_items.push_back(LogFormatItem::ptr(new StringLogFormatItem(m_fmt.substr(i, 1))));

	}
}

LogEventWrap::~LogEventWrap() {
	m_logger->log(m_event);
}

ConfigVar<std::set<LoggerDefine> >::ptr conf_log_set_var = Config::lookup("log_system", std::set<LoggerDefine>(), "static logs");

class LogIniter {
public:
	LogIniter() {
		conf_log_set_var->addCB([](const std::set<LoggerDefine>& old_val, const std::set<LoggerDefine>& new_val) {
			std::shared_ptr<LoggerManager> manager = LoggerManager::getInstance();
			for(auto& i : new_val) {
				Logger::ptr logger;
				auto it = old_val.find(i);
				if(it == old_val.end()) {
					// 新增logger
					logger = manager->getLogger(i.m_name);
					// logger.reset(new Logger(i.m_name));
				} else {
					// 修改logger
					logger = manager->getLogger(i.m_name);
				}
				
				logger->clearAppender();
				logger->setLevel(i.m_level);
				for(auto u : i.m_appenders) {
					LogAppender::ptr tmp_appender;
					if(u.m_type == 0) {
						tmp_appender.reset(new StdoutLogAppender());
					} else if(u.m_type == 1) {
						tmp_appender.reset(new FileLogAppender(u.m_file));
					}
					if(!u.m_format.empty()) {
						tmp_appender->setFormat(u.m_format);
					}
					tmp_appender->setLevel(u.m_level);
					logger->addAppender(tmp_appender);
				}
				if(!i.m_formatter.empty()) {
				//	logger->setFormat(i.m_formatter);
				}
				
			}
			for(auto& i : old_val) {
				auto it = new_val.find(i);
				if(it == new_val.end()) {
					// 删除logger（将level设置为0）
					Logger::ptr logger = manager->getLogger(i.m_name);
					logger->setLevel((LogLevel::Level)0);
					logger->clearAppender();
				}
			}
			});
	}
};

static LogIniter __log_init;

}
