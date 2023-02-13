/*
	待学：
		readv；
		weak_ptr;
*/
#include "hook.h"

#include <sys/ioctl.h>

#include "fd_manager.h"
#include "fiber.h"
#include "iomanager.h"

sylar::Logger::ptr t_logger = SYLAR_LOG_NAME("system");

namespace sylar {

static thread_local bool t_hook_enable = false;

#define HOOK_FUNC(XX) \
	XX(sleep) \
	XX(usleep) \
	XX(nanosleep) \
	XX(socket) \
	XX(connect) \
	XX(accept) \
	XX(read) \
	XX(readv) \
	XX(recv) \
	XX(recvfrom) \
	XX(recvmsg) \
	XX(write) \
	XX(writev) \
	XX(send) \
	XX(sendto) \
	XX(sendmsg) \
	XX(close) \
	XX(fcntl) \
	XX(ioctl) \
	XX(getsockopt) \
	XX(setsockopt) 

void hook_init() {		// 取出原来的函数赋值给'name'_f，以给要使用的函数使用
	static bool is_inited = false;
	if(is_inited) {
		return ;
	}

#define XX(name) \
	name ## _f = (name ## _fun)dlsym(RTLD_NEXT, #name);
	HOOK_FUNC(XX);
			// dlsym：根据name取出原来的函数，赋值给name_fun（头文件：dlfcn.h, 编译时加上 -ldl）, name_fun在下面定义了。
#undef XX
}

bool is_hook_enable(){
	return t_hook_enable;
}

void set_hook_enable(bool v){
	t_hook_enable = v;
}

// 使hook_init()能在main函数之前被调用
struct _HookIniter {
	_HookIniter() {
		hook_init();
	}
};

static _HookIniter hookIniter;

#undef HOK_FUNC
}

struct timer_info {
public:
	int canceled = false;
};

template<typename OriginFun, typename... Args>
static ssize_t do_io(int fd, OriginFun fun, const char* hook_fun_name, 
			uint64_t event, int timeout_so, Args&&... args) {
	if(!sylar::t_hook_enable) {
		return fun(fd, std::forward<Args>(args)...);
	}


	sylar::FdCtx::ptr tmp_fdCtx = sylar::FdMgr::GetInstance()->get(fd);
	if(!tmp_fdCtx) {
		return fun(fd, std::forward<Args>(args)...);
	}

	if(tmp_fdCtx->isClose()) {
		errno = EBADF;
		return -1;
	}

	if(!tmp_fdCtx->isSocket() || tmp_fdCtx->getUsernonblock()) {
		return fun(fd, std::forward<Args>(args)...);
	}

	uint64_t timeout = tmp_fdCtx->getTimeout(timeout_so);
	std::shared_ptr<timer_info> tinfo(new timer_info);

retry:
	int ret = fun(fd, std::forward<Args>(args)...);
	while(ret == -1 && errno == EINTR) {
		ret = fun(fd, std::forward<Args>(args)...);
	}
	if(ret == -1 && errno == EAGAIN) {
		sylar::IOManager* iom = sylar::IOManager::GetThis();
		std::weak_ptr<timer_info> winfo(tinfo);
		sylar::Timer::ptr timer;

		if(timeout != (uint64_t)(-1)) {
			// 增加定时器的作用：达到socket fd执行任务有超时时间的效果
			// 条件定时器：？？？
			timer = iom->addCondTimer(timeout, [iom, event, fd, winfo](){
				auto t = winfo.lock();
				if(!t || t->canceled) {
					return;
				}
				t->canceled = ETIMEDOUT;
				iom->cancelEvent(fd, (sylar::IOManager::Event)event); 	// 时间到了但是事件未完成：事件未完成，取消事件并返回
			}, winfo);
		}
		int rt = iom->addEvent(fd, (sylar::IOManager::Event)event);
		if(rt == -1) {
			SYLAR_LOG_DEBUG(t_logger) << hook_fun_name << ": addEvent(" 
									  << fd << ", " << event << ")error";
			if(timer) {
				timer->cancel();
			}
		} else {
			// 协程让出了执行权
	SYLAR_LOG_DEBUG(t_logger) << "<<<in hooked func: " << hook_fun_name << ">>>";
			sylar::Fiber::YieldToHold();
	SYLAR_LOG_DEBUG(t_logger) << "<<<in hooked func: " << hook_fun_name << ">>>";
			// 协程回到了这里（时间到了或者有事件来了）
			if(timer) { 	// 事件出发了但时间未到：定时器不需要工作了，进入retry，完成socket fd要执行的任务（事件到了，无需等待，直接响应）
				timer->cancel();
			}
			if(tinfo->canceled) {	// 时间到了且设置了信号：直接返回超时失败
				errno = tinfo->canceled;
				return -1;
			}
			goto retry;
		}
	}
	return ret;
}


// 用C的方式编译下面的语句，防止C++编译器给下面的函数加上为了分辨重载函数所用的“记号”
extern "C" {

// 创建原来的函数调用的容器：name_f
#define XX(name) \
	name ## _fun name ## _f = nullptr;
			// 创建name_f并赋为空
	HOOK_FUNC(XX);
#undef XX

// 创建与原来的函数调用一样函数签名的同名函数，覆盖原来的函数，以达到hook原来函数的效果

unsigned int sleep(unsigned int seconds) {
	if(!sylar::t_hook_enable) {
		return sleep_f(seconds);
	}

	sylar::Fiber::ptr fiber =  sylar::Fiber::GetThis();
	sylar::IOManager* iom = sylar::IOManager::GetThis();
	// iom->addTimer(seconds * 1000, [iom, fiber](){  // ???有了FIBER就出错！！！
	// 	iom->schedule(fiber);
	// });
	iom->addTimer(seconds * 1000, std::bind((void(sylar::Scheduler::*)(sylar::Fiber::ptr, int thread))&sylar::IOManager::schedule, iom, fiber, -1));
	sylar::Fiber::YieldToHold();
	return 0;
}

int usleep(useconds_t usec) {
	if(!sylar::t_hook_enable) {
		return usleep_f(usec);
	}

	sylar::Fiber::ptr fiber =  sylar::Fiber::GetThis();
	sylar::IOManager* iom = sylar::IOManager::GetThis();
	// iom->addTimer(usec / 1000, [iom, fiber](){
	// 	iom->schedule(fiber);
	// });
	iom->addTimer(usec / 1000, std::bind((void(sylar::Scheduler::*)(sylar::Fiber::ptr, int thread))&sylar::IOManager::schedule, iom, fiber, -1));
	sylar::Fiber::YieldToHold();
	return 0;
}

int nanosleep(const struct timespec *req, struct timespec *rem){
	if(!sylar::t_hook_enable) {
		return nanosleep_f(req, rem);
	}
	uint64_t timeout_ms = req->tv_sec * 1000 + req->tv_nsec / 1000 / 1000;
	sylar::Fiber::ptr fiber = sylar::Fiber::GetThis();
	sylar::IOManager* iom = sylar::IOManager::GetThis();
	iom->addTimer(timeout_ms, std::bind((void(sylar::Scheduler::*)(sylar::Fiber::ptr, int thread))&sylar::IOManager::schedule, iom, fiber, -1));
	sylar::Fiber::YieldToHold();
	return 0;
}

int socket(int domain, int type, int protocal) {
	if(!sylar::t_hook_enable) {
		return socket_f(domain, type, protocal);
	}
	int fd = socket_f(domain, type, protocal);
	if(fd < 0) {
		return -1;
	}

	sylar::FdMgr::GetInstance()->get(fd, true);
	return fd;
}

int connect(int sockfd, const struct sockaddr* addr, socklen_t addrlen) {
	return connect_with_timeout(sockfd, addr, addrlen, -1);
}

int accept(int s, struct sockaddr *addr, socklen_t *addrlen) {
	int fd = do_io(s, accept_f, "accept_f", sylar::IOManager::READ, SO_RCVTIMEO, addr, addrlen);
	if(fd < 0) {
		return -1;
	}
	sylar::FdMgr::GetInstance()->get(fd, true);
	return fd;
}

ssize_t read(int fd, void* buf, size_t count) {
	return do_io(fd, read_f, "read_f", sylar::IOManager::READ, SO_RCVTIMEO, buf, count);
}

ssize_t readv(int fd, const struct iovec* iov, int iocnt) {
	return do_io(fd, readv_f, "readv_f", sylar::IOManager::READ, SO_RCVTIMEO, iov, iocnt);
}

ssize_t recv(int sockfd, void* buf, size_t len, int flags) {
	return do_io(sockfd, recv_f, "recv_f", sylar::IOManager::READ, SO_RCVTIMEO, buf, len, flags);
}

ssize_t recvfrom(int sockfd, void* buf, size_t len, int flags, struct sockaddr* src_addr, socklen_t* addrlen) {
	return do_io(sockfd, recvfrom_f, "recvfrom_f", sylar::IOManager::READ, SO_RCVTIMEO, buf, len, flags, src_addr, addrlen);
}

ssize_t recvmsg(int sockfd, struct msghdr* msg, int flags) {
	return do_io(sockfd, recvmsg_f, "recvmsg_f", sylar::IOManager::READ, SO_RCVTIMEO, msg, flags);
}

ssize_t write(int fd, const void* buf, size_t count) {
	return do_io(fd, write_f, "write_f", sylar::IOManager::WRITE, SO_SNDTIMEO, buf, count);
}

ssize_t writev(int fd, const struct iovec* iov, int iocnt) {
	return do_io(fd, writev_f, "writev_f", sylar::IOManager::WRITE, SO_SNDTIMEO, iov, iocnt);
}

ssize_t send(int s, const void* msg, size_t len, int flags) {
	return do_io(s, send_f, "send_f", sylar::IOManager::WRITE, SO_SNDTIMEO, msg, len, flags);
}

ssize_t sendto(int s, const void* msg, size_t len, int flags, const sockaddr* to, socklen_t tolen) {
	return do_io(s, sendto_f, "sendto_f", sylar::IOManager::WRITE, SO_SNDTIMEO, msg, len, flags, to, tolen);
}

ssize_t sendmsg(int s, const struct msghdr* msg, int flags) {
	return do_io(s, sendmsg_f, "sendmsg_f", sylar::IOManager::WRITE, SO_SNDTIMEO, msg, flags);
}

int close(int fd) {
	if(!sylar::t_hook_enable) {
		return close_f(fd);
	}

	sylar::FdCtx::ptr tmp_fdCtx = sylar::FdMgr::GetInstance()->get(fd);
	if(tmp_fdCtx) {
		sylar::IOManager* iom = sylar::IOManager::GetThis();
		if(iom) {
			iom->cancelAll(fd);
		}
		sylar::FdMgr::GetInstance()->del(fd);
	}
	return close_f(fd);
}

int fcntl(int fd, int cmd, .../* args */ ) {
	va_list va;
	va_start(va, cmd);
	switch(cmd) {
		case F_SETFL:
			{
				int arg = va_arg(va, int);
				va_end(va);
				sylar::FdCtx::ptr tmp_fdCtx = sylar::FdMgr::GetInstance()->get(fd);
				if(!tmp_fdCtx || tmp_fdCtx->isClose() || !tmp_fdCtx->isSocket()) {
					return fcntl_f(fd, cmd, arg);
				}
				// 直接设置系统堵塞
				tmp_fdCtx->setSysnonblock(arg & O_NONBLOCK);
				// 根据用户阻塞设置fcntl_f
				if(tmp_fdCtx->getUsernonblock()) {
					arg |= O_NONBLOCK;
				} else {
					arg &= ~O_NONBLOCK;
				}
				return fcntl_f(fd, cmd, arg);
			}
		case F_GETFL:
			{
				va_end(va);
				int arg = fcntl_f(fd, cmd);
				sylar::FdCtx::ptr tmp_fdCtx = sylar::FdMgr::GetInstance()->get(fd);
				if(!tmp_fdCtx || tmp_fdCtx->isClose() || !tmp_fdCtx->isSocket()) {
					return arg;
				}
				if(tmp_fdCtx->getUsernonblock()) {
					return arg | O_NONBLOCK;
				} else {
					return arg & ~O_NONBLOCK;
				}
			}
		case F_DUPFD:
		case F_DUPFD_CLOEXEC:
		case F_SETFD:
		case F_SETOWN:
		case F_SETSIG:
		case F_SETLEASE:
		case F_NOTIFY:
		case F_SETPIPE_SZ:
			{
				int arg = va_arg(va, int);
				va_end(va);
				return fcntl_f(fd, cmd, arg);
			}
			break;
		case F_GETFD:
		case F_GETOWN:
		case F_GETSIG:
		case F_GETLEASE:
		case F_GETPIPE_SZ:
			{
				va_end(va);
				return fcntl_f(fd, cmd);
			}
			break;
		case F_SETLK:
		case F_SETLKW:
		case F_GETLK:
			{
				struct flock* arg = va_arg(va, struct flock*);
				va_end(va);
				return fcntl_f(fd, cmd, arg);
			}
			break;
		case F_GETOWN_EX:
		case F_SETOWN_EX:
			{
				struct f_owner_exlock* arg = va_arg(va, struct f_owner_exlock*);
				va_end(va);
				return fcntl_f(fd, cmd, arg);
			}
			break;
		default:
			va_end(va);
			return fcntl_f(fd, cmd);
			break;
	}
}

int ioctl(int d, unsigned long int request, ...) {
	va_list va;
	va_start(va, request);
	void* arg = va_arg(va, void*);
	va_end(va);

	switch(request) {
		case FIONBIO:
			{
				sylar::FdCtx::ptr tmp_fdCtx = sylar::FdMgr::GetInstance()->get(d);
				if(!tmp_fdCtx || tmp_fdCtx->isClose() || !tmp_fdCtx->isSocket()) {
					return ioctl_f(d, request, arg);
				}
				tmp_fdCtx->setUsernonblock(!!*(int*)arg);
			}
			break;
		default:
			break;
	}
	return ioctl_f(d, request, arg);
}

int getsockopt(int sockfd, int level, int optname, void* optval, socklen_t* optlen) {
	return getsockopt_f(sockfd, level, optname, optval, optlen);
}

int setsockopt(int sockfd, int level, int optname, const void* optval, socklen_t optlen) { 	// 在设置sockopt时若要设置SOL_SOCKET则把FdCtx的tiemout也设置一下
	if(!sylar::t_hook_enable) {
		return setsockopt_f(sockfd, level, optname, optval, optlen);
	}

	if(level == SOL_SOCKET) {
		if(optname == SO_RCVTIMEO || optname == SO_SNDTIMEO) {
			sylar::FdCtx::ptr tmp_fdCtx = sylar::FdMgr::GetInstance()->get(sockfd);
			if(tmp_fdCtx) {
				const timeval* v = (const timeval*)optval;
				tmp_fdCtx->setTimeout(optname, v->tv_sec * 1000 + v->tv_usec / 1000);
			}
		}
	}
	return setsockopt_f(sockfd, level, optname, optval, optlen);
}

int connect_with_timeout(int fd, const struct sockaddr* addr, socklen_t addrlen, uint64_t timeout_ms) {
	if(!sylar::t_hook_enable) {
		return connect_f(fd, addr, addrlen);
	}

	sylar::FdCtx::ptr tmp_fdCtx = sylar::FdMgr::GetInstance()->get(fd);
	if(!tmp_fdCtx || tmp_fdCtx->isClose()) {
		errno = EBADF;
		return -1;
	}

	if(tmp_fdCtx->getUsernonblock() || !tmp_fdCtx->isSocket()) {
		return connect_f(fd, addr, addrlen);
	}

	int ret = connect_f(fd, addr, addrlen);
	if(ret == 0) {
		return 0;
	} else if(ret != -1 || errno != EINPROGRESS) {
		return ret;
	}
	// connect返回-1且errno==EINPROGRESS则表示连接正在进行但是没有连接上，可监测fd的WRITE事件查看是否连接上了
	sylar::IOManager* iom = sylar::IOManager::GetThis();
	sylar::Timer::ptr timer;
	std::shared_ptr<timer_info> sinfo(new timer_info);
	std::weak_ptr<timer_info> winfo(sinfo);
	if(timeout_ms != (uint64_t)-1) {
		timer = iom->addCondTimer(timeout_ms, [iom, timer, winfo, fd](){
			auto t = winfo.lock();
			if(!t || t->canceled) {
				return;
			}
			t->canceled = SO_RCVTIMEO;
			iom->cancelEvent(fd, sylar::IOManager::WRITE);
		}, winfo);
	}
	int ret_1 = iom->addEvent(fd, sylar::IOManager::WRITE);
	if(ret_1 == -1) {
		if(timer) {
			timer->cancel();
		}
		SYLAR_LOG_ERROR(t_logger) << "iom->addEvent(" << fd << ", "
								  << sylar::IOManager::READ << ") error";
	} else {
		sylar::Fiber::YieldToHold();
		if(timer) {
			timer->cancel();
		}
		if(sinfo->canceled) {
			errno = sinfo->canceled;
			return -1;
		}
	}
	int error = 0;
	socklen_t len = sizeof(int);
	if(-1 == getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len)) {
		return -1;
	}
	if(error) {
		errno = error;
		return -1;
	} else {
		return 0;
	}
	return 0;
}



}
