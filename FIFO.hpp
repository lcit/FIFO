/*	=========================================================================
	Author: Leonardo Citraro
	Company: 
	Filename: FIFO.hpp
	Last modifed: 03.12.2016 by Leonardo Citraro
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
#include <mutex>
#include <condition_variable>
#include <errno.h>
#include <queue>
#include <chrono>
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
	queue<T>                _queue; 
	int                     _max_size;  
	std::condition_variable _condv; 
	std::mutex              _mutex;

public:
	FIFO() : _max_size(10.0){
	}
	FIFO(int size) : _max_size(size){
	}
	virtual ~FIFO(){
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
		std::unique_lock<std::mutex> _lock(_mutex);
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
		_condv.notify_one();
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
		std::unique_lock<std::mutex> _lock(_mutex);

		// if FIFO is empty wait until new data is avvailable.
		// This while loop is necessary if there are multiple 
		// threads pulling at the same time!
		while(_queue.empty()){ 
			_condv.wait(_lock);
		} 
		item = pull_pop_first();
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
        std::unique_lock<std::mutex> _lock(_mutex);
            
        //std::cout << "Hello form pull timeout: " << pthread_self() << "\n";

        // if FIFO is empty wait until new data is available.
        // This while loop is necessary if there are multiple
        // threads pulling at the same time!
        while(_queue.empty()) {
            if(_condv.wait_for(_lock, std::chrono::seconds(timeout))==std::cv_status::timeout)
                return 0;
        }
        item = pull_pop_first();
        return 1;
    }
	
	/// Returns the current number of item stocked. (Thread-safe)
	///
	/// @param no param
	/// @return current number of item in the fifo
	int size(){
		std::unique_lock<std::mutex> _lock(_mutex);
		return size_helper();
	}
	
	/// Sets the max FIFO size. (Thread-safe)
	///
	/// @param size: max fifo size
	/// @return no param
	void set_max_size(int size){
		std::unique_lock<std::mutex> _lock(_mutex);
		set_max_size_helper(size);
	}
	
	/// Gets the max FIFO size. (Thread-safe)
	///
	/// @param no param
	/// @return max fifo size
	int get_max_size(){
		std::unique_lock<std::mutex> _lock(_mutex);
		return get_max_size_helper();
	}
	
	/// Delete all the items. (Thread-safe)
	///
	/// @param no param
	/// @return no param
	void clear(){
		std::unique_lock<std::mutex> _lock(_mutex);
		try{
			clear_helper();
		}catch(...){
			throw;
		}
	}
    
	/// Check if FIFO is full. (Thread-safe)
	///
	/// @param no param
	/// @return no param
	bool is_full(){
		std::unique_lock<std::mutex> _lock(_mutex);
		return is_full_helper();
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
    /*	
	virtual T pull_pop_first(){
		// move() never throw
		T item = std::move(_queue.front());
		_queue.pop();
		return std::move(item);
	}	
    * */
    virtual T pull_pop_first(){
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
