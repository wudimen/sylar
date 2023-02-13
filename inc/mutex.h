#ifndef _MUTEX_H_
#define _MUTEX_H_

#include <semaphore.h>

namespace sylar {

// 信号量
class Semaphore {
public:
	Semaphore(uint32_t num) {
		sem_init(&m_sem, 0, num);
	}

	~Semaphore() {
		sem_destroy(&m_sem);
	}

	void wait() {
		sem_wait(&m_sem);
	}

	void notify() {
		sem_post(&m_sem);
	}
private:
	sem_t m_sem;
};

// 区域锁模板
template<typename T>
class ScopeLock {
public:
	ScopeLock(T& mutex)
		:m_mutex(mutex) {
		lock();
	}
	~ScopeLock() {
		unlock();
	}
	void lock() {
		if(!m_mutexed) {
			m_mutex.lock();
			m_mutexed = true;
		}
	}
	void unlock() {
		if(m_mutexed) {
			m_mutex.unlock();
			m_mutexed = false;
		}
	}
private:
	T& m_mutex;
	bool m_mutexed = false;
};

// 读锁模板
template<typename T>
class ReadLock {
public:
	ReadLock(T& mutex)
		:m_mutex(mutex) {
		lock();
	}

	~ReadLock() {
		unlock();
	}

	void lock() {
		if(!m_locked) {
			m_mutex.rdlock();
			m_locked = true;
		}
	}

	void unlock() {
		if(m_locked) {
			m_mutex.unlock();
			m_locked = false;
		}
	}
private:
	T& m_mutex;
	bool m_locked = false;
};

// 写锁模板
template<typename T>
class WriteLock {
public:
	WriteLock(T& mutex)
		:m_mutex(mutex) {
		lock();
	}

	~WriteLock() {
		unlock();
	}

	void lock() {
		if(!m_locked) {
			m_mutex.wrlock();
			m_locked = true;
		}
	}

	void unlock() {
		if(m_locked) {
			m_mutex.unlock();
			m_locked = false;
		}
	}
private:
	T& m_mutex;
	bool m_locked = false;
};


class Mutex {
public:
	typedef ScopeLock<Mutex> Lock;

	Mutex() {
		pthread_mutex_init(&m_mutex, nullptr);
	}

	~Mutex() {
		pthread_mutex_destroy(&m_mutex);
	}

	void lock() {
		pthread_mutex_lock(&m_mutex);
	}

	void unlock() {
		pthread_mutex_unlock(&m_mutex);
	}
private:
	pthread_mutex_t m_mutex;
};

class SpinLock {
public:
	typedef ScopeLock<SpinLock> Lock;

	SpinLock() {
//	std::cout << "--- spinmutex init---" << std::endl;
		pthread_spin_init(&m_mutex, 0);
	}

	~SpinLock() {
//	std::cout << "--- spinmutex destroy---" << std::endl;
		pthread_spin_destroy(&m_mutex);
	}

	void lock() {
		pthread_spin_lock(&m_mutex);
	}

	void unlock() {
		pthread_spin_unlock(&m_mutex);
	}
private:
	pthread_spinlock_t m_mutex;
};

class RWMutex {
public:
	typedef ReadLock<RWMutex> RLock;

	typedef WriteLock<RWMutex> WLock;

	RWMutex() {
//	std::cout << "--- rw mutex init---" << std::endl;
		pthread_rwlock_init(&m_mutex, nullptr);
	}
	
	~RWMutex() {
//	std::cout << "--- rw mutex destroy---" << std::endl;
		pthread_rwlock_destroy(&m_mutex);
	}

	void rdlock() {
		pthread_rwlock_rdlock(&m_mutex);
	}

	void wrlock() {
		pthread_rwlock_wrlock(&m_mutex);
	}

	void unlock() {
		pthread_rwlock_unlock(&m_mutex);
	}
private:
	pthread_rwlock_t m_mutex;
};

class NULLSpinLock {
public:
	typedef ScopeLock<NULLSpinLock> Lock;

	NULLSpinLock() {
//	std::cout << "--- spinmutex init---" << std::endl;
	}

	~NULLSpinLock() {
//	std::cout << "--- spinmutex destroy---" << std::endl;
	}

	void lock() {
	}

	void unlock() {
	}
private:
	pthread_spinlock_t m_mutex;
};


class NULLRWMutex {
public:
	typedef ReadLock<NULLRWMutex> RLock;

	typedef WriteLock<NULLRWMutex> WLock;

	NULLRWMutex() {
//	std::cout << "--- rw mutex init---" << std::endl;
	}
	
	~NULLRWMutex() {
//	std::cout << "--- rw mutex destroy---" << std::endl;
	}

	void rdlock() {
	}

	void wrlock() {
	}

	void unlock() {
	}
private:
	pthread_rwlock_t m_mutex;
};

}

#endif
