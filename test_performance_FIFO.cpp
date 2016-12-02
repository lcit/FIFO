/*	=========================================================================
	Author: Leonardo Citraro
	Company: 
	Filename: test_performance_FIFO.cpp
	Last modifed:   28.11.2016 by Leonardo Citraro
	Description:	Test of performance. The write (push) and read (pull) 
                    speeds are computed here using a single producer and 
                    a single consumer.

	=========================================================================

	=========================================================================
*/
#include <iostream>
#include <iomanip>
#include <memory>
#include <string>
#include <unistd.h>
#include <chrono>
#include <cassert>
#include <algorithm>
#include <utility>
#include "FIFO.hpp"

//#define DEBUG 1

template<typename TimeT = std::chrono::milliseconds>
struct measure
{
    template<typename F, typename ...Args>
    static typename TimeT::rep run(F&& func, Args&&... args)
    {
        auto start = std::chrono::steady_clock::now();
        std::forward<decltype(func)>(func)(std::forward<Args>(args)...);
        auto duration = std::chrono::duration_cast<TimeT>(std::chrono::steady_clock::now() - start);
        return duration.count();
    }
};

template<size_t N>
struct mean_stddev {
    template<typename F, typename ...Args>
    static auto run(F&& func, Args&&... args){
        std::array<double, N> buffer;
        for(auto& buf : buffer)
            buf = std::forward<decltype(func)>(func)(std::forward<Args>(args)...);
        auto sum = std::accumulate(std::begin(buffer), std::end(buffer), 0.0);
        auto mean = sum/buffer.size();
        std::array<double, N> diff;
        std::transform(std::begin(buffer), std::end(buffer), std::begin(diff), [mean](auto x) { return x - mean; });
        auto sq_sum = std::inner_product(std::begin(diff), std::end(diff), std::begin(diff), 0.0);
        auto stddev = std::sqrt(sq_sum/buffer.size());
        return std::make_pair(mean,stddev);
    }
};

auto generate_string = [](size_t len){
    std::string str = "";
    for(size_t i=0; i<len; ++i){
        // fill the string by avoiding special characters
        str += static_cast<char>(33+(rand()%(126-33)));
    } 
    return str;
};

// Simple item for the FIFO
class ITEM {
	public:
		std::string _id;
		int _value;		
		ITEM(const string id, const int value):_id(id), _value(value) {}
		~ITEM(){}
};

using MyFIFO = FIFO<std::unique_ptr<ITEM>, FIFOdumpTypes::DumpNewItem>;

auto write_test(size_t fifo_size, size_t string_size){

    MyFIFO fifo(fifo_size);
    
    // generate a string for the ITEM so we can test ITEMS of dfferent size
    std::string str = generate_string(string_size);   
    
    size_t n_pushes = fifo_size;
    
    auto duration = 0.0;
    // write ITEMS into the fifo
	for(auto i=0; i<n_pushes; ++i){
		auto item = std::make_unique<ITEM>(str, i);
        duration += measure<std::chrono::nanoseconds>::run([&](){return fifo.push(item);});
	}
    
    // return the number of writes per second
    return (n_pushes*1000000000)/duration;
}

auto read_test(size_t fifo_size, size_t string_size){

    MyFIFO fifo(fifo_size);
    
    // generate a string for the ITEM so we can test ITEMS of dfferent size
    std::string str = generate_string(string_size);
    
    size_t n_pulls = fifo_size;
    
    // fill the whole fifo
    for(auto i=0; i<n_pulls; ++i){
		auto item = std::make_unique<ITEM>(str, i);
		fifo.push(item);
	}
    
    auto duration = 0.0;
    // read all the ITEMS form the fifo
	for(auto i=0; i<n_pulls; ++i){
		std::unique_ptr<ITEM> item;
        duration += measure<std::chrono::nanoseconds>::run([&](){return fifo.pull(item);});
	}
    
    // return the number of reads per second
    return (n_pulls*1000000000)/duration;
}

int main(int argc, char* argv[]){
	
    std::array<size_t,3> fifo_sizes = {10,50,200};
    std::array<size_t,4> string_sizes = {10,1000,10000,1000000};
    
    for(size_t i=0; i<3; ++i){
        for(size_t j=0; j<4; ++j){
            auto write_results = mean_stddev<100>::run([&](){return write_test(fifo_sizes[i],string_sizes[j]);});
            auto read_results = mean_stddev<100>::run([&](){return read_test(fifo_sizes[i],string_sizes[j]);});
            std::cout   << "fifo-size=" << fifo_sizes[i] << " item-size=" << string_sizes[j] << "\n"
                        << "Push-speed: " << std::setw(8) << static_cast<int>(write_results.first) 
                        << std::setw(9) << ("+-" + std::to_string(static_cast<int>(write_results.second))) 
                        << " [push-per-second +- std-dev] \n"
                        << "Pull-speed: " << std::setw(8) << static_cast<int>(read_results.first) 
                        << std::setw(9) << ("+-" + std::to_string(static_cast<int>(read_results.second))) 
                        << " [pull-per-second +- std-dev]" << "\n"
                        << "------------------------------------------------------" << "\n";
        }
    }
               
	return 0;
}
