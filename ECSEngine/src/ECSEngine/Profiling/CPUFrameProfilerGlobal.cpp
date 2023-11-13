#include "ecspch.h"
#include "CPUFrameProfilerGlobal.h"
#include "CPUFrameProfiler.h"

namespace ECSEngine {

	CPUFrameProfiler* ECS_CPU_FRAME_PROFILER;

	void ChangeCPUFrameProfilerGlobal(CPUFrameProfiler* profiler)
	{
		ECS_CPU_FRAME_PROFILER = profiler;
	}

	void CPUFrameProfilerPush(unsigned int thread_id, Stream<char> name, unsigned char tag) {
		ECS_CPU_FRAME_PROFILER->Push(thread_id, name, tag);
	}

	void CPUFrameProfilerPop(unsigned int thread_id, float value) {
		ECS_CPU_FRAME_PROFILER->Pop(thread_id, value);
	}

}