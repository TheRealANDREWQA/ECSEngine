#pragma once
#include "../Core.h"
#include "../Containers/Stream.h"
#include "Timer.h"
#include "BasicTypes.h"

namespace ECSEngine {
	
	struct BenchmarkOptions {
		float buffer_step_jump = 2.0f;
		float spike_tolerancy = 35.0f; // Percentage
		int64_t timed_run = 0; // In Milliseconds
		int64_t timed_run_elapsed = -1;
		size_t step_index = 0;
		size_t min_step_count = 6;
		size_t max_step_count = 20;
		size_t iteration_count = 0;
		size_t iteration_index;
		size_t element_size = 1;
		size_t reserved;
	};

	struct ECSENGINE_API BenchmarkState {
		struct EmptyInitialize {
			void operator()(Stream<void> buffer) {}
		};

		BenchmarkState() : names(nullptr) {}
		BenchmarkState(AllocatorPolymorphic allocator, size_t functor_count, const BenchmarkOptions* options = nullptr, Stream<char>* names = nullptr);

		void EndTimer();

		// Given such that the data can be constructed here
		Stream<void> GetIterationBuffer();
		
		// The buffer that has the same data but changes for different functors
		Stream<void> GetCurrentBuffer();

		void GetString(CapacityStream<char>& report, bool verbose = true) const;

		bool KeepRunning();

		bool Run();

		void StartTimer();
		
		// Used for the optimizer to not throw away your result
		void DoNotOptimize(size_t value);

		Stream<char>* names;
		CapacityStream<ResizableStream<size_t>> durations;
		Timer timer;
		BenchmarkOptions options;
		Stream<void> iteration_buffer;
		Stream<void> current_buffer;
	};

}