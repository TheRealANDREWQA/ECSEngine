#pragma once

#define ECS_THREAD_TASK_TEMPLATE_BOOL_EXPORT(user_api, task_name) template user_api void task_name<false>(unsigned int, ECSEngine::World*, void*); \
template user_api void task_name<true>(unsigned int, ECSEngine::World*, void*)
#define ECS_THREAD_TASK_TEMPLATE_BOOL(task_name) template void task_name<false>(unsigned int, ECSEngine::World*, void*); \
template void task_name<true>(unsigned int, ECSEngine::World*, void*)