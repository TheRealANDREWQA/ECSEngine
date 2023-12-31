#pragma once
#include "../../Core.h"

namespace ECSEngine {

	// We don't include the axes here since it is only a wrapper around 3 arrows
	enum DebugPrimitive : unsigned char {
		ECS_DEBUG_PRIMITIVE_LINE,
		ECS_DEBUG_PRIMITIVE_SPHERE,
		ECS_DEBUG_PRIMITIVE_POINT,
		ECS_DEBUG_PRIMITIVE_RECTANGLE,
		ECS_DEBUG_PRIMITIVE_CROSS,
		ECS_DEBUG_PRIMITIVE_CIRCLE,
		ECS_DEBUG_PRIMITIVE_ARROW,
		ECS_DEBUG_PRIMITIVE_TRIANGLE,
		ECS_DEBUG_PRIMITIVE_AABB,
		ECS_DEBUG_PRIMITIVE_OOBB,
		ECS_DEBUG_PRIMITIVE_STRING,
		ECS_DEBUG_PRIMITIVE_GRID,
		ECS_DEBUG_PRIMITIVE_COUNT
	};

	enum DebugShaders : unsigned char {
		ECS_DEBUG_SHADER_TRANSFORM,
		ECS_DEBUG_SHADER_STRUCTURED_INSTANCED_DATA,
		ECS_DEBUG_SHADER_NORMAL_SINGLE_DRAW,
		ECS_DEBUG_SHADER_COUNT
	};

	enum DebugShaderOutput : unsigned char {
		ECS_DEBUG_SHADER_OUTPUT_COLOR,
		ECS_DEBUG_SHADER_OUTPUT_ID,
		ECS_DEBUG_SHADER_OUTPUT_COUNT
	};

	enum DebugVertexBuffers : unsigned char {
		ECS_DEBUG_VERTEX_BUFFER_SPHERE,
		ECS_DEBUG_VERTEX_BUFFER_POINT,
		ECS_DEBUG_VERTEX_BUFFER_CROSS,
		ECS_DEBUG_VERTEX_BUFFER_ARROW,
		ECS_DEBUG_VERTEX_BUFFER_CUBE,
		ECS_DEBUG_VERTEX_BUFFER_COUNT
	};

	// Use the DebugVertexBuffers enum to index into this
	ECSENGINE_API extern const wchar_t* ECS_DEBUG_PRIMITIVE_MESH_FILE[ECS_DEBUG_VERTEX_BUFFER_COUNT];

	enum DebugRasterizerStates {
		ECS_DEBUG_RASTERIZER_SOLID,
		ECS_DEBUG_RASTERIZER_WIREFRAME,
		ECS_DEBUG_RASTERIZER_SOLID_CULL,
		ECS_DEBUG_RASTERIZER_COUNT
	};

}