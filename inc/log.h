/*
	m_fileStream.open()：有错误，要以追加方式打开


	注意：
		shared_ptr：不要多个智能指针指向一块空间（Logger::setFormat调用LogAppender::setFormat时指向了同一块LogFormatter ———— L462）
*/
#ifndef _LOG_H_
#define _LOG_H_

#include <iostream>
#include <stdint.h>
#include <stdarg.h>
#include <sstream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <memory>

#include "thread.h"
#include "config.h"
#include "mutex.h"
#include "util.h"

#define __FILENAME__ (strrchr(__FILE__, '/') ? (strrchr(__FILE__, '/') + 1):__FILE__)		// 将日志文件名绝对路径改为相对路径（要使用两次strrcgr,效率低下）

#define SYLAR_LOG_NAME(name) \
	sylar::LoggerManager::getInstance()->getLogger(name)

#define SYLAR_LOG_ROOT() \
	sylar::LoggerManager::getInstance()->getRoot()

#define SYLAR_LOG_LEVEL(logger, LEVEL) \
	sylar::LogEventWrap(logger, sylar::LogEvent::ptr(new sylar::LogEvent(LEVEL, __FILENAME__, __LINE__, 0, sylar::GetThreadId(), sylar::GetFiberId(), time(0), sylar::Thread::GetName()))).getSS() 

#define SYLAR_LOG_DEBUG(logger) \
	SYLAR_LOG_LEVEL(logger, sylar::LogLevel::DEBUG)

#define SYLAR_LOG_INFO(logger) \
	SYLAR_LOG_LEVEL(logger, sylar::LogLevel::INFO)

#define SYLAR_LOG_WARM(logger) \
	SYLAR_LOG_LEVEL(logger, sylar::LogLevel::WARM)

#define SYLAR_LOG_ERROR(logger) \
	SYLAR_LOG_LEVEL(logger, sylar::LogLevel::ERROR)

#define SYLAR_LOG_FATAL(logger) \
	SYLAR_LOG_LEVEL(logger, sylar::LogLevel::FATAL)

#define SYLAR_LOG_FMT_LEVEL(logger, LEVEL, ...) \
	{ sylar::LogEventWrap(logger, sylar::LogEvent::ptr(new sylar::LogEvent(LEVEL, __FILENAME__, __LINE__, 0, sylar::GetThreadId(), sylar::GetFiberId(), time(0), sylar::Thread::GetName()))).write("%s", __VA_ARGS__); }

#define SYLAR_LOG_FMT_DEBUG(logger, ...) \
	SYLAR_LOG_FMT_LEVEL(logger, sylar::LogLevel::DEBUG, __VA_ARGS__)

#define SYLAR_LOG_FMT_INFO(logger, ...) \
	SYLAR_LOG_FMT_LEVEL(logger, sylar::LogLevel::INFO, __VA_ARGS__)

#define SYLAR_LOG_FMT_WARM(logger, ...) \
	SYLAR_LOG_FMT_LEVEL(logger, sylar::LogLevel::WARM, __VA_ARGS__)

#define SYLAR_LOG_FMT_ERROR(logger, ...) \
	SYLAR_LOG_FMT_LEVEL(logger, sylar::LogLevel::ERROR, __VA_ARGS__)

#define SYLAR_LOG_FMT_FATAL(logger, ...) \
	SYLAR_LOG_FMT_LEVEL(logger, sylar::LogLevel::FATAL, __VA_ARGS__)

namespace sylar {

class Logger;
class LoggerManager;
class LevelLogFormatItem;
class StringLogFormatItem;
template<typename F, typename T>
class LexicalCast;

class LogLevel{
public:
	enum Level{
		UNKNOW = 0,
		DEBUG = 1,
		INFO = 2,
		WARM = 3,
		ERROR = 4,
		FATAL = 5
	};

	static std::string toString(LogLevel::Level level);

	static LogLevel::Level fromString(const std::string& str);
};

class LogEvent{
public:
	typedef std::shared_ptr<LogEvent> ptr;
	LogEvent(LogLevel::Level level,const char* fileName, int32_t line, uint32_t elapse, uint32_t threadId, uint32_t fiberId, uint32_t time, std::string threadName)
		: m_level(level),
		  m_fileName(fileName),
		  m_line(line),
		  m_elapse(elapse),
		  m_threadId(threadId),
		  m_fiberId(fiberId),
		  m_time(time),
		  m_threadName(threadName) {
	}

	LogLevel::Level getLevel() {
		return m_level;
	}

	const char* getFilename() {
		return m_fileName;
	}

	int32_t getLine() {
		return m_line;
	}

	uint32_t getElapse() {
		return m_elapse;
	}

	uint32_t getFiberid() {
		return m_fiberId;
	}

	uint32_t getTime() {
		return m_time;
	}

	std::string getThreadname() {
		return m_threadName;
	}

	std::stringstream& getSS() {
		return m_ss;
	}

	uint32_t getThreadId() {
		return m_threadId;
	}
private:
	const char* m_fileName = nullptr;	// 文件名
	int32_t m_line = 0;			// 行号
	uint32_t m_elapse = 0;		// 运行时间
	uint32_t m_time = 0;		// 时间戳
	std::string m_threadName;	// 线程名称
	LogLevel::Level m_level;	// 日志事件等级
	uint32_t m_threadId = 0;	// 线程ID
	uint32_t m_fiberId = 0;		// 协程ID
	std::stringstream m_ss;
};

class LogEventWrap {
public:
	LogEventWrap(std::shared_ptr<Logger> logger, LogEvent::ptr event) : m_logger(logger),  m_event(event) {};

	void write(const char* fmt, ...) {
		va_list vl;
		va_start(vl, fmt);
		char* buf = nullptr;
		int len = vasprintf(&buf, fmt, vl);
		if(len == -1) {
			return ;
		}
		m_event->getSS() << std::string(buf, len);
		free(buf);
		va_end(vl);
		return ;
	}

	~LogEventWrap() ;
	std::stringstream& getSS() const {
		return m_event->getSS();
	}
private:
	LogEvent::ptr m_event;
	std::shared_ptr<Logger> m_logger;
};

class LogFormatter{
public:
	typedef std::shared_ptr<LogFormatter> ptr;

	LogFormatter(std::string fmt = "aTtTnTfT[l]T[]TF:LTsN") :m_fmt(fmt) {		//t:线程号 X:Tab l:日志等级 s:内容 N:换行
		analyseFmt();
	}
	void log(std::ostream& ss, std::shared_ptr<Logger> logger, LogEvent::ptr event) {	
		for(auto& i : m_items) {
			i->format(ss, logger, event);
		}
	}
	class LogFormatItem{
		public:
			typedef std::shared_ptr<LogFormatItem> ptr;

			LogFormatItem() {};
			virtual ~LogFormatItem() {};
			virtual void format(std::ostream& ss, std::shared_ptr<Logger> logger, LogEvent::ptr event) = 0;
	};
private:
	void analyseFmt();	//可以更牛逼！！！！！！！！
private:
	std::string m_fmt;
	std::vector<LogFormatItem::ptr> m_items;
};

class LogAppender{
public:
	typedef std::shared_ptr<LogAppender> ptr;
	typedef SpinLock MutexType;
	LogAppender() {
		m_formatter = LogFormatter::ptr(new LogFormatter());
	}
	virtual ~LogAppender() {};
	void setFormat(LogFormatter::ptr format) {
		MutexType::Lock lock(m_mutex);
		m_formatter = format;
		m_hasFormat = true;
	}
	void setFormat(std::string fmt) {
		LogFormatter::ptr format(new LogFormatter(fmt));
		setFormat(format);
	}
	void setLevel(LogLevel::Level level) {
		m_level = level;
	}
	bool hasFormat() {
		return m_hasFormat;
	}

virtual void log(std::shared_ptr<Logger> logger, LogEvent::ptr event) = 0;
protected:
	bool m_hasFormat = false;
	LogFormatter::ptr m_formatter;
	LogLevel::Level m_level;	//对logger中的日志进行分流打印，不同级别的日志可以按照这个level打印在不同位置
	MutexType m_mutex;
};

class StdoutLogAppender : public LogAppender {
public:	
	typedef std::shared_ptr<StdoutLogAppender> ptr;
	StdoutLogAppender() : LogAppender() {}
	void log(std::shared_ptr<Logger> logger, LogEvent::ptr event) override {
		MutexType::Lock lock(m_mutex);
		if(m_level > event->getLevel()) return;
		m_formatter->log(std::cout, logger, event);
	}
private:
	
};

class FileLogAppender : public LogAppender {
public:
	typedef std::shared_ptr<FileLogAppender> ptr;
	FileLogAppender(std::string name = "root") : LogAppender(), m_name(name) {
		reopen();
	}
	void log(std::shared_ptr<Logger> logger, LogEvent::ptr event) override {
		MutexType::Lock lock(m_mutex);
		if(m_level > event->getLevel()) return;
		m_formatter->log(m_fileStream, logger, event);
	}
private:
	bool reopen() {
		if(m_fileStream) {
			m_fileStream.close();
		}
		m_fileStream.open(m_name.c_str());
		return m_fileStream.is_open();
	}
	std::string m_name;
	std::ofstream m_fileStream;
};

class Logger : public std::enable_shared_from_this<Logger> {
public:
	typedef std::shared_ptr<Logger> ptr;
	typedef SpinLock MutexType;
	Logger(const std::string name = "root") :m_name(name) {
		
	}
	void setRoot(Logger::ptr root) {
		m_root = root;
	}
	std::string getName() const {
		return m_name;
	}
	void log(LogEvent::ptr event) {
		if(m_level <= event->getLevel()) {
			MutexType::Lock lock(m_mutex);
			auto self = shared_from_this();
			if(!m_appenders.empty()) {
				for(auto& i : m_appenders) {
					i->log(self, event);
				}
			} else {
				// std::cout<< "NO ROOT:\t"<<std::endl;//<< event->getSS();
				m_root->log(event);
			}
		}
	}
	void addAppender(LogAppender::ptr appender) {
		MutexType::Lock lock(m_mutex);
		m_appenders.push_back(appender);
	}
	void delAppender(LogAppender::ptr appender) {
		MutexType::Lock lock(m_mutex);
	}
	void clearAppender() {
		MutexType::Lock lock(m_mutex);
		// m_appenders.clear();
	}
	LogLevel::Level getLevel() const {
		return m_level;
	}
	void setLevel(LogLevel::Level level) {
		m_level = level;
	}
	void setFormat(LogFormatter::ptr format) {
		MutexType::Lock lock(m_mutex);
		for(auto& i : m_appenders) {
			if(!i->hasFormat()) {
				i->setFormat(format);
			}
		}
	}
	void setFormat(std::string fmt) {
		LogFormatter::ptr format(new LogFormatter(fmt));
		setFormat(format);
	}
		
private:
	std::vector<LogAppender::ptr> m_appenders;
	Logger::ptr m_root;
	std::string m_name;
	LogLevel::Level m_level = LogLevel::DEBUG;
	MutexType m_mutex;
};

class LoggerManager{
public:
	typedef std::shared_ptr<LoggerManager> ptr;
	typedef SpinLock MutexType;

	static ptr& getInstance() {
		static ptr logger(new LoggerManager());
		return logger;
	}
	Logger::ptr getRoot() {
		return m_root;
	}
	Logger::ptr getLogger(std::string name) {
		MutexType::Lock lock(m_mutex);
		auto it = m_loggers.find(name);
		if(it != m_loggers.end()) {
			return it->second;
		}

		Logger::ptr logger(new Logger(name));
		logger->setRoot(m_root);
		m_loggers[name] = logger;
		return logger;
	}
	LoggerManager() {
		MutexType::Lock lock(m_mutex);
		m_root = Logger::ptr(new Logger());
		m_root->addAppender(LogAppender::ptr(new StdoutLogAppender()));

		m_loggers[m_root->getName()] = m_root;
	}
private:
	std::map<std::string, Logger::ptr> m_loggers;
	Logger::ptr m_root;
	MutexType m_mutex;
};


class LogAppenderDefine{
public:
	typedef std::shared_ptr<LogAppenderDefine> ptr;
	LogAppenderDefine() {
		m_type = 1;
		m_format = "++m++n++N";
		m_level = LogLevel::Level::INFO;
		m_file = "./linjie/*.c";
	}

	bool operator== (const LogAppenderDefine& oth) {
		return oth.m_format == m_format && oth.m_level == m_level;
	}
public:
	int m_type;  // 0:StdoutLogAppender  	1:FileLogAppender
	std::string m_file;
	std::string m_format;
	LogLevel::Level m_level;
};

class LoggerDefine{
public:
	LoggerDefine() {
		m_name = "log_1";
		m_formatter = "...";
		m_level = LogLevel::Level::DEBUG;
		m_appenders.push_back(LogAppenderDefine());
	}
	LoggerDefine(std::string name, std::string format, std::string level) : m_name(name), m_formatter(format) {
		m_level = LogLevel::fromString(level);
	}
	typedef std::shared_ptr<LoggerDefine> ptr;
	bool operator== (const LoggerDefine& oth) const {
		return oth.m_name == m_name;
	}

	bool operator< (const LoggerDefine& oth) const {
		return oth.m_name > m_name;
	}
public:
	std::string m_name = "";
	std::string m_formatter = "";
	LogLevel::Level m_level = LogLevel::UNKNOW;
	std::vector<LogAppenderDefine> m_appenders;
};

template<>
class LexicalCast<LogAppenderDefine, std::string> {
public:
	std::string operator() (const LogAppenderDefine& lad) {
		std::stringstream ss;
		YAML::Node node(YAML::NodeType::Map);
		node["format"] = lad.m_format;
		node["level"] = LogLevel::toString(lad.m_level);
		if(lad.m_type == 1) {
			node["file"] = lad.m_file;
			node["type"] = "FileLogAppender";
		} else {
			node["type"] = "StdoutLogAppender";
		}
		return ss.str();
	}
};

template<>
class LexicalCast<std::string, LogAppenderDefine> {
public:
	LogAppenderDefine operator() (const std::string& str) {
// std::cout << str << std::endl;
		LogAppenderDefine lad;
		YAML::Node node = YAML::Load(str);
// std::cout << node["type"].Scalar() << " string"  << std::endl;
		if(node["type"].Scalar() == "StdoutLogAppender") {
			lad.m_type = 0;
		} else if(node["type"].Scalar() == "FileLogAppender") {
			lad.m_type = 1;
			if(node["file"].IsDefined()) {
				lad.m_file = node["file"].Scalar();
			}
		}
		lad.m_level = LogLevel::fromString(node["level"].Scalar());
		lad.m_format = node["format"].Scalar();

		return lad;
	}
};

template<>
class LexicalCast<LoggerDefine, std::string> {
public:
	std::string operator() (const LoggerDefine& lg) {
		std::stringstream ss;
		YAML::Node node(YAML::NodeType::Map);

		node["name"] = lg.m_name;
		node["formatter"] = lg.m_formatter;
		node["level"] = LogLevel::toString(lg.m_level);
		// node["appenders"] = LexicalCast<LogAppenderDefine, std::string> (lg.m_appenders);
		// node["appenders"] = LexicalCast<std::vector<LogAppenderDefine>, std::string>() (lg.m_appenders);
		// for(auto it = lg.m_appenders.begin(); it != lg.m_appenders.end(); ++it) {
		for(size_t i = 0; i < lg.m_appenders.size(); ++i) {
			// node["appenders"].push_back(YAML::Load(LexicalCast<LogAppenderDefine, std::string>() (it->get())));
			LogAppenderDefine lad = lg.m_appenders[i];
			std::stringstream sub_ss;
			YAML::Node sub_node(YAML::NodeType::Map);
			sub_node["format"] = lad.m_format;
			sub_node["level"] = LogLevel::toString(lad.m_level);
			if(lad.m_type == 1) {
				sub_node["file"] = lad.m_file;
				sub_node["type"] = "FileLogAppender";
			} else {
				sub_node["type"] = "StdoutLogAppender";
			}
			node["appenders"].push_back(sub_node);
		}

		ss << node;
		return ss.str();
	}
};

template<>
class LexicalCast<std::string, LoggerDefine> {
public:
	LoggerDefine operator() (const std::string& str) {
		LoggerDefine ld;
		YAML::Node node = YAML::Load(str);
		if(node["name"].IsDefined()) {
			ld.m_name = node["name"].as<std::string>();
		}
		if(node["formatter"].IsDefined()) {
			ld.m_formatter = node["formatter"].as<std::string>();
		}
		if(node["level"].IsDefined()) {
			ld.m_level = LogLevel::fromString(node["level"].Scalar());
		}
		if(node["appenders"].IsDefined()) {
			// ld.m_appenders = LexicalCast<std::string, T> (node["appenders"].as<std::string>());
			// ld.m_appenders.swap(LexicalCast<std::string, std::vector<LogAppenderDefine> >() (node["appenders"].as<std::string>()));
			// 一个一个转换
			for(size_t i = 0; i < node["appenders"].size(); ++i) {
				// LogAppenderDefine sub_app(LexicalCast<std::string, LogAppenderDefine>() (node["appenders"][i].Scalar()));
				std::stringstream sss;
				sss << node["appenders"][i];
// std::cout << "print in LoggerDefineTransform:" << node["appenders"][i].Scalar() << sss.str() << "node_size: " << node["appenders"].size() << std::endl;
				ld.m_appenders.push_back(LexicalCast<std::string, LogAppenderDefine>() (sss.str()));
			}
		}
		return ld;
	}
};

}

#endif
