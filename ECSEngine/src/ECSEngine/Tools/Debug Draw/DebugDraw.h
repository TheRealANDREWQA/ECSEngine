#pragma once
#include "..\..\Core.h"
#include "..\..\Rendering\Graphics.h"
#include "..\..\Allocators\MemoryManager.h"
#include "..\..\Math\Matrix.h"
#include "..\..\Containers\Deck.h"
#include "..\..\Internal\Multithreading\ConcurrentPrimitives.h"
#include "..\..\Rendering\RenderingStructures.h"

namespace ECSEngine {

	struct ResourceManager;
	
	enum DebugPrimitive {
		ECS_DEBUG_PRIMITIVE_LINE,
		ECS_DEBUG_PRIMITIVE_SPHERE,
		ECS_DEBUG_PRIMITIVE_POINT,
		ECS_DEBUG_PRIMITIVE_RECTANGLE,
		ECS_DEBUG_PRIMITIVE_CROSS,
		ECS_DEBUG_PRIMITIVE_CIRCLE,
		ECS_DEBUG_PRIMITIVE_ARROW,
		ECS_DEBUG_PRIMITIVE_AXES,
		ECS_DEBUG_PRIMITIVE_TRIANGLE,
		ECS_DEBUG_PRIMITIVE_AABB,
		ECS_DEBUG_PRIMITIVE_OOBB,
		ECS_DEBUG_PRIMITIVE_STRING,
		ECS_DEBUG_PRIMITIVE_COUNT
	};

	enum DebugShaders {
		ECS_DEBUG_SHADER_TRANSFORM,
		ECS_DEBUG_SHADER_STRUCTURED_INSTANCED_DATA,
		ECS_DEBUG_SHADER_NORMAL_SINGLE_DRAW,
		ECS_DEBUG_SHADER_COUNT
	};

	enum DebugVertexBuffers {
		ECS_DEBUG_VERTEX_BUFFER_SPHERE,
		ECS_DEBUG_VERTEX_BUFFER_POINT,
		ECS_DEBUG_VERTEX_BUFFER_CROSS,
		ECS_DEBUG_VERTEX_BUFFER_ARROW_CYLINDER,
		ECS_DEBUG_VERTEX_BUFFER_ARROW_HEAD,
		ECS_DEBUG_VERTEX_BUFFER_CUBE,
		ECS_DEBUG_VERTEX_BUFFER_COUNT
	};

	enum DebugRasterizerStates {
		ECS_DEBUG_RASTERIZER_SOLID,
		ECS_DEBUG_RASTERIZER_WIREFRAME,
		ECS_DEBUG_RASTERIZER_SOLID_CULL,
		ECS_DEBUG_RASTERIZER_COUNT
	};

	using DebugVertex = float3;

	struct DebugDrawCallOptions {
		bool wireframe = true;
		bool ignore_depth = false;
		float duration = 0.0f;
	};

	struct DebugLine {
		float3 start;
		float3 end;
		ColorFloat color;
		DebugDrawCallOptions options;
	};

	struct DebugSphere {
		float3 position;
		float radius;
		ColorFloat color;
		DebugDrawCallOptions options;
	};

	struct DebugPoint {
		float3 position;
		ColorFloat color;
		DebugDrawCallOptions options;
	};

	struct DebugRectangle {
		float3 corner0;
		float3 corner1;
		ColorFloat color;
		DebugDrawCallOptions options;
	};

	struct DebugCross {
		float3 position;
		float3 rotation;
		float size;
		ColorFloat color;
		DebugDrawCallOptions options;
	};

	struct DebugCircle {
		float3 position;
		float3 rotation;
		float radius;
		ColorFloat color;
		DebugDrawCallOptions options;
	};

	struct DebugArrow {
		float3 translation;
		float3 rotation;
		float length;
		float size;
		ColorFloat color;
		DebugDrawCallOptions options;
	};

	struct DebugAxes {
		float3 translation;
		float3 rotation;
		float size;
		ColorFloat color_x;
		ColorFloat color_y;
		ColorFloat color_z;
		DebugDrawCallOptions options;
	};

	struct DebugTriangle {
		float3 point0;
		float3 point1;
		float3 point2;
		ColorFloat color;
		DebugDrawCallOptions options;
	};

	struct DebugAABB {
		float3 translation;
		float3 scale;
		ColorFloat color;
		DebugDrawCallOptions options;
	};

	struct DebugOOBB {
		float3 translation;
		float3 rotation;
		float3 scale;
		ColorFloat color;
		DebugDrawCallOptions options;
	};

	struct DebugString {
		float3 position;
		float3 direction;
		float size;
		Stream<char> text;
		ColorFloat color;
		DebugDrawCallOptions options;
	};

	struct ECSENGINE_API DebugDrawer {
		DebugDrawer() : allocator(nullptr), graphics(nullptr) {}
		DebugDrawer(MemoryManager* allocator, Graphics* graphics, size_t thread_count);

		DebugDrawer(const DebugDrawer& other) = default;
		DebugDrawer& operator = (const DebugDrawer& other) = default;

#pragma region Add to the draw queue - single threaded

		void AddLine(float3 start, float3 end, ColorFloat color, DebugDrawCallOptions options = {});

		void AddSphere(float3 position, float radius, ColorFloat color, DebugDrawCallOptions options = {});

		void AddPoint(float3 position, ColorFloat color, DebugDrawCallOptions options = {false});

		// Corner0 is the top left corner, corner1 is the bottom right corner
		void AddRectangle(float3 corner0, float3 corner1, ColorFloat color, DebugDrawCallOptions options = {});

		void AddCross(float3 position, float3 rotation, float size, ColorFloat color, DebugDrawCallOptions options = {false});

		void AddCircle(float3 position, float3 rotation, float radius, ColorFloat color, DebugDrawCallOptions options = {});

		void AddArrow(float3 start, float3 end, float size, ColorFloat color, DebugDrawCallOptions options = {false});
		
		void AddArrowRotation(float3 translation, float3 rotation, float length, float size, ColorFloat color, DebugDrawCallOptions options = {false});

		void AddAxes(float3 translation, float3 rotation, float size, ColorFloat color_x, ColorFloat color_y, ColorFloat color_z, DebugDrawCallOptions options = {false});

		void AddTriangle(float3 point0, float3 point1, float3 point2, ColorFloat color, DebugDrawCallOptions options = {});

		void AddAABB(float3 translation, float3 scale, ColorFloat color, DebugDrawCallOptions options = {});

		void AddOOBB(float3 translation, float3 rotation, float3 scale, ColorFloat color, DebugDrawCallOptions options = {});

		void AddString(float3 position, float3 direction, float size, containers::Stream<char> text, ColorFloat color, DebugDrawCallOptions options = { false });

		void AddStringRotation(float3 position, float3 rotation, float size, containers::Stream<char> text, ColorFloat color, DebugDrawCallOptions options = { false });

#pragma endregion

#pragma region Add to the draw queue - multi threaded

		void AddLineThread(float3 start, float3 end, ColorFloat color, unsigned int thread_index, DebugDrawCallOptions options = {});

		void AddSphereThread(float3 position, float radius, ColorFloat color, unsigned int thread_index, DebugDrawCallOptions options = {});

		void AddPointThread(float3 position, ColorFloat color, unsigned int thread_index, DebugDrawCallOptions options = {false});

		// Corner0 is the top left corner, corner1 is the bottom right corner
		void AddRectangleThread(float3 corner0, float3 corner1, ColorFloat color, unsigned int thread_index, DebugDrawCallOptions options = {});

		void AddCrossThread(float3 position, float3 rotation, float size, ColorFloat color, unsigned int thread_index, DebugDrawCallOptions options = {});

		void AddCircleThread(float3 position, float3 rotation, float radius, ColorFloat color, unsigned int thread_index, DebugDrawCallOptions options = {});

		void AddArrowThread(float3 start, float3 end, float size, ColorFloat color, unsigned int thread_index, DebugDrawCallOptions options = {false});

		void AddArrowRotationThread(float3 translation, float3 rotation, float length, float size, ColorFloat color, unsigned int thread_index, DebugDrawCallOptions options = {false});

		void AddAxesThread(float3 translation, float3 rotation, float size, ColorFloat color_x, ColorFloat color_y, ColorFloat color_z, unsigned int thread_index, DebugDrawCallOptions options = {false});

		void AddTriangleThread(float3 point0, float3 point1, float3 point2, ColorFloat color, unsigned int thread_index, DebugDrawCallOptions options = {});

		void AddAABBThread(float3 translation, float3 scale, ColorFloat color, unsigned int thread_index, DebugDrawCallOptions options = {});

		void AddOOBBThread(float3 translation, float3 rotation, float3 scale, ColorFloat color, unsigned int thread_index, DebugDrawCallOptions options = {});

		// It must do an allocation from the memory manager under lock - possible expensive operation
		void AddStringThread(
			float3 position, 
			float3 direction,
			float size,
			containers::Stream<char> text,
			ColorFloat color, 
			unsigned int thread_index,
			DebugDrawCallOptions options = { false }
		);

		// It must do an allocation from the memory manager under lock - possible expensive operation
		void AddStringRotationThread(
			float3 position,
			float3 direction,
			float size,
			containers::Stream<char> text,
			ColorFloat color,
			unsigned int thread_index,
			DebugDrawCallOptions options = { false }
		);

#pragma endregion

#pragma region Draw immediately

		void DrawLine(float3 start, float3 end, ColorFloat color, DebugDrawCallOptions options = {});

		void DrawSphere(float3 position, float radius, ColorFloat color, DebugDrawCallOptions options = {});

		void DrawPoint(float3 position, ColorFloat color, DebugDrawCallOptions options = {});

		// Corner0 is the top left corner, corner1 is the bottom right corner
		void DrawRectangle(float3 corner0, float3 corner1, ColorFloat color, DebugDrawCallOptions options = {});

		void DrawCross(float3 position, float3 rotation, float size, ColorFloat color, DebugDrawCallOptions options = {});

		void DrawCircle(float3 position, float3 rotation, float radius, ColorFloat color, DebugDrawCallOptions options = {});

		void DrawArrow(float3 start, float3 end, float size, ColorFloat color, DebugDrawCallOptions options = {false});

		// Rotation expressed as radians
		void DrawArrowRotation(float3 translation, float3 rotation, float length, float size, ColorFloat color, DebugDrawCallOptions options = {false});

		void DrawAxes(float3 translation, float3 rotation, float size, ColorFloat color_x, ColorFloat color_y, ColorFloat color_z, DebugDrawCallOptions options = {false});

		void DrawTriangle(float3 point0, float3 point1, float3 point2, ColorFloat color, DebugDrawCallOptions options = {});

		void DrawAABB(float3 translation, float3 scale, ColorFloat color, DebugDrawCallOptions options = {});

		void DrawOOBB(float3 translation, float3 rotation, float3 scale, ColorFloat color, DebugDrawCallOptions options = {});

		// Text rotation is the rotation alongside the X axis - rotates the text in order to be seen from below, above, or at a specified angle
		void DrawString(
			float3 translation, 
			float3 direction, 
			float size, 
			containers::Stream<char> text,
			ColorFloat color, 
			DebugDrawCallOptions options = {false}
		);

		// Text rotation is the rotation alongside the X axis - rotates the text in order to be seen from below, above, or at a specified angle
		// The direction is specified as rotation in angles as degrees
		void DrawStringRotation(
			float3 translation, 
			float3 rotation, 
			float size, 
			containers::Stream<char> text, 
			ColorFloat color, 
			DebugDrawCallOptions options = {false}
		);

		// Draws the normals for an object
		void DrawNormals(
			VertexBuffer model_position, 
			VertexBuffer model_normals,
			float size, 
			ColorFloat color, 
			Matrix world_matrix,
			DebugDrawCallOptions options = {}
		);

		// Draws the normals for multiple objects of the same type
		void DrawNormals(
			VertexBuffer model_position,
			VertexBuffer model_normals,
			float size,
			ColorFloat color,
			containers::Stream<Matrix> world_matrices,
			DebugDrawCallOptions options = {}
		);

#pragma endregion

#pragma region Draw Deck elements

		// Draws all combinations - Wireframe depth, wireframe no depth, solid depth, solid no depth
		// Elements that have their duration 0 or negative after substraction with time delta
		// will be removed
		void DrawLineDeck(float time_delta);

		// Draws all combinations - Wireframe depth, wireframe no depth, solid depth, solid no depth
		// Elements that have their duration 0 or negative after substraction with time delta
		// will be removed
		void DrawSphereDeck(float time_delta);

		// Draws all combinations - Wireframe depth, wireframe no depth, solid depth, solid no depth
		// Elements that have their duration 0 or negative after substraction with time delta
		// will be removed
		void DrawPointDeck(float time_delta);

		// Draws all combinations - Wireframe depth, wireframe no depth, solid depth, solid no depth
		// Elements that have their duration 0 or negative after substraction with time delta
		// will be removed
		void DrawRectangleDeck(float time_delta);

		// Draws all combinations - Wireframe depth, wireframe no depth, solid depth, solid no depth
		// Elements that have their duration 0 or negative after substraction with time delta
		// will be removed
		void DrawCrossDeck(float time_delta);

		// Draws all combinations - Wireframe depth, wireframe no depth, solid depth, solid no depth
		// Elements that have their duration 0 or negative after substraction with time delta
		// will be removed
		void DrawCircleDeck(float time_delta);

		// Draws all combinations - Wireframe depth, wireframe no depth, solid depth, solid no depth
		// Elements that have their duration 0 or negative after substraction with time delta
		// will be removed
		void DrawArrowDeck(float time_delta);

		// Draws all combinations - Wireframe depth, wireframe no depth, solid depth, solid no depth
		// Elements that have their duration 0 or negative after substraction with time delta
		// will be removed
		void DrawAxesDeck(float time_delta);

		// Draws all combinations - Wireframe depth, wireframe no depth, solid depth, solid no depth
		// Elements that have their duration 0 or negative after substraction with time delta
		// will be removed
		void DrawTriangleDeck(float time_delta);

		// Draws all combinations - Wireframe depth, wireframe no depth, solid depth, solid no depth
		// Elements that have their duration 0 or negative after substraction with time delta
		// will be removed
		void DrawAABBDeck(float time_delta);

		// Draws all combinations - Wireframe depth, wireframe no depth, solid depth, solid no depth
		// Elements that have their duration 0 or negative after substraction with time delta
		// will be removed
		void DrawOOBBDeck(float time_delta);

		// Draws all combinations - Wireframe depth, wireframe no depth, solid depth, solid no depth
		// Elements that have their duration 0 or negative after substraction with time delta
		// will be removed
		void DrawStringDeck(float time_delta);

		void DrawAll(float time_delta);

#pragma endregion

#pragma region Flush

		// Flushes the thread queues for all types and for all threads
		void FlushAll();

		void FlushLine(unsigned int thread_index);

		void FlushSphere(unsigned int thread_index);

		void FlushPoint(unsigned int thread_index);

		void FlushRectangle(unsigned int thread_index);

		void FlushCross(unsigned int thread_index);

		void FlushCircle(unsigned int thread_index);

		void FlushArrow(unsigned int thread_index);

		void FlushAxes(unsigned int thread_index);

		void FlushTriangle(unsigned int thread_index);

		void FlushAABB(unsigned int thread_index);

		void FlushOOBB(unsigned int thread_index);

		void FlushString(unsigned int thread_index);

#pragma endregion

#pragma region Set state

		// It does not set the vertex buffers - it sets the shaders, input layout, topology,
		// rasterizer state and depth state
		void SetLineState(DebugDrawCallOptions options);

		// It does not set the vertex buffers - it sets the shaders, input layout, topology,
		// rasterizer state and depth state
		void SetSphereState(DebugDrawCallOptions options);

		// It does not set the vertex buffers - it sets the shaders, input layout, topology,
		// rasterizer state and depth state
		void SetPointState(DebugDrawCallOptions options);

		// It does not set the vertex buffers - it sets the shaders, input layout, topology,
		// rasterizer state and depth state
		void SetRectangleState(DebugDrawCallOptions options);

		// It does not set the vertex buffers - it sets the shaders, input layout, topology,
		// rasterizer state and depth state
		void SetCrossState(DebugDrawCallOptions options);

		// It does not set the vertex buffers - it sets the shaders, input layout, topology,
		// rasterizer state and depth state
		void SetCircleState(DebugDrawCallOptions options);

		// It does not set the vertex buffers - it sets the shaders, input layout, topology,
		// rasterizer state and depth state
		void SetArrowCylinderState(DebugDrawCallOptions options);

		// It does not set the vertex buffers - it sets the shaders, input layout, topology,
		// rasterizer state and depth state
		void SetArrowHeadState(DebugDrawCallOptions options);

		// It does not set the vertex buffers - it sets the shaders, input layout, topology,
		// rasterizer state and depth state
		void SetAxesState(DebugDrawCallOptions options);

		// It does not set the vertex buffers - it sets the shaders, input layout, topology,
		// rasterizer state and depth state
		void SetTriangleState(DebugDrawCallOptions options);

		// The AABB is treated as an OOBB with no rotation - so no state required

		// It does not set the vertex buffers - it sets the shaders, input layout, topology,
		// rasterizer state and depth state
		void SetOOBBState(DebugDrawCallOptions options);

		// It does not set the vertex buffers - it sets the shaders, input layout, topology,
		// rasterizer state and depth state
		void SetStringState(DebugDrawCallOptions options);

#pragma endregion

#pragma region Initialize

		// If creates with a default constructor or allocated
		void Initialize(MemoryManager* allocator, Graphics* graphics, size_t thread_count);

		// The circle vertex buffer is constructed by the drawer
		void InitializePrimitiveBuffers(ResourceManager* resource_manager);

#pragma endregion

#pragma region Bindings

		void BindSphereBuffers(VertexBuffer instanced_data);

		void BindPointBuffers(VertexBuffer instanced_data);

		void BindCrossBuffers(VertexBuffer instanced_data);

		void BindCircleBuffers(VertexBuffer instanced_data);

		void BindArrowCylinderBuffers(VertexBuffer instanced_data);

		void BindArrowHeadBuffers(VertexBuffer instanced_data);

		void BindCubeBuffers(VertexBuffer instanced_data);

		void BindStringBuffers(VertexBuffer instanced_data);

#pragma endregion

#pragma region Draw Calls

		void DrawCallLine(unsigned int instance_count);

		void DrawCallSphere(unsigned int instance_count);

		void DrawCallPoint(unsigned int instance_count);

		void DrawCallRectangle(unsigned int instance_count);

		void DrawCallCross(unsigned int instance_count);

		void DrawCallCircle(unsigned int instance_count);

		void DrawCallArrowCylinder(unsigned int instance_count);

		void DrawCallArrowHead(unsigned int instance_count);

		void DrawCallTriangle(unsigned int instance_count);
		
		void DrawCallAABB(unsigned int instance_count);

		void DrawCallOOBB(unsigned int instance_count);

#pragma endregion

		void BindShaders(unsigned int index);

		// It also bumps forward the buffer offset
		void CopyCharacterSubmesh(IndexBuffer index_buffer, unsigned int& buffer_offset, unsigned int alphabet_index);

		void UpdateCameraMatrix(Matrix new_matrix);

		GraphicsPipelineRenderState GetPreviousRenderState() const;

		void RestorePreviousRenderState(GraphicsPipelineRenderState state);

		MemoryManager* allocator;
		Graphics* graphics;
		containers::DeckPowerOfTwo<DebugLine, MemoryManager> lines;
		containers::DeckPowerOfTwo<DebugSphere, MemoryManager> spheres;
		containers::DeckPowerOfTwo<DebugPoint, MemoryManager> points;
		containers::DeckPowerOfTwo<DebugRectangle, MemoryManager> rectangles;
		containers::DeckPowerOfTwo<DebugCross, MemoryManager> crosses;
		containers::DeckPowerOfTwo<DebugCircle, MemoryManager> circles;
		containers::DeckPowerOfTwo<DebugArrow, MemoryManager> arrows;
		containers::DeckPowerOfTwo<DebugAxes, MemoryManager> axes;
		containers::DeckPowerOfTwo<DebugTriangle, MemoryManager> triangles;
		containers::DeckPowerOfTwo<DebugAABB, MemoryManager> aabbs;
		containers::DeckPowerOfTwo<DebugOOBB, MemoryManager> oobbs;
		containers::DeckPowerOfTwo<DebugString, MemoryManager> strings;
		containers::CapacityStream<DebugLine>* thread_lines;
		containers::CapacityStream<DebugSphere>* thread_spheres;
		containers::CapacityStream<DebugPoint>* thread_points;
		containers::CapacityStream<DebugRectangle>* thread_rectangles;
		containers::CapacityStream<DebugCross>* thread_crosses;
		containers::CapacityStream<DebugCircle>* thread_circles;
		containers::CapacityStream<DebugArrow>* thread_arrows;
		containers::CapacityStream<DebugAxes>* thread_axes;
		containers::CapacityStream<DebugTriangle>* thread_triangles;
		containers::CapacityStream<DebugAABB>* thread_aabbs;
		containers::CapacityStream<DebugOOBB>* thread_oobbs;
		containers::CapacityStream<DebugString>* thread_strings;
		SpinLock** thread_locks;
		unsigned int thread_count;
		ID3D11RasterizerState* rasterizer_states[ECS_DEBUG_RASTERIZER_COUNT];
		VertexBuffer positions_small_vertex_buffer;
		VertexBuffer instanced_small_vertex_buffer;
		StructuredBuffer instanced_small_structured_buffer;
		ResourceView instanced_structured_view;
		Mesh* primitive_meshes[ECS_DEBUG_VERTEX_BUFFER_COUNT];
		CoallescedMesh* string_mesh;
		VertexBuffer circle_buffer;
		VertexShader vertex_shaders[ECS_DEBUG_SHADER_COUNT];
		PixelShader pixel_shaders[ECS_DEBUG_SHADER_COUNT];
		InputLayout layout_shaders[ECS_DEBUG_SHADER_COUNT];
		Matrix camera_matrix;
		float2* string_character_bounds;
	};

}