#include "address.h"

#include <sstream>
#include <netdb.h>
#include <ifaddrs.h>
#include <stddef.h>
#include <byteswap.h>

#include "log.h"
// #include "endian.h"

namespace sylar {

static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

// 输出类型为T，长度为bits的掩码
template<class T>
static T CreateMask(uint32_t bits) {	// T:uint8_t, bits:3：1-->10000000-->10000-->01111
	return (1 << (sizeof(T) * 8 - bits) - 1);
}


// 计算一个数二进制时有多少'1'
template<class T>
static uint32_t CountBytes(T value) {
	uint32_t result = 0;
	for(; value; ++result) {	// 每一个循环消去一个'1'
		value &= value - 1;
	}
	return result;
}

uint32_t byteswapOnLittleEndian(uint32_t t) {
	return bswap_32(t);
}

uint16_t byteswapOnLittleEndian(uint16_t t) {
	return bswap_16(t);
}

// 通过sockaddr，创建对应的Address并返回
Address::ptr Address::Create(const sockaddr* addr, socklen_t addrlen) {
	if(addr == nullptr) {
		return nullptr;
	}

	Address::ptr result;
	switch(addr->sa_family) {
		case AF_INET:
			result.reset(new IPv4Address(*(const sockaddr_in*)addr));
			break;
		case AF_INET6:
			result.reset(new IPv6Address(*(const sockaddr_in6*)addr));
			break;
		default:
			result.reset(new UnknowAddress(*addr));
			break;
	}
	return result;
}

// 根据host地址(www.sylar.top[:80])，将所有符合条件的Address记录在result中（通过函数getaddrinfo()实现）
bool Address::Lookup(std::vector<Address::ptr>& result, const std::string& host, int family, int type, int protocol) {
	// 初始化addrinfo信息
	addrinfo hints, *results, *next;
	hints.ai_flags = 0;
	hints.ai_family = family;
	hints.ai_socktype = type;
	hints.ai_protocol = protocol;
	hints.ai_addrlen = 0;
	hints.ai_canonname = NULL;
	hints.ai_addr = NULL;
	hints.ai_next = NULL;

	std::string node;				// 地址（[]前面的）
	const char* service = NULL;		// 端口号（[]里的选填内容）

	// 看是否是ipv6的地址（里面有:，会影响后面的判断）
	if(!host.empty() && host[0] == '[') {
		const char* endipv6 = (const char*)memchr(host.c_str() + 1, ']', host.size() - 1);
		if(endipv6) {
			// ---TODO check out of range
			if(*(endipv6 + 1) == ':') {
				service = endipv6 + 2;
			}
			node = host.substr(1, endipv6 - host.c_str() - 1);
		}
	}
	
	// 不是ipv6的话直接查找':'，有的话后面就是端口号，没的话全是地址
	if(node.empty()) {
		service = (const char*)memchr(host.c_str(),  ':', host.size());
		if(service) {
			if(!memchr(service + 1, ':', host.c_str() + host.size() - service - 1)) {
				node = host.substr(0, service - host.c_str());
				++service;
			}
		}
	}

	if(node.empty()) {
		node = host;
	}

	// 分离完地址与端口号，查找相关地址
	int error = getaddrinfo(node.c_str(), service, &hints, &results);
	// 参数一：主机名或者地址串（ipv4/ipv6字符串形式)
	// 参数二：服务名或者端口号
	// 参数三：指向addrinfo结构体的指针（里面包含着我们期待返回的信息类型的限定条件）；
	// 参数四：指向符合条件的信息链表的头节点
	// 返回值：成功：0； 失败：非0的错误号，可用gai_strerror()解读
	if(error) {
		SYLAR_LOG_DEBUG(g_logger) << "Address::Lookup getaddrinfo(" << host << ", " << family << ", " << type << ") err=" << error << "errstr=" << gai_strerror(error);
		return false;
	}

	// 源地址要用于freeaddrinfo(),因此创一个新的指针进行遍历
	next = results;
	while(next) {
		result.push_back(Create(next->ai_addr, (socklen_t)next->ai_addrlen));
		next = next->ai_next;
	}

	freeaddrinfo(results);
	return !result.empty();
}


// 根据域名/主机名取出随机一个IP地址，并转换成Address
Address::ptr Address::LookupAny(const std::string& host, int family, int type, int protocol) {
	std::vector<Address::ptr> result;
	if(Lookup(result, host, family, type, protocol)) {
		return result[0];
	}
	return nullptr;
}


// 根据域名/主机名取出随机一个IP地址，并转换成IPAddress
std::shared_ptr<IPAddress> Address::LookupAnyIPAddress(const std::string& host, int family, int type, int protocol) {
	std::vector<Address::ptr> result;
	if(Lookup(result, host, family, type, protocol)) {
		for(auto& i : result) {
			IPAddress::ptr v = std::dynamic_pointer_cast<IPAddress>(i);
			if(v) {
				return v;
			}
		}
	}
	return nullptr;
}

// 获取本地IP地址，并转换成
// 		<IP地址名称， <ip地址的Address::ptr, 子网掩码长度> >
bool Address::GetInterfaceAddresses(std::multimap<std::string, std::pair<Address::ptr, uint32_t> >&result, int family) {
	struct ifaddrs *next, *results;
	if(getifaddrs(&results) != 0) {	// 获取本地ip地址
		SYLAR_LOG_DEBUG(g_logger) << "Address::GetInterfaceAddresses getifaddrs error: errno=" << errno << " errstr=" << strerror(errno);
		return false;
	}
	
	// 将ifaddrs类型数据转换成Address
	try {
		for(next = results; next; next = next->ifa_next) {
			Address::ptr addr;
			uint32_t prefix_len = ~0u;
			if(family != AF_UNSPEC && family != next->ifa_addr->sa_family) {
				continue;
			}
			switch(next->ifa_addr->sa_family) {
				case AF_INET:
					{
						addr = Create(next->ifa_addr, sizeof(sockaddr_in));
						uint32_t netmask = ((sockaddr_in*)next->ifa_netmask)->sin_addr.s_addr;
						prefix_len = CountBytes(netmask);
					}
					break;
				case AF_INET6:
					{
						addr = Create(next->ifa_addr, sizeof(sockaddr_in6));
						in6_addr& netmask = ((sockaddr_in6*)next->ifa_netmask)->sin6_addr;
						prefix_len = 0;
						for(int i = 0; i < 16; ++i) {
							prefix_len += CountBytes(netmask.s6_addr[i]);
						}
					}
					break;
				default:
					break;
			}
			if(addr) {
				result.insert(std::make_pair(next->ifa_name, std::make_pair(addr, prefix_len)));
			}
		}
	}catch (...) {
		SYLAR_LOG_ERROR(g_logger) << "Address::GetInterfaceAddress exception";
		freeifaddrs(results);
		return false;
	}
	freeifaddrs(results);
	return !result.empty();
}


// 利用上面的GetInterfaceAddress函数，获取名字为iface/类型为family的本地地址，并放入到result中
bool Address::GetInterfaceAddresses(std::vector<std::pair<Address::ptr, uint32_t> >& result, const std::string& iface, int family) {
	if(iface.empty() || iface == "*") {
		if(family == AF_INET || family == AF_UNSPEC) {
			result.push_back(std::make_pair(IPv4Address::ptr(new IPv4Address()), 0u));
		}
		if(family == AF_INET6 || family == AF_UNSPEC) {
			result.push_back(std::make_pair(IPv6Address::ptr(new IPv6Address()), 0u));
		}
		return true;
	}

	std::multimap<std::string, std::pair<Address::ptr, uint32_t> >results;
	if(!GetInterfaceAddresses(results, family)) {
		return false;
	}

	auto it = results.equal_range(iface);
	// equal_range：在multimap中寻找键值等于iface的可多个内容项，开头节点赋给it.first，结尾节点赋给it.second
	for(; it.first != it.second; ++it.first) {
		result.push_back(it.first->second);
	}
	return !result.empty();
}

// 返回类型
int Address::getFamily() const {
	return getAddr()->sa_family;
}


// 利用insert函数，输出Address的地址与端口号
std::string Address::toString() const {
	std::stringstream ss;
	insert(ss);
	return ss.str();
}


// 比较函数，用于使用需要排序的容器
bool Address::operator==(const Address& rhs) const {
	return getAddrLen() ==rhs.getAddrLen() 
		&& memcmp(getAddr(), rhs.getAddr(), getAddrLen()) == 0;
}

bool Address::operator<(const Address& rhs) const {
	socklen_t minlen = std::min(getAddrLen(), rhs.getAddrLen());
	int result = memcmp(getAddr(), rhs.getAddr(), minlen);
	if(result < 0) {
		return true;
	} else if(result > 0) {
		return false;
	} else if(getAddrLen() < rhs.getAddrLen()) {
		return true;
	}
	return false;
}

bool Address::operator!=(const Address& rhs) const {
	return !(*this == rhs);
}


// 使用getaddrinfo函数，寻找address对应的IP地址，设置端口，转换成IPAddress
IPAddress::ptr IPAddress::Create(const char* address, uint16_t port) {
	addrinfo hints, *results;
	hints.ai_flags = AI_NUMERICHOST;
	hints.ai_family = AF_UNSPEC;

	int error = getaddrinfo(address, NULL, &hints, &results);
	if(error) {
		SYLAR_LOG_ERROR(g_logger) << "Address::Create(" << address << " ," << port << ") error=" << error << " errno=" << errno << " errstr=" << strerror(errno);
		return nullptr;
	}

	try {
		IPAddress::ptr result = std::dynamic_pointer_cast<IPAddress>(Address::Create(results->ai_addr, (socklen_t)results->ai_addrlen));
		if(result) {
			result->setPort(port);
		}
		freeaddrinfo(results);
		return result;
	} catch (...) {
		freeaddrinfo(results);
		return nullptr;
	}
	return nullptr;
}


// 使用点分十进制IP地址与端口号初始化IPv4Address
IPv4Address::ptr IPv4Address::Create(const char* address, uint16_t port) {
	IPv4Address::ptr result(new IPv4Address);
	result->m_addr.sin_port = byteswapOnLittleEndian(port);
	int ret = inet_pton(AF_INET, address, &result->m_addr.sin_addr);
	if(ret <= 0) {
		SYLAR_LOG_ERROR(g_logger) << "IPv4Address::Create(" << address << ", " << port << ") ret=" << ret << " errno=" << errno << " errstr=" << strerror(errno);
		return nullptr;
	}
	return result;
}


// 使用sockaddr_in初始化IPv4Address
IPv4Address::IPv4Address(const sockaddr_in& address) {
	m_addr = address;
}


// 使用32位ip地址与16位端口号初始化IPv4Address
IPv4Address::IPv4Address(uint32_t address, uint16_t port) { 
	memset(&m_addr, 0, sizeof(m_addr));
	m_addr.sin_family = AF_INET;
	m_addr.sin_port = byteswapOnLittleEndian(port);
	m_addr.sin_addr.s_addr = byteswapOnLittleEndian(address);
}


// 获取m_addr
const sockaddr* IPv4Address::getAddr() const {
	return (sockaddr*)&m_addr;
}

sockaddr* IPv4Address::getAddr() {
	return (sockaddr*)&m_addr;
}

// 获取m_addr长度
socklen_t IPv4Address::getAddrLen() const {
	return sizeof(m_addr);
}

// 由sockaddr_in中的32位IP地址转换成点分十进制字符串，并输出到os中，并输出端口号到os中
std::ostream& IPv4Address::insert(std::ostream& os) const {
	uint32_t addr = byteswapOnLittleEndian(m_addr.sin_addr.s_addr);
	os << ((addr >> 24) & 0xff) << "."
		<< ((addr >> 16) & 0xff) << "."
		<< ((addr >> 8) & 0xff) << "."
		<< ((addr >> 0) & 0xff);
	os << ":" << byteswapOnLittleEndian(m_addr.sin_port);
	return os;
}


// 输出子网掩码为prefix_len位的广播地址
IPAddress::ptr IPv4Address::broadcastAddress(uint32_t prefix_len) {
	if(prefix_len > 32) {
		return nullptr;
	}

	sockaddr_in baddr(m_addr);
	baddr.sin_addr.s_addr |= byteswapOnLittleEndian(CreateMask<uint32_t>(prefix_len));
	return IPv4Address::ptr(new IPv4Address(baddr));
}

// 输出子网掩码为prefix_len位的广播地址
IPAddress::ptr IPv4Address::networkAddress(uint32_t prefix_len) {
	if(prefix_len > 32) {
		return nullptr;
	}

	sockaddr_in baddr(m_addr);
	baddr.sin_addr.s_addr &= byteswapOnLittleEndian(CreateMask<uint32_t>(prefix_len));
	return IPv4Address::ptr(new IPv4Address(baddr));
}


// 输出位数为prefix_len位的子网掩码
IPAddress::ptr IPv4Address::subnetMask(uint32_t prefix_len) {
	sockaddr_in subnet;
	memset(&subnet, 0, sizeof(subnet));
	subnet.sin_family = AF_INET;
	subnet.sin_addr.s_addr = ~byteswapOnLittleEndian(CreateMask<uint32_t>(prefix_len));
	return IPv4Address::ptr(new IPv4Address(subnet));
}

uint16_t IPv4Address::getPort() const {
	return byteswapOnLittleEndian(m_addr.sin_port);
}

void IPv4Address::setPort(uint16_t v) {
	m_addr.sin_port = byteswapOnLittleEndian(v);
}

// 用字符串形式地址初始化IPv6Address
IPv6Address::ptr IPv6Address::Create(const char* address, uint16_t port) {
	IPv6Address::ptr result(new IPv6Address);
	result->m_addr.sin6_port = byteswapOnLittleEndian(port);
	int ret = inet_pton(AF_INET6, address, &result->m_addr.sin6_addr);
	if(ret <= 0) {
		SYLAR_LOG_ERROR(g_logger) << "IPv6Address::Create(" << address << ", " << port << ") ret=" << ret << " errno=" << errno << " errstr=" << strerror(errno);
		return nullptr;
	}
	return result;
}

// 初始化一个空的IPv6Address
IPv6Address::IPv6Address() {
	memset(&m_addr, 0, sizeof(m_addr));
	m_addr.sin6_family = AF_INET6;
}


// 以字符串形式IPv6地址初始化IPv6Address
IPv6Address::IPv6Address(const sockaddr_in6& address) {
	m_addr = address;
}

// 以数组形式初始化
IPv6Address::IPv6Address(const uint8_t address[16], uint16_t port) {
	memset(&m_addr, 0, sizeof(m_addr));
	m_addr.sin6_family = AF_INET6;
	m_addr.sin6_port = byteswapOnLittleEndian(port);
	memcpy(&m_addr.sin6_addr.s6_addr, address, 16);
}


// 输出地址
const sockaddr* IPv6Address::getAddr() const {
	return (sockaddr*)&m_addr;
}

sockaddr* IPv6Address::getAddr() {
	return (sockaddr*)&m_addr;
}

// 输出地址长度
socklen_t IPv6Address::getAddrLen() const {
	return sizeof(m_addr);
}


// 将IPv6地址以标准形式与端口号输出到os中
std::ostream& IPv6Address::insert(std::ostream& os) const {
	os << "[";
	uint16_t* addr = (uint16_t*)m_addr.sin6_addr.s6_addr;
	bool used_zeros = false;
	for(size_t i = 0; i < 8; ++i) {
		if(addr[i] == 0 && !used_zeros) {
			continue;
		}
		if(i && addr[i - 1] == 0 && !used_zeros) {
			os << ":";
			used_zeros = true;
		}
		if(i) {
			os << ":";
		}
		os << std::hex << (int)byteswapOnLittleEndian(m_addr.sin6_port) << std::dec;
	}
	if(!used_zeros && addr[7] == 0) {
		os << "::";
	}

	os << "]:" << byteswapOnLittleEndian(m_addr.sin6_port);
	return os;
}


// 输出以prefix_len为长度的广播地址形成的IPAddress
IPAddress::ptr IPv6Address::broadcastAddress(uint32_t prefix_len) {
	sockaddr_in6 baddr(m_addr);
	baddr.sin6_addr.s6_addr[prefix_len / 8] |= CreateMask<uint64_t>(prefix_len % 8);
	for(int i = prefix_len / 8 + 1; i < 16; ++i) {
		baddr.sin6_addr.s6_addr[i] = 0xff;
	}
	return IPv6Address::ptr(new IPv6Address(baddr));
}


// 输出以prefix_len为长度的网络地址形成的IPAddress
IPAddress::ptr IPv6Address::networkAddress(uint32_t prefix_len) {
	sockaddr_in6 baddr(m_addr);
	baddr.sin6_addr.s6_addr[prefix_len / 8] &= CreateMask<uint64_t>(prefix_len % 8);
	for(int i = prefix_len / 8 + 1; i < 16; ++i) {
		baddr.sin6_addr.s6_addr[i] = 0x00;
	}
	return IPv6Address::ptr(new IPv6Address(baddr));
}


// 输出以prefix_len为长度的子网掩码形成的IPAddress
IPAddress::ptr IPv6Address::subnetMask(uint32_t prefix_len) {
	sockaddr_in6 subnet;
	memset(&subnet, 0, sizeof(subnet));
	subnet.sin6_family = AF_INET6;
	subnet.sin6_addr.s6_addr[prefix_len / 8] = ~CreateMask<uint8_t>(prefix_len % 8);

	for(size_t i = 0; i < prefix_len; ++i) {
		subnet.sin6_addr.s6_addr[i] = 0xff;
	}
	return IPv6Address::ptr(new IPv6Address(subnet));
}

// 获取端口号
uint16_t IPv6Address::getPort() const {
	return byteswapOnLittleEndian(m_addr.sin6_port);
}

void IPv6Address::setPort(uint16_t v) {
	m_addr.sin6_port = byteswapOnLittleEndian(v);
}

static const size_t MAX_PATH_LEN = sizeof(((sockaddr_un*)0)->sun_path) - 1;

UnixAddress::UnixAddress() {
	memset(&m_addr, 0, sizeof(m_addr));
	m_addr.sun_family = AF_UNIX;
	m_length = offsetof(sockaddr_un, sun_path) + MAX_PATH_LEN;
}


UnixAddress::UnixAddress(const std::string& path) {
	memset(&m_addr, 0, sizeof(m_addr));
	m_addr.sun_family = AF_UNIX;
	m_length = path.size() + 1;

	if(!path.empty() && path[0] == '\0') {
		--m_length;
	}

	if(m_length > sizeof(m_addr.sun_path)) {
		throw std::logic_error("path too long");
	}

	memcpy(m_addr.sun_path, path.c_str(), m_length);
	m_length += offsetof(sockaddr_un, sun_path);
}

const sockaddr* UnixAddress::getAddr() const {
	return (sockaddr*)&m_addr;
}

sockaddr* UnixAddress::getAddr() {
	return (sockaddr*)&m_addr;
}

socklen_t UnixAddress::getAddrLen() const {
	return m_length;
}

void UnixAddress::setAddrLen(uint32_t v) {
	m_length = v;
}

std::string UnixAddress::getPath() const {
	std::stringstream ss;
	if(m_length > offsetof(sockaddr_un, sun_path) && m_addr.sun_path[0] == '\0') {
		ss << "\\0" << std::string(m_addr.sun_path + 1, m_length - offsetof(sockaddr_un, sun_path) - 1);
	} else {
		ss << m_addr.sun_path;
	}

	return ss.str();
}

std::ostream& UnixAddress::insert(std::ostream& os) const {
	if(m_length > offsetof(sockaddr_un, sun_path) && m_addr.sun_path[0] == '\0') {
		os << "\\0" << std::string(m_addr.sun_path + 1, m_length - offsetof(sockaddr_un, sun_path) - 1);
	} else {
		os << m_addr.sun_path;
	}
	return os;
}

UnknowAddress::UnknowAddress(int family) {
	memset(&m_addr, 0, sizeof(m_addr));
	m_addr.sa_family = family;
}

UnknowAddress::UnknowAddress(const sockaddr& addr) {
	m_addr = addr;
}

const sockaddr* UnknowAddress::getAddr() const {
	return (sockaddr*)&m_addr;
}

sockaddr* UnknowAddress::getAddr() {
	return (sockaddr*)&m_addr;
}

socklen_t UnknowAddress::getAddrLen() const {
	return sizeof(m_addr);
}

std::ostream& UnknowAddress::insert(std::ostream& os) const {
	os << "[UnknowAddress family=" << m_addr.sa_family << "]";
	return os;
}

std::ostream& operator<<(std::ostream& os, const Address& addr) {
	return addr.insert(os);
}


}


