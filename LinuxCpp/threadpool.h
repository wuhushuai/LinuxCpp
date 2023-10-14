#pragma once
#include "locker.h"
#include <list>
using namespace std;

template <typename T>
class threadpool {
public:
	threadpool(int num = 8,int requestnum = 1000 ) ;
	~threadpool();
	bool append(T* request);
private:
	static void* worker(void* arg);
	void run();
	
private:
	bool m_stop;//停止符
	int m_thread_num;//线程数
	int m_request_num;//最大请求数
	pthread_t* m_pthreads;//线程池数组
	Locker m_locker;//互斥锁
	sem m_sem;//信号量
	std::list<T *> m_workqueue;//任务队列
};

template<typename T>
threadpool<T>::threadpool(int num, int requestnum) : m_thread_num(num), m_request_num(requestnum), m_stop  (false), m_pthreads  (NULL)
{
	if (m_thread_num <= 0 || m_request_num <= 0) {
		throw std::runtime_error("Failed to thread num or requestnum\n");
	}
	m_pthreads = new pthread_t[m_thread_num];
	if (!m_pthreads) {
		throw std::exception();
	}
	for (int i = 0; i < m_thread_num; i++)
	{
		printf("create the %dth thread\n", i);
		if ((pthread_create(m_pthreads + i, NULL, worker, this)) != 0) {
			//返回值：成功返回0，否则返回错误码。
			delete[] m_pthreads;
			m_pthreads = nullptr;
			throw std::runtime_error("create thread is error\n");
		}
		//创建完线程之后就可以将其与当前的进程分离，避免线程阻塞主线程和其他相关线程的运行。如果创建时失败，则抛出异常。
		if (pthread_detach(m_pthreads[i])) {
			delete[] m_pthreads;
			m_pthreads = nullptr;
			throw std::exception();
		}
	}
}

template<typename T>
threadpool<T>::~threadpool()
{
	m_stop = true;
	delete[] m_pthreads;
	m_pthreads = nullptr;
}

template<typename T>
bool threadpool<T>::append(T* request)
{
	m_locker.lock();//触发append就意味着有新请求来了，此时需要有线程来处理，所以为了安全要上锁
	if (m_workqueue.size() >= m_request_num) {
		m_locker.unlock();//是就解锁
		return false;//添加失败
	}
	m_workqueue.push_back(request);

	m_locker.unlock();

	m_sem.post();
	return true;

}

template<typename T>
void* threadpool<T>::worker(void* arg)
{
	threadpool* Qthis = (threadpool*)arg;
	Qthis->run();
	return Qthis;
}

template<typename T>
void threadpool<T>::run()
{
	while (!m_stop) {
		m_sem.wait();//成功后说明得到一条任务可以执行
		m_locker.lock();
		if (m_workqueue.empty()) {//双重检查，但在实际取任务前，还是需要再次检查任务队列是否为空
			m_locker.unlock();//若队列为空就解锁
			continue;
		}
		//取出队列头部的请求
		T* request = m_workqueue.front();
		m_workqueue.pop_front();
		m_locker.unlock();//解锁
		if (!request) {
			continue;
		}
		request->process();
		
	}

}
