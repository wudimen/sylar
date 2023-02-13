#include "sylar.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
/*
	文件描述符莫名其妙被关闭了？
*/

static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

void test_fiber() {

	int lfd = socket(AF_INET, SOCK_STREAM, 0);
	SYLAR_ASSERT(lfd > 0);

	sockaddr_in server_addr;
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(10086);
	inet_pton(AF_INET, "192.168.85.130", &server_addr.sin_addr.s_addr);

	int ret = bind(lfd, (const sockaddr*)&server_addr, sizeof(server_addr));
	SYLAR_ASSERT(!ret);
	listen(lfd, 10);

	sylar::IOManager::GetThis()->addEvent(lfd, sylar::IOManager::READ, [](){		
		SYLAR_LOG_DEBUG(g_logger) << "READ actived listen event!";
	});

	sylar::IOManager::GetThis()->addEvent(lfd, sylar::IOManager::WRITE, [](){		
		SYLAR_LOG_DEBUG(g_logger) << "WRITE actived listen event!";
	});
	//sylar::Scheduler s(3, false, "iomanager");
	//s.start();
	//s.stop();
}

void test_1() {
	sylar::IOManager s(1, false, "iomanager");
	s.schedule(&test_fiber);
}

sylar::Timer::ptr t;
void test_2() {
	sylar::IOManager m(1, false, "iomanager");
	t = m.addTimer(3000, [](){
		static int i = 0;
		SYLAR_LOG_INFO(g_logger) << "====test timer";
		if(++i == 3) {
			t->resetTimer(1000, true);
			// t->cancel();
		}
	}, true);
}

int main(void) {
	test_2();

	return 0;
}
