#ifndef _IOMANAGER_H_
#define _IOMANAGER_H_

#include <vector>
#include <memory>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>

#include "mutex.h"
#include "scheduler.h"
#include "timer.h"

namespace sylar {

class IOManager : public Scheduler, public TimerManager {
public:
	typedef RWMutex RWMutexType;
	typedef std::shared_ptr<IOManager> ptr;

	enum Event {
		NONE = 0x0,		//
		READ = 0x1,		//EPOLLIN
		WRITE = 0x4		//EPOLLOUT
	};

	IOManager(int thread_nums = 1, bool use_caller = false, std::string name = "");

	~IOManager();

public:
	struct Context {
		typedef Mutex MutexType;

		struct EventContext {
			Scheduler* scheduler = nullptr;
			Fiber::ptr fiber;
			std::function<void()> func;
		};

		EventContext& getEvtContext(Event e);

		void resetContext(EventContext& ctx);

		void triggerEvent(Event e);

		Event event = NONE;
		EventContext write;
		EventContext read;
		int fd = 0;
		MutexType mutex;
	};

	int addEvent(int fd, Event e, std::function<void()> func = nullptr);

	bool delEvent(int fd, Event e);

	bool cancelEvent(int fd, Event e);

	bool cancelAll(int fd);

	void OnInsertAtFirst() override { tickle(); }
public:
	bool stoping() override;

	void tickle() override;

	void idle() override;

private:
	void contextResize(uint32_t size);

public:
	static IOManager* GetThis();

private:
	int m_epfd = 0;
	std::atomic<uint32_t> m_pendingEventCount = {0};
	int m_tickleFds[2];
	std::vector<Context*> m_fdContexts;
	RWMutexType m_mutex;
};

}


#endif
