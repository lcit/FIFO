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

#include "FIFO.hpp"
#include <unistd.h>
#include <iostream>
#include <cstring>
#include <stdexcept>
#include <pthread.h>
#include <errno.h>
#include <queue>
#include <memory>
#include <chrono>

namespace tsFIFO {

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
    template <  typename T, 
                typename TimeT = std::chrono::seconds, 
                ActionIfFull action_if_full = ActionIfFull::DumpFirstEntry> class sFIFO 
                : public FIFO<T, action_if_full> {

        private:
            TimeT _max_size_seconds;    ///< max FIFO size in seconds
            TimeT _size_seconds;        ///< current FIFO size in seconds

        public:
            sFIFO() : FIFO<T, action_if_full>(), _max_size_seconds(TimeT()), _size_seconds(TimeT()){}
            sFIFO(TimeT size_seconds) 
                : FIFO<T, action_if_full>(), _max_size_seconds(size_seconds), _size_seconds(TimeT()){}
            ~sFIFO(){}

            /// Returns the current number of item stocked. (Thread-safe)
            ///
            /// @param no param
            /// @return current number of item in the fifo
            TimeT size_seconds(){
                std::unique_lock<std::mutex> _lock(this->_mutex);
                return _size_seconds;
            }

            /// Sets the max FIFO size. (Thread-safe)
            ///
            /// @param size: max fifo size
            /// @return no param
            void set_max_size_seconds(TimeT size){
                std::unique_lock<std::mutex> _lock(this->_mutex);
                _max_size_seconds = size;
            }

            /// Gets the max FIFO size. (Thread-safe)
            ///
            /// @param no param
            /// @return max fifo size
            TimeT get_max_size_seconds(){
                std::unique_lock<std::mutex> _lock(this->_mutex);
                return _max_size_seconds;
            }
            
            /// Clear whole FIFO (Thread-safe).
            ///
            /// @param no param
            /// @return no param
            void clear(){
                std::unique_lock<std::mutex> _lock(this->_mutex);
                try{
                    std::queue<T> empty;
                    std::swap(this->_queue,empty);
                    _size_seconds = TimeT();
                }catch(...){
                    throw;
                }
            }

        protected:

            /// Gets the first item then pop it	
            ///
            /// @param no param
            /// @return the item
            T pull_pop_first() override {
                T item = std::move(this->_queue.front());
                _size_seconds -= item->get_size_seconds();
                this->_queue.pop();
                return std::move(item);
            }

            /// Add item into the FIFO
            ///
            /// @param item: the item to add
            /// @return no return
            void push_last(T& item) override {
                _size_seconds += item->get_size_seconds();
                this->_queue.push(std::move(item)); 
            }
            
            /// Checks if FIFO is full.
            ///
            /// @param no param
            /// @return true or false
            bool is_full_helper() override {
                return (_size_seconds >= _max_size_seconds);
            }
    };
};

#endif
