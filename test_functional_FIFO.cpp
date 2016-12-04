/*	=========================================================================
	Author: Leonardo Citraro
	Company: 
	Filename: test_functional_FIFO.cpp
	Last modifed:   03.12.2016 by Leonardo Citraro
	Description:	Functional tests. Here we test if all the proposed
                    functionality work as expected. Then we test if the fifo 
                    is actually thread-safe using multiple producers 
                    and consumers.

	=========================================================================

	=========================================================================
*/
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <cassert>
#include <thread>
#include <mutex>
#include <unistd.h>
#include <sys/wait.h>
#include "FIFO.hpp"

//#define DEBUG 1

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
};

// Definition of the FIFOs we use here
using bigFIFO = tsFIFO::FIFO<ITEM*, tsFIFO::ActionIfFull::Nothing>;
using smallFIFO = tsFIFO::FIFO<ITEM*, tsFIFO::ActionIfFull::Nothing>;

// Some global variables for the threads
const int Nthreads = 10; // number of producers and consumers to create
const int Npushes = 10000; // number of push & pull to perform
bigFIFO fifo(100);
//td::array<std::vector<int>,Nthreads> verif;
int verif[Nthreads][Npushes] = {{0}};
std::mutex mtx;

// producer thread
void producer(int idx_producer){
	for(unsigned int i=0; i<Npushes; i++){
		ITEM* item = new ITEM("id", idx_producer, i);
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
		//ITEM* item;
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
		ITEM* item;        
		if(fifo.pull(item,100) == tsFIFO::Status::SUCCESS) {
#ifdef DEBUG        
            std::cout << pthread_self() << " Pulled item ------: " << item->_value << "\n";
#endif      
            mtx.lock();
            verif[item->_idx_producer][item->_value]++;
            delete item;
            mtx.unlock();     
        } else {
            break;
        }  
	}
#ifdef DEBUG    
    std::cout << pthread_self() << " stopped\n";
#endif    
}

int main(){
	// ===============================================
    // here we test the functionality of the FIFO
    // ===============================================
	smallFIFO fifo;
	
	fifo.set_max_size(5);
	assert(fifo.get_max_size() == 5);

	ITEM* item = new ITEM("id", 9);
	fifo.push(item);
	ITEM* item2 = new ITEM("id", 1);
	fifo.push(item2);
	
	ITEM* item3;
	fifo.pull(item3);
	assert(item3->_value==9);
    delete item3;
	
	assert(fifo.size()==1);
	
	ITEM* item4 = new ITEM("id", 2);
	fifo.push(item4);
	ITEM* item5 = new ITEM("id", 3);
	fifo.push(item5);
	ITEM* item6 = new ITEM("id", 4);
	fifo.push(item6);
	ITEM* item7 = new ITEM("id", 5);
	fifo.push(item7);
    
    assert(fifo.is_full()==true);
	
	ITEM* item8 = new ITEM("id", 6);
	// Here we try to push another element into the FIFO
	// but it is not possible since the fifo is full
    assert(fifo.push(item8)==tsFIFO::Status::FULL);
	
	fifo.pull(item3);
    assert(item3->_value==1);
    delete item3;
	fifo.pull(item3);
	assert(item3->_value==2);
    delete item3;
	fifo.pull(item3);
	assert(item3->_value==3);
    delete item3;
	fifo.pull(item3);
	assert(item3->_value==4);
    delete item3;
	fifo.pull(item3);
	assert(item3->_value==5);
    delete item3;
        
    // the fifo should be empty now
    assert(fifo.size()==0);
    
#ifdef DEBUG    
    std::cout << "Testing the pull timeout\n";
#endif     
    // since the fifo is empty if we call pull we should obtain a timeout
    assert(fifo.pull(item3, 100)==tsFIFO::Status::TIMEOUT);
    
    ITEM* item9 = new ITEM("id", 7);
	fifo.push(item9);
	ITEM* item10 = new ITEM("id", 8);
	fifo.push(item10);
    
    assert(fifo.size()==2);
    
    fifo.clear();
    assert(fifo.size()==0);

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
