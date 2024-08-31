#pragma once
#include "Export.h"

ECS_THREAD_TASK(InitializeDrawPipeline);

template<bool schedule_element>
ECS_THREAD_TASK(DrawMeshes);

// Used by the editor
template<bool schedule_element>
ECS_THREAD_TASK(DrawSelectables);

template<bool schedule_element>
ECS_THREAD_TASK(FlushRenderCommands);

// Used by the editor
template<bool schedule_element>
ECS_THREAD_TASK(DrawInstancedFramebuffer);

ECS_THREAD_TASK(RecalculateCamera);