#ifndef _THREAD_H_
#define _THREAD_H_

#include <functional>
#include <string>
#include <pthread.h>
#include <stdint.h>
#include <memory>


namespace sylar{

class Thread {
public:
	typedef std::shared_ptr<Thread> ptr;

	std::string& getName() { return m_name; };
	Thread(std::function<void()> cb, const std::string& name = "");

	~Thread();

	pid_t getId() { return m_pid; }

	void join();

public:

	static std::string& GetName();

	static Thread* GetThis();

	static void SetName(const std::string& name);

private:
	static void* run(void* arg);

private:
	pthread_t m_thread = 0;
	pid_t m_pid = -1;
	std::string m_name;
	std::function<void()> m_cb;
};

}


#endif
