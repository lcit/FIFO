/*	=========================================================================
	Author: Leonardo Citraro
	Company: 
	Filename: test_performance_FIFO.cpp
	Last modifed:   03.12.2016 by Leonardo Citraro
	Description:	Test of performance.

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
#include <thread>
#include <atomic>
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
		ITEM(const std::string id, const int value):_id(id), _value(value) {}
		~ITEM(){}
};

using MyFIFO = tsFIFO::FIFO<std::unique_ptr<ITEM>, tsFIFO::ActionIfFull::Nothing>;

bool stop = false;
const int Npushes = 1000000;
std::atomic<int> stop_count(0);
MyFIFO fifo(100);

int Nproducers, Nconsumers;

void producer(){
	for(unsigned int i=0; i<Npushes; i++){
		std::unique_ptr<ITEM> item = std::make_unique<ITEM>("id", i);
#ifdef DEBUG        
		cout << pthread_self() << " Pushing item: " << i << endl;
#endif
		while(fifo.push(item) != tsFIFO::Status::SUCCESS){
            usleep(1);
#ifdef DEBUG            
            cout << pthread_self() << " Fifo full: " << i << endl;
#endif            
        }
	}
}

void consumer(){
	while(1){
		std::unique_ptr<ITEM> item;
		if(fifo.pull(item, 100) == tsFIFO::Status::SUCCESS) {
            ;
#ifdef DEBUG        
            cout << pthread_self() << " Pulled item ------: " << item->_value << endl;
#endif            
        } else
            break;
	}
#ifdef DEBUG    
    cout << pthread_self() << " stopped" << endl;
#endif    
}

void run_threads_helper(size_t Nproducers, size_t Nconsumers){
    std::vector<std::thread> threads_producers(Nproducers);
    for(size_t i=0; i<Nproducers; ++i){
        threads_producers[i] = std::thread(producer);
    }   
    std::vector<std::thread> threads_consumers(Nconsumers);
    for(size_t i=0; i<Nconsumers; ++i){
        threads_consumers[i] = std::thread(consumer);
    }
    for(size_t i=0; i<Nproducers; ++i){
        threads_producers[i].join();
    }
    for(size_t i=0; i<Nconsumers; ++i){
        threads_consumers[i].join();
    }
#ifdef DEBUG
    std::cout << "run_threads() its over" << std::endl;
#endif
}

double run_threads(size_t Nproducers, size_t Nconsumers){
    double execution_time = measure<std::chrono::milliseconds>::run([&](){return run_threads_helper(Nproducers, Nconsumers);});
#ifdef DEBUG    
    std::cout << "run_threads() measured time: " << execution_time/1000000.0 << std::endl;
#endif    
    return Npushes*Nproducers/(execution_time);
}

int main(int argc, char* argv[]){
	
    std::array<size_t,3> fifo_sizes = {10,50,200};
    std::array<size_t,4> string_sizes = {10,1000,10000,1000000};
    
    std::cout << "Npushes=" << Npushes << "\n";
    std::cout << "        --------------------------- Consumer threads ---------------------------------" << "\n";
    std::cout << "   ";
    for(size_t j=1; j<9; ++j)
        std::cout   << std::setw(10) << j;
    std::cout << "\n";
    for(size_t i=1; i<9; ++i){
        std::cout   << "   " << i << "   ";
        for(size_t j=1; j<9; ++j){
            auto results = mean_stddev<3>::run([&](){return run_threads(i,j);});
            std::cout   << std::setw(4) << static_cast<int>(results.first)
                        << std::setw(5) << ("+-" + std::to_string(static_cast<int>(results.second))) 
                        << " ";
        }
        std::cout << "\n";
    }
    std::cout << "^^^^^^\nProducers\nthreads\n"; 
    
	return 0;
}
