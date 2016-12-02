/*	=========================================================================
	Author: Leonardo Citraro
	Company: 
	Filename: sFIFO.hpp
	Last modifed: 10.07.2016 by Leonardo Citraro
	Description: Thread-safe FIFO based on the Standard C++ library queue
				template class. This FIFO can be used when ITEMs' size is in seconds
				as for video frames. Therefore, the FIFO's size is in seconds.

=========================================================================

=========================================================================
*/

#ifndef __sFIFO_HPP__
#define __sFIFO_HPP__

#include <unistd.h>
#include <iostream>
#include <cstring>
#include <stdexcept>
#include <pthread.h>
#include <errno.h>
#include <queue>
#include <memory>
#include "FIFO.hpp"

using namespace std;

/// Thread-safe FIFO buffer using STL queue.
///
/// Example usage:
///
///     class ITEM {
///     	public:
///				std::string _value;
///				const float _size_seconds = 1.2;
///				ITEM(const std::string& value):_value(value){}
///				float get_size_seconds(){return _size_seconds;}
///     };
///
///     sFIFO<std::unique_ptr<ITEM>, FIFOdumpTypes::DumpNewItem> fifo(5);
///     std::unique_ptr<ITEM> temp = std::make_unique<ITEM>("an item");	
///     fifo.push(temp);
///     float size = fifo.size(); // <-- this value is in seconds
///     fifo.pull(temp);
template <typename T, FIFOdumpTypes dump_type> class sFIFO : public FIFO<T, dump_type>{

private:
	float _max_size_seconds;    ///< max FIFO size in seconds
	float _size_seconds;        ///< current FIFO size in seconds

public:
	sFIFO() : FIFO<T, dump_type>(), _max_size_seconds(1.0), _size_seconds(0.0){}
	sFIFO(float size_seconds) : FIFO<T, dump_type>(), _max_size_seconds(size_seconds), _size_seconds(0.0){}
	~sFIFO(){}

	/// Returns the current number of item stocked. (Thread-safe)
	///
	/// @param no param
	/// @return current number of item in the fifo
	float size(){
		int res = pthread_mutex_lock(&(this->_mutex));
		if(res < 0)
			throw std::runtime_error("sFIFO::size(): pthread_mutex_lock() failed due to \"" + std::string(std::strerror(res)) + "\"");
		float size = _size_seconds;
		res = pthread_mutex_unlock(&(this->_mutex));
		if(res < 0)
			throw std::runtime_error("sFIFO::size(): pthread_mutex_unlock() failed due to \"" + std::string(std::strerror(res)) + "\"");
		return size;
	}

	/// Sets the max FIFO size. (Thread-safe)
	///
	/// @param size: max fifo size
	/// @return no param
	void set_max_size(float size){
		int res = pthread_mutex_lock(&(this->_mutex));
		if(res < 0)
			throw std::runtime_error("sFIFO::set_max_size(): pthread_mutex_lock() failed due to \"" + std::string(std::strerror(res)) + "\"");
		_max_size_seconds = size;
		res = pthread_mutex_unlock(&(this->_mutex));
		if(res < 0)
			throw std::runtime_error("sFIFO::set_max_size(): pthread_mutex_unlock() failed due to \"" + std::string(std::strerror(res)) + "\"");
	}

	/// Gets the max FIFO size. (Thread-safe)
	///
	/// @param no param
	/// @return max fifo size
	float get_max_size(){
		int res = pthread_mutex_lock(&(this->_mutex));
		if(res < 0)
			throw std::runtime_error("sFIFO::get_max_size(): pthread_mutex_lock() failed due to \"" + std::string(std::strerror(res)) + "\"");
		float size = _max_size_seconds;
		res = pthread_mutex_unlock(&(this->_mutex));
		if(res < 0)
			throw std::runtime_error("sFIFO::get_max_size(): pthread_mutex_unlock() failed due to \"" + std::string(std::strerror(res)) + "\"");
		return size;
	}        

private:	

	/// Checks if FIFO is full
	///
	/// @param no param
	/// @return true or false
	bool is_full_helper() override{
		return (_size_seconds >= _max_size_seconds);
	}

	/// Gets the first item then pop it	
	///
	/// @param no param
	/// @return the item
	T pull_pop_first() override{
		T item = std::move(this->_queue.front());
		_size_seconds -= item->get_size_seconds();
		this->_queue.pop();
		return std::move(item);
	}

	/// Add item into the FIFO
	///
	/// @param item: the item to add
	/// @return no return
	void push_last(T& item) override{
		_size_seconds += item->get_size_seconds();
		this->_queue.push(std::move(item)); 
	}      

	/// Clear whole FIFO
	///
	/// @param no param
	/// @return no param
	void clear_helper() override{
		std::queue<T> empty;
		std::swap(this->_queue,empty);
		_size_seconds = 0.0;
	}
};

#endif
