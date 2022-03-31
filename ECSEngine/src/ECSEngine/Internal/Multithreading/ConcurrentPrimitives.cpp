#include "ecspch.h"
#include "ConcurrentPrimitives.h"

namespace ECSEngine {

	SpinLock& SpinLock::operator=(const SpinLock& other) {
		value.store(other.value.load(ECS_ACQUIRE), ECS_RELEASE);
		return *this;
	}

	void SpinLock::lock() {
		while (true) {
			// Optimistically assume the lock is free on the first try
			if (!value.exchange(true, ECS_ACQUIRE)) {
				return;
			}
			// Wait for lock to be released without generating cache misses
			while (value.load(ECS_RELAXED)) {
				// pause instruction to help multithreading
				_mm_pause();
			}
		}
	}

	void SpinLock::unlock() {
		value.store(false, ECS_RELEASE);
	}

	bool SpinLock::is_locked() const
	{
		return value.load(ECS_RELAXED);
	}

	void SpinLock::wait_locked() const
	{
		while (is_locked()) {
			_mm_pause();
		}
	}

	bool SpinLock::try_lock() {
		// First do a relaxed load to check if lock is free in order to prevent
		// unnecessary cache misses in while(!try_lock())
		return !value.load(ECS_RELAXED) && !value.exchange(true, ECS_ACQUIRE);
	}

	Semaphore::Semaphore() : count(0), target(0) {}

	Semaphore::Semaphore(unsigned int _target) : count(0), target(_target) {}

	unsigned int Semaphore::Exit() {
		return count.fetch_sub(1, ECS_ACQ_REL);
	}

	unsigned int Semaphore::Enter() {
		return count.fetch_add(1, ECS_ACQ_REL);
	}

	void Semaphore::ClearCount() {
		count.store(0, ECS_RELEASE);
	}

	void Semaphore::ClearTarget() {
		target = 0;
	}

	void Semaphore::SetTarget(unsigned int value) {
		target = value;
	}

	void Semaphore::SpinWait()
	{
		while (count.load(ECS_ACQUIRE) != target) {
			_mm_pause();
		}
	}

	void Semaphore::TickWait(size_t sleep_nanoseconds)
	{
		while (count.load(ECS_ACQUIRE) != target) {
			std::this_thread::sleep_for(std::chrono::nanoseconds(sleep_nanoseconds));
		}
	}

	ConditionVariable::ConditionVariable() : signal_count(0) {}

	ConditionVariable::ConditionVariable(unsigned int _signal_count) : signal_count(_signal_count) {}

	void ConditionVariable::Wait(unsigned int count) {
		cv.wait(lock, [=]() {
			return signal_count.load(ECS_ACQUIRE) >= count;
		});
		signal_count.store(0, ECS_RELEASE);
		lock.unlock();
	}

	void ConditionVariable::Notify(unsigned int count) {
		signal_count.fetch_add(count, ECS_ACQ_REL);
		std::lock_guard<SpinLock> lock_guard(lock);
		cv.notify_one();
	}

	void ConditionVariable::NotifyAll(unsigned int count) {
		signal_count.fetch_add(count, ECS_ACQ_REL);
		std::lock_guard<SpinLock> lock_guard(lock);
		cv.notify_all();
	}

	void ConditionVariable::Reset() {
		lock.unlock();
		signal_count.store(0, ECS_RELEASE);
	}


	bool ReadWriteLock::EnterWrite()
	{
		unsigned int count = write_lock.load(ECS_RELAXED);
		if (count == 0) {
			unsigned int index = write_lock.fetch_add(ECS_RELAXED);
			if (index == 0) {
				// Spin while the readers exit
				while (reader_count.load(ECS_ACQUIRE)) {
					_mm_pause();
				}
				return true;
			}
			else {
				write_lock.fetch_sub(ECS_RELAXED);
			}
		}
		return false;
	}

	void ReadWriteLock::EnterRead()
	{
		// Test to see if there is a pending write
		while (IsLocked()) {
			_mm_pause();
		}
		reader_count.fetch_add(1);
	}

	void ReadWriteLock::ExitRead()
	{
		reader_count.fetch_sub(1);
	}

	bool ReadWriteLock::IsLocked() const
	{
		return write_lock.load(ECS_RELAXED) > 0;
	}

	void ReadWriteLock::ExitWrite()
	{
		write_lock.fetch_sub(ECS_ACQ_REL);
	}

}