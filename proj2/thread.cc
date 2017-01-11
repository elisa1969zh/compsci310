#include <cstdlib>
#include <ucontext.h>
#include <iostream>
#include <deque>
#include <map>
#include "thread.h"
#include "interrupt.h"
using namespace std;

struct condition_t {
	unsigned int associatedLock;
	unsigned int conditionVar;
	//Need to add this comparator so that condition_t can be used in a map
	bool operator<(const condition_t& o) const {
		if(associatedLock == o.associatedLock) {
			return conditionVar < o.conditionVar;
		}
		else {
			return associatedLock < o.associatedLock;
		}
	}
};

deque<ucontext_t*> readyQueue; //Queue of threads approved to run
ucontext_t* current; //Current running thread
map<unsigned int, ucontext_t*> lockHolder; //Maps locks to the thread that holds the lock
map<unsigned int, deque<ucontext_t*> > lockMap; //Maps locks to a lock queue
map<condition_t, deque<ucontext_t*> > conditionMap; //Maps condition variables to a wait queue
bool initialized = false; //Stores whether or not thread_libinit has been called
deque<ucontext_t*> toDelete; //Hacky way to delete threads after they finish

//For debugging purposes
int printQueues() {
//	cout << "Current thread: " << current << endl;
	for(map<unsigned int, ucontext_t*>::iterator it=lockHolder.begin(); it!=lockHolder.end(); ++it) {
		cout << "Lock " << it->first << "->" << it->second << endl;
	}
	cout << "Ready queue: ";
	for(deque<ucontext_t*>::iterator it=readyQueue.begin(); it!=readyQueue.end(); ++it) {
		cout << *it << " -> ";
	}
	cout << "end" << endl;
	return 0;
}

void printCurrent(int t) {
	cout << t << " printing current context: " << current << endl;
}

//Switches to the next context in the ready queue and saves current context
void switchNext() {
	ucontext_t* currentContext = current;
	if (readyQueue.empty()){
		if(!toDelete.empty()) {
			ucontext_t* deleting = toDelete.back();
			toDelete.pop_back();
			delete [] (char*) deleting->uc_stack.ss_sp;
			delete deleting;
//			cout << "DELETE" << endl;
		}
		cout << "Thread library exiting." << endl;
		exit(0);
	}
	ucontext_t* nextContext = readyQueue.back();
	readyQueue.pop_back();
	current = nextContext;
//	cout << "Current thread: " << current << endl;
	swapcontext(currentContext, nextContext);
}

//Stub helper function to start new threads
void *start(thread_startfunc_t func, void *arg) {
	//Run function
	interrupt_enable();
//	cout << "func start: " << arg << endl;
	func(arg);
//	cout << "func finished in start" << endl;
	interrupt_disable();

	//FREE THE THREAD'S STACK AND THEN FREE THE THREAD
	//If function returns, run next thread
	if(readyQueue.empty()) {

		if(!toDelete.empty()) {
			ucontext_t* deleting = toDelete.back();
			toDelete.pop_back();
			delete [] (char*) deleting->uc_stack.ss_sp;
			delete deleting;
//			cout << "DELETE" << endl;
		}
		cout << "Thread library exiting.\n";
		exit(0);
	} 
	else {
		if(!toDelete.empty()) {
			ucontext_t* deleting = toDelete.back();
			toDelete.pop_back();
			delete [] (char*) deleting->uc_stack.ss_sp;
			delete deleting;
//			cout << "DELETE" << endl;
		}
		toDelete.push_front(current);
		
		ucontext_t* nextContext = readyQueue.back();
		readyQueue.pop_back();
		//ucontext_t* currentContext = new ucontext_t();
		current = nextContext;
		setcontext(nextContext);
	}
}

int thread_libinit(thread_startfunc_t func, void *arg) {
	interrupt_disable();
	//If thread_libinit has already been called, return error
	if(initialized) {
		interrupt_enable();
		return -1;
	}
	initialized = true;
	try {
		//Create context of initial thread and push it onto the ready queue
		ucontext_t* initial_thread = new ucontext_t();
		getcontext(initial_thread);
		char *stack = new char[STACK_SIZE];
	//	cout << "FIRST" << endl;
		initial_thread->uc_stack.ss_sp = stack;
		initial_thread->uc_stack.ss_size = STACK_SIZE;
		initial_thread->uc_stack.ss_flags = 0;
		initial_thread->uc_link = NULL;
		makecontext(initial_thread, (void (*)()) start, 2, func, arg);
		//Run initial thread
		current = initial_thread;
		//ucontext_t* original = new ucontext_t();
		setcontext(initial_thread);		
	}
	catch (exception& e) {
		interrupt_enable();
		return -1;		
	}
//	cout << "libinit" << endl;
	interrupt_enable();
	return -1;
}

int thread_create(thread_startfunc_t func, void *arg) {
	interrupt_disable();
	//If thread_libinit hasn't been called yet, return error
	if(!initialized) {
		interrupt_enable();
		return -1;
	}
	try {
		ucontext_t* new_thread = new ucontext_t();
		getcontext(new_thread);
		char *stack = new char[STACK_SIZE];
		new_thread->uc_stack.ss_sp = stack;
		new_thread->uc_stack.ss_size = STACK_SIZE;
		new_thread->uc_stack.ss_flags = 0;
		new_thread->uc_link = NULL;
		makecontext(new_thread, (void (*)()) start, 2, func, arg);
		readyQueue.push_front(new_thread);
	}
	catch (exception& e) {
		interrupt_enable();
		return -1;
	}
	
	//Create context of new thread and push in onto the ready queue
	
//	cout << "NEW" << endl;
	
//	cout << "new thread: " << new_thread << endl;
	interrupt_enable();
	return 0;
}

int thread_yield(void) {
	//If thread_libinit hasn't been called yet, return error
	interrupt_disable();
	if(!initialized) {
		interrupt_enable();
		return -1;
	}
	//Add current context to ready queue and swap into next context on the ready queue
	//	cout << "YIELDED" << endl;
	readyQueue.push_front(current);
//	cout << "Yield" << endl;
	switchNext();
	interrupt_enable();
	return 0;
}

int helper_lock(unsigned int lock) {
	//If lock is not taken, give context the lock
//	cout << "CURRENT HOLDER: " << current << endl;
	if(lockHolder.find(lock) == lockHolder.end()) {
		lockHolder.insert(pair<unsigned int, ucontext_t*>(lock, current));
//		cout << "LOCK GIVEN: " << current << endl;
	}
	//Otherwise, add the context to the lock queue
	else {
		//If lock is already held by current context, return error
		if(lockHolder.at(lock) == current) {
			return -1;
		}
		//If there is no existing lock queue for the lock, make one
		if(lockMap.find(lock) == lockMap.end()) {
//			cout << "hi" << endl;
			deque<ucontext_t*> lockQueue;
			lockQueue.push_front(current);
			lockMap.insert(pair<unsigned int, deque<ucontext_t*> >(lock, lockQueue));
		}
		//Otherwise use existing lock queue
		else {
//			cout << "ho" << endl;
			deque<ucontext_t*> lockQueue = lockMap.at(lock);
			lockMap.erase(lock);
			lockQueue.push_front(current);
			lockMap.insert(pair<unsigned int, deque<ucontext_t*> >(lock, lockQueue));
		}
//		cout << "lock" << endl;
		switchNext();
	}
	return 0;
}

int thread_lock(unsigned int lock) {
	interrupt_disable();
	//If thread_libinit hasn't been called yet, return error
	if(!initialized) {
		interrupt_enable();
		return -1;
	}

	int val = helper_lock(lock);

	interrupt_enable();
	return val;
}

int helper_unlock(unsigned int lock) {
	//If lock is not currently locked, return -1
	if (lockHolder.find(lock) == lockHolder.end()) {
		return -1;
	}

	//Otherwise, unlock the lock and give it to the next context in the lock queue
	else {
		if(lockHolder.at(lock) == current) {
//			cout << "UNLOCK SUCCESSFUL: " << endl;
			lockHolder.erase(lock);
			//Give lock to next context in the lock queue
			if(lockMap.find(lock) != lockMap.end()) {
				deque<ucontext_t*> lockQueue = lockMap.at(lock);
				if(!lockQueue.empty()) {
					//Move new lock holder to the ready queue
					ucontext_t* newHolder = lockQueue.back();
//					cout << "NEW HOLDER OF LOCK" << newHolder << endl;
					readyQueue.push_front(newHolder);				
					//Update lock queue
					lockQueue.pop_back();
					lockMap.erase(lock);
					lockMap.insert(pair<unsigned int, deque<ucontext_t*> >(lock, lockQueue));	
					lockHolder.insert(pair<unsigned int, ucontext_t*>(lock, newHolder));
				}
			}
			return 0;
		}
		//If current thread does not hold the lock, return error
		else {
//			cout << "UNLOCK UNSUCCESSFUL" << endl;
			return -1;
		}
	}
	return 0;
	
}

int thread_unlock(unsigned int lock) {
	interrupt_disable();
	//If thread_libinit hasn't been called yet, return error
	if(!initialized) {
		interrupt_enable();
		return -1;
	}

	int val = helper_unlock(lock);

	interrupt_enable();
	return val;
	
}

int thread_wait(unsigned int lock, unsigned int cond) {
	
	interrupt_disable();
	//If thread_libinit hasn't been called yet, return error
	if(!initialized) {
		interrupt_enable();
		return -1;
	}

	if(lockHolder.find(lock) == lockHolder.end()) {
		interrupt_enable();
		return -1;
	}
	if(lockHolder.at(lock) != current) {
		interrupt_enable();
		return -1;
	}

	//Unlock thread
	int unlockVal = helper_unlock(lock);
//	cout << "WAIT CALLED: THREAD UNLOCKED" << endl;

	condition_t condition = {lock, cond};
	//Put thread into the wait queue for the CV
	if(conditionMap.find(condition) == conditionMap.end()) {
//		cout << "PUT IN WAIT QUEUE (1): " << current << endl;
		deque<ucontext_t*> waitQueue;
		waitQueue.push_front(current);
		conditionMap.insert(pair<condition_t, deque<ucontext_t*> >(condition, waitQueue));
	}
	else {
//		cout << "PUT IN WAIT QUEUE (2): " << current << endl;
		deque<ucontext_t*> waitQueue = conditionMap.at(condition);
		conditionMap.erase(condition);
		waitQueue.push_front(current);
		conditionMap.insert(pair<condition_t, deque<ucontext_t*> >(condition, waitQueue));
	}
	//Switch in next thread
//	cout << "wait" << endl;
	switchNext();

	//this code restarts here
//	cout << "CURRENT THREAD: " << current << endl;
	int lockVal = helper_lock(lock);
	interrupt_enable();
	return 0;
}

int thread_signal(unsigned int lock, unsigned int cond) {
	interrupt_disable();
	//If thread_libinit hasn't been called yet, return error
	if(!initialized) {
		interrupt_enable();
		return -1;
	}
//	cout << "THREAD SIGNALLED" << endl;
	condition_t condition = {lock, cond};
	if(conditionMap.find(condition) != conditionMap.end()) {
		deque<ucontext_t*> waitQueue = conditionMap.at(condition);
		if(!waitQueue.empty()) {
			ucontext_t* wokenUp = waitQueue.back();
			waitQueue.pop_back();
			conditionMap.erase(condition);
			if(!waitQueue.empty()) {
				conditionMap.insert(pair<condition_t, deque<ucontext_t*> >(condition, waitQueue));
			}
			//Add woken up thread to lock queue
// 			if(lockMap.find(lock) == lockMap.end()) {
// 				deque<ucontext_t*> lockQueue;
// 				lockQueue.push_front(wokenUp);
// 				lockMap.insert(pair<unsigned int, deque<ucontext_t*> >(lock, lockQueue));
// //				cout << "THREAD WOKEN (1): "  << wokenUp << endl;
// 			}
// 			else {
// 				deque<ucontext_t*> lockQueue = lockMap.at(lock);
// 				lockMap.erase(lock);
// 				lockQueue.push_front(wokenUp);
// 				lockMap.insert(pair<unsigned int, deque<ucontext_t*> >(lock, lockQueue));
// //				cout << "THREAD WOKEN (2): "  << wokenUp << endl;
// 			}
			//add thread to readyqueue, to run
			readyQueue.push_front(wokenUp);
		}
	}
	interrupt_enable();
	return 0;
}

int thread_broadcast(unsigned int lock, unsigned int cond) {
	interrupt_disable();
	//If thread_libinit hasn't been called yet, return error
	if(!initialized) {
		interrupt_enable();
		return -1;
	}
//	cout << "THREAD BROADCAST" << endl;
	condition_t condition = {lock, cond};
	while(conditionMap.find(condition) != conditionMap.end()) {
		deque<ucontext_t*> waitQueue = conditionMap.at(condition);
		if(!waitQueue.empty()) {
			ucontext_t* wokenUp = waitQueue.back();
			waitQueue.pop_back();
			conditionMap.erase(condition);
			if(!waitQueue.empty()) {
				conditionMap.insert(pair<condition_t, deque<ucontext_t*> >(condition, waitQueue));
			}
			//Add woken up thread to lock queue
			// if(lockMap.find(lock) == lockMap.end()) {
			// 	deque<ucontext_t*> lockQueue;
			// 	lockQueue.push_front(wokenUp);
			// 	lockMap.insert(pair<unsigned int, deque<ucontext_t*> >(lock, lockQueue));
			// }
			// else {
			// 	deque<ucontext_t*> lockQueue = lockMap.at(lock);
			// 	lockMap.erase(lock);
			// 	lockQueue.push_front(wokenUp);
			// 	lockMap.insert(pair<unsigned int, deque<ucontext_t*> >(lock, lockQueue));
			// }
			readyQueue.push_front(wokenUp);
		}
//		cout << "THREADS WOKEN" << endl;
	}
	interrupt_enable();
	return 0;
}