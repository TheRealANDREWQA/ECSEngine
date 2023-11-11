#include "ecspch.h"
#include "FrameProfilerGlobal.h"
#include "FrameProfiler.h"

namespace ECSEngine {

	FrameProfiler* ECS_FRAME_PROFILER;

	void FrameProfilerPush(unsigned int thread_id, Stream<char> name, unsigned char tag) {
		ECS_FRAME_PROFILER->Push(thread_id, name, tag);
	}

	void FrameProfilerPop(unsigned int thread_id, float value) {
		ECS_FRAME_PROFILER->Pop(thread_id, value);
	}

}