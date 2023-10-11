#pragma once
#include "Export.h"

template<bool schedule_element>
ECS_THREAD_TASK(RenderTask);

ECS_THREAD_TASK(RenderTaskInitialize);

// Used by the editor
template<bool schedule_element>
ECS_THREAD_TASK(RenderSelectables);

ECS_THREAD_TASK(RenderSelectablesInitialize);

template<bool schedule_element>
ECS_THREAD_TASK(RenderFlush);

// Used by the editor
template<bool schedule_element>
ECS_THREAD_TASK(RenderInstancedFramebuffer);

ECS_THREAD_TASK(RenderCameraRecalculate);