#include "fd_manager.h"

#include <iostream>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/socket.h>

namespace sylar {

FdCtx::FdCtx(int fd)
	:m_fd(fd)
	,m_isInit(false)
	,m_isClose(false)
	,m_isSocket(false)
	,m_sysNonblock(false)
	,m_userNonblock(false)
	,m_recvTimeout(-1)
	,m_sendTimeout(-1){
	init();
}

FdCtx::~FdCtx() {
}

uint64_t FdCtx::getTimeout(int type) {
	if(type == SO_RCVTIMEO) {
		return m_recvTimeout;
	} else {
		return m_sendTimeout;
	}
}

void FdCtx::setTimeout(int type, uint64_t timeout) {
	if(type == SO_RCVTIMEO){
		m_recvTimeout = timeout;
	} else {
		m_sendTimeout = timeout;
	}
}

bool FdCtx::init() {
	if(m_isInit) {
		return true;
	}

	m_recvTimeout = -1;
	m_sendTimeout = -1;

	struct stat st;
	if(-1 == fstat(m_fd, &st)) {
	// std::cout << "m_fd: " << m_fd << strerror(errno) << "\n";
		m_isInit = false;
		m_isSocket = false;
	} else {
		m_isInit = true;
		m_isSocket = S_ISSOCK(st.st_mode);
	}

	if(m_isSocket) {
		int old_flag = fcntl(m_fd, F_GETFL);
		if(!(old_flag & O_NONBLOCK)) {
			fcntl(m_fd, F_SETFL, old_flag | O_NONBLOCK);
		}
		m_sysNonblock = true;
	} else {
		m_sysNonblock = false;
	}

	m_userNonblock = false;
	m_isClose = false;
	return m_isInit;
}

FdManager::FdManager() {
	m_datas.resize(64);
}

FdCtx::ptr FdManager::get(int fd, bool auto_create) {
	if(fd < 0) {
		return nullptr;
	}

	// RWMutexType::WLock lock_1(m_mutex);
	if((int)m_datas.size() < fd && auto_create == false) {
		return nullptr;
	} else if((int)m_datas.size() > fd && (m_datas[fd] || !auto_create)) {
		return m_datas[fd];
	}
	// lock_1.unlock();

	// RWMutexType::WLock lock_2(m_mutex);
	FdCtx::ptr tmp_fdCtx(new FdCtx(fd));
	if((int)m_datas.size() < fd) {
		m_datas.resize(fd * 1.5);
	}
	m_datas[fd] = tmp_fdCtx;
	return tmp_fdCtx;
}

void FdManager::del(int fd) {
	RWMutexType::WLock lock(m_mutex);
	if((int)m_datas.size() < fd) {
		return;
	}
	m_datas[fd].reset();
}


}
