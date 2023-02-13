#include "sylar.h"

static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

void test_socket() {
	sylar::IPAddress::ptr addr = sylar::Address::LookupAnyIPAddress("www.baidu.com");
	if(addr) {
		SYLAR_LOG_INFO(g_logger) << "get address:" << addr->toString();
	} else {
		SYLAR_LOG_ERROR(g_logger) << "get address faild";
		return;
	}

	sylar::Socket::ptr sock = sylar::Socket::CreateTCP(addr);
	addr->setPort(80);
	SYLAR_LOG_INFO(g_logger) << "addr=" << addr->toString();
	if(!sock->connect(addr)) {
		SYLAR_LOG_ERROR(g_logger) << "connect " << addr->toString() << "faild";
		return ;
	} else {
		SYLAR_LOG_INFO(g_logger) << "connect " << addr->toString() << "successfully";
	}

	const char buf[] = "GET / HTTP/1.0\r\n\r\n";
	int ret = sock->send(buf, sizeof(buf));
	if(ret <= 0) {
		SYLAR_LOG_ERROR(g_logger) << "send faild ret=" << ret;
		return;
	}

	std::string buff;
	buff.resize(4096);
	ret = sock->recv(&buff[0], buff.size());
	if(ret <= 0) {
		SYLAR_LOG_ERROR(g_logger) << "recv faild ret=" << ret;
		return;
	}

	buff.resize(ret);
	SYLAR_LOG_INFO(g_logger) << buff;


}

void test_2() {
	sylar::IPAddress::ptr addr = sylar::Address::LookupAnyIPAddress("www.baidu.com:80");
	if(addr) {
		SYLAR_LOG_INFO(g_logger) << "get address:" << addr->toString();
	} else {
		SYLAR_LOG_ERROR(g_logger) << "get address faild";
		return;
	}

	sylar::Socket::ptr sock = sylar::Socket::CreateTCP(addr);
	if(!sock->connect(addr)) {
		SYLAR_LOG_ERROR(g_logger) << "connect " << addr->toString() << "faild";
		return ;
	} else {
		SYLAR_LOG_INFO(g_logger) << "connect " << addr->toString() << "successfully";
	}

	uint64_t ts = sylar::GetCurrentUs();
	for(size_t i = 0; i < 10000000000ul; ++i) {
		if(int err = sock->getError()) {
			SYLAR_LOG_INFO(g_logger) << "err=" << err << " errno=" << errno << " errstr=" << strerror(errno);
			break;
		}

		static uint64_t betch = 10000000;
		if(i && (i % betch) == 0) {
			uint64_t ts2 = sylar::GetCurrentUs();
			SYLAR_LOG_INFO(g_logger) << "i=" << i << ": used: " << ((ts2 - ts) * 1.0 / betch) << " us";
			ts = ts2;
		}
	}
}

int main(void) {
	test_2();

	return 0;
}
