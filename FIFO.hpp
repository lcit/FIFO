/*	=========================================================================
	Author: Leonardo Citraro
	Company: 
	Filename: FIFO.hpp
	Last modifed: 10.07.2016 by Leonardo Citraro
	Description: Thread-safe FIFO based on the Standard C++ library queue
				 template class.

	=========================================================================

	=========================================================================
*/

#ifndef __FIFO_HPP__
#define __FIFO_HPP__

#include <unistd.h>
#include <iostream>
#include <cstring>
#include <string>
#include <stdexcept>
#include <pthread.h>
#include <errno.h>
#include <queue>
#include <memory>
#include <sys/time.h>

using namespace std;

// TODO: change names!!
enum class FIFOdumpTypes {
	DumpNewItem = 0, ///< with this option the FIFO dumps the new items
	DumpFirstEntry = 1 ///< with this option the FIFO dumps the oldest item entered
};

/// Thread-safe FIFO buffer using STL queue.
///
/// Example usage:
///
///     FIFO<std::unique_ptr<float>, FIFOdumpTypes::DumpNewItem> fifo(5);
///     std::unique_ptr<float> temp = std::make_unique<float>(2.1);	
///     fifo.push(temp);
///     int size = fifo.size();
///     fifo.pull(temp);
///
template <typename T, FIFOdumpTypes dump_type> class FIFO{

protected:
	queue<T>         _queue; 
	int              _max_size;  
	pthread_cond_t   _condv; 
	pthread_mutex_t  _mutex;

public:
	FIFO() : _max_size(10.0){
		// init the mutexe and the conditional variable
		int res = pthread_mutex_init(&_mutex, NULL);	
		if(res < 0)
			throw std::runtime_error("FIFO::FIFO(): pthread_mutex_init() failed due to \"" + std::string(std::strerror(res)) + "\"");
		res = pthread_cond_init(&_condv, NULL);
		if(res < 0)
			throw std::runtime_error("FIFO::FIFO(): pthread_cond_init() failed due to \"" + std::string(std::strerror(res)) + "\"");
	}
	FIFO(int size) : _max_size(size){
		// init the mutexe and the conditional variable
		int res = pthread_mutex_init(&_mutex, NULL);
		if(res < 0)
			throw std::runtime_error("FIFO::FIFO(): pthread_mutex_init() failed due to \"" + std::string(std::strerror(res)) + "\"");
		res = pthread_cond_init(&_condv, NULL);
		if(res < 0)
			throw std::runtime_error("FIFO::FIFO(): pthread_cond_init() failed due to \"" + std::string(std::strerror(res)) + "\"");
	}
	virtual ~FIFO(){
		// pthread_mutex_destroy() and pthread_cond_destroy() may fail if functions like  
		// pthread_cond_timedwait() or pthread_cond_wait() are active
		int res = pthread_mutex_destroy(&_mutex);
		if(res < 0){
			pthread_cond_signal(&_condv);
			pthread_mutex_destroy(&_mutex);
		}
		pthread_cond_destroy(&_condv);
	}

public:	
	/// Adds an item into the FIFO. (Thread-safe)
	///
	/// If the FIFO is full FIFOdumpTypes defines which item is dumped.
	///
	/// @param item: element to push into the fifo
	/// @return no return
	int push(T& item){
		int succ = 0;
		int res = pthread_mutex_lock(&_mutex);
		if(res < 0)
			throw std::runtime_error("FIFO::push(): pthread_mutex_lock() failed due to \"" + std::string(std::strerror(res)) + "\"");
		// if the FIFO is full one item is dumped
		if(is_full_helper()){
			// choose dumping method
			if(dump_type == FIFOdumpTypes::DumpNewItem){
				; // dump the new item
			}else if(dump_type == FIFOdumpTypes::DumpFirstEntry){
				// dump the the oldest item in the FIFO and add the new one
				pull_pop_first();
				push_last(item);
			}else{
				; // dump the new item
			} 
			succ = -1;
		}else{ 
			// add item to the FIFO
			push_last(item); 
		}
		// pthread_cond_signal() fails only if _condv is not initialized, so we don't need to take care of errors here
		pthread_cond_signal(&_condv);
		res = pthread_mutex_unlock(&_mutex);
		if(res < 0)
			throw std::runtime_error("FIFO::push(): pthread_mutex_unlock() failed due to \"" + std::string(std::strerror(res)) + "\"");
		return succ;
	}

	/// Retrieves an item from the FIFO. (Thread-safe)
	///
	/// The oldest element in the FIFO is pulled. If the fifo is empty
	/// this function blocks until new data are available.
	///
	/// @param item: element pulled from the fifo
	/// @return no return
	void pull(T& item){
		int res = pthread_mutex_lock(&_mutex);
		if(res < 0)
			throw std::runtime_error("FIFO::pull(): pthread_mutex_lock() failed due to \"" + std::string(std::strerror(res)) + "\"");

		// if FIFO is empty wait until new data is avvailable.			
		// This while loop is necessary if there are multiple 
		// threads pulling at the same time!
		while(_queue.empty()){ 
			// pthread_cond_wait() do not fail unless you have huge issues
			pthread_cond_wait(&_condv, &_mutex);	
		} 
		item = pull_pop_first();
		res = pthread_mutex_unlock(&_mutex);
		if(res < 0)
			throw std::runtime_error("FIFO::push(): pthread_mutex_unlock() failed due to \"" + std::string(std::strerror(res)) + "\"");
	}
    
    /// Retrieves an item from the FIFO.
    ///
    /// The oldest element in the FIFO is pulled. If the fifo is empty
    /// this function blocks until new data are available or the timeout is reached.
    ///
    /// @param item: element pulled from the fifo
    /// @param timeout: an integer defining the max amount of time to wait for new element in the FIFO
    /// @return 1: success, 0: timeout reached
    virtual int pull(T& item, unsigned timeout) {
        int res = pthread_mutex_lock(&_mutex);
        if(res < 0)
            throw std::runtime_error("FIFO::pull(): pthread_mutex_lock() failed due to \"" + std::string(std::strerror(res)) + "\"");

        // if FIFO is empty wait until new data is available.
        // This while loop is necessary if there are multiple
        // threads pulling at the same time!
        while(_queue.empty()) {
            struct timespec timeout_ts;
            struct timeval now_tv;
            gettimeofday(&now_tv, NULL);
            timeout_ts.tv_sec = now_tv.tv_sec+timeout;
            timeout_ts.tv_nsec = now_tv.tv_usec*1000UL;
            res = pthread_cond_timedwait(&_condv, &_mutex, &timeout_ts);
            if(res == ETIMEDOUT) {
                res = pthread_mutex_unlock(&_mutex);
                if(res < 0)
                    throw std::runtime_error("FIFO::pull(): pthread_mutex_unlock() failed due to \"" + std::string(std::strerror(res)) + "\"");
                return 0;
            } else if(res < 0)
                throw std::runtime_error("FIFO::pull(): pthread_cond_timedwait() failed due to \"" + std::string(std::strerror(res)) + "\"");
        }
        item = pull_pop_first();
        res = pthread_mutex_unlock(&_mutex);
        if(res < 0)
            throw std::runtime_error("FIFO::pull(): pthread_mutex_unlock() failed due to \"" + std::string(std::strerror(res)) + "\"");
        return 1;
    }
	
	/// Returns the current number of item stocked. (Thread-safe)
	///
	/// @param no param
	/// @return current number of item in the fifo
	int size(){
		int res = pthread_mutex_lock(&_mutex);
		if(res < 0)
			throw std::runtime_error("FIFO::size(): pthread_mutex_lock() failed due to \"" + std::string(std::strerror(res)) + "\"");
		int size = size_helper();
		res = pthread_mutex_unlock(&_mutex);
		if(res < 0)
			throw std::runtime_error("FIFO::size(): pthread_mutex_unlock() failed due to \"" + std::string(std::strerror(res)) + "\"");
		return size;
	}
	
	/// Sets the max FIFO size. (Thread-safe)
	///
	/// @param size: max fifo size
	/// @return no param
	void set_max_size(int size){
		int res = pthread_mutex_lock(&_mutex);
		if(res < 0)
			throw std::runtime_error("FIFO::set_max_size(): pthread_mutex_lock() failed due to \"" + std::string(std::strerror(res)) + "\"");
		set_max_size_helper(size);
		res = pthread_mutex_unlock(&_mutex);
		if(res < 0)
			throw std::runtime_error("FIFO::set_max_size(): pthread_mutex_unlock() failed due to \"" + std::string(std::strerror(res)) + "\"");
	}
	
	/// Gets the max FIFO size. (Thread-safe)
	///
	/// @param no param
	/// @return max fifo size
	int get_max_size(){
		int res = pthread_mutex_lock(&_mutex);
		if(res < 0)
			throw std::runtime_error("FIFO::get_max_size(): pthread_mutex_lock() failed due to \"" + std::string(std::strerror(res)) + "\"");
		int size = get_max_size_helper();
		res = pthread_mutex_unlock(&_mutex);
		if(res < 0)
			throw std::runtime_error("FIFO::get_max_size(): pthread_mutex_unlock() failed due to \"" + std::string(std::strerror(res)) + "\"");
		return size;
	}
	
	/// Delete all the items. (Thread-safe)
	///
	/// @param no param
	/// @return no param
	void clear(){
		int res = pthread_mutex_lock(&_mutex);
		if(res < 0)
			throw std::runtime_error("FIFO::clear(): pthread_mutex_lock() failed due to \"" + std::string(std::strerror(res)) + "\"");
		try{
			clear_helper();
		}catch(...){
			pthread_mutex_unlock(&_mutex);
			throw;
		}
		res = pthread_mutex_unlock(&_mutex);
		if(res < 0)
			throw std::runtime_error("FIFO::clear(): pthread_mutex_unlock() failed due to \"" + std::string(std::strerror(res)) + "\"");
	}
    
	/// Check if FIFO is full. (Thread-safe)
	///
	/// @param no param
	/// @return no param
	bool is_full(){
		bool succ;
		int res = pthread_mutex_lock(&_mutex);
		if(res < 0)
			throw std::runtime_error("FIFO::clear(): pthread_mutex_lock() failed due to \"" + std::string(std::strerror(res)) + "\"");
		succ = is_full_helper();
		res = pthread_mutex_unlock(&_mutex);
		if(res < 0)
			throw std::runtime_error("FIFO::clear(): pthread_mutex_unlock() failed due to \"" + std::string(std::strerror(res)) + "\"");
		return succ;
	}
    
	
protected:
	// the following functions are not thread-safe but
	// are useful when subclassing this template.
	// see sFIFO for more details	

	/// Checks if FIFO is full
	///
	/// @param no param
	/// @return true or false
	virtual bool is_full_helper(){
		return (_queue.size() >= _max_size);
	}	

	/// Gets the first item then pop it	
	///
	/// @param no param
	/// @return the item	
	virtual T pull_pop_first(){
		// move() never throw
		T item = std::move(_queue.front());
		_queue.pop();
		return std::move(item);
	}		

	/// Add item into the FIFO
	///
	/// @param item: the item to add
	/// @return no return
	virtual void push_last(T& item){
		_queue.push(std::move(item)); 
	}

	/// Clear whole FIFO
	///
	/// @param no param
	/// @return no param
	virtual void clear_helper(){
		std::queue<T> empty;
		// swap() throws if T's constructor throws
		std::swap(_queue,empty);
	}

protected:
	/// Current FIFO's size
	///
	/// @param no param
	/// @return size of the fifo
	int size_helper(){
		return _queue.size();
	}

	/// Set max FIFO's size
	///
	/// @param size: max FIFO size
	/// @return no param
	void set_max_size_helper(int size){
		_max_size = size;
	}

	/// Get max FIFO's size
	///
	/// @param no param
	/// @return max FIFO size
	int get_max_size_helper(){
		return _max_size;
	}
};

#endif
