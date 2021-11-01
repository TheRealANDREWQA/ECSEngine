#include "ecspch.h"
#include "DebugDraw.h"
#include "..\..\Utilities\FunctionInterfaces.h"
// Import the CharacterUVIndex
#include "..\..\Tools\UI\UIStructures.h"
#include "..\..\Internal\ResourceManager.h"

constexpr size_t SMALL_VERTEX_BUFFER_CAPACITY = 8;
constexpr size_t PER_THREAD_RESOURCES = 128;

#define POINT_SIZE 0.05f
#define CIRCLE_TESSELATION 32
#define ARROW_BASE_CONE_DARKEN_COLOR 0.5f
#define ARROW_CONE_DARKEN_COLOR 0.75f

namespace ECSEngine {

	enum ElementType {
		WIREFRAME_DEPTH,
		WIREFRAME_NO_DEPTH,
		SOLID_DEPTH,
		SOLID_NO_DEPTH,
		ELEMENT_COUNT
	};

	struct InstancedTransformData {
		float4 matrix_0;
		float4 matrix_1;
		float4 matrix_2;
		float4 matrix_3;
		ColorFloat color;
	};

	// ----------------------------------------------------------------------------------------------------------------------

	unsigned char GetMaskFromOptions(DebugDrawCallOptions options) {
		return (unsigned char)options.ignore_depth | (((unsigned char)(!options.wireframe) << 1) & 2);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	unsigned int GetMaximumCount(unsigned int* counts) {
		return std::max(std::max(counts[WIREFRAME_DEPTH], counts[WIREFRAME_NO_DEPTH]), std::max(counts[SOLID_DEPTH], counts[SOLID_NO_DEPTH]));
	}

	// ----------------------------------------------------------------------------------------------------------------------

	template<typename Element>
	ushort2* CalculateElementTypeCounts(MemoryManager* allocator, unsigned int* counts, ushort2** indices, DeckPowerOfTwo<Element, MemoryManager>* deck) {
		unsigned int total_count = deck->GetElementCount();
		ushort2* type_indices = (ushort2*)allocator->Allocate(sizeof(ushort2) * total_count * 4);
		SetIndicesTypeMask(indices, type_indices, total_count);

		for (size_t index = 0; index < deck->buffers.size; index++) {
			for (size_t subindex = 0; subindex < deck->buffers[index].size; subindex++) {
				unsigned char mask = GetMaskFromOptions(deck->buffers[index][subindex].options);
				// Assigned the indices and update the count
				type_indices[counts[mask]++] = { (unsigned short)index, (unsigned short)subindex };
			}
		}
		return type_indices;
	}

	template<typename Element>
	void UpdateElementDurations(DeckPowerOfTwo<Element, MemoryManager>* deck, float time_delta) {
		for (size_t index = 0; index < deck->buffers.size; index++) {
			for (size_t subindex = 0; subindex < deck->buffers[index].size; subindex++) {
				deck->buffers[index][subindex].options.duration -= time_delta;
				if (deck->buffers[index][subindex].options.duration < 0.0f) {
					deck->buffers[index].RemoveSwapBack(subindex);
				}
			}
		}
		deck->RecalculateFreeChunks();
	}

	// ----------------------------------------------------------------------------------------------------------------------

	float3* MapPositions(DebugDrawer* drawer) {
		return (float3*)drawer->graphics->MapBuffer(drawer->positions_small_vertex_buffer.buffer);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	// Can be used as InstancedTransformData or plain Color information for world position shader
	InstancedTransformData* MapInstancedVertex(DebugDrawer* drawer) {
		return (InstancedTransformData*)drawer->graphics->MapBuffer(drawer->instanced_small_vertex_buffer.buffer);
	}

	InstancedTransformData* MapInstancedStructured(DebugDrawer* drawer) {
		return (InstancedTransformData*)drawer->graphics->MapBuffer(drawer->instanced_small_structured_buffer.buffer);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void UnmapPositions(DebugDrawer* drawer) {
		drawer->graphics->UnmapBuffer(drawer->positions_small_vertex_buffer.buffer);
	}

	void UnmapInstancedVertex(DebugDrawer* drawer) {
		drawer->graphics->UnmapBuffer(drawer->instanced_small_vertex_buffer.buffer);
	}

	void UnmapInstancedStructured(DebugDrawer* drawer) {
		drawer->graphics->UnmapBuffer(drawer->instanced_small_structured_buffer.buffer);
	}

	void UnmapAllVertex(DebugDrawer* drawer) {
		UnmapPositions(drawer);
		UnmapInstancedVertex(drawer);
	}

	void UnmapAllStructured(DebugDrawer* drawer) {
		UnmapPositions(drawer);
		UnmapInstancedStructured(drawer);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	// Sets the color of the instaced data if the stream has a color and matrix as instanced data
	void SetInstancedColor(void* pointer, unsigned int index, Color color) {
		InstancedTransformData* data = (InstancedTransformData*)pointer;
		data[index].color = color;
	}

	// Sets the matrix of the instaced data if the stream has a color and matrix as instanced data
	void ECS_VECTORCALL SetInstancedMatrix(void* pointer, unsigned int index, Matrix matrix) {
		InstancedTransformData* data = (InstancedTransformData*)pointer;
		matrix.Store(data + index);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void HandleOptions(DebugDrawer* drawer, DebugDrawCallOptions options) {
		drawer->graphics->m_context->RSSetState(drawer->rasterizer_states[options.wireframe]);
		if (options.ignore_depth) {
			drawer->graphics->DisableDepth();
		}
		else {
			drawer->graphics->EnableDepth();
		}
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void SetRectanglePositionsFront(float3* positions, float3 corner0, float3 corner1) {
		// First rectangle triangle
		positions[0] = corner0;
		positions[1] = { corner1.x, corner0.y, corner1.z };
		positions[2] = { corner0.x, corner1.y, corner0.z };

		// Second rectangle triangle
		positions[3] = positions[1];
		positions[4] = corner1;
		positions[5] = positions[2];
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void BindSmallStructuredBuffers(DebugDrawer* drawer) {
		drawer->graphics->BindVertexBuffer(drawer->positions_small_vertex_buffer);
		drawer->graphics->BindVertexResourceView(drawer->instanced_structured_view);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void ConstructTesselatedCircle(float3* positions, float radius, size_t tesselation_count) {
		float angle_increment = 2 * PI / tesselation_count;
		for (size_t index = 0; index < tesselation_count; index++) {
			float current_angle = angle_increment * index;
			positions[index].x = cos(current_angle);
			positions[index].y = 0.0f;
			positions[index].z = sin(current_angle);
		}
	}

	// ----------------------------------------------------------------------------------------------------------------------

	// float3 rotation and the length is in the w component
	float4 ArrowStartEndToRotation(float3 start, float3 end) {
		float3 direction = start - end;
		float x_angle = atan2(direction.y, direction.z);
		float y_angle = atan2(direction.z, direction.x);
		float z_angle = atan2(direction.y, direction.x);
		Vector4 vector(direction);
		float length = Length3(vector).First();
		return { 0.0f, y_angle, z_angle, length };
	}

	// ----------------------------------------------------------------------------------------------------------------------

	// The ushort2* indices that will indicate the positions inside the deck
	void SetIndicesTypeMask(ushort2** pointers, ushort2* indices, unsigned int total_count) {
		pointers[WIREFRAME_DEPTH] = indices;
		pointers[WIREFRAME_NO_DEPTH] = indices + total_count;
		pointers[SOLID_DEPTH] = indices + 2 * total_count;
		pointers[SOLID_NO_DEPTH] = indices + 3 * total_count;
	}

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
		// Make wireframe false
		points.Add({ position, color, { false, options.ignore_depth, options.duration } });
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

	void DebugDrawer::AddCircle(float3 position, float3 rotation, float radius, Color color, DebugDrawCallOptions options)
	{
		circles.Add({ position, rotation, radius, color, options });
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::AddArrow(float3 start, float3 end, float size, Color color, DebugDrawCallOptions options)
	{
		float4 rotation_length = ArrowStartEndToRotation(start, end);
		arrows.Add({ start, {rotation_length.x, rotation_length.y, rotation_length.z}, rotation_length.w, size, color, options });
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

	void DebugDrawer::AddOOBB(float3 translation, float3 rotation, float3 scale, Color color, DebugDrawCallOptions options)
	{
		oobbs.Add({ translation, rotation, scale, color, options });
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
		// Make wireframe false
		options.wireframe = false;

		// Check to see if the queue is full
		// If it is, then a flush is needed to clear the queue
		if (thread_points[thread_index].IsFull()) {
			FlushPoint(thread_index);
		}
		thread_points[thread_index].Add({ position, color, options });
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

	void DebugDrawer::AddCircleThread(float3 position, float3 rotation, float radius, Color color, unsigned int thread_index, DebugDrawCallOptions options)
	{
		// Check to see if the queue is full
		// If it is, then a flush is needed to clear the queue
		if (thread_circles[thread_index].IsFull()) {
			FlushCircle(thread_index);
		}
		thread_circles[thread_index].Add({ position, rotation, radius, color, options });
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::AddArrowThread(float3 start, float3 end, float size, Color color, unsigned int thread_index, DebugDrawCallOptions options)
	{
		// Check to see if the queue is full
		// If it is, then a flush is needed to clear the queue
		if (thread_arrows[thread_index].IsFull()) {
			FlushArrow(thread_index);
		}
		float4 rotation_length = ArrowStartEndToRotation(start, end);
		thread_arrows[thread_index].Add({ start, {rotation_length.x, rotation_length.y, rotation_length.z}, rotation_length.w, size, color, options });
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

	void DebugDrawer::AddOOBBThread(float3 translation, float3 rotation, float3 scale, Color color, unsigned int thread_index, DebugDrawCallOptions options)
	{
		// Check to see if the queue is full
		// If it is, then a flush is needed to clear the queue
		if (thread_oobbs[thread_index].IsFull()) {
			FlushOOBB(thread_index);
		}
		thread_oobbs[thread_index].Add({ translation, rotation, scale, color, options });
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
		float3* positions = MapPositions(this);
		InstancedTransformData* instanced = MapInstancedStructured(this);

		positions[0] = start;
		positions[1] = end;
		SetInstancedMatrix(instanced, 0, MatrixTranspose(camera_matrix));
		SetInstancedColor(instanced, 0, color);

		UnmapAllStructured(this);

		SetLineState(options);
		BindSmallStructuredBuffers(this);

		graphics->DrawInstanced(2, 1);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::DrawSphere(float3 position, float radius, Color color, DebugDrawCallOptions options)
	{
		InstancedTransformData* instanced = MapInstancedVertex(this);
		SetInstancedColor(instanced, 0, color);
		Matrix matrix = MatrixScale(float3::Splat(radius)) * MatrixTranslation(position);
		SetInstancedMatrix(instanced, 0, MatrixTranspose(matrix * camera_matrix));
		UnmapInstancedVertex(this);

		SetSphereState(options);
		VertexBuffer vertex_buffers[2];
		vertex_buffers[0] = primitive_buffers[ECS_DEBUG_VERTEX_BUFFER_SPHERE];
		vertex_buffers[1] = instanced_small_vertex_buffer;
		graphics->BindVertexBuffers(Stream<VertexBuffer>(vertex_buffers, std::size(vertex_buffers)));

		graphics->DrawInstanced(primitive_buffers[ECS_DEBUG_VERTEX_BUFFER_SPHERE].size, 1);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::DrawPoint(float3 position, Color color, DebugDrawCallOptions options)
	{
		options.wireframe = false;

		InstancedTransformData* instanced = MapInstancedVertex(this);
		SetInstancedColor(instanced, 0, color);
		Matrix matrix = MatrixScale(float3::Splat(POINT_SIZE)) * MatrixTranslation(position);
		SetInstancedMatrix(instanced, 0, MatrixTranspose(matrix * camera_matrix));
		UnmapInstancedVertex(this);

		SetPointState(options);
		VertexBuffer vertex_buffers[2];
		vertex_buffers[0] = primitive_buffers[ECS_DEBUG_VERTEX_BUFFER_SPHERE];
		vertex_buffers[1] = instanced_small_vertex_buffer;
		graphics->BindVertexBuffers(Stream<VertexBuffer>(vertex_buffers, std::size(vertex_buffers)));

		graphics->DrawInstanced(primitive_buffers[ECS_DEBUG_VERTEX_BUFFER_SPHERE].size, 1);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	// Draw it double sided, so it appears on both sides
	void DebugDrawer::DrawRectangle(float3 corner0, float3 corner1, Color color, DebugDrawCallOptions options)
	{
		float3* positions = MapPositions(this);
		InstancedTransformData* instanced = MapInstancedStructured(this);

		SetRectanglePositionsFront(positions, corner0, corner1);

		SetInstancedColor(instanced, 0, color);
		SetInstancedMatrix(instanced, 0, MatrixTranspose(camera_matrix));
		UnmapAllStructured(this);

		SetRectangleState(options);
		BindSmallStructuredBuffers(this);

		graphics->DrawInstanced(6, 1);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::DrawCross(float3 position, float size, Color color, DebugDrawCallOptions options)
	{
		InstancedTransformData* instanced = MapInstancedVertex(this);

		Matrix matrix = MatrixScale(float3::Splat(size)) * MatrixTranslation(position);
		SetInstancedColor(instanced, 0, color);
		SetInstancedMatrix(instanced, 0, MatrixTranspose(matrix * camera_matrix));
		UnmapInstancedVertex(this);

		SetCrossState(options);
		VertexBuffer vertex_buffers[2];
		vertex_buffers[0] = primitive_buffers[ECS_DEBUG_VERTEX_BUFFER_CROSS];
		vertex_buffers[1] = instanced_small_vertex_buffer;
		graphics->BindVertexBuffers(Stream<VertexBuffer>(vertex_buffers, 2));

		graphics->DrawInstanced(primitive_buffers[ECS_DEBUG_VERTEX_BUFFER_CROSS].size, 1);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::DrawCircle(float3 position, float3 rotation, float radius, Color color, DebugDrawCallOptions options)
	{
		InstancedTransformData* instanced = MapInstancedVertex(this);
		Matrix matrix = MatrixScale(float3::Splat(radius)) * MatrixRotation(rotation) * MatrixTranslation(position);
		SetInstancedColor(instanced, 0, color);
		SetInstancedMatrix(instanced, 0, MatrixTranspose(matrix * camera_matrix));
		UnmapInstancedVertex(this);

		SetCircleState(options);
		VertexBuffer vertex_buffers[2];
		vertex_buffers[0] = primitive_buffers[ECS_DEBUG_VERTEX_BUFFER_CIRCLE];
		vertex_buffers[1] = instanced_small_vertex_buffer;
		graphics->BindVertexBuffers(Stream<VertexBuffer>(vertex_buffers, std::size(vertex_buffers)));

		graphics->DrawInstanced(6, 1);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::DrawArrow(float3 start, float3 end, float size, Color color, DebugDrawCallOptions options)
	{
		float4 rotation_length = ArrowStartEndToRotation(start, end);
		DrawArrowRotation(start, { rotation_length.x, rotation_length.y, rotation_length.z }, rotation_length.w, size, color, options);
	}

	void DebugDrawer::DrawArrowRotation(float3 translation, float3 rotation, float length, float size, Color color, DebugDrawCallOptions options)
	{
		InstancedTransformData* instanced = MapInstancedVertex(this);
		SetInstancedColor(instanced, 0, color);

		Matrix matrix = MatrixScale(length, size, size) * MatrixRotation(rotation) * MatrixTranslation(translation);
		SetInstancedMatrix(instanced, 0, MatrixTranspose(matrix * camera_matrix));
		UnmapInstancedVertex(this);

		SetArrowCylinderState(options);

		VertexBuffer vertex_buffers[2];
		vertex_buffers[0] = primitive_buffers[ECS_DEBUG_VERTEX_BUFFER_ARROW_CYLINDER];
		vertex_buffers[1] = instanced_small_vertex_buffer;
		Stream<VertexBuffer> stream_buffers(vertex_buffers, std::size(vertex_buffers));
		graphics->BindVertexBuffers(stream_buffers);

		graphics->DrawInstanced(primitive_buffers[ECS_DEBUG_VERTEX_BUFFER_ARROW_CYLINDER].size, 1);

		// The instanced buffer must be mapped again
		instanced = MapInstancedVertex(this);
		SetInstancedColor(instanced, 0, color);
		Vector4 direction = VectorGlobals::RIGHT_4;
		direction = RotateVectorSIMD(rotation, direction);
		float3 direction_3;
		direction.StorePartialConstant<3>(&direction_3);
		matrix = MatrixScale(float3::Splat(size)) * MatrixRotation(rotation) * MatrixTranslation(translation + float3::Splat(length) * direction_3);
		SetInstancedMatrix(instanced, 0, MatrixTranspose(matrix * camera_matrix));
		UnmapInstancedVertex(this);

		vertex_buffers[0] = primitive_buffers[ECS_DEBUG_VERTEX_BUFFER_ARROW_HEAD];
		graphics->BindVertexBuffers(stream_buffers);

		SetArrowHeadState(options);

		graphics->DrawInstanced(primitive_buffers[ECS_DEBUG_VERTEX_BUFFER_ARROW_HEAD].size, 1);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::DrawAxes(float3 translation, float3 rotation, float size, Color color, DebugDrawCallOptions options)
	{
		/*DrawArrowRotation(translation, rotation, size, color, options);
		DrawArrowRotation(translation, { rotation.x, rotation.y + PI / 2, rotation.z }, size, color, options);
		DrawArrowRotation(translation, { rotation.x, rotation.y, rotation.z + PI / 2 }, size, color, options);*/

		// Use the axes primitive
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::DrawTriangle(float3 point0, float3 point1, float3 point2, Color color, DebugDrawCallOptions options)
	{
		float3* positions = MapPositions(this);
		InstancedTransformData* instanced = MapInstancedStructured(this);

		positions[0] = point0;
		positions[1] = point1;
		positions[2] = point2;

		SetInstancedColor(instanced, 0, color);
		SetInstancedMatrix(instanced, 0, MatrixTranspose(camera_matrix));
		UnmapAllStructured(this);
	
		SetTriangleState(options);
		BindSmallStructuredBuffers(this);

		graphics->DrawInstanced(3, 1);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	// It uses a scaled cube to draw it
	void DebugDrawer::DrawAABB(float3 translation, float3 scale, Color color, DebugDrawCallOptions options)
	{
		DrawOOBB(translation, float3::Splat(0.0f), scale, color, options);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::DrawOOBB(float3 translation, float3 rotation, float3 scale, Color color, DebugDrawCallOptions options)
	{
		InstancedTransformData* instanced = MapInstancedVertex(this);
		SetInstancedColor(instanced, 0, color);
		SetInstancedMatrix(instanced, 0, MatrixTranspose(MatrixTransform(translation, rotation, scale) * camera_matrix));
		UnmapInstancedVertex(this);

		SetOOBBState(options);
		VertexBuffer vertex_buffers[2];
		vertex_buffers[0] = primitive_buffers[ECS_DEBUG_VERTEX_BUFFER_CUBE];
		vertex_buffers[1] = instanced_small_vertex_buffer;

		graphics->DrawInstanced(primitive_buffers[ECS_DEBUG_VERTEX_BUFFER_CUBE].size, 1);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::DrawString(float3 position, float size, Stream<char> text, Color color, DebugDrawCallOptions options)
	{

	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::DrawNormal(VertexBuffer positions, VertexBuffer normals, float size, Color color, DebugDrawCallOptions options)
	{
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::DrawNormalSlice(VertexBuffer positions, VertexBuffer normals, float size, Color color, Stream<uint2> slices, DebugDrawCallOptions options)
	{
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::DrawNormals(VertexBuffer model_position, VertexBuffer model_normals, float size, Color color, Stream<Matrix> world_matrices, DebugDrawCallOptions options)
	{
	}

	// ----------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Draw Deck elements

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::DrawLineDeck(float time_delta) {
		// Calculate the maximum amount of vertices needed for every type
		// and also get an integer mask to indicate the elements for each type
		unsigned int counts[ELEMENT_COUNT] = { 0 };
		ushort2* indices[ELEMENT_COUNT];
		auto deck_pointer = &lines;
		unsigned int total_count = deck_pointer->GetElementCount();

		if (total_count > 0) {
			// Allocate 4 times the memory needed to be sure that each type has enough indices
			ushort2* type_indices = CalculateElementTypeCounts(allocator, counts, indices, deck_pointer);

			// Get the max
			unsigned int vertex_count = GetMaximumCount(counts);

			// Create temporary buffers to hold the data
			VertexBuffer position_buffer = graphics->CreateVertexBuffer(sizeof(float3), vertex_count);
			StructuredBuffer instanced_buffer = graphics->CreateStructuredBuffer(sizeof(InstancedTransformData), vertex_count);

			// Bind the vertex buffer and the structured buffer now
			graphics->BindVertexBuffer(position_buffer);
			ResourceView instanced_view = graphics->CreateBufferView(instanced_buffer);
			graphics->BindVertexResourceView(instanced_view);

			Matrix transposed_camera = MatrixTranspose(camera_matrix);

			unsigned int current_count = counts[WIREFRAME_DEPTH];
			ushort2* current_indices = indices[WIREFRAME_DEPTH];

			auto map_buffers = [this](VertexBuffer position_buffer, StructuredBuffer instanced_buffer, float3** positions, void** instanced_data) {
				*positions = (float3*)graphics->MapBuffer(position_buffer.buffer);
				*instanced_data = graphics->MapBuffer(instanced_buffer.buffer);
			};

			auto unmap_buffers = [this](VertexBuffer position_buffer, StructuredBuffer instanced_buffer) {
				graphics->UnmapBuffer(position_buffer.buffer);
				graphics->UnmapBuffer(instanced_buffer.buffer);
			};

			auto map_for = [&current_count, &current_indices, this, transposed_camera, deck_pointer](float3* positions, void* instanced_data) {
				for (size_t index = 0; index < current_count; index++) {
					DebugLine* line = &deck_pointer->buffers[current_indices[index].x][current_indices[index].y]; 
					positions[index * 2] = line->start; 
					positions[index * 2 + 1] = line->end; 
					SetInstancedColor(instanced_data, index, line->color); 
					SetInstancedMatrix(instanced_data, index, transposed_camera); 
				} 
			};
			
			float3* positions = nullptr;
			void* instanced_data = nullptr;

			auto draw_call = [&](DebugDrawCallOptions options) {
				if (current_count > 0) {
					// Map the buffers
					map_buffers(position_buffer, instanced_buffer, &positions, &instanced_data);

					// Fill the buffers
					map_for(positions, instanced_data);

					// Unmap the buffers
					unmap_buffers(position_buffer, instanced_buffer);

					// Set the state
					SetLineState(options);

					// Issue the draw call
					graphics->DrawInstanced(2, current_count);
				}
			};

			// Draw all the Wireframe depth elements
			draw_call({ true, false });

			// Draw all the Wireframe no depth elements
			current_count = counts[WIREFRAME_NO_DEPTH];
			current_indices = indices[WIREFRAME_NO_DEPTH];
			draw_call({ true, true });

			// Draw all the Solid depth elements
			current_count = counts[SOLID_DEPTH];
			current_indices = indices[SOLID_DEPTH];
			draw_call({ false, false });

			// Draw all the Solid no depth elements
			current_count = counts[SOLID_NO_DEPTH];
			current_indices = indices[SOLID_NO_DEPTH];
			draw_call({ false, true });

			// Update the duration and remove those elements that expired
			UpdateElementDurations(deck_pointer, time_delta);

			// Release the temporary vertex buffer, structured buffer and the temporary allocation
			position_buffer.buffer->Release();
			instanced_view.view->Release();
			instanced_buffer.buffer->Release();
			allocator->Deallocate(type_indices);
		}
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::DrawSphereDeck(float time_delta) {
		// Calculate the maximum amount of vertices needed for every type
		// and also get an integer mask to indicate the elements for each type
		unsigned int counts[ELEMENT_COUNT] = { 0 };
		ushort2* indices[ELEMENT_COUNT];
		auto deck_pointer = &spheres;
		unsigned int total_count = deck_pointer->GetElementCount();

		if (total_count > 0) {
			// Allocate 4 times the memory needed to be sure that each type has enough indices
			ushort2* type_indices = CalculateElementTypeCounts(allocator, counts, indices, deck_pointer);

			// Get the max
			unsigned int vertex_count = GetMaximumCount(counts);

			// Create temporary buffers to hold the data
			VertexBuffer instanced_buffer = graphics->CreateVertexBuffer(sizeof(InstancedTransformData), vertex_count);

			// Bind the vertex buffers now
			VertexBuffer vbuffers[2];
			vbuffers[0] = primitive_buffers[ECS_DEBUG_VERTEX_BUFFER_SPHERE];
			vbuffers[1] = instanced_buffer;
			graphics->BindVertexBuffers(Stream<VertexBuffer>(vbuffers, std::size(vbuffers)));

			unsigned int current_count = counts[WIREFRAME_DEPTH];
			ushort2* current_indices = indices[WIREFRAME_DEPTH];

			auto map_buffers = [this](VertexBuffer instanced_buffer, void** instanced_data) {
				*instanced_data = graphics->MapBuffer(instanced_buffer.buffer);
			};

			auto unmap_buffers = [this](VertexBuffer instanced_buffer) {
				graphics->UnmapBuffer(instanced_buffer.buffer);
			};

			auto map_for = [&current_count, &current_indices, this, deck_pointer](void* instanced_data) {
				for (size_t index = 0; index < current_count; index++) {
					DebugSphere* sphere = &deck_pointer->buffers[current_indices[index].x][current_indices[index].y];
					SetInstancedColor(instanced_data, index, sphere->color);
					Matrix matrix = MatrixScale(float3::Splat(sphere->radius)) * MatrixTranslation(sphere->position);
					SetInstancedMatrix(instanced_data, index, MatrixTranspose(matrix * camera_matrix));
				}
			};

			void* instanced_data = nullptr;

			auto draw_call = [&](DebugDrawCallOptions options) {
				if (current_count > 0) {
					// Map the buffers
					map_buffers(instanced_buffer, &instanced_data);

					// Fill the buffers
					map_for(instanced_data);

					// Unmap the buffers
					unmap_buffers(instanced_buffer);

					// Set the state
					SetSphereState(options);
					// Issue the draw call
					graphics->DrawInstanced(primitive_buffers[ECS_DEBUG_VERTEX_BUFFER_SPHERE].size, current_count);
				}
			};

			// Draw all the Wireframe depth elements
			draw_call({ true, false });

			// Draw all the Wireframe no depth elements
			current_count = counts[WIREFRAME_NO_DEPTH];
			current_indices = indices[WIREFRAME_NO_DEPTH];
			draw_call({ true, true });

			// Draw all the Solid depth elements
			current_count = counts[SOLID_DEPTH];
			current_indices = indices[SOLID_DEPTH];
			draw_call({ false, false });

			// Draw all the Solid no depth elements
			current_count = counts[SOLID_NO_DEPTH];
			current_indices = indices[SOLID_NO_DEPTH];
			draw_call({ false, true });

			// Update the duration and remove those elements that expired
			UpdateElementDurations(deck_pointer, time_delta);

			// Release the temporary vertex buffers and the temporary allocation
			instanced_buffer.buffer->Release();
			allocator->Deallocate(type_indices);
		}
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::DrawPointDeck(float time_delta) {
		// Calculate the maximum amount of vertices needed for every type
		// and also get an integer mask to indicate the elements for each type
		unsigned int counts[ELEMENT_COUNT] = { 0 };
		ushort2* indices[ELEMENT_COUNT];
		auto deck_pointer = &points;
		unsigned int total_count = deck_pointer->GetElementCount();

		if (total_count > 0) {
			// Allocate 4 times the memory needed to be sure that each type has enough indices
			ushort2* type_indices = CalculateElementTypeCounts(allocator, counts, indices, deck_pointer);

			// Get the max
			unsigned int vertex_count = GetMaximumCount(counts);

			// Create temporary buffers to hold the data
			VertexBuffer instanced_buffer = graphics->CreateVertexBuffer(sizeof(InstancedTransformData), vertex_count);

			// Bind the vertex buffers now
			VertexBuffer vbuffers[2];
			vbuffers[0] = primitive_buffers[ECS_DEBUG_VERTEX_BUFFER_SPHERE];
			vbuffers[1] = instanced_buffer;
			graphics->BindVertexBuffers(Stream<VertexBuffer>(vbuffers, std::size(vbuffers)));

			unsigned int current_count = counts[WIREFRAME_DEPTH];
			ushort2* current_indices = indices[WIREFRAME_DEPTH];

			auto map_buffers = [this](VertexBuffer instanced_buffer, void** instanced_data) {
				*instanced_data = graphics->MapBuffer(instanced_buffer.buffer);
			};

			auto unmap_buffers = [this](VertexBuffer instanced_buffer) {
				graphics->UnmapBuffer(instanced_buffer.buffer);
			};

			auto map_for = [&current_count, &current_indices, this, deck_pointer](void* instanced_data) {
				for (size_t index = 0; index < current_count; index++) {
					DebugPoint* point = &deck_pointer->buffers[current_indices[index].x][current_indices[index].y];
					SetInstancedColor(instanced_data, index, point->color);
					Matrix matrix = MatrixScale(float3::Splat(POINT_SIZE)) * MatrixTranslation(point->position);
					SetInstancedMatrix(instanced_data, index, MatrixTranspose(matrix * camera_matrix));
				}
			};

			void* instanced_data = nullptr;

			auto draw_call = [&](DebugDrawCallOptions options) {
				if (current_count > 0) {
					// Map the buffers
					map_buffers(instanced_buffer, &instanced_data);

					// Fill the buffers
					map_for(instanced_data);

					// Unmap the buffers
					unmap_buffers(instanced_buffer);

					// Set the state
					SetPointState(options);
					// Issue the draw call
					graphics->DrawInstanced(primitive_buffers[ECS_DEBUG_VERTEX_BUFFER_SPHERE].size, current_count);
				}
			};

			// Draw all the Wireframe depth elements - it should be solid filled
			draw_call({ false, false });

			// Draw all the Wireframe no depth elements - it should be solid filled
			current_count = counts[WIREFRAME_NO_DEPTH];
			current_indices = indices[WIREFRAME_NO_DEPTH];
			draw_call({ false, true });

			// Draw all the Solid depth elements
			current_count = counts[SOLID_DEPTH];
			current_indices = indices[SOLID_DEPTH];
			draw_call({ false, false });

			// Draw all the Solid no depth elements
			current_count = counts[SOLID_NO_DEPTH];
			current_indices = indices[SOLID_NO_DEPTH];
			draw_call({ false, true });

			// Update the duration and remove those elements that expired
			UpdateElementDurations(deck_pointer, time_delta);

			// Release the temporary vertex buffers and the temporary allocation
			instanced_buffer.buffer->Release();
			allocator->Deallocate(type_indices);
		}
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::DrawRectangleDeck(float time_delta) {
		// Calculate the maximum amount of vertices needed for every type
		// and also get an integer mask to indicate the elements for each type
		unsigned int counts[ELEMENT_COUNT] = { 0 };
		ushort2* indices[ELEMENT_COUNT];
		auto deck_pointer = &rectangles;
		unsigned int total_count = deck_pointer->GetElementCount();

		if (total_count > 0) {
			// Allocate 4 times the memory needed to be sure that each type has enough indices
			ushort2* type_indices = CalculateElementTypeCounts(allocator, counts, indices, deck_pointer);

			// Get the max
			unsigned int vertex_count = GetMaximumCount(counts);

			// Create temporary buffers to hold the data
			VertexBuffer position_buffer = graphics->CreateVertexBuffer(sizeof(float3), vertex_count);
			StructuredBuffer instanced_buffer = graphics->CreateStructuredBuffer(sizeof(InstancedTransformData), vertex_count);

			// Bind the vertex buffer and the structured buffer now
			graphics->BindVertexBuffer(position_buffer);
			ResourceView instanced_view = graphics->CreateBufferView(instanced_buffer);
			graphics->BindVertexResourceView(instanced_view);

			Matrix transposed_camera = MatrixTranspose(camera_matrix);

			unsigned int current_count = counts[WIREFRAME_DEPTH];
			ushort2* current_indices = indices[WIREFRAME_DEPTH];

			auto map_buffers = [this](VertexBuffer position_buffer, StructuredBuffer instanced_buffer, float3** positions, void** instanced_data) {
				*positions = (float3*)graphics->MapBuffer(position_buffer.buffer);
				*instanced_data = graphics->MapBuffer(instanced_buffer.buffer);
			};

			auto unmap_buffers = [this](VertexBuffer position_buffer, StructuredBuffer instanced_buffer) {
				graphics->UnmapBuffer(position_buffer.buffer);
				graphics->UnmapBuffer(instanced_buffer.buffer);
			};

			auto map_for = [&current_count, &current_indices, this, transposed_camera, deck_pointer](float3* positions, void* instanced_data) {
				for (size_t index = 0; index < current_count; index++) {
					DebugRectangle* rectangle = &deck_pointer->buffers[current_indices[index].x][current_indices[index].y];
					SetRectanglePositionsFront(positions + index * 6, rectangle->corner0, rectangle->corner1);
					SetInstancedColor(instanced_data, index, rectangle->color);
					SetInstancedMatrix(instanced_data, index, transposed_camera);
				}
			};

			float3* positions = nullptr;
			void* instanced_data = nullptr;

			auto draw_call = [&](DebugDrawCallOptions options) {
				if (current_count > 0) {
					// Map the buffers
					map_buffers(position_buffer, instanced_buffer, &positions, &instanced_data);

					// Fill the buffers
					map_for(positions, instanced_data);

					// Unmap the buffers
					unmap_buffers(position_buffer, instanced_buffer);

					// Set the state
					SetRectangleState(options);

					// Issue the draw call
					graphics->DrawInstanced(6, current_count);
				}
			};

			// Draw all the Wireframe depth elements
			draw_call({ true, false });

			// Draw all the Wireframe no depth elements
			current_count = counts[WIREFRAME_NO_DEPTH];
			current_indices = indices[WIREFRAME_NO_DEPTH];
			draw_call({ true, true });

			// Draw all the Solid depth elements
			current_count = counts[SOLID_DEPTH];
			current_indices = indices[SOLID_DEPTH];
			draw_call({ false, false });

			// Draw all the Solid no depth elements
			current_count = counts[SOLID_NO_DEPTH];
			current_indices = indices[SOLID_NO_DEPTH];
			draw_call({ false, true });

			// Update the duration and remove those elements that expired
			UpdateElementDurations(deck_pointer, time_delta);

			// Release the temporary vertex buffer, structured buffer and the temporary allocation
			position_buffer.buffer->Release();
			instanced_view.view->Release();
			instanced_buffer.buffer->Release();
			allocator->Deallocate(type_indices);
		}
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::DrawCrossDeck(float time_delta) {
		// Calculate the maximum amount of vertices needed for every type
		// and also get an integer mask to indicate the elements for each type
		unsigned int counts[ELEMENT_COUNT] = { 0 };
		ushort2* indices[ELEMENT_COUNT];
		auto deck_pointer = &crosses;
		unsigned int total_count = deck_pointer->GetElementCount();

		if (total_count > 0) {
			// Allocate 4 times the memory needed to be sure that each type has enough indices
			ushort2* type_indices = CalculateElementTypeCounts(allocator, counts, indices, deck_pointer);

			// Get the max
			unsigned int vertex_count = GetMaximumCount(counts);

			// Create temporary buffers to hold the data
			VertexBuffer instanced_buffer = graphics->CreateVertexBuffer(sizeof(InstancedTransformData), vertex_count);

			// Bind the vertex buffers now
			VertexBuffer vbuffers[2];
			vbuffers[0] = primitive_buffers[ECS_DEBUG_VERTEX_BUFFER_CROSS];
			vbuffers[1] = instanced_buffer;
			graphics->BindVertexBuffers(Stream<VertexBuffer>(vbuffers, std::size(vbuffers)));

			unsigned int current_count = counts[WIREFRAME_DEPTH];
			ushort2* current_indices = indices[WIREFRAME_DEPTH];

			auto map_buffers = [this](VertexBuffer instanced_buffer, void** instanced_data) {
				*instanced_data = graphics->MapBuffer(instanced_buffer.buffer);
			};

			auto unmap_buffers = [this](VertexBuffer instanced_buffer) {
				graphics->UnmapBuffer(instanced_buffer.buffer);
			};

			auto map_for = [&current_count, &current_indices, this, deck_pointer](void* instanced_data) {
				for (size_t index = 0; index < current_count; index++) {
					DebugCross* cross = &deck_pointer->buffers[current_indices[index].x][current_indices[index].y];
					SetInstancedColor(instanced_data, index, cross->color);
					Matrix matrix = MatrixScale(float3::Splat(cross->size)) * MatrixTranslation(cross->position);
					SetInstancedMatrix(instanced_data, index, MatrixTranspose(matrix * camera_matrix));
				}
			};

			void* instanced_data = nullptr;

			auto draw_call = [&](DebugDrawCallOptions options) {
				if (current_count > 0) {
					// Map the buffers
					map_buffers(instanced_buffer, &instanced_data);

					// Fill the buffers
					map_for(instanced_data);

					// Unmap the buffers
					unmap_buffers(instanced_buffer);

					// Set the state
					SetCrossState(options);
					// Issue the draw call
					graphics->DrawInstanced(primitive_buffers[ECS_DEBUG_VERTEX_BUFFER_CROSS].size, current_count);
				}
			};

			// Draw all the Wireframe depth elements - it should be solid filled
			draw_call({ false, false });

			// Draw all the Wireframe no depth elements - it should be solid filled
			current_count = counts[WIREFRAME_NO_DEPTH];
			current_indices = indices[WIREFRAME_NO_DEPTH];
			draw_call({ false, true });

			// Draw all the Solid depth elements
			current_count = counts[SOLID_DEPTH];
			current_indices = indices[SOLID_DEPTH];
			draw_call({ false, false });

			// Draw all the Solid no depth elements
			current_count = counts[SOLID_NO_DEPTH];
			current_indices = indices[SOLID_NO_DEPTH];
			draw_call({ false, true });

			// Update the duration and remove those elements that expired
			UpdateElementDurations(deck_pointer, time_delta);

			// Release the temporary vertex buffers and the temporary allocation
			instanced_buffer.buffer->Release();
			allocator->Deallocate(type_indices);
		}
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::DrawCircleDeck(float time_delta) {
		// Calculate the maximum amount of vertices needed for every type
		// and also get an integer mask to indicate the elements for each type
		unsigned int counts[ELEMENT_COUNT] = { 0 };
		ushort2* indices[ELEMENT_COUNT];
		auto deck_pointer = &circles;
		unsigned int total_count = deck_pointer->GetElementCount();

		if (total_count > 0) {
			// Allocate 4 times the memory needed to be sure that each type has enough indices
			ushort2* type_indices = CalculateElementTypeCounts(allocator, counts, indices, deck_pointer);

			// Get the max
			unsigned int vertex_count = GetMaximumCount(counts);

			// Create temporary buffers to hold the data
			VertexBuffer instanced_buffer = graphics->CreateVertexBuffer(sizeof(InstancedTransformData), vertex_count);

			// Bind the vertex buffers now
			VertexBuffer vbuffers[2];
			vbuffers[0] = primitive_buffers[ECS_DEBUG_VERTEX_BUFFER_CIRCLE];
			vbuffers[1] = instanced_buffer;
			graphics->BindVertexBuffers(Stream<VertexBuffer>(vbuffers, std::size(vbuffers)));

			unsigned int current_count = counts[WIREFRAME_DEPTH];
			ushort2* current_indices = indices[WIREFRAME_DEPTH];

			auto map_buffers = [this](VertexBuffer instanced_buffer, void** instanced_data) {
				*instanced_data = graphics->MapBuffer(instanced_buffer.buffer);
			};

			auto unmap_buffers = [this](VertexBuffer instanced_buffer) {
				graphics->UnmapBuffer(instanced_buffer.buffer);
			};

			auto map_for = [&current_count, &current_indices, this, deck_pointer](void* instanced_data) {
				for (size_t index = 0; index < current_count; index++) {
					DebugCircle* circle = &deck_pointer->buffers[current_indices[index].x][current_indices[index].y];
					SetInstancedColor(instanced_data, index, circle->color);
					Matrix matrix = MatrixScale(float3::Splat(circle->radius)) * MatrixRotation(circle->rotation) * MatrixTranslation(circle->position);
					SetInstancedMatrix(instanced_data, index, MatrixTranspose(matrix * camera_matrix));
				}
			};

			void* instanced_data = nullptr;

			auto draw_call = [&](DebugDrawCallOptions options) {
				if (current_count > 0) {
					// Map the buffers
					map_buffers(instanced_buffer, &instanced_data);

					// Fill the buffers
					map_for(instanced_data);

					// Unmap the buffers
					unmap_buffers(instanced_buffer);

					// Set the state
					SetCircleState(options);
					// Issue the draw call
					graphics->DrawInstanced(primitive_buffers[ECS_DEBUG_VERTEX_BUFFER_CIRCLE].size, current_count);
				}
			};

			// Draw all the Wireframe depth elements - it should be solid filled
			draw_call({ false, false });

			// Draw all the Wireframe no depth elements - it should be solid filled
			current_count = counts[WIREFRAME_NO_DEPTH];
			current_indices = indices[WIREFRAME_NO_DEPTH];
			draw_call({ false, true });

			// Draw all the Solid depth elements
			current_count = counts[SOLID_DEPTH];
			current_indices = indices[SOLID_DEPTH];
			draw_call({ false, false });

			// Draw all the Solid no depth elements
			current_count = counts[SOLID_NO_DEPTH];
			current_indices = indices[SOLID_NO_DEPTH];
			draw_call({ false, true });

			// Update the duration and remove those elements that expired
			UpdateElementDurations(deck_pointer, time_delta);

			// Release the temporary vertex buffers and the temporary allocation
			instanced_buffer.buffer->Release();
			allocator->Deallocate(type_indices);
		}
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::DrawArrowDeck(float time_delta) {
		// Calculate the maximum amount of vertices needed for every type
		// and also get an integer mask to indicate the elements for each type
		unsigned int counts[ELEMENT_COUNT] = { 0 };
		ushort2* indices[ELEMENT_COUNT];
		auto deck_pointer = &arrows;
		unsigned int total_count = deck_pointer->GetElementCount();

		if (total_count > 0) {
			// Allocate 4 times the memory needed to be sure that each type has enough indices
			ushort2* type_indices = CalculateElementTypeCounts(allocator, counts, indices, deck_pointer);

			// Get the max
			unsigned int vertex_count = GetMaximumCount(counts);

			// Create temporary buffers to hold the data
			VertexBuffer instanced_buffer = graphics->CreateVertexBuffer(sizeof(InstancedTransformData), vertex_count);

			// Bind the vertex buffers now
			VertexBuffer vbuffers[2];
			vbuffers[0] = primitive_buffers[ECS_DEBUG_VERTEX_BUFFER_ARROW_CYLINDER];
			vbuffers[1] = instanced_buffer;
			graphics->BindVertexBuffers(Stream<VertexBuffer>(vbuffers, std::size(vbuffers)));

			unsigned int current_count = counts[WIREFRAME_DEPTH];
			ushort2* current_indices = indices[WIREFRAME_DEPTH];

			auto map_buffers = [this](VertexBuffer instanced_buffer, void** instanced_data) {
				*instanced_data = graphics->MapBuffer(instanced_buffer.buffer);
			};

			auto unmap_buffers = [this](VertexBuffer instanced_buffer) {
				graphics->UnmapBuffer(instanced_buffer.buffer);
			};

			auto map_for_cylinder = [&current_count, &current_indices, this, deck_pointer](void* instanced_data) {
				for (size_t index = 0; index < current_count; index++) {
					DebugArrow* arrow = &deck_pointer->buffers[current_indices[index].x][current_indices[index].y];
					SetInstancedColor(instanced_data, index, arrow->color);
					Matrix matrix = MatrixScale(arrow->length, arrow->size, arrow->size) * MatrixRotation(arrow->rotation) * MatrixTranslation(arrow->translation);
					SetInstancedMatrix(instanced_data, index, MatrixTranspose(matrix * camera_matrix));
				}
			};

			auto map_for_header = [&current_count, &current_indices, this, deck_pointer](void* instanced_data) {
				for (size_t index = 0; index < current_count; index++) {
					DebugArrow* arrow = &deck_pointer->buffers[current_indices[index].x][current_indices[index].y];
					SetInstancedColor(instanced_data, index, arrow->color);
					Vector4 direction_simd = VectorGlobals::RIGHT_4;
					direction_simd = RotateVectorSIMD(arrow->rotation, direction_simd);
					float3 direction;
					direction_simd.StorePartialConstant<3>(&direction);
					Matrix matrix = MatrixScale(float3::Splat(arrow->size)) * MatrixRotation(arrow->rotation) 
						* MatrixTranslation(arrow->translation + float3::Splat(arrow->length) * direction);
					SetInstancedMatrix(instanced_data, index, MatrixTranspose(matrix * camera_matrix));
				}
			};

			void* instanced_data = nullptr;

			auto draw_call_cylinder = [&](DebugDrawCallOptions options) {
				if (current_count > 0) {
					// Map the buffers
					map_buffers(instanced_buffer, &instanced_data);

					// Fill the buffers
					map_for_cylinder(instanced_data);

					// Unmap the buffers
					unmap_buffers(instanced_buffer);

					// Set the state
					SetArrowCylinderState(options);
					// Issue the draw call
					graphics->DrawInstanced(primitive_buffers[ECS_DEBUG_VERTEX_BUFFER_ARROW_HEAD].size, current_count);
				}
			};

			auto draw_call_head = [&](DebugDrawCallOptions options) {
				if (current_count > 0) {
					// Map the buffers
					map_buffers(instanced_buffer, &instanced_data);

					// Fill the buffers
					map_for_cylinder(instanced_data);

					// Unmap the buffers
					unmap_buffers(instanced_buffer);

					// Set the state
					SetArrowHeadState(options);
					// Issue the draw call
					graphics->DrawInstanced(primitive_buffers[ECS_DEBUG_VERTEX_BUFFER_ARROW_HEAD].size, current_count);
				}
			};

			// Draw all the cylinder Wireframe depth elements - it should be solid filled
			draw_call_cylinder({ false, false });

			// Draw all the cylinder Wireframe no depth elements - it should be solid filled
			current_count = counts[WIREFRAME_NO_DEPTH];
			current_indices = indices[WIREFRAME_NO_DEPTH];
			draw_call_cylinder({ false, true });

			// Draw all the cylinder Solid depth elements
			current_count = counts[SOLID_DEPTH];
			current_indices = indices[SOLID_DEPTH];
			draw_call_cylinder({ false, false });

			// Draw all the cylinder Solid no depth elements
			current_count = counts[SOLID_NO_DEPTH];
			current_indices = indices[SOLID_NO_DEPTH];
			draw_call_cylinder({ false, true });

			current_count = counts[WIREFRAME_DEPTH];
			current_count = counts[WIREFRAME_DEPTH];

			// Draw all the head Wireframe depth elements - it should be solid filled
			draw_call_head({ false, false });

			// Draw all the head Wireframe no depth elements - it should be solid filled
			current_count = counts[WIREFRAME_NO_DEPTH];
			current_indices = indices[WIREFRAME_NO_DEPTH];
			draw_call_head({ false, true });

			// Draw all the head Solid depth elements
			current_count = counts[SOLID_DEPTH];
			current_indices = indices[SOLID_DEPTH];
			draw_call_head({ false, false });

			// Draw all the head Solid no depth elements
			current_count = counts[SOLID_NO_DEPTH];
			current_indices = indices[SOLID_NO_DEPTH];
			draw_call_head({ false, true });

			// Update the duration and remove those elements that expired
			UpdateElementDurations(deck_pointer, time_delta);

			// Release the temporary vertex buffers and the temporary allocation
			instanced_buffer.buffer->Release();
			allocator->Deallocate(type_indices);
		}
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::DrawAxesDeck(float time_delta) {
		// Calculate the maximum amount of vertices needed for every type
		// and also get an integer mask to indicate the elements for each type
		unsigned int counts[ELEMENT_COUNT] = { 0 };
		ushort2* indices[ELEMENT_COUNT];
		auto deck_pointer = &axes;
		unsigned int total_count = deck_pointer->GetElementCount();

		if (total_count > 0) {
			// Allocate 4 times the memory needed to be sure that each type has enough indices
			ushort2* type_indices = CalculateElementTypeCounts(allocator, counts, indices, deck_pointer);

			// Get the max
			unsigned int vertex_count = GetMaximumCount(counts);

			// Create temporary buffers to hold the data
			VertexBuffer instanced_buffer = graphics->CreateVertexBuffer(sizeof(InstancedTransformData), vertex_count);

			// Bind the vertex buffers now
			VertexBuffer vbuffers[2];
			vbuffers[0] = primitive_buffers[ECS_DEBUG_VERTEX_BUFFER_AXES];
			vbuffers[1] = instanced_buffer;
			graphics->BindVertexBuffers(Stream<VertexBuffer>(vbuffers, std::size(vbuffers)));

			unsigned int current_count = counts[WIREFRAME_DEPTH];
			ushort2* current_indices = indices[WIREFRAME_DEPTH];

			auto map_buffers = [this](VertexBuffer instanced_buffer, void** instanced_data) {
				*instanced_data = graphics->MapBuffer(instanced_buffer.buffer);
			};

			auto unmap_buffers = [this](VertexBuffer instanced_buffer) {
				graphics->UnmapBuffer(instanced_buffer.buffer);
			};

			auto map_for = [&current_count, &current_indices, this, deck_pointer](void* instanced_data) {
				for (size_t index = 0; index < current_count; index++) {
					DebugAxes* axes = &deck_pointer->buffers[current_indices[index].x][current_indices[index].y];
					SetInstancedColor(instanced_data, index, axes->color);
					Matrix matrix = MatrixScale(float3::Splat(axes->size)) * MatrixRotation(axes->rotation) * MatrixTranslation(axes->translation);
					SetInstancedMatrix(instanced_data, index, MatrixTranspose(matrix * camera_matrix));
				}
			};

			void* instanced_data = nullptr;

			auto draw_call = [&](DebugDrawCallOptions options) {
				if (current_count > 0) {
					// Map the buffers
					map_buffers(instanced_buffer, &instanced_data);

					// Fill the buffers
					map_for(instanced_data);

					// Unmap the buffers
					unmap_buffers(instanced_buffer);

					// Set the state
					SetAxesState(options);
					// Issue the draw call
					graphics->DrawInstanced(primitive_buffers[ECS_DEBUG_VERTEX_BUFFER_AXES].size, current_count);
				}
			};

			// Draw all the Wireframe depth elements - it should be solid filled
			draw_call({ false, false });

			// Draw all the Wireframe no depth elements - it should be solid filled
			current_count = counts[WIREFRAME_NO_DEPTH];
			current_indices = indices[WIREFRAME_NO_DEPTH];
			draw_call({ false, true });

			// Draw all the Solid depth elements
			current_count = counts[SOLID_DEPTH];
			current_indices = indices[SOLID_DEPTH];
			draw_call({ false, false });

			// Draw all the Solid no depth elements
			current_count = counts[SOLID_NO_DEPTH];
			current_indices = indices[SOLID_NO_DEPTH];
			draw_call({ false, true });

			// Update the duration and remove those elements that expired
			UpdateElementDurations(deck_pointer, time_delta);

			// Release the temporary vertex buffers and the temporary allocation
			instanced_buffer.buffer->Release();
			allocator->Deallocate(type_indices);
		}
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::DrawTriangleDeck(float time_delta) {
		// Calculate the maximum amount of vertices needed for every type
		// and also get an integer mask to indicate the elements for each type
		unsigned int counts[ELEMENT_COUNT] = { 0 };
		ushort2* indices[ELEMENT_COUNT];
		auto deck_pointer = &triangles;
		unsigned int total_count = deck_pointer->GetElementCount();

		if (total_count > 0) {
			// Allocate 4 times the memory needed to be sure that each type has enough indices
			ushort2* type_indices = CalculateElementTypeCounts(allocator, counts, indices, deck_pointer);

			// Get the max
			unsigned int vertex_count = GetMaximumCount(counts);

			// Create temporary buffers to hold the data
			VertexBuffer position_buffer = graphics->CreateVertexBuffer(sizeof(float3), vertex_count);
			StructuredBuffer instanced_buffer = graphics->CreateStructuredBuffer(sizeof(InstancedTransformData), vertex_count);

			// Bind the vertex buffer and the structured buffer now
			graphics->BindVertexBuffer(position_buffer);
			ResourceView instanced_view = graphics->CreateBufferView(instanced_buffer);
			graphics->BindVertexResourceView(instanced_view);

			Matrix transposed_camera = MatrixTranspose(camera_matrix);

			unsigned int current_count = counts[WIREFRAME_DEPTH];
			ushort2* current_indices = indices[WIREFRAME_DEPTH];

			auto map_buffers = [this](VertexBuffer position_buffer, StructuredBuffer instanced_buffer, float3** positions, void** instanced_data) {
				*positions = (float3*)graphics->MapBuffer(position_buffer.buffer);
				*instanced_data = graphics->MapBuffer(instanced_buffer.buffer);
			};

			auto unmap_buffers = [this](VertexBuffer position_buffer, StructuredBuffer instanced_buffer) {
				graphics->UnmapBuffer(position_buffer.buffer);
				graphics->UnmapBuffer(instanced_buffer.buffer);
			};

			auto map_for = [&current_count, &current_indices, this, transposed_camera, deck_pointer](float3* positions, void* instanced_data) {
				for (size_t index = 0; index < current_count; index++) {
					DebugTriangle* triangle = &deck_pointer->buffers[current_indices[index].x][current_indices[index].y];
					positions[index * 3] = triangle->point0;
					positions[index * 3 + 1] = triangle->point1;
					positions[index * 3 + 2] = triangle->point2;
					SetInstancedColor(instanced_data, index, triangle->color);
					SetInstancedMatrix(instanced_data, index, transposed_camera);
				}
			};

			float3* positions = nullptr;
			void* instanced_data = nullptr;

			auto draw_call = [&](DebugDrawCallOptions options) {
				if (current_count > 0) {
					// Map the buffers
					map_buffers(position_buffer, instanced_buffer, &positions, &instanced_data);

					// Fill the buffers
					map_for(positions, instanced_data);

					// Unmap the buffers
					unmap_buffers(position_buffer, instanced_buffer);

					// Set the state
					SetTriangleState(options);

					// Issue the draw call
					graphics->DrawInstanced(3, current_count);
				}
			};

			// Draw all the Wireframe depth elements
			draw_call({ true, false });

			// Draw all the Wireframe no depth elements
			current_count = counts[WIREFRAME_NO_DEPTH];
			current_indices = indices[WIREFRAME_NO_DEPTH];
			draw_call({ true, true });

			// Draw all the Solid depth elements
			current_count = counts[SOLID_DEPTH];
			current_indices = indices[SOLID_DEPTH];
			draw_call({ false, false });

			// Draw all the Solid no depth elements
			current_count = counts[SOLID_NO_DEPTH];
			current_indices = indices[SOLID_NO_DEPTH];
			draw_call({ false, true });

			// Update the duration and remove those elements that expired
			UpdateElementDurations(deck_pointer, time_delta);

			// Release the temporary vertex buffer, structured buffer and the temporary allocation
			position_buffer.buffer->Release();
			instanced_view.view->Release();
			instanced_buffer.buffer->Release();
			allocator->Deallocate(type_indices);
		}
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::DrawAABBDeck(float time_delta) {
		// Calculate the maximum amount of vertices needed for every type
		// and also get an integer mask to indicate the elements for each type
		unsigned int counts[ELEMENT_COUNT] = { 0 };
		ushort2* indices[ELEMENT_COUNT];
		auto deck_pointer = &aabbs;
		unsigned int total_count = deck_pointer->GetElementCount();

		if (total_count > 0) {
			// Allocate 4 times the memory needed to be sure that each type has enough indices
			ushort2* type_indices = CalculateElementTypeCounts(allocator, counts, indices, deck_pointer);

			// Get the max
			unsigned int vertex_count = GetMaximumCount(counts);

			// Create temporary buffers to hold the data
			VertexBuffer instanced_buffer = graphics->CreateVertexBuffer(sizeof(InstancedTransformData), vertex_count);

			// Bind the vertex buffers now
			VertexBuffer vbuffers[2];
			vbuffers[0] = primitive_buffers[ECS_DEBUG_VERTEX_BUFFER_CUBE];
			vbuffers[1] = instanced_buffer;
			graphics->BindVertexBuffers(Stream<VertexBuffer>(vbuffers, std::size(vbuffers)));

			unsigned int current_count = counts[WIREFRAME_DEPTH];
			ushort2* current_indices = indices[WIREFRAME_DEPTH];

			auto map_buffers = [this](VertexBuffer instanced_buffer, void** instanced_data) {
				*instanced_data = graphics->MapBuffer(instanced_buffer.buffer);
			};

			auto unmap_buffers = [this](VertexBuffer instanced_buffer) {
				graphics->UnmapBuffer(instanced_buffer.buffer);
			};

			auto map_for = [&current_count, &current_indices, this, deck_pointer](void* instanced_data) {
				for (size_t index = 0; index < current_count; index++) {
					DebugAABB* aabb = &deck_pointer->buffers[current_indices[index].x][current_indices[index].y];
					SetInstancedColor(instanced_data, index, aabb->color);
					Matrix matrix = MatrixScale(aabb->scale) * MatrixTranslation(aabb->translation);
					SetInstancedMatrix(instanced_data, index, MatrixTranspose(matrix * camera_matrix));
				}
			};

			void* instanced_data = nullptr;

			auto draw_call = [&](DebugDrawCallOptions options) {
				if (current_count > 0) {
					// Map the buffers
					map_buffers(instanced_buffer, &instanced_data);

					// Fill the buffers
					map_for(instanced_data);

					// Unmap the buffers
					unmap_buffers(instanced_buffer);

					// Set the state
					SetOOBBState(options);
					// Issue the draw call
					graphics->DrawInstanced(primitive_buffers[ECS_DEBUG_VERTEX_BUFFER_CUBE].size, current_count);
				}
			};

			// Draw all the Wireframe depth elements - it should be solid filled
			draw_call({ false, false });

			// Draw all the Wireframe no depth elements - it should be solid filled
			current_count = counts[WIREFRAME_NO_DEPTH];
			current_indices = indices[WIREFRAME_NO_DEPTH];
			draw_call({ false, true });

			// Draw all the Solid depth elements
			current_count = counts[SOLID_DEPTH];
			current_indices = indices[SOLID_DEPTH];
			draw_call({ false, false });

			// Draw all the Solid no depth elements
			current_count = counts[SOLID_NO_DEPTH];
			current_indices = indices[SOLID_NO_DEPTH];
			draw_call({ false, true });

			// Update the duration and remove those elements that expired
			UpdateElementDurations(deck_pointer, time_delta);

			// Release the temporary vertex buffers and the temporary allocation
			instanced_buffer.buffer->Release();
			allocator->Deallocate(type_indices);
		}
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::DrawOOBBDeck(float time_delta) {
		// Calculate the maximum amount of vertices needed for every type
		// and also get an integer mask to indicate the elements for each type
		unsigned int counts[ELEMENT_COUNT] = { 0 };
		ushort2* indices[ELEMENT_COUNT];
		auto deck_pointer = &oobbs;
		unsigned int total_count = deck_pointer->GetElementCount();

		if (total_count > 0) {
			// Allocate 4 times the memory needed to be sure that each type has enough indices
			ushort2* type_indices = CalculateElementTypeCounts(allocator, counts, indices, deck_pointer);

			// Get the max
			unsigned int vertex_count = GetMaximumCount(counts);

			// Create temporary buffers to hold the data
			VertexBuffer instanced_buffer = graphics->CreateVertexBuffer(sizeof(InstancedTransformData), vertex_count);

			// Bind the vertex buffers now
			VertexBuffer vbuffers[2];
			vbuffers[0] = primitive_buffers[ECS_DEBUG_VERTEX_BUFFER_CUBE];
			vbuffers[1] = instanced_buffer;
			graphics->BindVertexBuffers(Stream<VertexBuffer>(vbuffers, std::size(vbuffers)));

			unsigned int current_count = counts[WIREFRAME_DEPTH];
			ushort2* current_indices = indices[WIREFRAME_DEPTH];

			auto map_buffers = [this](VertexBuffer instanced_buffer, void** instanced_data) {
				*instanced_data = graphics->MapBuffer(instanced_buffer.buffer);
			};

			auto unmap_buffers = [this](VertexBuffer instanced_buffer) {
				graphics->UnmapBuffer(instanced_buffer.buffer);
			};

			auto map_for = [&current_count, &current_indices, this, deck_pointer](void* instanced_data) {
				for (size_t index = 0; index < current_count; index++) {
					DebugOOBB* oobb = &deck_pointer->buffers[current_indices[index].x][current_indices[index].y];
					SetInstancedColor(instanced_data, index, oobb->color);
					Matrix matrix = MatrixTransform(oobb->translation, oobb->rotation, oobb->scale);
					SetInstancedMatrix(instanced_data, index, MatrixTranspose(matrix * camera_matrix));
				}
			};

			void* instanced_data = nullptr;

			auto draw_call = [&](DebugDrawCallOptions options) {
				if (current_count > 0) {
					// Map the buffers
					map_buffers(instanced_buffer, &instanced_data);

					// Fill the buffers
					map_for(instanced_data);

					// Unmap the buffers
					unmap_buffers(instanced_buffer);

					// Set the state
					SetOOBBState(options);
					// Issue the draw call
					graphics->DrawInstanced(primitive_buffers[ECS_DEBUG_VERTEX_BUFFER_CUBE].size, current_count);
				}
			};

			// Draw all the Wireframe depth elements - it should be solid filled
			draw_call({ false, false });

			// Draw all the Wireframe no depth elements - it should be solid filled
			current_count = counts[WIREFRAME_NO_DEPTH];
			current_indices = indices[WIREFRAME_NO_DEPTH];
			draw_call({ false, true });

			// Draw all the Solid depth elements
			current_count = counts[SOLID_DEPTH];
			current_indices = indices[SOLID_DEPTH];
			draw_call({ false, false });

			// Draw all the Solid no depth elements
			current_count = counts[SOLID_NO_DEPTH];
			current_indices = indices[SOLID_NO_DEPTH];
			draw_call({ false, true });

			// Update the duration and remove those elements that expired
			UpdateElementDurations(deck_pointer, time_delta);

			// Release the temporary vertex buffers and the temporary allocation
			instanced_buffer.buffer->Release();
			allocator->Deallocate(type_indices);
		}
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::DrawStringDeck(float time_delta) {

	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::DrawAll(float time_delta)
	{
		FlushAll();
		DrawLineDeck(time_delta);
		DrawSphereDeck(time_delta);
		DrawPointDeck(time_delta);
		DrawRectangleDeck(time_delta);
		DrawCrossDeck(time_delta);
		DrawCircleDeck(time_delta);
		DrawArrowDeck(time_delta);
		DrawAxesDeck(time_delta);
		DrawTriangleDeck(time_delta);
		DrawAABBDeck(time_delta);
		DrawOOBBDeck(time_delta);
		DrawStringDeck(time_delta);
	}

	// ----------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Flush

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::FlushAll()
	{
		for (size_t index = 0; index < thread_count; index++) {
			FlushLine(index);
			FlushSphere(index);
			FlushPoint(index);
			FlushRectangle(index);
			FlushCross(index);
			FlushCircle(index);
			FlushArrow(index);
			FlushAxes(index);
			FlushTriangle(index);
			FlushAABB(index);
			FlushOOBB(index);
			FlushString(index);
		}
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::FlushLine(unsigned int thread_index)
	{
		if (thread_lines[thread_index].size > 0) {
			thread_locks[ECS_DEBUG_PRIMITIVE_LINE]->lock();
			DebugLine* copy_position = lines.GetEntries(thread_lines[thread_index].size);
			if (copy_position == nullptr) {
				allocator->Lock();
				lines.AllocateChunks(1);
				copy_position = lines.GetEntries(thread_lines[thread_index].size);
				allocator->Unlock();
			}
			thread_lines[thread_index].CopyTo(copy_position);
			thread_lines[thread_index].size = 0;
			thread_locks[ECS_DEBUG_PRIMITIVE_LINE]->unlock();
		}
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::FlushSphere(unsigned int thread_index)
	{
		if (thread_spheres[thread_index].size > 0) {
			thread_locks[ECS_DEBUG_PRIMITIVE_SPHERE]->lock();
			DebugSphere* copy_position = spheres.GetEntries(thread_spheres[thread_index].size);
			if (copy_position == nullptr) {
				allocator->Lock();
				spheres.AllocateChunks(1);
				copy_position = spheres.GetEntries(thread_spheres[thread_index].size);
				allocator->Unlock();
			}
			thread_spheres[thread_index].CopyTo(copy_position);
			thread_spheres[thread_index].size = 0;
			thread_locks[ECS_DEBUG_PRIMITIVE_SPHERE]->unlock();
		}
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::FlushPoint(unsigned int thread_index)
	{
		if (thread_points[thread_index].size > 0) {
			thread_locks[ECS_DEBUG_PRIMITIVE_POINT]->lock();
			DebugPoint* copy_position = points.GetEntries(thread_points[thread_index].size);
			if (copy_position == nullptr) {
				allocator->Lock();
				points.AllocateChunks(1);
				copy_position = points.GetEntries(thread_points[thread_index].size);
				allocator->Unlock();
			}
			thread_points[thread_index].CopyTo(copy_position);
			thread_points[thread_index].size = 0;
			thread_locks[ECS_DEBUG_PRIMITIVE_POINT]->unlock();
		}
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::FlushRectangle(unsigned int thread_index)
	{
		if (thread_rectangles[thread_index].size > 0) {
			thread_locks[ECS_DEBUG_PRIMITIVE_RECTANGLE]->lock();
			DebugRectangle* copy_position = rectangles.GetEntries(thread_rectangles[thread_index].size);
			if (copy_position == nullptr) {
				allocator->Lock();
				rectangles.AllocateChunks(1);
				copy_position = rectangles.GetEntries(thread_rectangles[thread_index].size);
				allocator->Unlock();
			}
			thread_rectangles[thread_index].CopyTo(copy_position);
			thread_locks[ECS_DEBUG_PRIMITIVE_RECTANGLE]->unlock();
		}
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::FlushCross(unsigned int thread_index)
	{
		if (thread_crosses[thread_index].size > 0) {
			thread_locks[ECS_DEBUG_PRIMITIVE_CROSS]->lock();
			DebugCross* copy_position = crosses.GetEntries(thread_crosses[thread_index].size);
			if (copy_position == nullptr) {
				allocator->Lock();
				crosses.AllocateChunks(1);
				copy_position = crosses.GetEntries(thread_crosses[thread_index].size);
				allocator->Unlock();
			}
			thread_crosses[thread_index].CopyTo(copy_position);
			thread_locks[ECS_DEBUG_PRIMITIVE_CROSS]->unlock();
		}
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::FlushCircle(unsigned int thread_index)
	{
		if (thread_circles[thread_index].size > 0) {
			thread_locks[ECS_DEBUG_PRIMITIVE_CIRCLE]->lock();
			DebugCircle* copy_position = circles.GetEntries(thread_circles[thread_index].size);
			if (copy_position == nullptr) {
				allocator->Lock();
				circles.AllocateChunks(1);
				copy_position = circles.GetEntries(thread_circles[thread_index].size);
				allocator->Unlock();
			}
			thread_circles[thread_index].CopyTo(copy_position);
			thread_locks[ECS_DEBUG_PRIMITIVE_CIRCLE]->unlock();
		}
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::FlushArrow(unsigned int thread_index)
	{
		if (thread_arrows[thread_index].size > 0) {
			thread_locks[ECS_DEBUG_PRIMITIVE_ARROW]->lock();
			DebugArrow* copy_position = arrows.GetEntries(thread_arrows[thread_index].size);
			if (copy_position == nullptr) {
				allocator->Lock();
				arrows.AllocateChunks(1);
				copy_position = arrows.GetEntries(thread_arrows[thread_index].size);
				allocator->Unlock();
			}
			thread_arrows[thread_index].CopyTo(copy_position);
			thread_locks[ECS_DEBUG_PRIMITIVE_ARROW]->unlock();
		}
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::FlushAxes(unsigned int thread_index)
	{
		if (thread_axes[thread_index].size > 0) {
			thread_locks[ECS_DEBUG_PRIMITIVE_AXES]->lock();
			DebugAxes* copy_position = axes.GetEntries(thread_axes[thread_index].size);
			if (copy_position == nullptr) {
				allocator->Lock();
				axes.AllocateChunks(1);
				copy_position = axes.GetEntries(thread_axes[thread_index].size);
				allocator->Unlock();
			}
			thread_axes[thread_index].CopyTo(copy_position);
			thread_locks[ECS_DEBUG_PRIMITIVE_AXES]->unlock();
		}
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::FlushTriangle(unsigned int thread_index)
	{
		if (thread_triangles[thread_index].size > 0) {
			thread_locks[ECS_DEBUG_PRIMITIVE_TRIANGLE]->lock();
			DebugTriangle* copy_position = triangles.GetEntriesWithAllocation(thread_triangles[thread_index].size);
			if (copy_position == nullptr) {
				allocator->Lock();
				triangles.AllocateChunks(1);
				copy_position = triangles.GetEntries(thread_triangles[thread_index].size);
				allocator->Unlock();
			}
			thread_triangles[thread_index].CopyTo(copy_position);
			thread_locks[ECS_DEBUG_PRIMITIVE_TRIANGLE]->unlock();
		}
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::FlushAABB(unsigned int thread_index)
	{
		if (thread_aabbs[thread_index].size > 0) {
			thread_locks[ECS_DEBUG_PRIMITIVE_AABB]->lock();
			DebugAABB* copy_position = aabbs.GetEntriesWithAllocation(thread_aabbs[thread_index].size);
			if (copy_position == nullptr) {
				allocator->Lock();
				aabbs.AllocateChunks(1);
				copy_position = aabbs.GetEntries(thread_aabbs[thread_index].size);
				allocator->Unlock();
			}
			thread_aabbs[thread_index].CopyTo(copy_position);
			thread_locks[ECS_DEBUG_PRIMITIVE_AABB]->unlock();
		}
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::FlushOOBB(unsigned int thread_index)
	{
		if (thread_oobbs[thread_index].size > 0) {
			thread_locks[ECS_DEBUG_PRIMITIVE_OOBB]->lock();
			DebugOOBB* copy_position = oobbs.GetEntriesWithAllocation(thread_oobbs[thread_index].size);
			if (copy_position == nullptr) {
				allocator->Lock();
				oobbs.AllocateChunks(1);
				copy_position = oobbs.GetEntries(thread_oobbs[thread_index].size);
				allocator->Unlock();
			}
			thread_oobbs[thread_index].CopyTo(copy_position);
			thread_locks[ECS_DEBUG_PRIMITIVE_OOBB]->unlock();
		}
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::FlushString(unsigned int thread_index)
	{
		if (thread_strings[thread_index].size > 0) {
			thread_locks[ECS_DEBUG_PRIMITIVE_STRING]->lock();
			DebugString* copy_position = strings.GetEntriesWithAllocation(thread_strings[thread_index].size);
			if (copy_position == nullptr) {
				allocator->Lock();
				strings.AllocateChunks(1);
				copy_position = strings.GetEntries(thread_strings[thread_index].size);
				allocator->Unlock();
			}
			thread_strings[thread_index].CopyTo(copy_position);
			thread_locks[ECS_DEBUG_PRIMITIVE_STRING]->unlock();
		}
	}

	// ----------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Set state

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::SetLineState(DebugDrawCallOptions options) {
		unsigned int shader_type = ECS_DEBUG_SHADER_STRUCTURED_INSTANCED_DATA;
		BindShaders(shader_type);
		HandleOptions(this, options);
		graphics->BindTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::SetSphereState(DebugDrawCallOptions options) {
		unsigned int shader_type = ECS_DEBUG_SHADER_TRANSFORM;
		BindShaders(shader_type);
		HandleOptions(this, options);
		graphics->BindTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::SetPointState(DebugDrawCallOptions options) {
		unsigned int shader_type = ECS_DEBUG_SHADER_TRANSFORM;
		BindShaders(shader_type);
		HandleOptions(this, options);
		graphics->BindTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::SetRectangleState(DebugDrawCallOptions options) {
		unsigned int shader_type = ECS_DEBUG_SHADER_STRUCTURED_INSTANCED_DATA;
		BindShaders(shader_type);
		HandleOptions(this, options);
		graphics->BindTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::SetCrossState(DebugDrawCallOptions options) {
		unsigned int shader_type = ECS_DEBUG_SHADER_TRANSFORM;
		BindShaders(shader_type);
		HandleOptions(this, options);
		graphics->BindTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::SetCircleState(DebugDrawCallOptions options) {
		unsigned int shader_type = ECS_DEBUG_SHADER_TRANSFORM;
		BindShaders(shader_type);
		HandleOptions(this, options);
		graphics->BindTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::SetArrowCylinderState(DebugDrawCallOptions options) {
		unsigned int shader_type = ECS_DEBUG_SHADER_TRANSFORM;
		BindShaders(shader_type);
		HandleOptions(this, options);
		graphics->BindTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::SetArrowHeadState(DebugDrawCallOptions options) {
		unsigned int shader_type = ECS_DEBUG_SHADER_TRANSFORM;
		BindShaders(shader_type);
		HandleOptions(this, options);
		graphics->BindTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::SetAxesState(DebugDrawCallOptions options) {

	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::SetTriangleState(DebugDrawCallOptions options) {
		unsigned int shader_type = ECS_DEBUG_SHADER_STRUCTURED_INSTANCED_DATA;
		HandleOptions(this, options);
		BindShaders(shader_type);
		graphics->BindTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::SetOOBBState(DebugDrawCallOptions options) {
		unsigned int shader_type = ECS_DEBUG_SHADER_TRANSFORM;
		HandleOptions(this, options);
		BindShaders(shader_type);
		graphics->BindTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::SetStringState(DebugDrawCallOptions options) {

	}

	// ----------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Initialize

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::Initialize(MemoryManager* _allocator, Graphics* _graphics, size_t _thread_count) {
		allocator = _allocator;
		graphics = _graphics;
		thread_count = _thread_count;

		// Initialize the small vertex buffers
		positions_small_vertex_buffer = graphics->CreateVertexBuffer(sizeof(float3), SMALL_VERTEX_BUFFER_CAPACITY);
		instanced_small_vertex_buffer = graphics->CreateVertexBuffer(sizeof(InstancedTransformData), SMALL_VERTEX_BUFFER_CAPACITY);
		instanced_small_structured_buffer = graphics->CreateStructuredBuffer(sizeof(InstancedTransformData), SMALL_VERTEX_BUFFER_CAPACITY);
		instanced_structured_view = graphics->CreateBufferView(instanced_small_structured_buffer);

		// Initialize the shaders and input layouts
		vertex_shaders[ECS_DEBUG_SHADER_TRANSFORM] = graphics->CreateVertexShaderFromSource(ToStream(ECS_VERTEX_SHADER_SOURCE(DebugTransform)));
		pixel_shaders[ECS_DEBUG_SHADER_TRANSFORM] = graphics->CreatePixelShaderFromSource(ToStream(ECS_PIXEL_SHADER_SOURCE(DebugTransform)));
		layout_shaders[ECS_DEBUG_SHADER_TRANSFORM] = graphics->ReflectVertexShaderInput(vertex_shaders[ECS_DEBUG_SHADER_TRANSFORM]);

		vertex_shaders[ECS_DEBUG_SHADER_STRUCTURED_INSTANCED_DATA] = graphics->CreateVertexShaderFromSource(ToStream(ECS_VERTEX_SHADER_SOURCE(DebugStructured)));
		pixel_shaders[ECS_DEBUG_SHADER_STRUCTURED_INSTANCED_DATA] = graphics->CreatePixelShaderFromSource(ToStream(ECS_PIXEL_SHADER_SOURCE(DebugStructured)));
		layout_shaders[ECS_DEBUG_SHADER_STRUCTURED_INSTANCED_DATA] = graphics->ReflectVertexShaderInput(vertex_shaders[ECS_DEBUG_SHADER_STRUCTURED_INSTANCED_DATA]);

		// Initialize the raster states

		// Wireframe
		D3D11_RASTERIZER_DESC rasterizer_desc = {};
		rasterizer_desc.AntialiasedLineEnable = TRUE;
		rasterizer_desc.CullMode = D3D11_CULL_NONE;
		rasterizer_desc.DepthClipEnable = TRUE;
		rasterizer_desc.FillMode = D3D11_FILL_WIREFRAME;
		HRESULT result = graphics->m_device->CreateRasterizerState(&rasterizer_desc, rasterizer_states + ECS_DEBUG_RASTERIZER_WIREFRAME);
		ECS_ASSERT(!FAILED(result));

		// Solid - disable the back face culling
		rasterizer_desc.FillMode = D3D11_FILL_SOLID;
		rasterizer_desc.CullMode = D3D11_CULL_NONE;
		result = graphics->m_device->CreateRasterizerState(&rasterizer_desc, rasterizer_states + ECS_DEBUG_RASTERIZER_SOLID);
		ECS_ASSERT(!FAILED(result));

		// Solid - with back face culling
		rasterizer_desc.CullMode = D3D11_CULL_BACK;
		result = graphics->m_device->CreateRasterizerState(&rasterizer_desc, rasterizer_states + ECS_DEBUG_RASTERIZER_SOLID_CULL);
		ECS_ASSERT(!FAILED(result));

		// Initialize the string buffers
		string_buffers = (VertexBuffer*)allocator->Allocate(sizeof(VertexBuffer) * (unsigned int)Tools::CharacterUVIndex::Unknown);

		size_t total_memory = 0;
		size_t thread_capacity_stream_size = sizeof(CapacityStream<void>) * thread_count;

		total_memory += thread_lines->MemoryOf(PER_THREAD_RESOURCES) * thread_count;
		total_memory += thread_spheres->MemoryOf(PER_THREAD_RESOURCES) * thread_count;
		total_memory += thread_points->MemoryOf(PER_THREAD_RESOURCES) * thread_count;
		total_memory += thread_rectangles->MemoryOf(PER_THREAD_RESOURCES) * thread_count;
		total_memory += thread_crosses->MemoryOf(PER_THREAD_RESOURCES) * thread_count;
		total_memory += thread_circles->MemoryOf(PER_THREAD_RESOURCES) * thread_count;
		total_memory += thread_arrows->MemoryOf(PER_THREAD_RESOURCES) * thread_count;
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

		thread_arrows = (CapacityStream<DebugArrow>*)buffer;
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
			thread_arrows[index].InitializeFromBuffer(buffer, 0, PER_THREAD_RESOURCES);
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

	void DebugDrawer::BindShaders(unsigned int index)
	{
		graphics->BindVertexShader(vertex_shaders[index]);
		graphics->BindPixelShader(pixel_shaders[index]);
		graphics->BindInputLayout(layout_shaders[index]);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::UpdateCameraMatrix(Matrix new_matrix)
	{
		camera_matrix = new_matrix;
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::SetPreviousRenderState()
	{
		graphics->m_context->OMGetBlendState(&previous_blend_state, previous_blend_factors, &previous_blend_mask);
		graphics->m_context->RSGetState(&previous_rasterizer_state);
		graphics->m_context->OMGetDepthStencilState(&previous_depth_stencil_state, &previous_stencil_ref);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::RestorePreviousRenderState()
	{
		graphics->m_context->OMSetBlendState(previous_blend_state, previous_blend_factors, previous_blend_mask);
		graphics->m_context->RSSetState(previous_rasterizer_state);
		graphics->m_context->OMSetDepthStencilState(previous_depth_stencil_state, previous_stencil_ref);
	}


	// ----------------------------------------------------------------------------------------------------------------------

	// ----------------------------------------------------------------------------------------------------------------------

#pragma endregion

	// ----------------------------------------------------------------------------------------------------------------------
	
	// ----------------------------------------------------------------------------------------------------------------------
	
	// ----------------------------------------------------------------------------------------------------------------------

	// ----------------------------------------------------------------------------------------------------------------------
}