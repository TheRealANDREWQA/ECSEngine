#include "ecspch.h"
#include "AllocatorBase.h"
#include "AllocatorCallsDebug.h"
#include "../Profiling/AllocatorProfilingGlobal.h"

namespace ECSEngine {

	void AllocatorBase::SetDebugMode(const char* name, bool resizable)
	{
		m_debug_mode = true;
		DebugAllocatorManagerChangeOrAddEntry(this, name, resizable, m_allocator_type);
	}

	void AllocatorBase::SetProfilingMode(const char* name)
	{
		m_profiling_mode = true;
		AllocatorProfilingAddEntry(this, m_allocator_type, name);
	}

}