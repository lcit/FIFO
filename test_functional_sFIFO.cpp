/*	=========================================================================
	Author: Leonardo Citraro
	Company: 
	Filename: test_functional_sFIFO.cpp
	Last modifed:   03.12.2016 by Leonardo Citraro
	Description:	Functional tests. Here we test if all the proposed
                    functionality work as expected. Then we test if the fifo 
                    is actually thread-safe using multiple producers 
                    and consumers.

	=========================================================================

	=========================================================================
*/
#include "sFIFO.hpp"
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <cassert>
#include <thread>
#include <mutex>
#include <chrono>
#include <unistd.h>
#include <sys/wait.h>

//#define DEBUG 1

using TimeUnit = std::chrono::milliseconds;

// Test item for the FIFO
// Here we keep track of the ID of the producer that produced the item so
// we can analyse afterward if all the items are present (~and int he right order)
class ITEM {
	public:
		std::string _id;
		int _idx_producer;
        int _value;
        ITEM(const std::string id, const int value) 
                :_id(id),_idx_producer(0), _value(value) {}
		ITEM(const std::string id, const int idx_producer, const int value) 
                :_id(id),_idx_producer(idx_producer), _value(value) {}
		~ITEM(){}
        TimeUnit get_size_seconds(){return TimeUnit(1200);}
};

// Definition of the FIFOs we use here
using bigFIFO = tsFIFO::sFIFO<std::unique_ptr<ITEM>, TimeUnit, tsFIFO::ActionIfFull::Nothing>;
using smallFIFO = tsFIFO::sFIFO<std::unique_ptr<ITEM>, TimeUnit, tsFIFO::ActionIfFull::Nothing>;

// Some global variables for the threads
const int Nthreads = 10; // number of producers and consumers to create
const int Npushes = 10000; // number of push & pull to perform
bigFIFO fifo(TimeUnit(100));
//td::array<std::vector<int>,Nthreads> verif;
int verif[Nthreads][Npushes] = {{0}};
std::mutex mtx;

// producer thread
void producer(int idx_producer){
	for(unsigned int i=0; i<Npushes; i++){
		std::unique_ptr<ITEM> item = std::make_unique<ITEM>("id", idx_producer, i);
#ifdef DEBUG        
		std::cout << pthread_self() << " Pushing item: " << i << "\n";
#endif
		while(fifo.push(item) != tsFIFO::Status::SUCCESS){
            usleep(1000);
#ifdef DEBUG            
            std::cout << pthread_self() << " Fifo full: " << i << "\n";
#endif            
        }
        usleep(10);
	}
}

//// consumer thread
//// there is a major problem here if we use a vector to store the returned items.
//// It is not guaranteed that the order in which the items exit the FIFO is the same
//// as the order of the items in the vector. So we can't test if the items ar pulled in the right order.
//void consumer(){
	//while(1){
		//std::unique_ptr<ITEM> item;
		//if(fifo.pull(item,1) > 0) {
            //// !!!!! there is a race condition here !!!!!!
            //// if two consumers have pulled one item each, it is not guaranteed
            //// that the first is going to put its item in the vector at first!
            //// The only way (we found) to fix this is to put mtx.lock() before the if
            //// but then we do not test the cocurrency.
            //mtx.lock();
            //verif[item->_idx_producer].push_back(item->_value); 
            //mtx.unlock();     
        //} else {
            //break;
        //}  
	//}   
//}

// consumer thread
void consumer(){
	while(1){
		std::unique_ptr<ITEM> item;        
		if(fifo.pull(item,100) == tsFIFO::Status::SUCCESS) {
#ifdef DEBUG        
            std::cout << pthread_self() << " Pulled item ------: " << item->_value << "\n";
#endif      
            mtx.lock();
            verif[item->_idx_producer][item->_value]++;
            mtx.unlock();     
        } else {
            break;
        }  
	}
#ifdef DEBUG    
    std::cout << pthread_self() << " stopped\n";
#endif    
}

bool is_equal(TimeUnit a, TimeUnit b, const float epsilon = 1e-5){
    //auto c = a - b;
    //return c < epsilon && -c < epsilon;
    return a==b;
}

int main(){
	// ===============================================
    // here we test the functionality of the FIFO
    // ===============================================
	smallFIFO fifo;
	
	fifo.set_max_size_seconds(TimeUnit(5000));
	assert(fifo.get_max_size_seconds() == TimeUnit(5000));
    
    fifo.set_max_size_seconds(std::chrono::seconds(5));
	assert(fifo.get_max_size_seconds() == std::chrono::seconds(5));

	std::unique_ptr<ITEM> item = std::make_unique<ITEM>("id", 9);
	fifo.push(item);
	std::unique_ptr<ITEM> item2 = std::make_unique<ITEM>("id", 1);
	fifo.push(item2);
	
	std::unique_ptr<ITEM> item3;
	fifo.pull(item3);
	assert(item3->_value==9);
	
	assert(fifo.size()==1);
    assert(is_equal(fifo.size_seconds(),TimeUnit(1200)));
	
	std::unique_ptr<ITEM> item4 = std::make_unique<ITEM>("id", 2);
	fifo.push(item4);
	std::unique_ptr<ITEM> item5 = std::make_unique<ITEM>("id", 3);
	fifo.push(item5);
	std::unique_ptr<ITEM> item6 = std::make_unique<ITEM>("id", 4);
	fifo.push(item6);
	std::unique_ptr<ITEM> item7 = std::make_unique<ITEM>("id", 5);
	fifo.push(item7);
    
    assert(fifo.is_full()==true);
	
	std::unique_ptr<ITEM> item8 = std::make_unique<ITEM>("id", 6);
	// Here we try to push another element into the FIFO
	// but it is not possible since the fifo is full
    assert(fifo.push(item8)==tsFIFO::Status::FULL);
	
	fifo.pull(item3);
    assert(item3->_value==1);
	fifo.pull(item3);
	assert(item3->_value==2);
	fifo.pull(item3);
	assert(item3->_value==3);
	fifo.pull(item3);
	assert(item3->_value==4);
	fifo.pull(item3);
	assert(item3->_value==5);
        
    // the fifo should be empty now
    assert(fifo.size()==0);
    assert(is_equal(fifo.size_seconds(),TimeUnit(0)));
    
#ifdef DEBUG    
    std::cout << "Testing the pull timeout\n";
#endif     
    // since the fifo is empty if we call pull we should obtain a timeout
    assert(fifo.pull(item3, 100)==tsFIFO::Status::TIMEOUT);
    
    std::unique_ptr<ITEM> item9 = std::make_unique<ITEM>("id", 7);
	fifo.push(item9);
	std::unique_ptr<ITEM> item10 = std::make_unique<ITEM>("id", 8);
	fifo.push(item10);
    
    assert(fifo.size()==2);
    assert(is_equal(fifo.size_seconds(),TimeUnit(2400)));
    
    fifo.clear();
    assert(fifo.size()==0);
    assert(is_equal(fifo.size_seconds(),TimeUnit(0)));

    // ===============================================
	// Here instead we test if the FIFO is thread-safe
    // ===============================================
    std::array<std::thread,Nthreads> consumers;
    std::array<std::thread,Nthreads> producers;
    for(size_t i=0; i<Nthreads; ++i){
        consumers[i] = std::thread(consumer);
        producers[i] = std::thread(producer,i);
    }
	
	for(size_t i=0; i<Nthreads; ++i){
        consumers[i].join();
        producers[i].join();
    }
        
    for(int i=0; i<Nthreads; ++i){  
        for(int j=0; j<Npushes; ++j){
            // there must be one item only for each cell in the array otherwise the FIFO is broken
#ifdef DEBUG            
            if(verif[i][j]!=1)
                std::cout << "verif[" << i << "][" << j << "]=" << verif[i][j] << " Error\n";
#endif                
            assert(verif[i][j]==1);            
        }
    }
   
    std::cout << "=======================================\n";
    std::cout << "==========    Test passed!!   =========\n";
    std::cout << "=======================================\n";
    
	return 0;
}
