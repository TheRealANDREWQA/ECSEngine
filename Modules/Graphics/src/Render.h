#pragma once
#include "Export.h"

template<bool schedule_element>
ECS_THREAD_TASK(RenderTask);

ECS_THREAD_TASK(RenderTaskInitialize);