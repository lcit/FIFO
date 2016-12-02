/*	=========================================================================
	Author: Leonardo Citraro
	Company: 
	Filename: test_FIFO.cpp
	Last modifed:   10.07.2016 by Leonardo Citraro
	Description:	Test of the thread-safe FIFO. In the first part we just test
					some of the functionality of the FIFO whereas in the
					second part we test if it is actually thread-safe. You will
					see that there are no items missing on the screen even if
					multiple thread are accessing the fifo simultanously.

	=========================================================================

	=========================================================================
*/
#include <iostream>
#include <memory>
#include <string>
#include <unistd.h>
#include "FIFO.hpp"

using namespace std;

// Simple item for the FIFO
class ITEM {
	public:
		string _id;
		int _value;		
		ITEM(const string id, const int value):_id(id), _value(value) {}
		~ITEM(){}
};

// Definition of the FIFO we use here
typedef FIFO<std::unique_ptr<ITEM>, FIFOdumpTypes::DumpNewItem> bigFIFO;
typedef FIFO<std::unique_ptr<ITEM>, FIFOdumpTypes::DumpNewItem> smallFIFO;
//typedef FIFO<unsigned, FIFOdumpTypes::DumpFirstEntry> smallFIFO2;


// forward declaration of thread functions
void *writing(void *arg);
void *reading(void *arg);

int main(){
	
	smallFIFO fifo;
	
	cout << "Let's play a bit wit our FIFO." << endl;
	cout << "Fifo current size:" << fifo.size() << endl;
	cout << "Fifo max size:" << fifo.get_max_size() << endl;
	
	cout << "Here we set the fifo to size=5" << endl;
	fifo.set_max_size(5);
	cout << "Fifo max size:" << fifo.get_max_size() << endl;
	
	cout << "Here we add two element; one with value=9 and the other value=1." << endl;
	std::unique_ptr<ITEM> item = std::make_unique<ITEM>("id", 9);
	fifo.push(item);
	std::unique_ptr<ITEM> item2 = std::make_unique<ITEM>("id", 1);
	fifo.push(item2);
	
	cout << "Fifo size:" << fifo.size() << endl;
	
	cout << "Pulling one element (should be the 9):" << endl;
	std::unique_ptr<ITEM> item3;
	fifo.pull(item3);
	cout << item3->_value << endl;
	
	cout << "Fifo size:" << fifo.size() << endl;
	
	cout << "Adding 4 other elements: [1]+[2,3,4,5] = [1,2,3,4,5]" << endl;
	std::unique_ptr<ITEM> item4 = std::make_unique<ITEM>("id", 2);
	fifo.push(item4);
	std::unique_ptr<ITEM> item5 = std::make_unique<ITEM>("id", 3);
	fifo.push(item5);
	std::unique_ptr<ITEM> item6 = std::make_unique<ITEM>("id", 4);
	fifo.push(item6);
	std::unique_ptr<ITEM> item7 = std::make_unique<ITEM>("id", 5);
	fifo.push(item7);
	
	std::unique_ptr<ITEM> item8 = std::make_unique<ITEM>("id", 6);
	cout << "Here we try to push another element into the FIFO \n";
	cout << "but it is not possible since fifo_size=5. fifo.push() returns: " << fifo.push(item8) << endl;
	
	cout << "Pulling all elements" << endl;
	fifo.pull(item3);
	cout << item3->_value << endl;
	fifo.pull(item3);
	cout << item3->_value << endl;
	fifo.pull(item3);
	cout << item3->_value << endl;
	fifo.pull(item3);
	cout << item3->_value << endl;
	fifo.pull(item3);
	cout << item3->_value << endl;

	// Here we test if the FIFO is thread-safe
	{
	cout << "\n\nMulti threading test..." << endl;
	cout << "One thread writes into the FIFO and 2 other threads read.\n" << endl;
	
	bigFIFO fifo(20);
	
	// the first thread read from the fifo	
	pthread_t id1;
	pthread_create(&id1, 0, reading, (void *) &fifo);
	
	// the second thread is laso a reader
	pthread_t id3;
	pthread_create(&id3, 0, reading, (void *) &fifo);
	
	// the third thread write to the fifo
	pthread_t id2;
	pthread_create(&id2, 0, writing, (void *) &fifo);

	pthread_join(id1, NULL);
	pthread_join(id2, NULL);
	}
	return 0;
}

void *writing(void *arg){
	bigFIFO* fifo = (bigFIFO*) arg;
	for(unsigned int i=0; i<20; i++){
		std::unique_ptr<ITEM> item = std::make_unique<ITEM>("id", i);
		cout << pthread_self() << " Pushing item: " << i << endl;
		if(fifo->push(item)<0)
			cout << pthread_self() << " fifo is full." << endl;
		usleep(10);
	}
	return NULL;
}

void *reading(void *arg){
	bigFIFO* fifo = (bigFIFO*) arg;
	std::unique_ptr<ITEM> item;
	for(unsigned int i=0; i<20; i++){
		//cout << pthread_self() << " Before pulling.." << endl;
		fifo->pull(item);
		cout << pthread_self() << " Pulled item ------: " << item->_value << endl;
		usleep(1000);
		if(item->_value == 19) exit(0);
	}
}
