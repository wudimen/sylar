#ifndef _SOCKET_H_
#define _SOCKET_H_

#include <netinet/tcp.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <unistd.h>
#include <memory>

#include "address.h"
// #include "noncopyable.h"

namespace sylar {

class Socket : public std::enable_shared_from_this<Socket>/*, Noncopyable*/ {
public:
	typedef std::shared_ptr<Socket> ptr;
	typedef std::weak_ptr<Socket> weak_ptr;

	enum Type {
		TCP = SOCK_STREAM,
		UDP = SOCK_DGRAM,
	};

	enum Family {
		IPv4 = AF_INET,
		IPv6 = AF_INET6,
		UNIX = AF_UNIX,
	};

	// 静态方法：根据地址信息创建对应Socket
	static Socket::ptr CreateTCP(sylar::Address::ptr address);

	static Socket::ptr CreateUDP(sylar::Address::ptr address);

	static Socket::ptr CreateTCPSocket();

	static Socket::ptr CreateUDPSocket();

	static Socket::ptr CreateTCPSocket6();

	static Socket::ptr CreateUDPSocket6();

	static Socket::ptr CreateUnixTCPSocket();

	static Socket::ptr CreateUnixUDPSocket();

	Socket(int family, int type, int protocol = 0);

	virtual ~Socket();

	// timeout相关
	int64_t getSendTimeout();

	int64_t getRecvTimeout();

	void setSendTimeout(int64_t v);

	void setRecvTimeout(int64_t v);

	// 设置socket相关
	bool getOption(int level, int option, void* result, socklen_t* len);

	template<class T>
	bool getOption(int level, int option, T& result) {
		socklen_t length = sizeof(T);
		return getOption(level, option, &result, &length);
	}

	bool setOption(int level, int option, const void* result, socklen_t len);

	template<class T>
	bool setOption(int level, int option, const T& value) {
		return setOption(level, option, &value, sizeof(T));
	}

	// 连接相关
	virtual Socket::ptr accept();

	virtual bool bind(const Address::ptr addr);

	virtual bool connect(const Address::ptr addr, uint64_t timeout_ms = -1);

	virtual bool reconnect(uint64_t timeout_ms = -1);

	virtual bool listen(int backlog = SOMAXCONN);

	virtual bool close();

	// 信息发送/接受相关
	virtual int send(const void* buffer, size_t length, int flags = 0);

	virtual int send(const iovec* buffers, size_t length, int flags = 0);

	virtual int sendTo(const void* buffer, size_t length, const Address::ptr to, int flags = 0);

	virtual int sendTo(const iovec* buffers, size_t length, const Address::ptr to, int flags = 0);

	virtual int recv(void* buffer, size_t length, int flags = 0);

	virtual int recv(iovec* buffers, size_t length, int flags = 0);

	virtual int recvFrom(void* buffer, size_t length, Address::ptr from, int flags = 0);

	virtual int recvFrom(iovec* buffes, size_t length, Address::ptr from, int flags = 0);

	// 获取远端地址
	Address::ptr getRemoteAddress();

	// 获取本地地址
	Address::ptr getLocalAddress();

	// 获取协议族
	int getFamily() const { return m_family;}

	// 获取类型
	int getType() const { return m_type;}

	// 获取协议
	int getProtocol() const { return m_protocol;}

	// 是否连接
	bool isConnected() const { return m_isConnected;}

	// 是否有效
	bool isVaild() const { return m_sock != -1;}

	// 返回Socket错误
	int getError();

	// 输出信息到os流中
	virtual std::ostream& dump(std::ostream& os) const;

	// 输出所有信息
	virtual std::string toString() const;

	// 返回socket句柄
	int getSocket() const { return m_sock;}

	// 取消读
	bool cancelRead();

	// 取消写
	bool cancelWrite();

	// 取消Accept
	bool cancelAccept();

	// 取消所有
	bool cancelAll();

protected:
	// 初始化socket
	void initSock();

	// 新建一个新的socket
	void newSock();

	// socket初始化过程
	virtual bool init(int sock);

private:
	int m_sock;			// socket句柄
	int m_family;		// 协议族
	int m_type;			// 类型
	int m_protocol;		// 协议类型	
	bool m_isConnected;	// 是否连接
	Address::ptr m_localAddress;	// 本地地址
	Address::ptr m_remoteAddress;	// 远端地址
};

}

#endif
