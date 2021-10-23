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

		std::atomic<bool> value = { 0 };
	};

	struct ECSENGINE_API Semaphore {
		Semaphore();
		Semaphore(unsigned int target);

		Semaphore(const Semaphore& other) = default;
		Semaphore& operator = (const Semaphore& other) = default;

		void Enter();

		void ClearTarget();

		void ClearCount();

		void Exit();

		void SetTarget(unsigned int value);

		// It will spin wait until the target is reached
		void SpinWait();

		std::atomic<unsigned int> count;
		unsigned int target;
	};

	// must be constructed with operator new or placement new
	struct ECSENGINE_API ConditionVariable {
		ConditionVariable();
		ConditionVariable(unsigned int signal_count);

		void Wait(unsigned int count = 1);
		void Reset();
		void NotifyAll(unsigned int count = 1);
		void Notify(unsigned int count = 1);

		SpinLock lock;
		std::condition_variable_any cv;
		std::atomic<unsigned int> signal_count;

	};

}
