#include "socket.h"

#include <limits.h>

#include "log.h"
#include "iomanager.h"
#include "fd_manager.h"
#include "macro.h"
#include "hook.h"

namespace sylar {

#define SYLAR_LIKELY(x) (x)
#define SYLAR_UNLIKELY(x) (x)

static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

// 静态方法：根据地址信息创建对应Socket
Socket::ptr Socket::CreateTCP(sylar::Address::ptr address){
	Socket::ptr sock(new Socket(address->getFamily(), TCP, 0));
	return sock;
}

Socket::ptr Socket::CreateUDP(sylar::Address::ptr address){
	Socket::ptr sock(new Socket(address->getFamily(), UDP, 0));
	sock->newSock();
	sock->m_isConnected = true;
	return sock;
}

Socket::ptr Socket::CreateTCPSocket(){
	Socket::ptr sock(new Socket(IPv4, TCP, 0));
	return sock;
}

Socket::ptr Socket::CreateUDPSocket(){
	Socket::ptr sock(new Socket(IPv4, UDP, 0));
	sock->newSock();
	sock->m_isConnected = true;
	return sock;
}

Socket::ptr Socket::CreateTCPSocket6(){
	Socket::ptr sock(new Socket(IPv6, TCP, 0));
	return sock;
}

Socket::ptr Socket::CreateUDPSocket6(){
	Socket::ptr sock(new Socket(IPv6, UDP, 0));
	sock->newSock();
	sock->m_isConnected = true;
	return sock;
}

Socket::ptr Socket::CreateUnixTCPSocket(){
	Socket::ptr sock(new Socket(UNIX, TCP, 0));
	return sock;
}

Socket::ptr Socket::CreateUnixUDPSocket(){
	Socket::ptr sock(new Socket(UNIX, UDP, 0));
	return sock;
}

Socket::Socket(int family, int type, int protocol)
	:m_sock(-1)
	,m_family(family)
	,m_type(type)
	,m_protocol(protocol)
	,m_isConnected(false) {
}

Socket::~Socket(){
	close();
}

// timeout相关
int64_t Socket::getSendTimeout(){
	FdCtx::ptr ctx = FdMgr::GetInstance()->get(m_sock);
	if(ctx) {
		return ctx->getTimeout(SO_SNDTIMEO);
	}
	return -1;
}

int64_t Socket::getRecvTimeout(){
	FdCtx::ptr ctx = FdMgr::GetInstance()->get(m_sock);
	if(ctx) {
		return ctx->getTimeout(SO_RCVTIMEO);
	}
	return -1;
}

void Socket::setSendTimeout(int64_t v){
	struct timeval tv{int(v / 1000), int(v % 1000 * 1000)};
	setOption(SOL_SOCKET, SO_SNDTIMEO, tv);
}

void Socket::setRecvTimeout(int64_t v){
	struct timeval tv{int(v / 1000), int(v % 1000 * 1000)};
	setOption(SOL_SOCKET, SO_RCVTIMEO, tv);
}

// 设置socket相关
bool Socket::getOption(int level, int option, void* result, socklen_t* len){
	int ret = getsockopt(m_sock, level, option, result, (socklen_t*)len);
	if(ret) {
		SYLAR_LOG_DEBUG(g_logger) << "getOption sock=" << m_sock
			<< " level=" << level << " option=" << option
			<< " errno=" << errno << " errstr=" << strerror(errno);
		return false;
	}
	return true;
}

bool Socket::setOption(int level, int option, const void* result, socklen_t len){
	if(setsockopt(m_sock, level, option, result, (socklen_t)len)) {
		SYLAR_LOG_DEBUG(g_logger) << "setOption sock=" << m_sock
			<< " level=" << level << " option=" << option
			<< " errno=" << errno << " errstr=" << strerror(errno);
		return false;
	}
	return true;
}


// 连接相关
Socket::ptr Socket::accept(){
	Socket::ptr sock(new Socket(m_family, m_type, m_protocol));
	int newsock = ::accept(m_sock, nullptr, nullptr);
	if(newsock == -1) {
		SYLAR_LOG_DEBUG(g_logger) << "accept(" << m_sock << ") errno="
			<< errno << " errstr=" << strerror(errno);
		return nullptr;
	}
	if(sock->init(newsock)) {
		return sock;
	}
	return nullptr;
}

bool Socket::bind(const Address::ptr addr){
	if(!isVaild()) {
		newSock();
		if(SYLAR_UNLIKELY(!isVaild())) {
			return false;
		}
	}

	if(SYLAR_UNLIKELY(addr->getFamily() != m_family)) {
		SYLAR_LOG_ERROR(g_logger) << "bind sock.family=" << m_family
			<< " addr.family=" << addr->getFamily()
			<< " not equal, addr=" << addr->toString();
		return false;
	}

	UnixAddress::ptr uaddr = std::dynamic_pointer_cast<UnixAddress>(addr);
	if(uaddr) {
		Socket::ptr sock = Socket::CreateUnixTCPSocket();
		if(sock->connect(uaddr)) {
			return false;
		} else {
			// sylar::FSUtil::Unlink(uaddr->getPath(), true);
		}
	}

	if(::bind(m_sock, addr->getAddr(), addr->getAddrLen())) {
		SYLAR_LOG_ERROR(g_logger) << "bind error errno=" << errno
			<< " errstr=" << strerror(errno);
		return false;
	}

	getLocalAddress();
	return true;
}

bool Socket::connect(const Address::ptr addr, uint64_t timeout_ms){
	m_remoteAddress = addr;

	if(!isVaild()) {
		newSock();
		if(SYLAR_UNLIKELY(!isVaild())) {
		std::cout << "----false_1\n";
			return false;
		}
	}

	if(SYLAR_UNLIKELY(addr->getFamily() != m_family)) {
		SYLAR_LOG_ERROR(g_logger) << "connect sock.family=" << m_family
			<< " addr.family=" << addr->getFamily() << " not equal, addr=" 
			<< addr->toString();
		std::cout << "----false_2\n";
		return false;
	}


	if(timeout_ms == (uint64_t)-1) {
		if(::connect(m_sock, addr->getAddr(), addr->getAddrLen())) {
			SYLAR_LOG_ERROR(g_logger) << "sock=" << m_sock
				<< " connect(" << addr->toString()
				<< ") errno=" << errno << "errstr=" << strerror(errno);
			close();
		std::cout << "----false_3\n";
			return false;
		}
	} else {
		if(::connect_with_timeout(m_sock, addr->getAddr(), addr->getAddrLen(), timeout_ms)) {
			SYLAR_LOG_ERROR(g_logger) << " sock=" << m_sock << " connect("
				<<addr->toString() << ") timeout=" << timeout_ms << " error errno=" << errno << " errstr=" << strerror(errno);
			close();
		std::cout << "----false_4\n";
			return false;
		}
	}
	m_isConnected = true;
	getRemoteAddress();
	getLocalAddress();
	return true;
}

bool Socket::reconnect(uint64_t timeout_ms){
	if(!m_remoteAddress) {
		SYLAR_LOG_ERROR(g_logger) << "reconnect m_remoteAdderss is null";
		return false;
	}
	m_localAddress.reset();
	return connect(m_remoteAddress, timeout_ms);
}

bool Socket::listen(int backlog){
	if(!isVaild()) {
		SYLAR_LOG_ERROR(g_logger) << "listen errro errno=" << errno
			<< " errstr=" << strerror(errno);
		return false;
	}
	if(::listen(m_sock, backlog)) {
		SYLAR_LOG_ERROR(g_logger) << "listen error errno=" << errno
			<< " errstr=" << strerror(errno);
		return false;
	}
	return true;
}

bool Socket::close(){
	if(!m_isConnected && m_sock == -1) {
		return true;
	}
	m_isConnected = false;
	if(m_sock != -1) {
		::close(m_sock);
		m_sock = -1;
		return true;
	}
	return false;
}

// 信息发送/接受相关
int Socket::send(const void* buffer, size_t length, int flags){
	if(isConnected()) {
		return ::send(m_sock, buffer, length, flags);
	}
	return-1;
}

int Socket::send(const iovec* buffers, size_t length, int flags){
	if(isConnected()) {
		msghdr msg;
		memset(&msg, 0, sizeof(msg));
		msg.msg_iov = (iovec*)buffers;
		msg.msg_iovlen = length;
		return ::sendmsg(m_sock, &msg, flags);
	}
	return -1;
}

int Socket::sendTo(const void* buffer, size_t length, const Address::ptr to, int flags){
	if(isConnected()) {
		return ::sendto(m_sock, buffer, length, flags, to->getAddr(), to->getAddrLen());
	}
	return -1;
}

int Socket::sendTo(const iovec* buffers, size_t length, const Address::ptr to, int flags){
	if(isConnected()) {
		msghdr msg;
		memset(&msg, 0, sizeof(msg));
		msg.msg_iov = (iovec*)buffers;
		msg.msg_iovlen = length;
		msg.msg_name = to->getAddr();
		msg.msg_namelen = to->getAddrLen();
		return ::sendmsg(m_sock, &msg, flags);
	}
	return -1;
}

int Socket::recv(void* buffer, size_t length, int flags){
	if(isConnected()) {
		return ::recv(m_sock, buffer, length, flags);
	}
	return -1;
}

int Socket::recv(iovec* buffers, size_t length, int flags){
	if(isConnected()) {
		msghdr msg;
		memset(&msg, 0, sizeof(msg));
		msg.msg_iov = (iovec*)buffers;
		msg.msg_iovlen = length;
		return ::recvmsg(m_sock, &msg, flags);
	}
	return -1;
}

int Socket::recvFrom(void* buffer, size_t length, Address::ptr from, int flags){
	if(isConnected()) {
		socklen_t len = from->getAddrLen();
		return ::recvfrom(m_sock, buffer, length, flags, from->getAddr(), &len);
	}
	return -1;
}

int Socket::recvFrom(iovec* buffers, size_t length, Address::ptr from, int flags){
	if(isConnected()) {
		msghdr msg;
		memset(&msg, 0, sizeof(msg));
		msg.msg_iov = (iovec*)buffers;
		msg.msg_iovlen = length;
		msg.msg_name = from->getAddr();
		msg.msg_namelen = from->getAddrLen();
		return recvmsg(m_sock, &msg, flags);
	}
	return -1;
}

// 获取远端地址
Address::ptr Socket::getRemoteAddress(){
	if(m_remoteAddress) {
		return m_remoteAddress;
	}

	Address::ptr result;
	switch(m_family) {
		case AF_INET:
			result.reset(new IPv4Address());
			break;
		case AF_INET6:
			result.reset(new IPv6Address());
			break;
		case AF_UNIX:
			result.reset(new UnixAddress());
			break;
		default:
			result.reset(new UnknowAddress(m_family));
			break;
	}

	socklen_t addrlen = result->getAddrLen();
	if(getpeername(m_sock, result->getAddr(), &addrlen)) {
		return Address::ptr(new UnknowAddress(m_family));
	}
	if(m_family == AF_UNIX) {
		UnixAddress::ptr addr = std::dynamic_pointer_cast<UnixAddress>(result);
		addr->setAddrLen(addrlen);
	}
	m_remoteAddress = result;
	return m_remoteAddress;
}

// 获取本地地址
Address::ptr Socket::getLocalAddress(){
	if(m_localAddress) {
		return m_localAddress;
	}

	Address::ptr result;
	switch(m_family) {
		case AF_INET:
			result.reset(new IPv4Address());
			break;
		case AF_INET6:
			result.reset(new IPv6Address());
			break;
		case AF_UNIX:
			result.reset(new UnixAddress());
			break;
		default:
			result.reset(new UnknowAddress(m_family));
			break;
	}

	socklen_t addrlen = result->getAddrLen();
	if(getsockname(m_sock, result->getAddr(), &addrlen)) {
		SYLAR_LOG_ERROR(g_logger) << "getsockname error sock=" << m_sock
			<< " errno=" << errno << " errstr=" << strerror(errno);
		return Address::ptr(new UnknowAddress(m_family));
	}
	if(m_family == AF_UNIX) {
		UnixAddress::ptr addr = std::dynamic_pointer_cast<UnixAddress>(result);
		addr->setAddrLen(addrlen);
	}
	m_localAddress = result;
	return m_localAddress;
}

// 返回Socket错误
int Socket::getError(){
	int error = 0;
	socklen_t len = sizeof(error);
	if(!getOption(SOL_SOCKET, SO_ERROR, &error, &len)) {
		error = errno;
	}
	return error;
}

// 输出信息到os流中
std::ostream& Socket::dump(std::ostream& os) const{
	os << "[Socket sock=" << m_sock
	   << " is_connected=" << m_isConnected
	   << " family=" << m_family
	   << " type=" << m_type
	   << " protocol=" << m_protocol;
	if(m_localAddress) {
		os << " localAddress=" << m_localAddress->toString();
	}
	if(m_remoteAddress) {
		os << " remoteAddress=" << m_remoteAddress->toString();
	}
	os << "]";
	return os;
}

// 输出所有信息
std::string Socket::toString() const{
	std::stringstream ss;
	dump(ss);
	return ss.str();
}

// 取消读
bool Socket::cancelRead(){
	return IOManager::GetThis()->cancelEvent(m_sock, sylar::IOManager::READ);
}

// 取消写
bool Socket::cancelWrite(){
	return IOManager::GetThis()->cancelEvent(m_sock, sylar::IOManager::WRITE);
}

// 取消Accept
bool Socket::cancelAccept(){
	return IOManager::GetThis()->cancelEvent(m_sock, sylar::IOManager::READ);
}

// 取消所有
bool Socket::cancelAll(){
	return IOManager::GetThis()->cancelAll(m_sock);
}

// 初始化socket
void Socket::initSock(){
	int val = 1;
	setOption(SOL_SOCKET, SO_REUSEADDR, val);
	if(m_type == SOCK_STREAM) {
		setOption(IPPROTO_TCP, TCP_NODELAY, val);
	}
}

// 新建一个新的socket
void Socket::newSock() {
	m_sock = socket(m_family, m_type, m_protocol);
	if(SYLAR_LIKELY(m_sock != -1)) {
		initSock();
	} else {
		SYLAR_LOG_ERROR(g_logger) << "socket(" << m_family
			<< ", " << m_type << ", " << m_protocol << ") errno=" 
			<< errno << " errstr=" << strerror(errno);
	}
}

// socket初始化过程
bool Socket::init(int sock) {
	FdCtx::ptr ctx = FdMgr::GetInstance()->get(sock);
	if(ctx && ctx->isSocket() && !ctx->isClose()) {
		m_sock = sock;
		m_isConnected = true;
		initSock();
		getLocalAddress();
		getRemoteAddress();
		return true;
	}
	return false;
}

}
