#pragma once

// These macros can be set on certain fields to indicate to the reflection system what type of handle that is
#define ECS_MESH_HANDLE
#define ECS_TEXTURE_HANDLE
#define ECS_GPU_SAMPLER_HANDLE

// This allows any shader to be bound to the given handle
#define ECS_SHADER_HANDLE

// Can specify the shader type directly such that is restricted to that type only
#define ECS_VERTEX_SHADER_HANDLE
#define ECS_PIXEL_SHADER_HANDLE
#define ECS_COMPUTE_SHADER_HANDLE

#define ECS_MATERIAL_HANDLE
#define ECS_MISC_HANDLE