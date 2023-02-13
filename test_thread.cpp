#include "sylar.h"

// thread.h test
void test_1_sub() {
	while(1) {
		sleep(1);
		std::cout << "thread test...  name: " << sylar::Thread::GetName() << std::endl;
	}
}
void test_1() {
	std::vector<sylar::Thread::ptr> thrs;
	for(size_t i = 0; i < 3; ++i) {
		thrs.push_back(sylar::Thread::ptr(new sylar::Thread(&test_1_sub, "main_" + std::to_string(i))));
	}
	for(auto& i : thrs) {
		i->join();
	}
}

// mutex.h test
uint32_t nums = 0;
sylar::Mutex mutex;
sylar::RWMutex rwmutex;
sylar::SpinLock spmutex;
void test_2_sub() {
	for(size_t i = 0; i < 1000000; ++i) {
		// sylar::Mutex::Lock lock(mutex);
		// sylar::RWMutex::WLock lock_2(rwmutex);
		sylar::SpinLock::Lock lock_3(spmutex);
		nums++;
	}
}
void test_2() {
	std::vector<sylar::Thread::ptr> thrs;
	for(size_t i = 0; i < 3; ++i){
		sylar::Thread::ptr tmp_thr(new sylar::Thread(&test_2_sub, "test_2_" + std::to_string(i)));
		thrs.push_back(tmp_thr);
	}
	for(auto i : thrs) {
		i->join();
	}
	std::cout << "final result: " << nums << std::endl;
}

// Semaphore test
uint32_t nums_3 = 0;
sylar::Semaphore m_sem(0);
void test_3_sub_1() {
	for(size_t i = 0; i < 20; ++i) {
		nums_3 = 0;
		// m_sem.wait();
		std::cout << "nums value: " << nums_3 << std::endl;
	}
}
void test_3_sub_2() {
	for(size_t i = 0; i < 20; ++i) {
		nums_3  = 1;
		// m_sem.notify();
	}
}
void test_3() {
	std::vector<sylar::Thread::ptr> thrs;
	sylar::Thread::ptr tmp_thr(new sylar::Thread(&test_3_sub_1, "test_2_" + std::to_string(1)));
	thrs.push_back(tmp_thr);
	tmp_thr = sylar::Thread::ptr(new sylar::Thread(&test_3_sub_2, "test_2_" + std::to_string(2)));
	thrs.push_back(tmp_thr);
	for(auto i : thrs) {
		i->join();
	}
}

int main(void) {
	test_2();


	return 0;
}
