#pragma once
#include <pthread.h>
#include <exception>
#include <ctime>
#include <stdexcept>
#include <semaphore.h>

class Locker {
public:
	Locker() {
		if ((pthread_mutex_init(&m_mutex, NULL)) != 0) {
			throw std::exception();
		}
	}
	~Locker() {
		pthread_mutex_destroy(&m_mutex);

	}
	bool lock() {
		return pthread_mutex_lock(&m_mutex) == 0;
	}
	bool unlock() {
		return pthread_mutex_unlock(&m_mutex) == 0;
	}

	pthread_mutex_t* get() {
		return &m_mutex;
	}

private:
	pthread_mutex_t m_mutex;
};


//����������
class condtion {
public:
	condtion() {

		if (pthread_cond_init(&m_cond, NULL) != 0) {
			throw std::exception();
		}

	}
	~condtion() {

		pthread_cond_destroy(&m_cond);

	}
	bool wait(pthread_mutex_t* mutex) {

		int ret = pthread_cond_wait(&m_cond, mutex);
		return ret == 0;

	}
	//��Ϊ��λ�ĳ�ʱ
	int timedwait(pthread_mutex_t* mutex, int seconds) {
		struct timespec ts;
		clock_gettime(CLOCK_REALTIME, &ts);
		ts.tv_sec += seconds;
		//�������Ȼ�ȡ��ǰʱ�䣬Ȼ�󽫳�ʱʱ����ӵ���ǰʱ��
		int ret = pthread_cond_timedwait(&m_cond, mutex, &ts);
		if (ret != 0 && ret != ETIMEDOUT) {
			throw std::runtime_error("Failed to wait on condition variable with timeout\n");
		}
		return ret;
	}
	//signal()��������һ���ȴ������������ϵ��߳�
	void signal() {
		int ret = pthread_cond_signal(&m_cond);
		if (ret != 0) {
			throw std::runtime_error("Failed to signal condition variable\n");
		}
	}
	//broadcast()�����������еȴ������������ϵ��̡߳�
	void broadcast() {
		int ret = pthread_cond_broadcast(&m_cond);
		if (ret != 0) {
			throw std::runtime_error("Failed to broadcast on condition variable\n");
		}
	}
private:
	pthread_cond_t m_cond;
};

// �ź�����
class sem {
public:
	sem() {
		if (sem_init(&m_sem, 0, 0) != 0) {
			throw std::exception();
		}
	}
	sem(int num) {
		if (sem_init(&m_sem, 0, num) != 0) {
			throw std::exception();
		}
	}
	~sem() {
		sem_destroy(&m_sem);
	}
	// �ȴ��ź���
	bool wait() {
		return sem_wait(&m_sem) == 0;
	}
	// �����ź���
	bool post() {
		return sem_post(&m_sem) == 0;
	}
private:
	sem_t m_sem;
};
