#include "ecspch.h"
#include "DebugDraw.h"
#include "..\..\Utilities\FunctionInterfaces.h"

constexpr size_t SMALL_VERTEX_BUFFER_CAPACITY = 128;
constexpr size_t PER_THREAD_RESOURCES = 128;

namespace ECSEngine {

	// ----------------------------------------------------------------------------------------------------------------------

	DebugDrawer::DebugDrawer(MemoryManager* allocator, Graphics* graphics, size_t thread_count) {
		Initialize(allocator, graphics, thread_count);
	}

#pragma region Add to the queue - single threaded

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::AddLine(float3 start, float3 end, Color color, DebugDrawCallOptions options)
	{
		lines.Add({ start, end, color, options });
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::AddSphere(float3 position, float radius, Color color, DebugDrawCallOptions options)
	{
		spheres.Add({ position, radius, color, options });
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::AddPoint(float3 position, Color color, DebugDrawCallOptions options)
	{
		points.Add({ position, color, options });
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::AddRectangle(float3 corner0, float3 corner1, Color color, DebugDrawCallOptions options)
	{
		rectangles.Add({ corner0, corner1, color, options });
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::AddCross(float3 position, float size, Color color, DebugDrawCallOptions options)
	{
		crosses.Add({ position, size, color, options });
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::AddCircle(float3 position, float radius, Color color, DebugDrawCallOptions options)
	{
		circles.Add({ position, radius, color, options });
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::AddAxes(float3 translation, float3 rotation, float size, Color color, DebugDrawCallOptions options)
	{
		axes.Add({ translation, rotation, size, color, options });
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::AddTriangle(float3 point0, float3 point1, float3 point2, Color color, DebugDrawCallOptions options)
	{
		triangles.Add({ point0, point1, point2, color, options });
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::AddAABB(float3 min_coordinates, float3 max_coordinates, Color color, DebugDrawCallOptions options)
	{
		aabbs.Add({ min_coordinates, max_coordinates, color, options });
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::AddOOBB(Matrix transformation, Color color, DebugDrawCallOptions options)
	{
		oobbs.Add({ transformation, color, options });
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::AddString(float3 position, float size, containers::Stream<char> text, Color color, DebugDrawCallOptions options)
	{
		Stream<char> text_copy = function::StringCopy(allocator, text);
		strings.Add({ position, size, text_copy, color, options });
	}


	// ----------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Add to queue - multi threaded

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::AddLineThread(float3 start, float3 end, Color color, unsigned int thread_index, DebugDrawCallOptions options)
	{
		// Check to see if the queue is full
		// If it is, then a flush is needed to clear the queue
		if (thread_lines[thread_index].IsFull()) {
			FlushLine(thread_index);
		}
		thread_lines[thread_index].Add({ start, end, color, options });
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::AddSphereThread(float3 position, float radius, Color color, unsigned int thread_index, DebugDrawCallOptions options)
	{
		// Check to see if the queue is full
		// If it is, then a flush is needed to clear the queue
		if (thread_spheres[thread_index].IsFull()) {
			FlushSphere(thread_index);
		}
		thread_spheres[thread_index].Add({ position, radius, color });
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::AddPointThread(float3 position, Color color, unsigned int thread_index, DebugDrawCallOptions options)
	{
		// Check to see if the queue is full
		// If it is, then a flush is needed to clear the queue
		if (thread_points[thread_index].IsFull()) {
			FlushPoint(thread_index);
		}
		thread_points[thread_index].Add({position, color, options});
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::AddRectangleThread(float3 corner0, float3 corner1, Color color, unsigned int thread_index, DebugDrawCallOptions options)
	{
		// Check to see if the queue is full
		// If it is, then a flush is needed to clear the queue
		if (thread_rectangles[thread_index].IsFull()) {
			FlushRectangle(thread_index);
		}
		thread_rectangles[thread_index].Add({ corner0, corner1, color, options });
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::AddCrossThread(float3 position, float size, Color color, unsigned int thread_index, DebugDrawCallOptions options)
	{
		// Check to see if the queue is full
		// If it is, then a flush is needed to clear the queue
		if (thread_crosses[thread_index].IsFull()) {
			FlushCross(thread_index);
		}
		thread_crosses[thread_index].Add({ position, size, color, options });
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::AddCircleThread(float3 position, float radius, Color color, unsigned int thread_index, DebugDrawCallOptions options)
	{
		// Check to see if the queue is full
		// If it is, then a flush is needed to clear the queue
		if (thread_circles[thread_index].IsFull()) {
			FlushCircle(thread_index);
		}
		thread_circles[thread_index].Add({ position, radius, color, options });
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::AddAxesThread(float3 translation, float3 rotation, float size, Color color, unsigned int thread_index, DebugDrawCallOptions options)
	{
		// Check to see if the queue is full
		// If it is, then a flush is needed to clear the queue
		if (thread_axes[thread_index].IsFull()) {
			FlushAxes(thread_index);
		}
		thread_axes[thread_index].Add({ translation, rotation, size, color, options });
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::AddTriangleThread(float3 point0, float3 point1, float3 point2, Color color, unsigned int thread_index, DebugDrawCallOptions options)
	{
		// Check to see if the queue is full
		// If it is, then a flush is needed to clear the queue
		if (thread_triangles[thread_index].IsFull()) {
			FlushTriangle(thread_index);
		}
		thread_triangles[thread_index].Add({ point0, point1, point2, color, options });
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::AddAABBThread(float3 min_coordinates, float3 max_coordinates, Color color, unsigned int thread_index, DebugDrawCallOptions options)
	{
		// Check to see if the queue is full
		// If it is, then a flush is needed to clear the queue
		if (thread_aabbs[thread_index].IsFull()) {
			FlushAABB(thread_index);
		}
		thread_aabbs[thread_index].Add({ min_coordinates, max_coordinates, color, options });
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::AddOOBBThread(Matrix transformation, Color color, unsigned int thread_index, DebugDrawCallOptions options)
	{
		// Check to see if the queue is full
		// If it is, then a flush is needed to clear the queue
		if (thread_oobbs[thread_index].IsFull()) {
			FlushOOBB(thread_index);
		}
		thread_oobbs[thread_index].Add({ transformation, color, options });
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::AddStringThread(float3 position, float size, containers::Stream<char> text, Color color, unsigned int thread_index, DebugDrawCallOptions options)
	{
		// Check to see if the queue is full
		// If it is, then a flush is needed to clear the queue
		if (thread_strings[thread_index].IsFull()) {
			FlushString(thread_index);
		}
		Stream<char> string_copy = function::StringCopyTs(allocator, text);
		thread_strings[thread_index].Add({ position, size, string_copy, color, options });
	}

	// ----------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Draw immediately

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::DrawLine(float3 start, float3 end, Color color, DebugDrawCallOptions options)
	{
		void* vertex_buffer = graphics->MapBuffer(small_vertex_buffer.buffer);

		graphics->UnmapBuffer(small_vertex_buffer.buffer);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::DrawSphere(float3 position, float radius, Color color, DebugDrawCallOptions options)
	{
		void* vertex_buffer = graphics->MapBuffer(small_vertex_buffer.buffer);

		graphics->UnmapBuffer(small_vertex_buffer.buffer);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::DrawPoint(float3 position, Color color, DebugDrawCallOptions options)
	{
		void* vertex_buffer = graphics->MapBuffer(small_vertex_buffer.buffer);

		graphics->UnmapBuffer(small_vertex_buffer.buffer);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::DrawRectangle(float3 corner0, float3 corner1, Color color, DebugDrawCallOptions options)
	{
		void* vertex_buffer = graphics->MapBuffer(small_vertex_buffer.buffer);

		graphics->UnmapBuffer(small_vertex_buffer.buffer);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::DrawCross(float3 position, float size, Color color, DebugDrawCallOptions options)
	{
		void* vertex_buffer = graphics->MapBuffer(small_vertex_buffer.buffer);

		graphics->UnmapBuffer(small_vertex_buffer.buffer);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::DrawCircle(float3 position, float radius, Color color, DebugDrawCallOptions options)
	{
		void* vertex_buffer = graphics->MapBuffer(small_vertex_buffer.buffer);

		graphics->UnmapBuffer(small_vertex_buffer.buffer);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::DrawAxes(float3 translation, float3 rotation, float size, Color color, DebugDrawCallOptions options)
	{
		void* vertex_buffer = graphics->MapBuffer(small_vertex_buffer.buffer);

		graphics->UnmapBuffer(small_vertex_buffer.buffer);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::DrawTriangle(float3 point0, float3 point1, float3 point2, Color color, DebugDrawCallOptions options)
	{
		void* vertex_buffer = graphics->MapBuffer(small_vertex_buffer.buffer);

		graphics->UnmapBuffer(small_vertex_buffer.buffer);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::DrawAABB(float3 min_coordinates, float3 max_coordinates, Color color, DebugDrawCallOptions options)
	{
		void* vertex_buffer = graphics->MapBuffer(small_vertex_buffer.buffer);

		graphics->UnmapBuffer(small_vertex_buffer.buffer);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::DrawOOBB(Matrix transformation, Color color, DebugDrawCallOptions options)
	{
		void* vertex_buffer = graphics->MapBuffer(small_vertex_buffer.buffer);

		graphics->UnmapBuffer(small_vertex_buffer.buffer);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::DrawString(float3 position, float size, containers::Stream<char> text, Color color, DebugDrawCallOptions options)
	{
		void* vertex_buffer = graphics->MapBuffer(small_vertex_buffer.buffer);

		graphics->UnmapBuffer(small_vertex_buffer.buffer);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::DrawNormal(VertexBuffer positions, VertexBuffer normals, float size, Color color, DebugDrawCallOptions options)
	{
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::DrawNormalSlice(VertexBuffer positions, VertexBuffer normals, float size, Color color, containers::Stream<uint2> slices, DebugDrawCallOptions options)
	{
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::DrawNormals(VertexBuffer model_position, VertexBuffer model_normals, float size, Color color, containers::Stream<Matrix> world_matrices, DebugDrawCallOptions options)
	{
	}

	// ----------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Initialize

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::Initialize(MemoryManager* _allocator, Graphics* _graphics, size_t _thread_count) {
		allocator = _allocator;
		graphics = _graphics;
		thread_count = _thread_count;

		small_vertex_buffer = graphics->CreateVertexBuffer(sizeof(float3), SMALL_VERTEX_BUFFER_CAPACITY);
		size_t total_memory = 0;
		size_t thread_capacity_stream_size = sizeof(CapacityStream<void>) * thread_count;

		total_memory += thread_lines->MemoryOf(PER_THREAD_RESOURCES) * thread_count;
		total_memory += thread_spheres->MemoryOf(PER_THREAD_RESOURCES) * thread_count;
		total_memory += thread_points->MemoryOf(PER_THREAD_RESOURCES) * thread_count;
		total_memory += thread_rectangles->MemoryOf(PER_THREAD_RESOURCES) * thread_count;
		total_memory += thread_crosses->MemoryOf(PER_THREAD_RESOURCES) * thread_count;
		total_memory += thread_circles->MemoryOf(PER_THREAD_RESOURCES) * thread_count;
		total_memory += thread_axes->MemoryOf(PER_THREAD_RESOURCES) * thread_count;
		total_memory += thread_triangles->MemoryOf(PER_THREAD_RESOURCES) * thread_count;
		total_memory += thread_aabbs->MemoryOf(PER_THREAD_RESOURCES) * thread_count;
		total_memory += thread_oobbs->MemoryOf(PER_THREAD_RESOURCES) * thread_count;
		total_memory += thread_strings->MemoryOf(PER_THREAD_RESOURCES) * thread_count;

		// Add the actual capacity stream sizes
		total_memory += thread_capacity_stream_size * ECS_DEBUG_PRIMITIVE_COUNT;

		// The locks will be padded to different cache lines
		total_memory += (ECS_CACHE_LINE_SIZE + sizeof(SpinLock*)) * ECS_DEBUG_PRIMITIVE_COUNT;

		void* allocation = allocator->Allocate(total_memory);
		uintptr_t buffer = (uintptr_t)allocation;

		thread_lines = (CapacityStream<DebugLine>*)buffer;
		buffer += thread_capacity_stream_size;

		thread_spheres = (CapacityStream<DebugSphere>*)buffer;
		buffer += thread_capacity_stream_size;

		thread_points = (CapacityStream<DebugPoint>*)buffer;
		buffer += thread_capacity_stream_size;

		thread_rectangles = (CapacityStream<DebugRectangle>*)buffer;
		buffer += thread_capacity_stream_size;

		thread_crosses = (CapacityStream<DebugCross>*)buffer;
		buffer += thread_capacity_stream_size;

		thread_circles = (CapacityStream<DebugCircle>*)buffer;
		buffer += thread_capacity_stream_size;

		thread_axes = (CapacityStream<DebugAxes>*)buffer;
		buffer += thread_capacity_stream_size;

		thread_triangles = (CapacityStream<DebugTriangle>*)buffer;
		buffer += thread_capacity_stream_size;

		thread_aabbs = (CapacityStream<DebugAABB>*)buffer;
		buffer += thread_capacity_stream_size;

		thread_oobbs = (CapacityStream<DebugOOBB>*)buffer;
		buffer += thread_capacity_stream_size;

		thread_strings = (CapacityStream<DebugString>*)buffer;
		buffer += thread_capacity_stream_size;

		for (size_t index = 0; index < thread_count; index++) {
			thread_lines[index].InitializeFromBuffer(buffer, 0, PER_THREAD_RESOURCES);
			thread_spheres[index].InitializeFromBuffer(buffer, 0, PER_THREAD_RESOURCES);
			thread_points[index].InitializeFromBuffer(buffer, 0, PER_THREAD_RESOURCES);
			thread_rectangles[index].InitializeFromBuffer(buffer, 0, PER_THREAD_RESOURCES);
			thread_crosses[index].InitializeFromBuffer(buffer, 0, PER_THREAD_RESOURCES);
			thread_circles[index].InitializeFromBuffer(buffer, 0, PER_THREAD_RESOURCES);
			thread_axes[index].InitializeFromBuffer(buffer, 0, PER_THREAD_RESOURCES);
			thread_triangles[index].InitializeFromBuffer(buffer, 0, PER_THREAD_RESOURCES);
			thread_aabbs[index].InitializeFromBuffer(buffer, 0, PER_THREAD_RESOURCES);
			thread_oobbs[index].InitializeFromBuffer(buffer, 0, PER_THREAD_RESOURCES);
			thread_strings[index].InitializeFromBuffer(buffer, 0, PER_THREAD_RESOURCES);
		}

		thread_locks = (SpinLock**)buffer;
		buffer += sizeof(SpinLock*) * ECS_DEBUG_PRIMITIVE_COUNT;

		// Padd the locks to different cache lines
		for (size_t index = 0; index < thread_count; index++) {
			thread_locks[index] = (SpinLock*)buffer;
			thread_locks[index]->value.store(0, ECS_RELAXED);
			buffer += ECS_CACHE_LINE_SIZE;
		}
	}


	// ----------------------------------------------------------------------------------------------------------------------

	// ----------------------------------------------------------------------------------------------------------------------

	// ----------------------------------------------------------------------------------------------------------------------

#pragma endregion

	// ----------------------------------------------------------------------------------------------------------------------
	
	// ----------------------------------------------------------------------------------------------------------------------
	
	// ----------------------------------------------------------------------------------------------------------------------

	// ----------------------------------------------------------------------------------------------------------------------
}