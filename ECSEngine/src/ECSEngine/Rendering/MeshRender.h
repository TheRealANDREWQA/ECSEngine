#pragma once
#include "../Core.h"
#include "RenderingStructures.h"

namespace ECSEngine {

	struct Graphics;

	ECSENGINE_API void CreateCubeVertexBuffer(Graphics* graphics, float positive_span, VertexBuffer& vertex_buffer, IndexBuffer& index_buffer);

}