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

	bool SpinLock::try_lock() {
		// First do a relaxed load to check if lock is free in order to prevent
		// unnecessary cache misses if someone does while(!try_lock())
		return !value.load(ECS_RELAXED) && !value.exchange(true, ECS_ACQUIRE);
	}

	Semaphore::Semaphore() : count(0), target(0) {}

	Semaphore::Semaphore(unsigned int value) : count(0), target(value) {}

	Semaphore& Semaphore::operator = (const Semaphore& other) {
		target = other.target;
		count.store(other.count.load(ECS_ACQUIRE), ECS_RELEASE);
		return *this;
	}

	void Semaphore::Exit() {
		count.fetch_sub(1, ECS_ACQ_REL);
	}

	void Semaphore::Enter() {
		count.fetch_add(1, ECS_ACQ_REL);
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

	ConditionVariable::ConditionVariable() : signal_count(0) {}

	ConditionVariable::ConditionVariable(unsigned int _signal_count) : signal_count(_signal_count) {}

	void ConditionVariable::Wait(unsigned int count) {
		unsigned int current_count = signal_count.load(ECS_ACQUIRE);
		cv.wait(lock, [&]() {
			return (signal_count.load(ECS_ACQUIRE) - current_count) >= count;
		});
		signal_count.store(0, ECS_RELEASE);
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
		signal_count.store(0, ECS_RELEASE);
	}


}