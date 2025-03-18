#ifndef _MEDIAUSE_H_
#define _MEDIAUSE_H_

#include <queue>
#include <mutex>
#include<condition_variable>

namespace MediaUse {

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
		T* data;
		int head;
		int rear;
		int capacity;
		int realCapacity;
	};

	template<typename T>
	AVFifoLoop<T>::AVFifoLoop() :data(nullptr), head(0), rear(0), capacity(0), realCapacity(0) {
		
	}

	template<typename T>
	AVFifoLoop<T>::AVFifoLoop(int capacity) :head(0), rear(0), capacity(capacity), realCapacity(capacity + 1) {
		data = new T[capacity + 1];
	}

	template<typename T>
	AVFifoLoop<T>::~AVFifoLoop() {
		delete[] data;
	}

	template<typename T>
	bool AVFifoLoop<T>::push(T data) {
		if (this->full()) return false;
		this->data[rear] = data;
		rear = (rear + 1) % realCapacity;
		return true;
	}

	template<typename T>
	void AVFifoLoop<T>::pop() {
		if (this->empty()) return;
		head = (head + 1) % realCapacity;
	}

	template<typename T>
	bool AVFifoLoop<T>::empty() {
		if (head == rear) return true;
		return false;
	}

	template<typename T>
	bool AVFifoLoop<T>::full() {
		if (head == ((rear + 1) % realCapacity)) {
			return true;
		}
		return false;
	}

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

	template<typename T>
	void AVFifoLoop<T>::setCapacity(int capacity) {
		if (data) {
			delete[] data;
		}
		data = new T[capacity + 1];
		realCapacity = capacity + 1;
        head = 0;
        rear = 0;
	}

	template<typename T>
	T& AVFifoLoop<T>::front() {
		return data[head];
	}

	template<typename T>
	T& AVFifoLoop<T>::back() {
		if (rear - 1 < 0) {
			return data[realCapacity - 1];
		}
		else {
			return data[rear - 1];
		}
	}

	class AVDataInfo {
	public:
		AVDataInfo();
		AVDataInfo(unsigned char* data, int64_t pts, size_t size);
		~AVDataInfo();
		unsigned char* data;
		int64_t pts;
		size_t size;
		void clear();
	};

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

		std::queue<T> queue;
		std::mutex mutex;
		std::condition_variable cv;

	};

	template<typename T>
	MediaDataQueue<T>::MediaDataQueue() {

	}

	template<typename T>
	MediaDataQueue<T>::~MediaDataQueue() {

	}

	template<typename T>
	void MediaDataQueue<T>::push(T data) {
		std::lock_guard<std::mutex> lock(mutex);
		queue.push(data);
		cv.notify_one();
	}

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

	template<typename T>
	T MediaDataQueue<T>::back() {
		T data;
		std::lock_guard<std::mutex> lock(mutex);
		data = queue.back();
		return data;
	}

	template<typename T>
	void MediaDataQueue<T>::wait() {
		std::unique_lock<std::mutex> lock(mutex);
		cv.wait(lock, [this]() {return !(queue.empty()); });
	}

	template<typename T>
	bool MediaDataQueue<T>::waitFor(int64_t millisecond) {
		std::unique_lock<std::mutex> lock(mutex);
		return cv.wait_for(lock, std::chrono::milliseconds(millisecond), [this]() {return !(queue.empty()); });
	}

	template<typename T>
	void MediaDataQueue<T>::waitOrCondition(const bool* cdt) {
		std::unique_lock<std::mutex> lock(mutex);
		cv.wait(lock, [this, cdt]() {return !(queue.empty()) || (*cdt); });
	}

	template<typename T>
	void MediaDataQueue<T>::waitAndCondition(const bool* cdt) {
		std::unique_lock<std::mutex> lock(mutex);
		cv.wait(lock, [this, cdt]() {return !(queue.empty()) && (*cdt); });
	}

	template<typename T>
	void MediaDataQueue<T>::notify_all() {
		std::lock_guard<std::mutex> lock(mutex);
		cv.notify_all();
	}

	template<typename T>
	void MediaDataQueue<T>::clear() {
		std::lock_guard<std::mutex> lock(mutex);
		std::queue<T>().swap(queue);
	}

	template<typename T>
	void MediaDataQueue<T>::clearWithDelete() {
		std::lock_guard<std::mutex> lock(mutex);
		while (!queue.empty()) {
			queue.front().clear();
			queue.pop();
		}
	}

	template<typename T>
	bool MediaDataQueue<T>::empty() {
		std::lock_guard<std::mutex> lock(mutex);
		return queue.empty();
	}

	template<typename T>
	size_t MediaDataQueue<T>::size() {
		std::lock_guard<std::mutex> lock(mutex);
		return queue.size();
	}


};




#endif//_MEDIAUSE_H_
