#pragma once
#include "..\..\Core.h"
#include "..\..\Rendering\Graphics.h"
#include "..\..\Allocators\MemoryManager.h"
#include "..\..\Math\Matrix.h"
#include "..\..\Containers\Deque.h"
#include "..\..\Internal\Multithreading\ConcurrentPrimitives.h"

namespace ECSEngine {

	struct ResourceManager;
	
	enum DebugPrimitive {
		ECS_DEBUG_PRIMITIVE_LINE,
		ECS_DEBUG_PRIMITIVE_SPHERE,
		ECS_DEBUG_PRIMITIVE_POINT,
		ECS_DEBUG_PRIMITIVE_RECTANGLE,
		ECS_DEBUG_PRIMITIVE_CROSS,
		ECS_DEBUG_PRIMITIVE_CIRCLE,
		ECS_DEBUG_PRIMITIVE_AXES,
		ECS_DEBUG_PRIMITIVE_TRIANGLE,
		ECS_DEBUG_PRIMITIVE_AABB,
		ECS_DEBUG_PRIMITIVE_OOBB,
		ECS_DEBUG_PRIMITIVE_STRING,
		ECS_DEBUG_PRIMITIVE_COUNT
	};

	using DebugVertex = float3;

	struct DebugDrawCallOptions {
		bool wireframe = true;
		bool ignore_depth = false;
		bool screen_space = false;
		float duration = 0.0f;
	};

	struct DebugLine {
		float3 start;
		float3 end;
		Color color;
		DebugDrawCallOptions options;
	};

	struct DebugSphere {
		float3 position;
		float radius;
		Color color;
		DebugDrawCallOptions options;
	};

	struct DebugPoint {
		float3 position;
		Color color;
		DebugDrawCallOptions options;
	};

	struct DebugRectangle {
		float3 corner0;
		float3 corner1;
		Color color;
		DebugDrawCallOptions options;
	};

	struct DebugCross {
		float3 position;
		float size;
		Color color;
		DebugDrawCallOptions options;
	};

	struct DebugCircle {
		float3 position;
		float radius;
		Color color;
		DebugDrawCallOptions options;
	};

	struct DebugAxes {
		float3 translation;
		float3 rotation;
		float size;
		Color color;
		DebugDrawCallOptions options;
	};

	struct DebugTriangle {
		float3 point0;
		float3 point1;
		float3 point2;
		Color color;
		DebugDrawCallOptions options;
	};

	struct DebugAABB {
		float3 min_coordinates;
		float3 max_coordinates;
		Color color;
		DebugDrawCallOptions options;
	};

	struct DebugOOBB {
		Matrix transformation;
		Color color;
		DebugDrawCallOptions options;
	};

	struct DebugString {
		float3 position;
		float size;
		Stream<char> text;
		Color color;
		DebugDrawCallOptions options;
	};

	struct ECSENGINE_API DebugDrawer {
		DebugDrawer() : allocator(nullptr), graphics(nullptr) {}
		DebugDrawer(MemoryManager* allocator, Graphics* graphics, size_t thread_count);

		DebugDrawer(const DebugDrawer& other) = default;
		DebugDrawer& operator = (const DebugDrawer& other) = default;

#pragma region Add to the draw queue - single threaded

		void AddLine(float3 start, float3 end, Color color, DebugDrawCallOptions options = {});

		void AddSphere(float3 position, float radius, Color color, DebugDrawCallOptions options = {});

		void AddPoint(float3 position, Color color, DebugDrawCallOptions options = {});

		void AddRectangle(float3 corner0, float3 corner1, Color color, DebugDrawCallOptions options = {});

		void AddCross(float3 position, float size, Color color, DebugDrawCallOptions options = {});

		void AddCircle(float3 position, float radius, Color color, DebugDrawCallOptions options = {});

		void AddAxes(float3 translation, float3 rotation, float size, Color color, DebugDrawCallOptions options = {});

		void AddTriangle(float3 point0, float3 point1, float3 point2, Color color, DebugDrawCallOptions options = {});

		void AddAABB(float3 min_coordinates, float3 max_coordinates, Color color, DebugDrawCallOptions options = {});

		void AddOOBB(Matrix transformation, Color color, DebugDrawCallOptions options = {});

		void AddString(float3 position, float size, containers::Stream<char> text, Color color, DebugDrawCallOptions options = { false });

#pragma endregion

#pragma region Add to the draw queue - multi threaded

		void AddLineThread(float3 start, float3 end, Color color, unsigned int thread_index, DebugDrawCallOptions options = {});

		void AddSphereThread(float3 position, float radius, Color color, unsigned int thread_index, DebugDrawCallOptions options = {});

		void AddPointThread(float3 position, Color color, unsigned int thread_index, DebugDrawCallOptions options = {});

		void AddRectangleThread(float3 corner0, float3 corner1, Color color, unsigned int thread_index, DebugDrawCallOptions options = {});

		void AddCrossThread(float3 position, float size, Color color, unsigned int thread_index, DebugDrawCallOptions options = {});

		void AddCircleThread(float3 position, float radius, Color color, unsigned int thread_index, DebugDrawCallOptions options = {});

		void AddAxesThread(float3 translation, float3 rotation, float size, Color color, unsigned int thread_index, DebugDrawCallOptions options = {});

		void AddTriangleThread(float3 point0, float3 point1, float3 point2, Color color, unsigned int thread_index, DebugDrawCallOptions options = {});

		void AddAABBThread(float3 min_coordinates, float3 max_coordinates, Color color, unsigned int thread_index, DebugDrawCallOptions options = {});

		void AddOOBBThread(Matrix transformation, Color color, unsigned int thread_index, DebugDrawCallOptions options = {});

		// It must do an allocation from the memory manager under lock - possible expensive operation
		void AddStringThread(float3 position, float size, containers::Stream<char> text, Color color, unsigned int thread_index, DebugDrawCallOptions options = { false });

#pragma endregion

#pragma region Draw immediately

		void DrawLine(float3 start, float3 end, Color color, DebugDrawCallOptions options = {});

		void DrawSphere(float3 position, float radius, Color color, DebugDrawCallOptions options = {});

		void DrawPoint(float3 position, Color color, DebugDrawCallOptions options = {});

		void DrawRectangle(float3 corner0, float3 corner1, Color color, DebugDrawCallOptions options = {});

		void DrawCross(float3 position, float size, Color color, DebugDrawCallOptions options = {});

		void DrawCircle(float3 position, float radius, Color color, DebugDrawCallOptions options = {});

		void DrawAxes(float3 translation, float3 rotation, float size, Color color, DebugDrawCallOptions options = {});

		void DrawTriangle(float3 point0, float3 point1, float3 point2, Color color, DebugDrawCallOptions options = {});

		void DrawAABB(float3 min_coordinates, float3 max_coordinates, Color color, DebugDrawCallOptions options = {});

		void DrawOOBB(Matrix transformation, Color color, DebugDrawCallOptions options = {});

		void DrawString(float3 position, float size, containers::Stream<char> text, Color color, DebugDrawCallOptions options = {});

		void DrawNormal(VertexBuffer positions, VertexBuffer normals, float size, Color color, DebugDrawCallOptions options = {});

		void DrawNormalSlice(
			VertexBuffer positions, 
			VertexBuffer normals, 
			float size, 
			Color color, 
			containers::Stream<uint2> slices, 
			DebugDrawCallOptions options = {}
		);

		// Draws the normals for multiple objects of the same type
		void DrawNormals(
			VertexBuffer model_position, 
			VertexBuffer model_normals,
			float size, 
			Color color, 
			containers::Stream<Matrix> world_matrices,
			DebugDrawCallOptions options = {}
		);

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

		void FlushAxes(unsigned int thread_index);

		void FlushTriangle(unsigned int thread_index);

		void FlushAABB(unsigned int thread_index);

		void FlushOOBB(unsigned int thread_index);

		void FlushString(unsigned int thread_index);

#pragma endregion

#pragma region Initialize

		void Initialize(MemoryManager * allocator, Graphics * graphics, size_t thread_count);

		void InitializePrimitiveBuffers(ResourceManager* resource_manager, containers::Stream<Stream<wchar_t>> primitive_meshes);

		// Initializes the vertex shader, the pixel shader, the input layout and the vertex constant buffer used
		// to transform the vertices
		void InitializeRenderResources(containers::Stream<wchar_t> vertex_shader, containers::Stream<wchar_t> pixel_shader);

#pragma endregion

		MemoryManager* allocator;
		Graphics* graphics;
		containers::CapacityStream<VertexBuffer> primitive_buffers;
		containers::DequePowerOfTwo<DebugLine, MemoryManager> lines;
		containers::DequePowerOfTwo<DebugSphere, MemoryManager> spheres;
		containers::DequePowerOfTwo<DebugPoint, MemoryManager> points;
		containers::DequePowerOfTwo<DebugRectangle, MemoryManager> rectangles;
		containers::DequePowerOfTwo<DebugCross, MemoryManager> crosses;
		containers::DequePowerOfTwo<DebugCircle, MemoryManager> circles;
		containers::DequePowerOfTwo<DebugAxes, MemoryManager> axes;
		containers::DequePowerOfTwo<DebugTriangle, MemoryManager> triangles;
		containers::DequePowerOfTwo<DebugAABB, MemoryManager> aabbs;
		containers::DequePowerOfTwo<DebugOOBB, MemoryManager> oobbs;
		containers::DequePowerOfTwo<DebugString, MemoryManager> strings;
		containers::CapacityStream<DebugLine>* thread_lines;
		containers::CapacityStream<DebugSphere>* thread_spheres;
		containers::CapacityStream<DebugPoint>* thread_points;
		containers::CapacityStream<DebugRectangle>* thread_rectangles;
		containers::CapacityStream<DebugCross>* thread_crosses;
		containers::CapacityStream<DebugCircle>* thread_circles;
		containers::CapacityStream<DebugAxes>* thread_axes;
		containers::CapacityStream<DebugTriangle>* thread_triangles;
		containers::CapacityStream<DebugAABB>* thread_aabbs;
		containers::CapacityStream<DebugOOBB>* thread_oobbs;
		containers::CapacityStream<DebugString>* thread_strings;
		SpinLock** thread_locks;
		size_t thread_count;
		VertexBuffer small_vertex_buffer;
		VertexShader vertex_shader;
		PixelShader pixel_shader;
		InputLayout layout_shader;
	};

}