#include "sylar.h"

sylar::Logger::ptr tt_logger = SYLAR_LOG_NAME("system");

void test() {
	std::vector<sylar::Address::ptr> addrs;
	SYLAR_LOG_INFO(tt_logger) << "begin";
	bool v = sylar::Address::Lookup(addrs, "localhost:3080");
	SYLAR_LOG_INFO(tt_logger) << "end";
	if(!v) {
		SYLAR_LOG_INFO(tt_logger) << "lookup fail";
		return;
	}

	for(size_t i = 0; i < addrs.size(); ++i) {
		SYLAR_LOG_INFO(tt_logger) << i << "-" << addrs[i]->toString();
	}

	auto addr = sylar::Address::LookupAny("localhost:4080");
	if(addr) {
		SYLAR_LOG_INFO(tt_logger) << *addr;
	} else {
		SYLAR_LOG_INFO(tt_logger) << "error";
	}
}

void test_iface() {
	std::multimap<std::string, std::pair<sylar::Address::ptr, uint32_t> > results;

	bool v = sylar::Address::GetInterfaceAddresses(results);
	if(!v) {
		SYLAR_LOG_INFO(tt_logger) << "GetInterfaceAddress faild";
		return;
	}

	for(auto& i : results) {
		SYLAR_LOG_INFO(tt_logger) << i.first << "-" << i.second.first->toString() << i.second.second;
	}
}

void test_ipv4() {
	auto addr = sylar::IPAddress::Create("127.0.0.8");
	if(addr) {
		SYLAR_LOG_INFO(tt_logger) << addr->toString();
	}
}

int main(void) {
	test();
	
	return 0;
}
