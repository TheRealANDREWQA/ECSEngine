#pragma once
#include "ecspch.h"
#include "../Core.h"

#define ECS_ACQUIRE std::memory_order_acquire
#define ECS_RELEASE std::memory_order_release
#define ECS_RELAXED std::memory_order_relaxed
#define ECS_ACQ_REL std::memory_order_acq_rel
#define ECS_SEQ_CST std::memory_order_seq_cst


namespace ECSEngine {

	struct ECSENGINE_API SpinLock {
		ECS_INLINE SpinLock() {
			value.store(0, ECS_RELAXED);
		}
		ECS_INLINE SpinLock(const SpinLock& other) {
			value.store(other.value.load(ECS_RELAXED), ECS_RELAXED);
		}
		ECS_INLINE SpinLock& operator = (const SpinLock& other) {
			value.store(other.value.load(ECS_RELAXED), ECS_RELAXED);
			return *this;
		}

		ECS_INLINE void clear() {
			value.store(false, ECS_RELAXED);
		}

		void lock();
		
		bool try_lock();
		
		void unlock();

		bool is_locked() const;

		void wait_locked();

		// The thread will use a sleep behaviour if after many tries
		// the lock doesn't get once locked
		void wait_signaled();

		std::atomic<unsigned char> value;
	};

	struct ECSENGINE_API AtomicFlag {
		ECS_INLINE AtomicFlag() {
			Clear();
		}
		ECS_INLINE AtomicFlag(const AtomicFlag& other) {
			value.store(other.value.load(ECS_RELAXED), ECS_RELAXED);
		}
		ECS_INLINE AtomicFlag& operator = (const AtomicFlag& other) {
			value.store(other.value.load(ECS_RELAXED), ECS_RELAXED);
			return *this;
		}

		void Wait();

		ECS_INLINE bool IsWaiting() const {
			return value.load(ECS_RELAXED);
		}

		void Signal();

		ECS_INLINE void Clear() {
			value.store(0, ECS_RELEASE);
		}

		std::atomic<bool> value = false;
	};

	ECSENGINE_API void BitLock(std::atomic<unsigned char>& byte, unsigned char bit_index);

	// It will wake up all threads that wait on this address
	ECSENGINE_API void BitLockWithNotify(std::atomic<unsigned char>& byte, unsigned char bit_index);

	ECSENGINE_API bool BitTryLock(std::atomic<unsigned char>& byte, unsigned char bit_index);

	ECSENGINE_API void BitUnlock(std::atomic<unsigned char>& byte, unsigned char bit_index);

	ECSENGINE_API bool BitIsLocked(const std::atomic<unsigned char>& byte, unsigned char bit_index);

	ECSENGINE_API bool BitIsUnlocked(const std::atomic<unsigned char>& byte, unsigned char bit_index);

	// Waits while the bit is locked (it waits until it gets unlocked).
	// Can enable sleep wait (if the spinning for some time didn't yield a response then it will go to sleep
	// on that address - must be woken using BitLockWithNotify)
	ECSENGINE_API void BitWaitLocked(std::atomic<unsigned char>& byte, unsigned char bit_index, bool sleep_wait = false);

	// Waits while the bit is unlocked (it waits until it gets locked).
	// Can enable sleep wait (if the spinning for some time didn't yield a response then it will go to sleep
	// on that address - must be woken using BitLockWithNotify)
	ECSENGINE_API void BitWaitUnlocked(std::atomic<unsigned char>& byte, unsigned char bit_index, bool sleep_wait = false);

	struct ECSENGINE_API Semaphore {
		ECS_INLINE Semaphore() : count(0), target(0) {}
		ECS_INLINE Semaphore(unsigned int _target) : count(0), target(_target) {}

		ECS_INLINE Semaphore(const Semaphore& other) {
			count.store(other.count.load(ECS_RELAXED), ECS_RELAXED);
			target.store(other.target.load(ECS_RELAXED), ECS_RELAXED);
		}
		ECS_INLINE Semaphore& operator = (const Semaphore& other) {
			count.store(other.count.load(ECS_RELAXED), ECS_RELAXED);
			target.store(other.target.load(ECS_RELAXED), ECS_RELAXED);
			return *this;
		}

		ECS_INLINE void ClearCount() {
			count.store(0, ECS_RELEASE);
		}

		ECS_INLINE void ClearTarget() {
			target.store(0, ECS_RELEASE);
		}

		// Acquire load
		ECS_INLINE unsigned int Count() const {
			return count.load(ECS_ACQUIRE);
		}

		// Releasae store
		ECS_INLINE void SetCount(unsigned int value) {
			count.store(value, ECS_RELEASE);
		}

		// Release store and wakes up waiting threads
		ECS_INLINE void SetCountEx(unsigned int value) {
			count.store(value, ECS_RELEASE);
			WakeByAddressAll(&count);
		}

		// Acquire load
		ECS_INLINE unsigned int Target() const {
			return target.load(ECS_ACQUIRE);
		}

		// Release store
		ECS_INLINE void SetTarget(unsigned int value) {
			target.store(value, ECS_RELEASE);
		}

		// Release store and wakes up waiting threads
		ECS_INLINE void SetTargetEx(unsigned int value) {
			target.store(value, ECS_RELEASE);
			WakeByAddressAll(&count);
		}

		// Returns the previous count value
		unsigned int Enter(unsigned int count = 1);

		// Returns the previous count value
		// The difference between the two variants is that this uses
		// a futex call to wake any waiting threads
		unsigned int EnterEx(unsigned int count = 1);

		// This version check the count value to that of the check_value and if they match
		// Only then it will commit the write (this can be used to avoid generating too many cache writes)
		// Returns the previous count value if it commited else -1
		unsigned int TryEnter(unsigned int check_value, unsigned int count = 1);

		// Returns the previous count value
		unsigned int Exit(unsigned int count = 1);

		// Returns the previous count value
		// The difference between the two variants is that this uses
		// a futex call to wake any waiting threads
		unsigned int ExitEx(unsigned int count = 1);

		// Relaxed load
		ECS_INLINE bool CheckCount(unsigned int value) const {
			return count.load(ECS_RELAXED) == value;
		}

		// Relaxed load
		ECS_INLINE bool CheckTarget(unsigned int value) const {
			return target.load(ECS_RELAXED) == value;
		}

		// Default behaviour - waits until count is the same as target
		// If one of the parameters is not -1, then it will wait until that value is reached
		void SpinWait(unsigned int count_value = -1, unsigned int target_value = -1);

		// One of the parameters needs to be different from -1 and the other -1
		// It can wait only on a single address at a time
		// The difference between the two variants is that this uses
		// a futex call to wait for threads to exit
		void SpinWaitEx(unsigned int count_value, unsigned int target_value);

		// It will check a value at certain intervals. In between it will sleep.
		// Default behaviour - wait until count is the same as target
		// If one of the parameters is not -1, then it will wait until the value is reached
		void TickWait(size_t sleep_milliseconds, unsigned int count_value = -1, unsigned int target_value = -1);

		std::atomic<unsigned int> count;
		std::atomic<unsigned int> target;
	};

	struct ECSENGINE_API ConditionVariable {
		ECS_INLINE ConditionVariable() : signal_count(0) {}
		ECS_INLINE ConditionVariable(int _signal_count) : signal_count(_signal_count) {}

		ECS_INLINE ConditionVariable(const ConditionVariable& other) {
			signal_count.store(other.signal_count.load(ECS_RELAXED), ECS_RELAXED);
		}
		ECS_INLINE ConditionVariable& operator = (const ConditionVariable& other) {
			signal_count.store(other.signal_count.load(ECS_RELAXED), ECS_RELAXED);
			return *this;
		}

		// You can wait to be notified count times
		// If multiple wait are issued with counts different from 1, the order in which they are woken up is
		// from the last one to call wait to the first one
		void Wait(int count = 1);

		// Sets the signal count to 0
		void Reset();

		// Sets the signal count to 0 and notifies any sleeping threads
		void ResetAndNotify();

		// Increments the signal_count by the given count and wakes up all the threads waiting
		// Returns the signal count value before the addition
		int NotifyAll(int count = 1);

		// Increments the signal_count by the given count and wakes up a single thread that is waiting
		// Returns the signal count value before the addition
		int Notify(int count = 1);

		// Returns the number of threads waiting on this variable - basically the signal count
		// If you use wait with a count greater than 1, then this will no longer reflect the exact thread count
		unsigned int WaitingThreadCount() const;

		// Returns the current signal value
		unsigned int SignalCount() const;

		std::atomic<int> signal_count;
	};

	// There can be at max 256 concurrent readers
	struct ECSENGINE_API ReadWriteLock {
		ECS_INLINE ReadWriteLock() : reader_count(0), lock() {}

		ECS_INLINE ReadWriteLock(const ReadWriteLock& other) {
			reader_count.store(other.reader_count.load(ECS_RELAXED), ECS_RELAXED);
			lock = other.lock;
		}
		ECS_INLINE ReadWriteLock& operator = (const ReadWriteLock& other) {
			reader_count.store(other.reader_count.load(ECS_RELAXED), ECS_RELAXED);
			lock = other.lock;
			return *this;
		}

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
		ECS_INLINE ByteReadWriteLock() : count(0) {}

		ECS_INLINE ByteReadWriteLock(const ByteReadWriteLock& other) {
			count.store(other.count.load(ECS_RELAXED), ECS_RELAXED);
		}
		ECS_INLINE ByteReadWriteLock& operator = (const ByteReadWriteLock& other) {
			count.store(other.count.load(ECS_RELAXED), ECS_RELAXED);
			return *this;
		}

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
				return variable.load(ECS_ACQUIRE) < value_to_compare;
			});
		}
		else if constexpr (WaitType == '>') {
			loop([&]() {
				return variable.load(ECS_ACQUIRE) > value_to_compare;
			});
		}
		else if constexpr (WaitType == '=') {
			loop([&]() {
				return variable.load(ECS_ACQUIRE) == value_to_compare;
			});
		}
		else if constexpr (WaitType == '!') {
			loop([&]() {
				return variable.load(ECS_ACQUIRE) != value_to_compare;
			});
		}

#undef SPIN_COUNT_UNTIL_WAIT
	}

	template<char WaitType, typename IntType>
	void TickWait(size_t milliseconds, const std::atomic<IntType>& variable, IntType value_to_compare) {
		const size_t SPIN_COUNTER = 64;

		auto loop = [=](auto condition) {
			while (condition()) {
				for (size_t index = 0; index < SPIN_COUNTER; index++) {
					if (!condition()) {
						return;
					}
					_mm_pause();
				}
				std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
			}
		};

		if constexpr (WaitType == '<') {
			loop([&]() {
				return variable.load(ECS_ACQUIRE) < value_to_compare;
			});
		}
		else if constexpr (WaitType == '>') {
			loop([&]() {
				return variable.load(ECS_ACQUIRE) > value_to_compare;
			});
		}
		else if constexpr (WaitType == '=') {
			loop([&]() {
				return variable.load(ECS_ACQUIRE) == value_to_compare;
			});
		}
		else if constexpr (WaitType == '!') {
			loop([&]() {
				return variable.load(ECS_ACQUIRE) != value_to_compare;
			});
		}
	}

	template<typename Functor>
	ECS_INLINE void ThreadSafeFunctor(SpinLock* spin_lock, Functor&& functor) {
		spin_lock->lock();
		functor();
		spin_lock->unlock();
	}

	template<typename Functor>
	ECS_INLINE auto ThreadSafeFunctorReturn(SpinLock* spin_lock, Functor&& functor) {
		spin_lock->lock();
		auto return_value = functor();
		spin_lock->unlock();
		return return_value;
	}

	struct AsyncCallbackData {
		bool success;
		// Data that you register
		void* data;
		// Data provided by the async action
		void* async_data;
	};

	typedef void (*AsyncCallbackFunction)(AsyncCallbackData* callback_data);

	// Data_size 0 means that the data is stable and doesn't need to be copied
	struct AsyncCallback {
		AsyncCallbackFunction function = nullptr;
		void* data;
		size_t data_size;
	};

}
