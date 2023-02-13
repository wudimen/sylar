#ifndef _FD_MANAGER_H_
#define _FD_MANAGER_H_

#include <memory>
#include <vector>

#include "singleton.h"
#include "mutex.h"

namespace sylar {

class FdCtx : public std::enable_shared_from_this<FdCtx> {
public:
	typedef std::shared_ptr<FdCtx> ptr;

	FdCtx(int fd);
	~FdCtx();

	bool isInit() const { return m_isInit;}
	bool isClose() const { return m_isClose;}
	bool isSocket() const { return m_isSocket;}

	bool getSysnonblock() const { return m_sysNonblock;}
	void setSysnonblock(bool v) { m_sysNonblock = v;}

	bool getUsernonblock() const { return m_userNonblock;}
	void setUsernonblock(bool v) {m_userNonblock = v;}

	uint64_t getTimeout(int type);
	void setTimeout(int type, uint64_t timeout);
private:
	bool init();
private:
	int m_fd;
	bool m_sysNonblock: 1;
	bool m_userNonblock: 1;
	bool m_isSocket: 1;
	bool m_isInit: 1;
	bool m_isClose: 1;
	uint64_t m_recvTimeout = -1;
	uint64_t m_sendTimeout = -1;
};

class FdManager {
public:
	typedef std::shared_ptr<FdManager> ptr;
	typedef RWMutex RWMutexType;
	
	FdManager();

	FdCtx::ptr get(int fd, bool auto_create = false);

	void del(int fd);
private:
	std::vector<FdCtx::ptr> m_datas;
	RWMutexType m_mutex;
};

typedef Singleton<FdManager> FdMgr;

}

#endif
