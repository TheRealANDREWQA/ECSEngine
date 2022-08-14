#pragma once
#include "ecspch.h"
#include "../../Core.h"

#ifndef ECS_ACQUIRE
#define ECS_ACQUIRE std::memory_order_acquire
#endif

#ifndef ECS_RELEASE
#define ECS_RELEASE std::memory_order_release
#endif

#ifndef ECS_RELAXED
#define ECS_RELAXED std::memory_order_relaxed
#endif

#ifndef ECS_ACQ_REL
#define ECS_ACQ_REL std::memory_order_acq_rel
#endif

#ifndef ECS_SEQ_CST
#define ECS_SEQ_CST std::memory_order_seq_cst
#endif


namespace ECSEngine {

	struct ECSENGINE_API SpinLock {
		SpinLock& operator = (const SpinLock& other);

		void lock();
		
		bool try_lock();
		
		void unlock();

		bool is_locked() const;

		void wait_locked();

		// The thread will use a sleep behaviour if after many tries
		// the lock doesn't get once locked
		void wait_signaled();

		std::atomic<unsigned char> value = { 0 };
	};

	ECSENGINE_API void BitLock(std::atomic<unsigned char>& byte, unsigned char bit_index);

	ECSENGINE_API bool BitTryLock(std::atomic<unsigned char>& byte, unsigned char bit_index);

	ECSENGINE_API void BitUnlock(std::atomic<unsigned char>& byte, unsigned char bit_index);

	ECSENGINE_API bool BitIsLocked(const std::atomic<unsigned char>& byte, unsigned char bit_index);

	ECSENGINE_API void BitWaitLocked(std::atomic<unsigned char>& byte, unsigned char bit_index);

	struct ECSENGINE_API Semaphore {
		Semaphore();
		Semaphore(unsigned int target);

		Semaphore(const Semaphore& other);
		Semaphore& operator = (const Semaphore& other);

		unsigned int Enter(unsigned int count = 1);

		void ClearTarget();

		void ClearCount();

		unsigned int Exit(unsigned int count = 1);

		// It will spin wait until the target is reached
		// Default behaviour - waits until count is the same as target
		// If one of the values is not -1, then it will wait until that value is reached
		void SpinWait(unsigned int count_value = -1, unsigned int target_value = -1);

		// It will check the value at certain intervals. In between it will sleep.
		// Default behaviour - wait until count is the same as target
		// If one of the values is not -1, then it will wait until the value is reached
		void TickWait(size_t sleep_microseconds, unsigned int count_value = -1, unsigned int target_value = -1);

		std::atomic<unsigned int> count;
		std::atomic<unsigned int> target;
	};

	struct ECSENGINE_API ConditionVariable {
		ConditionVariable();
		ConditionVariable(int signal_count);

		// You can wait to be notified count times
		// If multiple wait are issued with counts different from 1, the order in which they are woken up is
		// from the last one to call wait to the first one
		void Wait(int count = 1);

		// Sets the signal count to 0
		void Reset();

		// Increments the signal_count by the given count and wakes up all the threads waiting
		void NotifyAll(int count = 1);

		// Increments the signal_count by the given count and wakes up a single thread that is waiting
		void Notify(int count = 1);

		// Returns the number of threads waiting on this variable
		unsigned int WaitingThreadCount();

		std::atomic<int> signal_count;
	};

	// There can be at max 256 concurrent readers
	struct ECSENGINE_API ReadWriteLock {
		ReadWriteLock() : reader_count(0), lock() {}

		void Clear();

		void EnterWrite();

		// Returns true if the lock was acquired
		bool TryEnterWrite();

		void EnterRead();

		void ExitRead();

		bool IsLocked() const;

		void ExitWrite();

		// It uses the highest bit of the reader_count as another lock
		// It will allow you to write exclusively even when there are other
		// readers or normal writers.
		// It returns a bool that must be given back to the ReadToWriteUnlock call
		// There can be at max 128 concurrent readers for this function
		bool TransitionReadToWriteLock();

		// The bool needs to be given from the return of the Transition call
		// There can be at max 128 concurrent readers for this function
		void ReadToWriteUnlock(bool was_locked_by_us);
		
		std::atomic<unsigned char> reader_count;
		SpinLock lock;
	};

	// The highest bit (the 7th) is used for the writer lock
	// It still uses the WaitOnAddress Win32 function for sleep resolution upon spinning too much
	// There can be at max 128 concurrent readers
	struct ECSENGINE_API ByteReadWriteLock {
		ByteReadWriteLock() : count(0) {}

		void Clear();

		void EnterWrite();

		// Returns true if the lock was acquired
		bool TryEnterWrite();

		void EnterRead();

		void ExitRead();

		bool IsLocked() const;

		void ExitWrite();

		// It uses the 6th of the reader_count as another lock
		// It will allow you to write exclusively even when there are other
		// readers or normal writers.
		// It returns a bool that must be given back to the ReadToWriteUnlock call
		// There should be at max 64 concurrent readers for this to work!!!
		bool TransitionReadToWriteLock();

		// The bool needs to be given from the return of the Transition call
		// There should be at max 64 concurrent readers for this to work!!!
		void ReadToWriteUnlock(bool was_locked_by_us);

		std::atomic<unsigned char> count;
	};

	// This version of the SpinWait doesn't use the WaitOnAddress Win32 function that is 
	// "comparable" with a linux futex because we when the value is changed nobody will wake us up. 
	// Instead, it uses SwitchToThread and Sleep to try to give the thread's quantum to other threads to reduce usage.
	template<char WaitType, typename IntType>
	void SpinWait(const std::atomic<IntType>& variable, IntType value_to_compare) {
#define SPIN_COUNT_UNTIL_WAIT (2'000)

		auto loop = [](auto check_condition) {
			while (check_condition()) {
				for (size_t index = 0; index < SPIN_COUNT_UNTIL_WAIT; index++) {
					if (!check_condition()) {
						return;
					}
					_mm_pause();
				}
				// Try to switch to another thread on the same hardware CPU
				BOOL switched = SwitchToThread();
				if (!switched) {
					// If that failed, try to give the quantum to a thread on other CPU's aswell
					Sleep(0);
				}
			}
		};

		if constexpr (WaitType == '<') {
			loop([&]() {
				return variable.load(ECS_RELAXED) < value_to_compare;
			});
		}
		else if constexpr (WaitType == '>') {
			loop([&]() {
				return variable.load(ECS_RELAXED) > value_to_compare;
			});
		}
		else if constexpr (WaitType == '=') {
			loop([&]() {
				return variable.load(ECS_RELAXED) == value_to_compare;
			});
		}
		else if constexpr (WaitType == '!') {
			loop([&]() {
				return variable.load(ECS_RELAXED) != value_to_compare;
			});
		}

#undef SPIN_COUNT_UNTIL_WAIT
	}

	template<char WaitType, typename IntType>
	void TickWait(size_t microseconds, const std::atomic<IntType>& variable, IntType value_to_compare) {
		const size_t SPIN_COUNTER = 64;

		auto loop = [=](auto condition) {
			while (condition()) {
				for (size_t index = 0; index < SPIN_COUNTER; index++) {
					if (!condition()) {
						return;
					}
					_mm_pause();
				}
				std::this_thread::sleep_for(std::chrono::microseconds(microseconds));
			}
		};

		if constexpr (WaitType == '<') {
			loop([&]() {
				return variable.load(ECS_RELAXED) < value_to_compare;
			});
		}
		else if constexpr (WaitType == '>') {
			loop([&]() {
				return variable.load(ECS_RELAXED) > value_to_compare;
			});
		}
		else if constexpr (WaitType == '=') {
			loop([&]() {
				return variable.load(ECS_RELAXED) == value_to_compare;
			});
		}
		else if constexpr (WaitType == '!') {
			loop([&]() {
				return variable.load(ECS_RELAXED) != value_to_compare;
			});
		}
	}

}
