#pragma once
#include "../Core.h"
#include "../Containers/Stream.h"
#include "../Utilities/Timer.h"

namespace ECSEngine {

	struct CPUFrameProfiler;

	enum ECS_CPU_PROFILE_TAG : unsigned char {
		ECS_CPU_PROFILE_DEFAULT = 0,
		ECS_CPU_PROFILE_WAITING
	};

	// Global variable such that performance measuring macros/functions can simply
	// Refer to this one without having to provide the instance manually for each function
	ECSENGINE_API extern CPUFrameProfiler* ECS_CPU_FRAME_PROFILER;

	ECSENGINE_API void ChangeCPUFrameProfilerGlobal(CPUFrameProfiler* profiler);

	ECSENGINE_API void CPUFrameProfilerPush(unsigned int thread_id, Stream<char> name, unsigned char tag = 0);

	ECSENGINE_API void CPUFrameProfilerPop(unsigned int thread_id, float value);

	struct CPUFrameProfilingTimer {
		ECS_INLINE CPUFrameProfilingTimer(unsigned int _thread_id, Stream<char> name, unsigned char tag = 0) : thread_id(_thread_id) {
			CPUFrameProfilerPush(thread_id, name, tag);
		}

		ECS_INLINE ~CPUFrameProfilingTimer() {
			CPUFrameProfilerPop(thread_id, timer.GetDurationFloat(ECS_TIMER_DURATION_US));
		}

		Timer timer;
		unsigned int thread_id;
	};

	// These are to be used in conjunction with thread tasks

	// You can give an optional tag, by default it will be 0
#define ECS_CPU_PROFILE(name, ...) ECSEngine::CPUFrameProfilingTimer __profiling_timer##__LINE__(thread_id, name, __VA_ARGS__);

	// You can give an optional tag, by default it will be 0
#define ECS_CPU_PROFILE_FOR_EACH(name, ...) ECSEngine::CPUFrameProfilingTimer __profiling_timer##__LINE__(for_each_data->thread_id, name, __VA_ARGS__);

}