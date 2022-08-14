#include "ecspch.h"
#include "ConcurrentPrimitives.h"

namespace ECSEngine {

	// ----------------------------------------------------------------------------------------------

	SpinLock& SpinLock::operator=(const SpinLock& other) {
		value.store(other.value.load(ECS_ACQUIRE), ECS_RELEASE);
		return *this;
	}

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
		const size_t SPIN_COUNT_UNTIL_WAIT = 2'000;

		// Wait for lock to be released without generating cache misses
		while (is_locked()) {
			for (size_t index = 0; index < SPIN_COUNT_UNTIL_WAIT; index++) {
				if (is_locked()) {
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
		const size_t SPIN_COUNT_UNTIL_WAIT = 2'000;

		while (!is_locked()) {
			for (size_t index = 0; index < SPIN_COUNT_UNTIL_WAIT; index++) {
				if (!is_locked()) {
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

	bool IsBitUnlocked(unsigned char byte, unsigned char bit_index) {
		return (byte & ECS_BIT(bit_index)) == 0;
	}

	void BitLock(std::atomic<unsigned char>& byte, unsigned char bit_index)
	{
		while (true) {
			// Optimistically assume the lock is free on the first try
			unsigned char before = byte.fetch_or(ECS_BIT(bit_index), ECS_ACQUIRE);

			// The lock is free if the 7th bit is cleared
			if (IsBitUnlocked(before, bit_index)) {
				return;
			}

			BitWaitLocked(byte, bit_index);
		}
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
		unsigned char value = byte.load(ECS_RELAXED);
		return !IsBitUnlocked(value, bit_index);
	}

	void BitWaitLocked(std::atomic<unsigned char>& byte, unsigned char bit_index)
	{
		const size_t SPIN_COUNT_UNTIL_WAIT = 2'000;

		// Wait for lock to be released without generating cache misses
		while (BitIsLocked(byte, bit_index)) {
			for (size_t index = 0; index < SPIN_COUNT_UNTIL_WAIT; index++) {
				if (BitIsLocked(byte, bit_index)) {
					return;
				}
				// pause instruction to help multithreading
				_mm_pause();
			}

			// Do this in a loop since the WaitOnAddress can wake the thread spuriously
			unsigned char locked_value = 0x80;

			unsigned char current_value = byte.load(ECS_RELAXED);
			while (!IsBitUnlocked(byte.load(ECS_RELAXED), bit_index)) {
				BOOL success = WaitOnAddress(&byte, &current_value, sizeof(unsigned char), INFINITE);
			}
		}
	}

	// ----------------------------------------------------------------------------------------------

	Semaphore::Semaphore() : count(0), target(0) {}

	Semaphore::Semaphore(unsigned int _target) : count(0), target(_target) {}

	Semaphore::Semaphore(const Semaphore& other)
	{
		count.store(other.count.load(ECS_RELAXED), ECS_RELAXED);
		target.store(other.target.load(ECS_RELAXED), ECS_RELAXED);
	}

	Semaphore& Semaphore::operator=(const Semaphore& other)
	{
		count.store(other.count.load(ECS_RELAXED), ECS_RELAXED);
		target.store(other.target.load(ECS_RELAXED), ECS_RELAXED);
		return *this;
	}

	unsigned int Semaphore::Exit(unsigned int exit_count) {
		return count.fetch_sub(exit_count, ECS_ACQ_REL);
	}

	unsigned int Semaphore::Enter(unsigned int enter_count) {
		return count.fetch_add(enter_count, ECS_ACQ_REL);
	}

	void Semaphore::ClearCount() {
		count.store(0, ECS_RELEASE);
	}

	void Semaphore::ClearTarget() {
		target.store(0, ECS_RELEASE);
	}

	void Semaphore::SpinWait(unsigned int count_value, unsigned int target_value)
	{
#define SPIN_COUNT_UNTIL_SLEEP (2'000)

		auto loop = [](auto condition) {
			while (condition()) {
				for (size_t index = 0; index < SPIN_COUNT_UNTIL_SLEEP; index++) {
					if (!condition()) {
						return;
					}
					_mm_pause();
				}
				// Use SwitchToThread to try and give the quantum to a thread on the same process
				BOOL switched = SwitchToThread();
				if (!switched) {
					// Use Sleep(0) to try to give the quantum to a thread on other processors aswell
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

#undef SPIN_COUNT_UNTIL_SLEEP
	}

	void Semaphore::TickWait(size_t sleep_microseconds, unsigned int count_value, unsigned int target_value)
	{
		const size_t SPIN_COUNT = 64;

		if (count_value == -1 && target_value == -1) {
			while (count.load(ECS_ACQUIRE) != target.load(ECS_ACQUIRE)) {
				for (size_t index = 0; index < SPIN_COUNT; index++) {
					if (count.load(ECS_ACQUIRE) == target.load(ECS_ACQUIRE)) {
						return;
					}
				}
				std::this_thread::sleep_for(std::chrono::nanoseconds(sleep_microseconds));
			}
		}
		else if (count_value != -1) {
			ECSEngine::TickWait<'!'>(sleep_microseconds, count, count_value);
		}
		else {
			ECSEngine::TickWait<'!'>(sleep_microseconds, target, count_value);
		}
	}

	// ----------------------------------------------------------------------------------------------

	ConditionVariable::ConditionVariable() : signal_count(0) {}

	ConditionVariable::ConditionVariable(int _signal_count) : signal_count(_signal_count) {}

	void ConditionVariable::Wait(int count) {
		int initial_count = signal_count.fetch_sub(count, ECS_RELAXED);
		int current_count = initial_count - count;
		while (signal_count.load(ECS_RELAXED) < 0) {
			WaitOnAddress(&signal_count, &current_count, sizeof(unsigned int), INFINITE);
			current_count = signal_count.load(ECS_RELAXED);
		}
	}

	void ConditionVariable::Notify(int count) {
		signal_count.fetch_add(count, ECS_RELAXED);
		WakeByAddressSingle(&signal_count);
	}

	void ConditionVariable::NotifyAll(int count) {
		signal_count.fetch_add(count, ECS_RELAXED);
		WakeByAddressAll(&signal_count);
	}

	void ConditionVariable::Reset() {
		signal_count.store(0, ECS_RELAXED);
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
		reader_count.fetch_add(1, ECS_RELEASE);
		// No need to retest again since the writer will wait for us to exit
	}

	void ReadWriteLock::ExitRead()
	{
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
}