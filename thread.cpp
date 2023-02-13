#include "thread.h"
#include <iostream>

#include "log.h"

namespace sylar {

sylar::LoggerManager::ptr g_manager = sylar::LoggerManager::getInstance();
sylar::Logger::ptr g_logger = g_manager->getLogger("system");

static thread_local std::string t_thread_name = "UNKNOW";
static thread_local Thread* t_thread = nullptr;

Thread::Thread(std::function<void()> cb, const std::string& name) :
	m_name(name),
	m_cb(cb) {
	if(m_name.empty()) {
		m_name = "UNKNOW";
	}

	int ref = pthread_create(&m_thread, NULL, &Thread::run, this);
	if(ref) {
		SYLAR_LOG_ERROR(g_logger) << "THREAD CREATE ERROR";
		throw std::logic_error("pthread_create error");
	}

	// semaphore.wait();
}

Thread::~Thread() {
	if(m_thread) {
		pthread_detach(m_thread);
	}
}

void Thread::join() {
	if(m_thread) {
		int ref = pthread_join(m_thread, nullptr);
		if(ref) {
			SYLAR_LOG_ERROR(g_logger) << "pthread_join error";
			throw std::logic_error("pthread_join error");
		}
		m_thread = 0;
	}
}

std::string& Thread::GetName() {
	return t_thread_name;
}

Thread* Thread::GetThis() {
	return t_thread;
}

void Thread::SetName(const std::string& name) {
	if(name.empty()) {
		return;
	}
	if(t_thread) {
		t_thread->m_name = name;
		return;
	}
	t_thread_name = name;
}

void* Thread::run(void* arg) {
	Thread* th = (Thread*)arg;
	t_thread_name = th->m_name;
	t_thread = th;
	th->m_pid = GetThreadId();		// XXXXXXXXXXXXXXXXXXXXXX
	pthread_setname_np(pthread_self(), th->m_name.substr(0, 15).c_str());

	std::function<void()> cbs;
	cbs.swap(th->m_cb);

	cbs();
	// semaphore.signal()

	return nullptr;
}

}
