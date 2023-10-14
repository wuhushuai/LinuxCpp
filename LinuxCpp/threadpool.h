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
	bool m_stop;//ֹͣ��
	int m_thread_num;//�߳���
	int m_request_num;//���������
	pthread_t* m_pthreads;//�̳߳�����
	Locker m_locker;//������
	sem m_sem;//�ź���
	std::list<T *> m_workqueue;//�������
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
			//����ֵ���ɹ�����0�����򷵻ش����롣
			delete[] m_pthreads;
			m_pthreads = nullptr;
			throw std::runtime_error("create thread is error\n");
		}
		//�������߳�֮��Ϳ��Խ����뵱ǰ�Ľ��̷��룬�����߳��������̺߳���������̵߳����С��������ʱʧ�ܣ����׳��쳣��
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
	m_locker.lock();//����append����ζ�������������ˣ���ʱ��Ҫ���߳�����������Ϊ�˰�ȫҪ����
	if (m_workqueue.size() >= m_request_num) {
		m_locker.unlock();//�Ǿͽ���
		return false;//���ʧ��
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
		m_sem.wait();//�ɹ���˵���õ�һ���������ִ��
		m_locker.lock();
		if (m_workqueue.empty()) {//˫�ؼ�飬����ʵ��ȡ����ǰ��������Ҫ�ٴμ����������Ƿ�Ϊ��
			m_locker.unlock();//������Ϊ�վͽ���
			continue;
		}
		//ȡ������ͷ��������
		T* request = m_workqueue.front();
		m_workqueue.pop_front();
		m_locker.unlock();//����
		if (!request) {
			continue;
		}
		request->process();
		
	}

}
