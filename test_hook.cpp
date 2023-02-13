/*
	fd_manager.cpp : 加上锁后会出非法指令的错误！！！
*/
#include "sylar.h"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

void test_sleep() {
	sylar::IOManager iom(1);
	iom.schedule([](){
		sleep(2);
		SYLAR_LOG_INFO(g_logger) << "sleep 2 sec";
	});

	iom.schedule([](){
		sleep(3);
		SYLAR_LOG_INFO(g_logger) << "sleep 3 sec";
	});
	SYLAR_LOG_INFO(g_logger) << "test_sleep";
}

int test_socket() {
	int cfd = socket(AF_INET, SOCK_STREAM, 0);

	sockaddr_in s_addr;
	memset(&s_addr, 0, sizeof(s_addr));
	s_addr.sin_family = AF_INET;
	s_addr.sin_port = htons(80);
	inet_pton(AF_INET, "36.152.44.96", &s_addr.sin_addr.s_addr);

	// sylar::IOManager* iom = sylar::IOManager::GetThis();
	// syliom->addEvent(cfd, sylar::IOManager::READ, [](){});
	// syliom->addEvent(cfd, sylar::IOManager::WRITE, [](){});

	int ret = connect(cfd, (sockaddr*)&s_addr, sizeof(sockaddr));
	if(ret == -1) {
		return -1;
	}

	const char send_buf[] = "GET / HTTP/1.1\r\n\r\n";
	ret = send(cfd, send_buf, sizeof(send_buf), 0);
	SYLAR_LOG_DEBUG(g_logger) << "send len: " << ret;

	if(ret <= 0) {
		return -1;
	}

	std::string recv_buf;
	recv_buf.resize(4096);

	ret = recv(cfd, &recv_buf[0], recv_buf.size(), 0);
	SYLAR_LOG_DEBUG(g_logger) << "recv len: " << ret;
	if(ret <= 0) {
		return -1;
	}

	recv_buf.resize(ret);
	SYLAR_LOG_DEBUG(g_logger) << recv_buf;

	return 0;
}

int main(void) {

	sylar::IOManager iom;
	iom.schedule(&test_socket);
	return 0;
}
