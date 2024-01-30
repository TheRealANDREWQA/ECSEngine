#include "ecspch.h"
#include "ConcurrentPrimitives.h"
#include "../Utilities/Assert.h"

namespace ECSEngine {

	// This value seems to be the sweet spot for best balance
	// Between CPU usage (through spinning) and latency (waking up a thread
	// has its latency which can be critical to us)
#define GLOBAL_SPIN_COUNT 500

	ECS_INLINE bool IsBitUnlocked(unsigned char byte, unsigned char bit_index) {
		return (byte & ECS_BIT(bit_index)) == 0;
	}

	// ----------------------------------------------------------------------------------------------

	void SpinLock::Lock() {
		while (true) {
			// Optimistically assume the lock is free on the first try
			if (!value.exchange(1, ECS_ACQUIRE)) {
				return;
			}

			WaitLocked();
		}
	}

	void SpinLock::Unlock() {
		value.store(0, ECS_RELEASE);
		WakeByAddressAll(&value);
	}

	bool SpinLock::IsLocked() const
	{
		return value.load(ECS_RELAXED) > 0;
	}

	void SpinLock::WaitLocked() 
	{
		// Wait for lock to be released without generating cache misses
		while (IsLocked()) {
			for (size_t index = 0; index < GLOBAL_SPIN_COUNT; index++) {
				if (!IsLocked()) {
					return;
				}
				// pause instruction to help multithreading
				_mm_pause();
			}

			// Do this in a loop since the WaitOnAddress can wake the thread spuriously
			unsigned char true_var = 1;
			while (value.load(ECS_RELAXED)) {
				BOOL success = WaitOnAddress(&value, &true_var, sizeof(unsigned char), INFINITE);
			}
		}
	}

	void SpinLock::WaitSignaled()
	{
		while (!IsLocked()) {
			for (size_t index = 0; index < GLOBAL_SPIN_COUNT; index++) {
				if (IsLocked()) {
					return;
				}
				_mm_pause();
			}

			// Here we don't want to use a loop since the thread will be woken up
			// when the value becomes 0
			if (!IsLocked()) {
				unsigned char compare_value = 0;
				WaitOnAddress(&value, &compare_value, sizeof(unsigned char), INFINITE);
			}
		}
	}

	void SpinLock::LockNotify()
	{
		while (true) {
			// Optimistically assume the lock is free on the first try
			if (!value.exchange(1, ECS_ACQUIRE)) {
				WakeByAddressAll(&value);
				return;
			}

			WaitLocked();
		}
	}

	bool SpinLock::TryLock() {
		// First do a relaxed load to check if lock is free in order to prevent
		// unnecessary cache misses in while(!try_lock())
		return value.load(ECS_RELAXED) == 0 && value.exchange(1, ECS_ACQUIRE) == 0;
	}

	// ----------------------------------------------------------------------------------------------

	void BitLock(std::atomic<unsigned char>& byte, unsigned char bit_index)
	{
		while (true) {
			unsigned char before = byte.fetch_or(ECS_BIT(bit_index), ECS_ACQUIRE);

			// The lock is free if the bit is cleared
			if (IsBitUnlocked(before, bit_index)) {
				return;
			}

			BitWaitLocked(byte, bit_index);
		}
	}

	void BitLockWithNotify(std::atomic<unsigned char>& byte, unsigned char bit_index)
	{
		BitLock(byte, bit_index);
		WakeByAddressAll(&byte);
	}

	bool BitTryLock(std::atomic<unsigned char>& byte, unsigned char bit_index)
	{
		// First do a relaxed load to check if lock is free in order to prevent
		// unnecessary cache misses in while(!try_lock())
		unsigned char load_value = byte.load(ECS_RELAXED);
		if (IsBitUnlocked(load_value, bit_index)) {
			// Try setting the bit now
			unsigned char exchange_value = byte.fetch_or(ECS_BIT(bit_index), ECS_ACQUIRE);
			return IsBitUnlocked(exchange_value, bit_index);
		}
		return false;
	}

	void BitUnlock(std::atomic<unsigned char>& byte, unsigned char bit_index)
	{
		byte.fetch_and(~ECS_BIT(bit_index), ECS_RELEASE);
		WakeByAddressAll(&byte);
	}

	bool BitIsLocked(const std::atomic<unsigned char>& byte, unsigned char bit_index)
	{
		return !BitIsUnlocked(byte, bit_index);
	}

	bool BitIsUnlocked(const std::atomic<unsigned char>& byte, unsigned char bit_index)
	{
		unsigned char current_value = byte.load(ECS_RELAXED);
		return IsBitUnlocked(current_value, bit_index);
	}

	// Check bit should return true while it should wait
	template<typename CheckBit>
	void BitWaitState(std::atomic<unsigned char>& byte, unsigned char bit_index, bool sleep_wait, CheckBit&& check_bit) {
		const size_t SPIN_COUNT_UNTIL_WAIT = GLOBAL_SPIN_COUNT;

		// Use acquire semantics such that further reads don't cross this barrier

		// Wait for lock to be released without generating cache misses
		while (check_bit(byte.load(ECS_ACQUIRE), bit_index)) {
			for (size_t index = 0; index < SPIN_COUNT_UNTIL_WAIT; index++) {
				if (!check_bit(byte.load(ECS_ACQUIRE), bit_index)) {
					return;
				}
				// pause instruction to help multithreading
				_mm_pause();
			}

			if (sleep_wait) {
				// Do this in a loop since the WaitOnAddress can wake the thread spuriously
				unsigned char current_value = byte.load(ECS_ACQUIRE);
				while (check_bit(current_value, bit_index)) {
					BOOL success = WaitOnAddress(&byte, &current_value, sizeof(unsigned char), INFINITE);
					current_value = byte.load(ECS_ACQUIRE);
				}
			}
		}
	}

	void BitWaitLocked(std::atomic<unsigned char>& byte, unsigned char bit_index, bool sleep_wait)
	{
		BitWaitState(byte, bit_index, sleep_wait, [](unsigned char value, unsigned char bit_index) {
			return !IsBitUnlocked(value, bit_index);
		});
	}

	// ----------------------------------------------------------------------------------------------

	void BitWaitUnlocked(std::atomic<unsigned char>& byte, unsigned char bit_index, bool sleep_wait)
	{
		BitWaitState(byte, bit_index, sleep_wait, [](unsigned char value, unsigned char bit_index) {
			return IsBitUnlocked(value, bit_index);
		});
	}

	// ----------------------------------------------------------------------------------------------

	unsigned int Semaphore::Exit(unsigned int exit_count) {
		unsigned int value = count.fetch_sub(exit_count, ECS_RELEASE);
		return value;
	}

	unsigned int Semaphore::ExitEx(unsigned int exit_count)
	{
		unsigned int value = count.fetch_sub(exit_count, ECS_RELEASE);
		WakeByAddressAll(&count);
		return value;
	}

	unsigned int Semaphore::Enter(unsigned int enter_count) {
		return count.fetch_add(enter_count, ECS_ACQUIRE);
	}

	unsigned int Semaphore::EnterEx(unsigned int enter_count) {
		unsigned int value = count.fetch_add(enter_count, ECS_ACQUIRE);
		WakeByAddressAll(&count);
		return value;
	}

	unsigned int Semaphore::TryEnter(unsigned int check_value, unsigned int enter_count) {
		unsigned int value = count.load(ECS_RELAXED);
		if (value == check_value) {
			// Use a compare exchange
			// We need acquire barrier in case we successfully changed the count
			return count.compare_exchange_weak(value, value + enter_count, ECS_ACQUIRE) ? value : -1;
		}
		else {
			return -1;
		}
	}

	void Semaphore::SpinWait(unsigned int count_value, unsigned int target_value)
	{
		auto loop = [](auto condition) {
			while (condition()) {
				for (size_t index = 0; index < GLOBAL_SPIN_COUNT; index++) {
					if (!condition()) {
						return;
					}
					_mm_pause();
				}
				// Use SwitchToThread to try and give the quantum to a thread on the same core
				BOOL switched = SwitchToThread();
				if (!switched) {
					// Use Sleep(0) to try to give the quantum to a thread on other processors as well
					Sleep(0);
				}
			}
		};

		// Acquire semantics to prevent reads cross this barrier
		if (count_value == -1 && target_value == -1) {
			loop([&]() {
				return count.load(ECS_ACQUIRE) != target.load(ECS_ACQUIRE);
			});
		}
		else if (count_value != -1) {
			loop([&]() {
				return count.load(ECS_ACQUIRE) != count_value;
			});
		}
		else {
			loop([&]() {
				return target.load(ECS_ACQUIRE) != target_value;
			});
		}
	}

	void Semaphore::SpinWaitEx(unsigned int count_value, unsigned int target_value)
	{
		auto loop = [](std::atomic<unsigned int>* wait_address, auto condition) {
			while (condition()) {
				// This value here should mirror
				for (size_t index = 0; index < GLOBAL_SPIN_COUNT; index++) {
					if (!condition()) {
						return;
					}
					_mm_pause();
				}

				// Do this in a loop since the WaitOnAddress can wake the thread spuriously
				// Acquire semantics in order to prevent reads crossing this barrier
				unsigned int current_value = wait_address->load(ECS_ACQUIRE);
				while (condition()) {
					BOOL success = WaitOnAddress(wait_address, &current_value, sizeof(unsigned int), INFINITE);
					current_value = wait_address->load(ECS_ACQUIRE);
				}
			}
		};

		if (count_value != -1) {
			loop(&count, [&]() {
				return count.load(ECS_ACQUIRE) != count_value;
				});
		}
		else {
			loop(&target, [&]() {
				return target.load(ECS_ACQUIRE) != target_value;
			});
		}
	}

	void Semaphore::TickWait(size_t sleep_milliseconds, unsigned int count_value, unsigned int target_value)
	{
		const size_t SPIN_COUNT = 64;

		if (count_value == -1 && target_value == -1) {
			while (count.load(ECS_ACQUIRE) != target.load(ECS_ACQUIRE)) {
				for (size_t index = 0; index < SPIN_COUNT; index++) {
					if (count.load(ECS_ACQUIRE) == target.load(ECS_ACQUIRE)) {
						return;
					}
				}
				std::this_thread::sleep_for(std::chrono::nanoseconds(sleep_milliseconds));
			}
		}
		else if (count_value != -1) {
			ECSEngine::TickWait<'!'>(sleep_milliseconds, count, count_value);
		}
		else {
			ECSEngine::TickWait<'!'>(sleep_milliseconds, target, count_value);
		}
	}

	// ----------------------------------------------------------------------------------------------

	void ConditionVariable::Wait(int count) {
		int initial_count = signal_count.fetch_sub(count, ECS_ACQUIRE);
		int current_count = initial_count - count;
		while (current_count < 0) {
			WaitOnAddress(&signal_count, &current_count, sizeof(int), INFINITE);
			current_count = signal_count.load(ECS_ACQUIRE);
		}
	}

	int ConditionVariable::Notify(int count) {
		int previous_count = signal_count.fetch_add(count, ECS_RELEASE);
		WakeByAddressSingle(&signal_count);
		return previous_count;
	}

	int ConditionVariable::NotifyAll(int count) {
		int previous_count = signal_count.fetch_add(count, ECS_RELEASE);
		WakeByAddressAll(&signal_count);
		return previous_count;
	}

	void ConditionVariable::Reset() {
		signal_count.store(0, ECS_RELAXED);
	}

	void ConditionVariable::ResetAndNotify() {
		// Use release semantics to not have writes cross this barrier
		signal_count.store(0, ECS_RELEASE);
		WakeByAddressAll(&signal_count);
	}

	unsigned int ConditionVariable::WaitingThreadCount() const
	{
		return SignalCount();
	}

	unsigned int ConditionVariable::SignalCount() const
	{
		int current_count = signal_count.load(ECS_RELAXED);
		if (current_count < 0) {
			return -current_count;
		}

		return 0;
	}

	// ----------------------------------------------------------------------------------------------

	void ReadWriteLock::Clear()
	{
		lock.Clear();
		reader_count.store(0, ECS_RELAXED);
	}

	void ReadWriteLock::EnterWrite()
	{
		lock.Lock();
		SpinWait<'!'>(reader_count, (unsigned char)0);
	}

	bool ReadWriteLock::TryEnterWrite()
	{
		bool acquired_lock = lock.TryLock();
		if (acquired_lock) {
			// Spin wait while the readers exit
			SpinWait<'!'>(reader_count, (unsigned char)0);
			return true;
		}
		return false;
	}

	void ReadWriteLock::EnterRead()
	{
		// Test to see if there is a pending write
		lock.WaitLocked();

		// An acquire memory barrier should be enough - we are interested in protecting
		// everything that is being done after the atomic operation
		// The use case is
		// EnterRead()
		// ... stuff here
		// ExitRead()
		reader_count.fetch_add(1, ECS_ACQUIRE);
		// No need to retest again since the writer will wait for us to exit
	}

	void ReadWriteLock::ExitRead()
	{
		// A single release barrier should be enough since we are interested
		// in protecting all operations before the atomic
		reader_count.fetch_sub(1, ECS_RELEASE);
	}

	bool ReadWriteLock::IsLocked() const
	{
		return lock.IsLocked();
	}

	void ReadWriteLock::ExitWrite()
	{
		lock.Unlock();
	}

	bool ReadWriteLock::TransitionReadToWriteLock()
	{
		// We don't care about the return of this
		// We should need to make sure that the lock is held
		bool is_locked_by_us = lock.TryLock();

		unsigned char reader_locked_check_value = ECS_BIT(7) | ECS_BIT(0);

		// Try to lock the secondary lock
		// If we don't get it, it means someone else tried to transition
		// and got before us and we need to give up our reader count
		// And acquire the reader lock and writer lock after that
		// Firstly acquire the writer lock in order to not allow other writers to come in
		bool reader_lock_gained = BitTryLock(reader_count, 7);
		if (!reader_lock_gained) {
			ExitRead();
			// We also need to acquire the write lock if we haven't previously
			if (!is_locked_by_us) {
				lock.Lock();
				is_locked_by_us = true;
			}

			// Acquire the reader lock
			BitLock(reader_count, 7);

			// We also need to change the compare value - since we already gave up our reader lock
			reader_locked_check_value = ECS_BIT(7);
		}

		// Wait for all the other readers to exit
		SpinWait<'!'>(reader_count, reader_locked_check_value);

		// This works since if another thread tries to enter in write mode
		// It will be blocked until reader count gets to 0
		// And since we locked the top bit it will have to wait for us

		return is_locked_by_us;
	}

	void ReadWriteLock::ReadToWriteUnlock(bool was_locked_by_us)
	{
		// We need to update the reader count before unlocking the main lock

		// The reader count should be 1, need to make it 0
		// Use a simple write for this
		reader_count.store(0, ECS_RELEASE);

		if (was_locked_by_us) {
			lock.Unlock();
		}
	}

	// ----------------------------------------------------------------------------------------------

	void ByteReadWriteLock::Clear()
	{
		count.store(0, ECS_RELAXED);
	}

	void ByteReadWriteLock::EnterWrite()
	{
		BitLock(count, 7);
		SpinWait<'!'>(count, (unsigned char)ECS_BIT(7));
	}

	bool ByteReadWriteLock::TryEnterWrite()
	{
		bool acquired_lock = BitTryLock(count, 7);
		if (acquired_lock) {
			// Spin wait while the readers exit
			SpinWait<'!'>(count, (unsigned char)ECS_BIT(7));
			return true;
		}
		return false;
	}

	void ByteReadWriteLock::EnterRead()
	{
		// Test to see if there is a pending write
		BitWaitLocked(count, 7);
		unsigned int previous_count = count.fetch_add(1, ECS_RELEASE);
		// No need to retest again since the writer will wait for us to exit

		// Assert that there are not too many readers at the same time
		ECS_ASSERT(previous_count < ECS_BIT(6) - 1);
	}

	void ByteReadWriteLock::ExitRead()
	{
		count.fetch_sub(1, ECS_RELEASE);
	}

	bool ByteReadWriteLock::IsLocked() const
	{
		return BitIsLocked(count, 7);
	}

	void ByteReadWriteLock::ExitWrite()
	{
		BitUnlock(count, 7);
	}

	bool ByteReadWriteLock::TransitionReadToWriteLock()
	{
		// Lock the normal writer lock
		bool is_locked_by_us = BitTryLock(count, 7);

		unsigned char reader_value_to_check = ECS_BIT(7) | ECS_BIT(6) | ECS_BIT(0);

		// Try to lock the secondary lock
		// If we don't get it, it means someone else tried to transition
		// and got before us and we need to give up our reader count
		// And acquire the reader lock and writer lock after that
		// Firstly acquire the writer lock in order to not allow other writers to come in
		bool reader_lock_acquired = BitTryLock(count, 6);
		if (!reader_lock_acquired) {
			// Give up our read lock
			ExitRead();

			// Acquire the writer lock if we haven't previously
			if (!is_locked_by_us) {
				BitLock(count, 7);
				is_locked_by_us = true;
			}

			// Acquire the reader lock
			BitLock(count, 6);

			// Also change the compare value since we gave up our
			reader_value_to_check = ECS_BIT(7) | ECS_BIT(6);
		}

		// If multiple threads try to enter write from read at the same time 
		// it is fine since, when exiting, the thread will write ECS_BIT(7)
		// into the value which will preserve the lock bit
		SpinWait<'!'>(count, reader_value_to_check);
		return is_locked_by_us;
	}

	void ByteReadWriteLock::ReadToWriteUnlock(bool was_locked_by_us)
	{
		if (was_locked_by_us) {
			// Store 0 directly into the count
			count.store(0, ECS_RELEASE);
		}
		else {
			// Store ECS_BIT(7) instead
			count.store(ECS_BIT(7), ECS_RELEASE);
		}
	}

	// ----------------------------------------------------------------------------------------------

	void AtomicFlag::Wait()
	{
		// Here we would normally need ACQUIRE barrier in order to not have reads be moved before this
		// value, but it is incompatible with the store, the same goes for ACQ_REL.
		// Here, we could use ECS_RELEASE since we will be using a load with acquire inside the
		// while anyway and would be enough to ensure correct access for reads
		value.store(true, ECS_RELEASE);

		// Do this in a loop since the WaitOnAddress can wake the thread spuriously
		bool current_value = true;
		while (current_value) {
			BOOL success = WaitOnAddress(&value, &current_value, sizeof(unsigned char), INFINITE);
			current_value = value.load(ECS_ACQUIRE);
		}
	}

	// ----------------------------------------------------------------------------------------------

	void AtomicFlag::Signal()
	{
		value.store(false, ECS_RELEASE);
		WakeByAddressAll(&value);
	}

	// ----------------------------------------------------------------------------------------------
}