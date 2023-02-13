/*
	errno的错误号会延续一段时间，前面的出错但是后面的没出错，会保存错误号
*/
#include "iomanager.h"
#include "log.h"

#include <iostream>

namespace sylar {

Logger::ptr t_logger = SYLAR_LOG_NAME("system");

IOManager::Context::EventContext& IOManager::Context::getEvtContext(Event e){
	switch(e) {
		case WRITE:
			return write;
			break;
		case READ:
			return read;
		default:
			SYLAR_ASSERT2(false, "error Event");
	}
	throw std::invalid_argument("error Event in getEvtContext");
}

void IOManager::Context::resetContext(EventContext& ctx){
	ctx.scheduler = nullptr;
	ctx.fiber.reset();
	ctx.func = nullptr;
}

void IOManager::Context::triggerEvent(Event e){
	SYLAR_ASSERT(event & e);		// 确保己方Context中包含了e事件
	event = (Event)(event & ~e);		// 去除准备触发的事件
	EventContext tmp_ectx = getEvtContext(e);
	if(tmp_ectx.func) {
		tmp_ectx.scheduler->schedule(tmp_ectx.func);
	} else {
		tmp_ectx.scheduler->schedule(tmp_ectx.fiber);
	}
	tmp_ectx.scheduler = nullptr;
}

IOManager::IOManager(int thread_nums, bool use_caller, std::string name)
	:Scheduler(thread_nums, use_caller, name) {
	m_epfd = epoll_create(5000);
	SYLAR_ASSERT(m_epfd > 0);

	int ret = pipe(m_tickleFds);
	SYLAR_ASSERT(!ret);
ret = write(m_tickleFds[1], "1", 1);
SYLAR_LOG_INFO(t_logger) << "errno str: " << strerror(errno);
SYLAR_ASSERT(ret != -1);

	epoll_event tmp_event;
	memset(&tmp_event, 0, sizeof(tmp_event));
	tmp_event.data.fd = m_tickleFds[0];
	tmp_event.events = EPOLLIN | EPOLLET;
	ret = fcntl(m_tickleFds[0], F_SETFD, O_NONBLOCK);
	SYLAR_ASSERT(!ret);
	ret = epoll_ctl(m_epfd, EPOLL_CTL_ADD, m_tickleFds[0], &tmp_event);
	SYLAR_ASSERT(!ret);

	contextResize(32);
	start();
}

IOManager::~IOManager() {
	stop();
	close(m_tickleFds[0]);
	close(m_tickleFds[1]);
	close(m_epfd);

	for(size_t i = 0; i < m_fdContexts.size(); ++i) {
		delete m_fdContexts[i];
	}
}

//success:0   error:-1
int IOManager::addEvent(int fd, Event e, std::function<void()> func) {
// 根据fd大小重置m_fdContexts大小，将fd对应的事件加入到epfd中，（若cb为空则将当前协程设为回调事件）
	SYLAR_ASSERT(fd > 0);
	IOManager::Context* tmp_ctx = nullptr;
	RWMutexType::RLock lock1(m_mutex);
	if((int)m_fdContexts.size() > fd) {
		tmp_ctx = m_fdContexts[fd];
		lock1.unlock();
	} else {
		lock1.unlock();
		RWMutexType::WLock lock2(m_mutex);
		contextResize(fd * 1.5);
		tmp_ctx = m_fdContexts[fd];
	}

	IOManager::Context::MutexType::Lock lock2(tmp_ctx->mutex);

	SYLAR_ASSERT(!(tmp_ctx->event & e));

	int op = tmp_ctx->event ? EPOLL_CTL_MOD : EPOLL_CTL_ADD;
// std::cout << "---tmp_ctx->event: " << tmp_ctx->event << " op: " << op << " EPOLL_CTL_MOD: " << EPOLL_CTL_MOD << " EPOLL_CTL_ADD: " << EPOLL_CTL_ADD << "\n";
	epoll_event tmp_event;
	tmp_event.events = EPOLLET | tmp_ctx->event | e;
	tmp_event.data.ptr = tmp_ctx;
	int ret = epoll_ctl(m_epfd, op, fd, &tmp_event);
	if(ret) {
		SYLAR_LOG_ERROR(t_logger) << "epoll_ctl(" << m_epfd 
									<< ", " << op 
									<< ", " << fd
									<< ", " << (EPOLL_EVENTS)tmp_event.events << ") : "
									<< ret << "(errno: " << errno << " ==>"
									<< strerror(errno) << std::endl;
		return -1;	
	}

	++m_pendingEventCount;
	tmp_ctx->event = (Event)(tmp_ctx->event | e);
	IOManager::Context::EventContext& tmp_ectx = tmp_ctx->getEvtContext(e);
	SYLAR_ASSERT(!tmp_ectx.scheduler && !tmp_ectx.fiber && !tmp_ectx.func);
	tmp_ectx.scheduler = sylar::Scheduler::GetThis();
	if(func) {
		tmp_ectx.func.swap(func);
	} else { 	// 如果没有传func参数，则以调用位置后面的语句为回调事件
		tmp_ectx.fiber = Fiber::GetThis();
		SYLAR_ASSERT2(tmp_ectx.fiber->getState() == Fiber::EXEC, "addEvent error : no func, state: " + std::to_string((int)tmp_ectx.fiber->getState()));
	}
	return 0;
}

bool IOManager::delEvent(int fd, Event e){
// 取出事件，将事件从epfd中删除，重置EventContext
	if((int)m_fdContexts.size() < fd) {
		SYLAR_LOG_INFO(t_logger) << "no such fd in delEvent";
		return false;
	}
	RWMutexType::WLock lock1(m_mutex);
	IOManager::Context* tmp_ctx = m_fdContexts[fd];
	lock1.unlock();
	SYLAR_ASSERT(tmp_ctx->event & e);

	IOManager::Context::MutexType::Lock lock2(tmp_ctx->mutex);
	epoll_event tmp_event;
	tmp_event.events = tmp_ctx->event & ~e;
	int op = tmp_event.events ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
	tmp_event.events = tmp_event.events | EPOLLET;
	tmp_event.data.ptr = tmp_ctx;
	int ret = epoll_ctl(m_epfd, op, fd, &tmp_event);
	if(ret) {
		SYLAR_LOG_ERROR(t_logger) << "epoll_ctl(" << m_epfd 
									<< ", " << fd 
									<< ", " << op
									<< ", " << (EPOLL_EVENTS)tmp_event.events << ") : "
									<< ret << "(errno: " << errno << " ==>"
									<< strerror(errno) << std::endl;
		return false;	
	}
	--m_pendingEventCount;

	tmp_ctx->event = (Event)(tmp_ctx->event & ~e);
	IOManager::Context::EventContext& tmp_ectx = tmp_ctx->getEvtContext(e);
	tmp_ctx->resetContext(tmp_ectx);

	return true;
}

bool IOManager::cancelEvent(int fd, Event e){
// 取出事件，将事件从epfd中删除，触发事件
	if((int)m_fdContexts.size() < fd) {
		SYLAR_LOG_INFO(t_logger) << "no such fd in delEvent";
		return false;
	}
	RWMutexType::WLock lock1(m_mutex);
	IOManager::Context* tmp_ctx = m_fdContexts[fd];
	lock1.unlock();
	SYLAR_ASSERT(tmp_ctx->event & e);

	IOManager::Context::MutexType::Lock lock2(tmp_ctx->mutex);
	epoll_event tmp_event;
	tmp_event.events = tmp_ctx->event & ~e;
	int op = tmp_event.events ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
	tmp_event.events = tmp_event.events | EPOLLET;
	tmp_event.data.ptr = tmp_ctx;
	int ret = epoll_ctl(m_epfd, op, fd, &tmp_event);
	if(ret) {
		SYLAR_LOG_ERROR(t_logger) << "epoll_ctl(" << m_epfd 
									<< ", " << fd 
									<< ", " << op
									<< ", " << (EPOLL_EVENTS)tmp_event.events << ") : "
									<< ret << "(errno: " << errno << " ==>"
									<< strerror(errno) << std::endl;
		return false;	
	}
	--m_pendingEventCount;

	tmp_ctx->triggerEvent(e);

	return true;
}



bool IOManager::cancelAll(int fd){
// 取出事件，将该fd的所有事件从epfd中删除，触发所有事件
	if((int)m_fdContexts.size() < fd) {
		SYLAR_LOG_INFO(t_logger) << "no such fd in delEvent";
		return false;
	}
	RWMutexType::WLock lock1(m_mutex);
	IOManager::Context* tmp_ctx = m_fdContexts[fd];
	lock1.unlock();

	IOManager::Context::MutexType::Lock lock2(tmp_ctx->mutex);
	int ret = epoll_ctl(m_epfd, EPOLL_CTL_DEL, fd, nullptr);
	if(ret) {
		SYLAR_LOG_ERROR(t_logger) << "epoll_ctl(" << m_epfd 
									<< ", " << "EPOLL_CTL_DEL" 
									<< ", " << fd
									<< ", " << "nullptr" << ") : "
									<< ret << "(errno: " << errno << " ==>"
									<< strerror(errno) << std::endl;
		return false;	
	}
	--m_pendingEventCount;

	if(tmp_ctx->event & WRITE) {
		tmp_ctx->triggerEvent(WRITE);
	}
	if(tmp_ctx->event & READ) {
		tmp_ctx->triggerEvent(READ);
	}

	SYLAR_ASSERT(tmp_ctx->event == 0);
	return true;
}

bool IOManager::stoping() {
	 /* std::cout << "----stop msg: Scheduler::stoping(): " << (Scheduler::stoping() ? "true": "false")
	 				<< "\tm_pendingEventCount == 0: " << ((m_pendingEventCount == 0) ? "true": "false")
	 				<< "\tm_pendingEventCount: " << m_pendingEventCount
					<< std::endl;
	*/return Scheduler::stoping()
			&& !hasTimer()
			&& m_pendingEventCount == 0;
			// && getNextTime() = ~0ll;
}

void IOManager::tickle() {
	if(m_idleThreadCount <= 0) {
		return ;
	}
}

void IOManager::idle() {
// epoll_eait取出有事件的fd，处理时间后让出协程调度资格
	const int MAX_EPOLL_EVENT = 256;
	epoll_event recv_events[MAX_EPOLL_EVENT];
	int MAX_TIMEOUT = 3000;

	while(true) {
		if(stoping()) {
			SYLAR_LOG_INFO(t_logger) << /*"name = " << getName() <<*/ " idle end";
			break;
		}

		uint64_t time_out_1 = getNextTime();
		uint64_t time_out = ((time_out_1 == ~0ull) ? MAX_TIMEOUT : time_out_1);

		int recv_nums = 0;
		while(true) {
			// 定时器计时
			recv_nums = epoll_wait(m_epfd, recv_events, MAX_EPOLL_EVENT, time_out);
			if(recv_nums <= 0 && errno == EINTR) {
			} else {
				break;
			}
		}
		std::vector<std::function<void()> > funcs;
		listPassedTimer(funcs);
		for(auto i : funcs) {
			i();
		}
		for(size_t i = 0; i < recv_nums; ++i) {
			epoll_event& event = recv_events[i];
			if(event.data.fd == m_tickleFds[0]) {
				char recv_char;
				while(recv(event.data.fd, &recv_char, 1, 0) > 0);
				continue;
			}

			IOManager::Context* tmp_ctx = (IOManager::Context*)event.data.ptr;
			IOManager::Context::MutexType::Lock lock(tmp_ctx->mutex);
			if(event.events & (EPOLLERR | EPOLLHUP)) {
				event.events |= (EPOLLIN | EPOLLOUT) & tmp_ctx->event;
			}

			int real_event = NONE, left_event = NONE;
			if(tmp_ctx->event & EPOLLIN) {
				real_event |= EPOLLIN;
			}
			if(tmp_ctx->event & EPOLLOUT) {
				real_event |= EPOLLOUT;
			}

			if(real_event == NONE) {
				continue;
			}

			left_event = tmp_ctx->event & ~real_event;
			event.events = left_event | EPOLLET;
			int op = left_event ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
			// std::cout << "----events: " << left_event << " fd: " << tmp_ctx->fd << " op: " << op << " DEL: " << EPOLL_CTL_DEL << std::endl;
			int ret = epoll_ctl(m_epfd, op, tmp_ctx->fd, &event);
			if(ret) {
				SYLAR_LOG_ERROR(t_logger) << "epoll_ctl(" << m_epfd 
											<< ", " << op
											<< ", " << m_tickleFds[0] 
											<< ", " << (EPOLL_EVENTS)event.events << ") : "
											<< ret << "(errno: " << errno << " ==>"
											<< strerror(errno) << std::endl;
				continue;
			}

			if(real_event & READ) {
				tmp_ctx->triggerEvent(READ);
				--m_pendingEventCount;
			}
			if(real_event & WRITE) {
				tmp_ctx->triggerEvent(WRITE);
				--m_pendingEventCount;
			}

		}

		// 获取智能指针的内置指针进行调出，防止智能指针的计数永远大于0，不析构
		Fiber::ptr tmp_fiber = Fiber::GetThis();
		auto raw_ptr = tmp_fiber.get();
		tmp_fiber.reset();
		raw_ptr->swapOut();
	}
}

void IOManager::contextResize(uint32_t size){
// std::cout << "----from " << m_fdContexts.size() << " to " << size << "\n";
	m_fdContexts.resize(size);
	for(size_t i = 0; i < size; ++i) {
		if(!m_fdContexts[i]) {
			m_fdContexts[i] = new Context;
			m_fdContexts[i]->fd = i;
		}
	}
}

IOManager* IOManager::GetThis(){
	return dynamic_cast<IOManager*>(Scheduler::GetThis());
}

}
