#pragma once
#include "../Core.h"
#include "../Containers/Stream.h"
#include "../Utilities/Timer.h"

namespace ECSEngine {

	struct FrameProfiler;

	enum ECS_PROFILE_TAG : unsigned char {
		ECS_PROFILE_DEFAULT = 0,
		ECS_PROFILE_WAITING
	};

	// Global variable such that performance measuring macros/functions can simply
	// Refer to this one without having to provide the instance manually for each function
	ECSENGINE_API extern FrameProfiler* ECS_FRAME_PROFILER;

	ECSENGINE_API void FrameProfilerPush(unsigned int thread_id, Stream<char> name, unsigned char tag = 0);

	ECSENGINE_API void FrameProfilerPop(unsigned int thread_id, float value);

	struct FrameProfilingTimer {
		ECS_INLINE FrameProfilingTimer(unsigned int _thread_id, Stream<char> name, unsigned char tag = 0) : thread_id(_thread_id) {
			FrameProfilerPush(thread_id, name, tag);
		}

		~FrameProfilingTimer() {
			// Get the duration in nanoseconds and then transform into microseconds
			size_t nanoseconds = timer.GetDuration(ECS_TIMER_DURATION_NS);
			float microseconds = (float)nanoseconds * 0.001f;
			FrameProfilerPop(thread_id, microseconds);
		}

		Timer timer;
		unsigned int thread_id;
	};

	// These are to be used in conjunction with thread tasks

	// You can give an optional tag, by default it will be 0
#define ECS_PROFILE(name, ...) ECSEngine::FrameProfilingTimer __profiling_timer##__LINE__(thread_id, name, __VA_ARGS__);

	// You can give an optional tag, by default it will be 0
#define ECS_PROFILE_FOR_EACH(name, ...) ECSEngine::FrameProfilingTimer __profiling_timer##__LINE__(for_each_data->thread_id, name, __VA_ARGS__);

}