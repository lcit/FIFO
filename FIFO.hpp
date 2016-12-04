/*	=========================================================================
	Author: Leonardo Citraro
	Company: 
	Filename: FIFO.hpp
	Last modifed:   04.12.2016 by Leonardo Citraro
	Description:    Thread-safe FIFO based on the Standard C++ library queue
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

namespace tsFIFO {

    enum class ActionIfFull {
        Nothing = 0, ///< if the FIFO is full the push() just returns
        DumpFirstEntry = 1 ///< if the FIFO is full the push() dumps the oldest item entered and push the new item in
    };

    enum class Status {
        ERROR = -1, ///< when something weird happen
        SUCCESS, ///< when the function call do what you want
        FULL, ///< when the FIFO is full. It does not implies that there was an error.
        TIMEOUT
    };

    /// Thread-safe FIFO buffer using STL queue.
    ///
    /// Example usage:
    ///
    ///     tsFIFO::FIFO<std::unique_ptr<float>, tsFIFO::ActionIfFull::Nothing> fifo(5);
    ///     std::unique_ptr<float> temp = std::make_unique<float>(2.1);	
    ///     fifo.push(temp);
    ///     int size = fifo.size();
    ///     fifo.pull(temp);
    ///
    template <typename T, ActionIfFull action_if_full = ActionIfFull::DumpFirstEntry> class FIFO{

    protected:
        std::queue<T>           _queue; 
        int                     _max_size;
        std::condition_variable _condv; 
        std::mutex              _mutex;

    public:
        FIFO() : _max_size(0){}
        FIFO(int size) : _max_size(size){}
        virtual ~FIFO(){}

    public:
        /// Adds an item into the FIFO. (Thread-safe)
        ///
        /// If the FIFO is full ActionIfFull defines the action to undertake.
        ///
        /// @param item: element to push into the fifo
        /// @return no return
        Status push(T& item){
            std::unique_lock<std::mutex> _lock(_mutex);
            // must use _helper() otherwise we lock the mutex twice
            if(is_full_helper()){
                if(action_if_full == ActionIfFull::Nothing){
                    ; // nothing to do
                }else if(action_if_full == ActionIfFull::DumpFirstEntry){
                    pull_pop_first(); // dump the the oldest item
                    push_last(item); // add the new one
                }
                return Status::FULL; 
            }else{ 
                push_last(item); // add item into the FIFO
            }
            _condv.notify_one();
            return Status::SUCCESS;
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
            // if FIFO is empty wait until new data is available.
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
        virtual Status pull(T& item, unsigned timeout) {
            std::unique_lock<std::mutex> _lock(_mutex);
            // if FIFO is empty wait until new data is available.
            // This while loop is necessary if there are multiple
            // threads pulling at the same time!
            while(_queue.empty()) {
                if(_condv.wait_for(_lock, std::chrono::milliseconds(timeout))==std::cv_status::timeout)
                    return Status::TIMEOUT;
            }
            item = pull_pop_first();
            return Status::SUCCESS;
        }
        
        /// Returns the current number of item stocked. (Thread-safe)
        ///
        /// @param no param
        /// @return current number of item in the fifo
        int size() {
            std::unique_lock<std::mutex> _lock(_mutex);
            return _queue.size();
        }
        
        /// Sets the max FIFO size. (Thread-safe)
        ///
        /// @param size: max fifo size
        /// @return no param
        void set_max_size(int size) {
            std::unique_lock<std::mutex> _lock(_mutex);
            _max_size = size;
        }
        
        /// Gets the max FIFO size. (Thread-safe)
        ///
        /// @param no param
        /// @return max fifo size
        int get_max_size() {
            std::unique_lock<std::mutex> _lock(_mutex);
            return _max_size;
        }
        
        /// Delete all the items. (Thread-safe)
        ///
        /// @param no param
        /// @return no param
        void clear() {
            std::unique_lock<std::mutex> _lock(_mutex);
            try{
                std::queue<T> empty;
                // swap() throws if T's constructor throws
                std::swap(_queue,empty);
            }catch(...){
                throw;
            }
        }
        
        /// Check if FIFO is full. (Thread-safe)
        ///
        /// @param no param
        /// @return no param
        bool is_full() {
            std::unique_lock<std::mutex> _lock(_mutex);
            return is_full_helper();
        }
        
    protected:
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
        
        /// Check if FIFO is full.
        ///
        /// @param no param
        /// @return no param
        virtual bool is_full_helper() {
            return (_queue.size() >= _max_size);
        }
    };
};

#endif
