#ifndef _MEDIAUSE_H_
#define _MEDIAUSE_H_

/**
* @File name:    MediaUse.h
* @Author:       Li
* @Version:      1.0
* @Date:         2025-03-07
* @Description:  供Cpplayer使用的一些数据类型（AVFifoLoop、AVDataInfo、MediaDataQueue）
**/


#include <queue>
#include <mutex>
#include<condition_variable>




/**
* @Author:       Li
* @Version:      1.0
* @Date:         2025-03-26
* @Description:  命名空间，用于区分
**/
namespace MediaUse {


    /**
    * @Author:       Li
    * @Version:      1.0
    * @Date:         2025-03-26
    * @Description:  一种循环队列
    **/
	template<typename T>
	class AVFifoLoop {
	public:
		AVFifoLoop();
		AVFifoLoop(int capacity);
		~AVFifoLoop();
		bool push(T data);
		void pop();
		bool empty();
		bool full();
		int size();
		void setCapacity(int capacity);
		T& front();
		T& back();
	private:
        T* data;//数据原地址（from alloc）
        int head;//队头下标
        int rear;//队尾下标
        int capacity;//队列容量
        int realCapacity;//队列实际容量
	};



    /**
    * @Author:       Li
    * @Date:         2025-03-26
    * @Version:      1.0
    * @Brief:        默认构造函数
    * @Param:        void
    * @Return:       null
    **/
	template<typename T>
	AVFifoLoop<T>::AVFifoLoop() :data(nullptr), head(0), rear(0), capacity(0), realCapacity(0) {
		
	}

    /**
    * @Author:       Li
    * @Date:         2025-03-26
    * @Version:      1.0
    * @Brief:        带指定队列容量构造函数
    * @Param:        void
    * @Return:       void
    **/
	template<typename T>
	AVFifoLoop<T>::AVFifoLoop(int capacity) :head(0), rear(0), capacity(capacity), realCapacity(capacity + 1) {
		data = new T[capacity + 1];
	}

    /**
    * @Author:       Li
    * @Date:         2025-03-26
    * @Version:      1.0
    * @Brief:        稀构函数
    * @Param:        void
    * @Return:       void
    **/
	template<typename T>
	AVFifoLoop<T>::~AVFifoLoop() {
        if(data){
            delete[] data;
        }
	}

    /**
    * @Author:       Li
    * @Date:         2025-03-26
    * @Version:      1.0
    * @Brief:        向队尾输入一个数据，如果队列已满或其他错误，返回false；成功返回true
    * @Param:        @data     T（需要支持普通拷贝构造）
    * @Return:       bool(success is true)
    **/
	template<typename T>
	bool AVFifoLoop<T>::push(T data) {
		if (this->full()) return false;
		this->data[rear] = data;
		rear = (rear + 1) % realCapacity;
		return true;
	}

    /**
    * @Author:       Li
    * @Date:         2025-03-26
    * @Version:      1.0
    * @Brief:        队头弹出一个数据，但没有返回
    * @Param:        void
    * @Return:       void
    **/
	template<typename T>
	void AVFifoLoop<T>::pop() {
		if (this->empty()) return;
		head = (head + 1) % realCapacity;
	}

    /**
    * @Author:       Li
    * @Date:         2025-03-26
    * @Version:      1.0
    * @Brief:        判断队列是否为空的，为空返回true
    * @Param:        void
    * @Return:       bool
    **/
	template<typename T>
	bool AVFifoLoop<T>::empty() {
		if (head == rear) return true;
		return false;
	}

    /**
    * @Author:       Li
    * @Date:         2025-03-26
    * @Version:      1.0
    * @Brief:        判断队列是否为满的，为满返回true，调用前必需先指定其容量（setCapacity或构造时指定）
    * @Param:        void
    * @Return:       bool
    **/
	template<typename T>
	bool AVFifoLoop<T>::full() {
		if (head == ((rear + 1) % realCapacity)) {
			return true;
		}
		return false;
	}

    /**
    * @Author:       Li
    * @Date:         2025-03-26
    * @Version:      1.0
    * @Brief:        获取队列当前有效元素个数
    * @Param:        void
    * @Return:       int
    **/
	template<typename T>
	int AVFifoLoop<T>::size() {
		if (rear > head) {
			return rear - head;
		}
		else if (rear < head) {
			return realCapacity - (head - rear);
		}
		else {
			return 0;
		}
	}

    /**
    * @Author:       Li
    * @Date:         2025-03-26
    * @Version:      1.0
    * @Brief:        摘要
    * @Param:        @capacity int 重新指定队列容量
    * @Return:       void
    **/
	template<typename T>
	void AVFifoLoop<T>::setCapacity(int capacity) {
		if (data) {
			delete[] data;
		}
		data = new T[capacity + 1];
        realCapacity = capacity + 1;//实际容量多一
        head = 0;
        rear = 0;
	}

    /**
    * @Author:       Li
    * @Date:         2025-03-26
    * @Version:      1.0
    * @Brief:        获取队头元素，调用前必需先指定其容量（setCapacity或构造时指定）
    * @Param:        void
    * @Return:       T& 队头元素的引用
    **/
	template<typename T>
	T& AVFifoLoop<T>::front() {
		return data[head];
	}

    /**
    * @Author:       Li
    * @Date:         2025-03-26
    * @Version:      1.0
    * @Brief:        获取队尾元素，调用前必需先指定其容量（setCapacity或构造时指定）
    * @Param:        void
    * @Return:       T& 队尾元素的引用
    **/
	template<typename T>
	T& AVFifoLoop<T>::back() {
		if (rear - 1 < 0) {
			return data[realCapacity - 1];
		}
		else {
			return data[rear - 1];
		}
	}



    /**
    * @Author:       Li
    * @Version:      1.0
    * @Date:         2025-03-26
    * @Description:  AVDataInfo 储存一帧音视频数据的数据类型（数据地址、pts、大小），并提供删除函数
    **/
	class AVDataInfo {
	public:
		AVDataInfo();
		AVDataInfo(unsigned char* data, int64_t pts, size_t size);
		~AVDataInfo();
        unsigned char* data;//数据地址
        int64_t pts;//帧的pts
        size_t size;//数据大小（自定义）
		void clear();
	};




    /**
    * @Author:       Li
    * @Version:      1.0
    * @Date:         2025-03-26
    * @Description:  MediaDataQueue线程安全的队列，基于std::queue实现，附带条件等待函数
    **/
	template<typename T>
	class MediaDataQueue {
	public:

		MediaDataQueue();
		~MediaDataQueue();

		void push(T data);
		T pop();
		T back();
		void wait();
		bool waitFor(int64_t millisecond);
		void waitOrCondition(const bool* cdt);
		void waitAndCondition(const bool* cdt);
		void notify_all();
		void clear();
		void clearWithDelete();
		bool empty();
		size_t size();

	private:

        std::queue<T> queue;//实际的队列
        std::mutex mutex;//锁
        std::condition_variable cv;//条件变量

	};

    /**
    * @Author:       Li
    * @Date:         2025-03-26
    * @Version:      1.0
    * @Brief:        默认构造函数
    * @Param:        void
    * @Return:       void
    **/
	template<typename T>
	MediaDataQueue<T>::MediaDataQueue() {

	}

    /**
    * @Author:       Li
    * @Date:         2025-03-26
    * @Version:      1.0
    * @Brief:        稀构函数
    * @Param:        void
    * @Return:       void
    **/
	template<typename T>
	MediaDataQueue<T>::~MediaDataQueue() {

	}

    /**
    * @Author:       Li
    * @Date:         2025-03-26
    * @Version:      1.0
    * @Brief:        向队尾输入一个数据
    * @Param:        @data T （需要支持拷贝构造）
    * @Return:       void
    **/
	template<typename T>
	void MediaDataQueue<T>::push(T data) {
		std::lock_guard<std::mutex> lock(mutex);
		queue.push(data);
		cv.notify_one();
	}

    /**
    * @Author:       Li
    * @Date:         2025-03-26
    * @Version:      1.0
    * @Brief:        队头出队，返回队头元素
    * @Param:        void
    * @Return:       T （值返回方式）
    **/
	template<typename T>
	T MediaDataQueue<T>::pop() {
		T data;
		std::lock_guard<std::mutex> lock(mutex);
		if (!queue.empty()) {
			data = queue.front();
			queue.pop();
		}
		return data;
	}

    /**
    * @Author:       Li
    * @Date:         2025-03-26
    * @Version:      1.0
    * @Brief:        返回队尾元素，如果没有则返回默认构造的元素
    * @Param:        void
    * @Return:       T （值返回方式）
    **/
	template<typename T>
	T MediaDataQueue<T>::back() {
		T data;
		std::lock_guard<std::mutex> lock(mutex);
		data = queue.back();
		return data;
	}

    /**
    * @Author:       Li
    * @Date:         2025-03-26
    * @Version:      1.0
    * @Brief:        等待队列不为空
    * @Param:        void
    * @Return:       void
    **/
	template<typename T>
	void MediaDataQueue<T>::wait() {
		std::unique_lock<std::mutex> lock(mutex);
		cv.wait(lock, [this]() {return !(queue.empty()); });
	}

    /**
    * @Author:       Li
    * @Date:         2025-03-26
    * @Version:      1.0
    * @Brief:        等待队列不为空，直到指定的最大等待时间
    * @Param:        @millisecond int64_t 最大等待时间，单位ms
    * @Return:       bool 如果队列不为空则返回true，如果超时则返回false
    **/
	template<typename T>
	bool MediaDataQueue<T>::waitFor(int64_t millisecond) {
		std::unique_lock<std::mutex> lock(mutex);
		return cv.wait_for(lock, std::chrono::milliseconds(millisecond), [this]() {return !(queue.empty()); });
	}

    /**
    * @Author:       Li
    * @Date:         2025-03-26
    * @Version:      1.0
    * @Brief:        等待队列不为空或者输入伴随的条件为true
    * @Param:        @cdt (const bool *) 等待伴随的条件
    * @Return:       void
    **/
	template<typename T>
	void MediaDataQueue<T>::waitOrCondition(const bool* cdt) {
		std::unique_lock<std::mutex> lock(mutex);
		cv.wait(lock, [this, cdt]() {return !(queue.empty()) || (*cdt); });
	}

    /**
    * @Author:       Li
    * @Date:         2025-03-26
    * @Version:      1.0
    * @Brief:        等待队列不为空且输入伴随的条件为true
    * @Param:        @cdt (const bool *) 等待伴随的条件
    * @Return:       void
    **/
	template<typename T>
	void MediaDataQueue<T>::waitAndCondition(const bool* cdt) {
		std::unique_lock<std::mutex> lock(mutex);
		cv.wait(lock, [this, cdt]() {return !(queue.empty()) && (*cdt); });
	}

    /**
    * @Author:       Li
    * @Date:         2025-03-26
    * @Version:      1.0
    * @Brief:        通知所有所有等待该队列的对象
    * @Param:        void
    * @Return:       void
    **/
	template<typename T>
	void MediaDataQueue<T>::notify_all() {
		std::lock_guard<std::mutex> lock(mutex);
		cv.notify_all();
	}

    /**
    * @Author:       Li
    * @Date:         2025-03-26
    * @Version:      1.0
    * @Brief:        清空队列，不对队列元素做任何事
    * @Param:        void
    * @Return:       void
    **/
	template<typename T>
	void MediaDataQueue<T>::clear() {
		std::lock_guard<std::mutex> lock(mutex);
		std::queue<T>().swap(queue);
	}

    /**
    * @Author:       Li
    * @Date:         2025-03-26
    * @Version:      1.0
    * @Brief:        清空队列，并调用每个元素的clear函数，类型T必须实现clear函数
    * @Param:        void
    * @Return:       void
    **/
	template<typename T>
	void MediaDataQueue<T>::clearWithDelete() {
		std::lock_guard<std::mutex> lock(mutex);
		while (!queue.empty()) {
			queue.front().clear();
			queue.pop();
		}
	}

    /**
    * @Author:       Li
    * @Date:         2025-03-26
    * @Version:      1.0
    * @Brief:        判断队列是否为空
    * @Param:        void
    * @Return:       bool 队列为空返回true
    **/
	template<typename T>
	bool MediaDataQueue<T>::empty() {
		std::lock_guard<std::mutex> lock(mutex);
		return queue.empty();
	}

    /**
    * @Author:       Li
    * @Date:         2025-03-26
    * @Version:      1.0
    * @Brief:        返回当前队列所包含的元素个数
    * @Param:        void
    * @Return:       size_t 队列元素个数
    **/
	template<typename T>
	size_t MediaDataQueue<T>::size() {
		std::lock_guard<std::mutex> lock(mutex);
		return queue.size();
	}


};




#endif//_MEDIAUSE_H_
