#include <cstdlib>
#include <iostream>
#include <fstream>
#include <queue>
#include "thread.h"
using namespace std;

int *board; //Array of orders on the board
int maxOrders; //Maximum number of orders
int numCurOrders; //Current orders on the board
int numCashiers; //Number of cashiers
int liveCashiers; //Number of cashiers still serving
int *done; //Indicates which cashiers are done
unsigned int boardLock = 1; //Lock to control access to the board
unsigned int printLock = 2; //Lock to control access to printing
unsigned int returned = 1; //Signals that a spot has opened up on the board
unsigned int added = 2; //Signals that an order has been added to the board

void printBoard(); //For debugging

//Stores order file and cashier number
struct cashierInfo_t {
	char *file;
	int number;
};

int findClosestOrder(int lastOrder) {
	int closestOrderCashier = 0;
	int closestOrder = 2001;
	for(int i=0; i<numCashiers; i=i+1) {
		if(board[i] >= 0 and abs(lastOrder - board[i]) < abs(lastOrder - closestOrder)) {
			closestOrderCashier = i;
			closestOrder = board[i];
		}
	}
	return closestOrderCashier;
}

//For debugging
void printBoard() {
	for(int i=0; i<numCashiers; i=i+1) {
		cout << board[i] << " ";
	}
	cout << endl;
}

//Cashier thread
void *cashier(void *arg) {
	cashierInfo_t* info = (cashierInfo_t*) arg;
	char *myFile = info->file;
	int cashierNum = info->number;
	free(info);
	ifstream orderFile (myFile);
	int order;
	//Parsing orders one by one
	queue<int> orderQueue;
	while(orderFile >> order) {
		orderQueue.push(order);
	}
	while(!orderQueue.empty()) {
		thread_lock(boardLock);
		//If an order is already placed by cashier or if the board is full, wait
		while(board[cashierNum] >= 0 or numCurOrders == maxOrders) {
			thread_wait(boardLock, returned);
		}
		//Update board with order
		board[cashierNum] = orderQueue.front();
		orderQueue.pop();
		numCurOrders = numCurOrders + 1;
		//Print out order
		thread_lock(printLock);
		cout << "POSTED: cashier " << cashierNum << " sandwich " << board[cashierNum] << endl;
		thread_unlock(printLock);
		//Signal that an order has been added
		thread_signal(boardLock, added);
		//Checks if thread finishes
		if(orderQueue.empty()) {
			done[cashierNum] = 1;
		}
		thread_unlock(boardLock);
	}
}

//Maker thread
void *maker(void *arg) {
	char **arguments = (char**) arg;
	//Create cashier threads
	for(int i=0; i<liveCashiers; i=i+1) {
		cashierInfo_t* cashierInit = (cashierInfo_t*) malloc(sizeof(cashierInfo_t));
		cashierInit->file = arguments[i+2];
		cashierInit->number = i;
		thread_create((thread_startfunc_t) cashier, cashierInit);
	}
	//Keep track of the last sandwich made
	int lastSandwich = -1;
	//Process orders
	while(1) {
		thread_lock(boardLock);
		//Stop processing when all orders and cashiers are done	
		if(numCurOrders == 0 and liveCashiers == 0) {		
			break;
		}
		//The number of orders for a full board is min(maxOrders, liveCashiers)
		int fullBoard;
		if(maxOrders < liveCashiers) {
			fullBoard = maxOrders;
		}
		else {
			fullBoard = liveCashiers;
		}
		//If board is not full, wait
		while(numCurOrders < fullBoard) {
			thread_wait(boardLock, added);
		}
		//Process order
		int nextCashier = findClosestOrder(lastSandwich);
		lastSandwich = board[nextCashier];
		//Update live cashiers count if thread is done
		if(done[nextCashier] == 1) {
			liveCashiers = liveCashiers - 1;
		}
		//Update board
		board[nextCashier] = -1;
		numCurOrders = numCurOrders - 1;
		//Signal that an order has been processed
		thread_broadcast(boardLock, returned);
		thread_lock(printLock);
		cout << "READY: cashier " << nextCashier << " sandwich " << lastSandwich << endl;
		thread_unlock(printLock);
		thread_unlock(boardLock);
	}
	//Free board
	free(board);
}

int main(int argc, char *argv[]) {
	maxOrders = atoi(argv[1]);
	numCurOrders = 0;
	numCashiers = argc-2;
	liveCashiers = numCashiers;
	board = (int*) malloc(numCashiers*sizeof(int));
	done = (int*) malloc(numCashiers*sizeof(int));
	for(int i=0; i < numCashiers; i=i+1) {
		board[i] = -1; //-1 means that the cashier's slot on the board is free
		done[i] = 0; //0 means not done, 1 means done
	}
	thread_libinit((thread_startfunc_t) maker, argv);
	return 0;
}