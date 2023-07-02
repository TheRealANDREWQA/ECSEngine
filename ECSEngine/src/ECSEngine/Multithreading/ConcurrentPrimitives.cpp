#include "ecspch.h"
#include "ConcurrentPrimitives.h"

namespace ECSEngine {

#define GLOBAL_SPIN_COUNT 1'000

	ECS_INLINE bool IsBitUnlocked(unsigned char byte, unsigned char bit_index) {
		return (byte & ECS_BIT(bit_index)) == 0;
	}

	// ----------------------------------------------------------------------------------------------

	void SpinLock::lock() {
		while (true) {
			// Optimistically assume the lock is free on the first try
			if (!value.exchange(1, ECS_ACQUIRE)) {
				return;
			}

			wait_locked();
		}
	}

	void SpinLock::unlock() {
		value.store(0, ECS_RELEASE);
		WakeByAddressAll(&value);
	}

	bool SpinLock::is_locked() const
	{
		return value.load(ECS_RELAXED) > 0;
	}

	void SpinLock::wait_locked()
	{
		const size_t SPIN_COUNT_UNTIL_WAIT = GLOBAL_SPIN_COUNT;

		// Wait for lock to be released without generating cache misses
		while (is_locked()) {
			for (size_t index = 0; index < SPIN_COUNT_UNTIL_WAIT; index++) {
				if (!is_locked()) {
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

	void SpinLock::wait_signaled()
	{
		const size_t SPIN_COUNT_UNTIL_WAIT = GLOBAL_SPIN_COUNT;

		while (!is_locked()) {
			for (size_t index = 0; index < SPIN_COUNT_UNTIL_WAIT; index++) {
				if (is_locked()) {
					return;
				}
				_mm_pause();
			}

			// Here we don't want to use a loop since the thread will be woken up
			// when the value becomes 0
			if (!is_locked()) {
				unsigned char compare_value = 0;
				WaitOnAddress(&value, &compare_value, sizeof(unsigned char), INFINITE);
			}
		}
	}

	bool SpinLock::try_lock() {
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

		// Wait for lock to be released without generating cache misses
		while (check_bit(byte.load(ECS_RELAXED), bit_index)) {
			for (size_t index = 0; index < SPIN_COUNT_UNTIL_WAIT; index++) {
				if (!check_bit(byte.load(ECS_RELAXED), bit_index)) {
					return;
				}
				// pause instruction to help multithreading
				_mm_pause();
			}

			if (sleep_wait) {
				// Do this in a loop since the WaitOnAddress can wake the thread spuriously
				unsigned char current_value = byte.load(ECS_RELAXED);
				while (check_bit(current_value, bit_index)) {
					BOOL success = WaitOnAddress(&byte, &current_value, sizeof(unsigned char), INFINITE);
					current_value = byte.load(ECS_RELAXED);
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
		return count.fetch_sub(exit_count, ECS_RELEASE);
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
			return Enter(enter_count);
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
				// Use SwitchToThread to try and give the quantum to a thread on the same process
				BOOL switched = SwitchToThread();
				if (!switched) {
					// Use Sleep(0) to try to give the quantum to a thread on other processors as well
					Sleep(0);
				}
			}
		};

		if (count_value == -1 && target_value == -1) {
			loop([&]() {
				return count.load(ECS_ACQUIRE) != target.load(ECS_ACQUIRE);
			});
		}
		else if (count_value != -1) {
			loop([&]() {
				return count.load(ECS_RELAXED) != count_value;
			});
		}
		else {
			loop([&]() {
				return target.load(ECS_RELAXED) != target_value;
			});
		}
	}

	void Semaphore::SpinWaitEx(unsigned int count_value, unsigned int target_value)
	{
		auto loop = [](std::atomic<unsigned int>* wait_address, auto condition) {
			while (condition()) {
				for (size_t index = 0; index < GLOBAL_SPIN_COUNT; index++) {
					if (!condition()) {
						return;
					}
					_mm_pause();
				}
				
				// Do this in a loop since the WaitOnAddress can wake the thread spuriously
				unsigned int current_value = wait_address->load(ECS_RELAXED);
				while (condition()) {
					BOOL success = WaitOnAddress(wait_address, &current_value, sizeof(unsigned int), INFINITE);
					current_value = wait_address->load(ECS_RELAXED);
				}
			}
		};

		if (count_value != -1) {
			loop(&count, [&]() {
				return count.load(ECS_RELAXED) != count_value;
				});
		}
		else {
			loop(&target, [&]() {
				return target.load(ECS_RELAXED) != target_value;
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
		int initial_count = signal_count.fetch_sub(count, ECS_ACQ_REL);
		int current_count = initial_count - count;
		while (current_count < 0) {
			WaitOnAddress(&signal_count, &current_count, sizeof(int), INFINITE);
			current_count = signal_count.load(ECS_RELAXED);
		}
	}

	void ConditionVariable::Notify(int count) {
		signal_count.fetch_add(count, ECS_ACQ_REL);
		WakeByAddressSingle(&signal_count);
	}

	void ConditionVariable::NotifyAll(int count) {
		signal_count.fetch_add(count, ECS_ACQ_REL);
		WakeByAddressAll(&signal_count);
	}

	void ConditionVariable::Reset() {
		signal_count.store(0, ECS_RELAXED);
	}

	void ConditionVariable::ResetAndNotify() {
		Reset();
		WakeByAddressAll(&signal_count);
	}

	unsigned int ConditionVariable::WaitingThreadCount()
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
		lock.unlock();
		reader_count.store(0, ECS_RELAXED);
	}

	void ReadWriteLock::EnterWrite()
	{
		lock.lock();
		SpinWait<'!'>(reader_count, (unsigned char)0);
	}

	bool ReadWriteLock::TryEnterWrite()
	{
		bool acquired_lock = lock.try_lock();
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
		lock.wait_locked();

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
		return lock.is_locked();
	}

	void ReadWriteLock::ExitWrite()
	{
		lock.unlock();
	}

	bool ReadWriteLock::TransitionReadToWriteLock()
	{
		// We don't care about the return of this
		// We should need to make sure that the lock is held
		bool is_locked_by_us = lock.try_lock();

		// Lock the secondary lock
		BitLock(reader_count, 7);

		// Wait for all the other readers to exit
		SpinWait<'!'>(reader_count, (unsigned char)ECS_BIT(7));

		return is_locked_by_us;
	}

	void ReadWriteLock::ReadToWriteUnlock(bool was_locked_by_us)
	{
		if (was_locked_by_us) {
			lock.unlock();
		}

		// The reader count should be 1, need to make it 0
		// Use a simple write for this
		reader_count.store(0, ECS_RELEASE);
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
		count.fetch_add(1, ECS_RELEASE);
		// No need to retest again since the writer will wait for us to exit
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

		// Lock the secondary lock
		BitLock(count, 6);

		SpinWait<'!'>(count, (unsigned char)(ECS_BIT(7) | ECS_BIT(6)));
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

	void AtomicFlag::wait()
	{
		value.store(true, ECS_RELAXED);

		// Do this in a loop since the WaitOnAddress can wake the thread spuriously
		bool current_value = true;
		while (current_value) {
			BOOL success = WaitOnAddress(&value, &current_value, sizeof(unsigned char), INFINITE);
			current_value = value.load(ECS_RELAXED);
		}
	}

	void AtomicFlag::signal()
	{
		value.store(false, ECS_RELAXED);
	}

	// ----------------------------------------------------------------------------------------------
}