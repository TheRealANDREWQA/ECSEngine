#include "ecspch.h"
#include "DebugDraw.h"
#include "../../Utilities/FunctionInterfaces.h"
#include "../../Resources/ResourceManager.h"
#include "../../Rendering/GraphicsHelpers.h"
#include "../../Math/Quaternion.h"
#include "../../Math/Conversion.h"

constexpr size_t SMALL_VERTEX_BUFFER_CAPACITY = 8;
constexpr size_t PER_THREAD_RESOURCES = 32;

#define POINT_SIZE 0.3f
#define CIRCLE_TESSELATION 32
#define ARROW_HEAD_DARKEN_COLOR 0.9f
#define AXES_X_SCALE 3.0f

#define STRING_SPACE_SIZE 0.2f
#define STRING_TAB_SIZE STRING_SPACE_SIZE * 4
#define STRING_NEW_LINE_SIZE 0.85f
#define STRING_CHARACTER_SPACING 0.05f

#define DECK_CHUNK_SIZE 128
#define DECK_POWER_OF_TWO 7

namespace ECSEngine {

	// In consort with DebugVertexBuffers
	const wchar_t* ECS_DEBUG_PRIMITIVE_MESH_FILE[] = {
		L"Resources/DebugPrimitives/Sphere.glb",
		L"Resources/DebugPrimitives/Point.glb",
		L"Resources/DebugPrimitives/Cross.glb",
		L"Resources/DebugPrimitives/ArrowCylinder.glb",
		L"Resources/DebugPrimitives/ArrowHead.glb",
		L"Resources/DebugPrimitives/Cube.glb"
	};

	const wchar_t* STRING_MESH_FILE = L"Resources/DebugPrimitives/Alphabet.glb";

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
	ushort2* CalculateElementTypeCounts(MemoryManager* allocator, unsigned int* counts, ushort2** indices, DeckPowerOfTwo<Element>* deck) {
		unsigned int total_count = deck->GetElementCount();
		ushort2* type_indices = (ushort2*)allocator->Allocate(sizeof(ushort2) * total_count * 4);
		SetIndicesTypeMask(indices, type_indices, total_count);

		for (size_t index = 0; index < deck->buffers.size; index++) {
			for (size_t subindex = 0; subindex < deck->buffers[index].size; subindex++) {
				unsigned char mask = GetMaskFromOptions(deck->buffers[index][subindex].options);
				// Assigned the indices and update the count
				indices[mask][counts[mask]++] = { (unsigned short)index, (unsigned short)subindex };
			}
		}
		return type_indices;
	}

	// ----------------------------------------------------------------------------------------------------------------------

	template<typename Element>
	void UpdateElementDurations(DeckPowerOfTwo<Element>* deck, float time_delta) {
		for (size_t index = 0; index < deck->buffers.size; index++) {
			for (int64_t subindex = 0; subindex < deck->buffers[index].size; subindex++) {
				deck->buffers[index][subindex].options.duration -= time_delta;
				if (deck->buffers[index][subindex].options.duration < 0.0f) {
					deck->buffers[index].RemoveSwapBack(subindex);
					subindex--;
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
	void SetInstancedColor(void* pointer, unsigned int index, ColorFloat color) {
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
		drawer->graphics->BindRasterizerState(drawer->rasterizer_states[options.wireframe]);
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
		float4 result;

		float3 direction = start - end;
		Vector4 direction_simd(direction);
		Vector4 angles = DirectionToRotation(direction_simd);
		angles.Store(&result);

		float length = Length3(direction_simd).First();
		result.w = length;

		return result;
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

	void ECS_VECTORCALL FillInstancedArrowCylinder(void* instanced_data, size_t buffer_index, const DebugArrow* arrow, Matrix camera_matrix) {
		SetInstancedColor(instanced_data, buffer_index, arrow->color);
		float3 direction_3 = GetRightVector(arrow->rotation);
		float3 current_translation = arrow->translation + float3::Splat(arrow->length / 2.0f) * direction_3;
		Matrix rotation_matrix = MatrixRotation(arrow->rotation);
		SetInstancedMatrix(
			instanced_data, 
			buffer_index, 
			MatrixMVPToGPU(MatrixTranslation(current_translation), rotation_matrix, MatrixScale(arrow->length / 2.0f, arrow->size, arrow->size), camera_matrix)
		);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void ECS_VECTORCALL FillInstancedArrowHead(void* instanced_data, size_t buffer_index, const DebugArrow* arrow, Matrix camera_matrix) {
		float3 direction_3 = GetRightVector(arrow->rotation);
		SetInstancedColor(instanced_data, buffer_index, arrow->color * ARROW_HEAD_DARKEN_COLOR);
		SetInstancedMatrix(
			instanced_data, 
			buffer_index,
			MatrixMVPToGPU(
				MatrixTranslation(arrow->translation + float3::Splat(arrow->length) * direction_3), 
				MatrixRotation(arrow->rotation), 
				MatrixScale(float3::Splat(arrow->size)), 
				camera_matrix
			)
		);
	}

	// ----------------------------------------------------------------------------------------------------------------------

#define AXES_SIZE_REDUCE_FACTOR 0.28f

	ECS_INLINE float AxesReducedScale(float size) {
		return size * AXES_SIZE_REDUCE_FACTOR;;
	}

	ECS_INLINE float CylinderXScale(float reduced_size) {
		return AXES_X_SCALE * reduced_size;
	}

	void ECS_VECTORCALL FillInstancedAxesArrowCylinders(void* instanced_data, size_t buffer_index, const DebugAxes* axes, Matrix camera_matrix) {
		// Magic constant to make it consistent in measurement units - if setting a sphere of radius 1.0f and axes of size 1.0f
		// then the axes should be radiuses for that sphere
		float size = AxesReducedScale(axes->size);
		
		// The X axis
		SetInstancedColor(instanced_data, buffer_index, axes->color_x);

		float cylinder_x_scale = CylinderXScale(size) * 0.5f;
		// Translate the cylinder after it has been scaled such that they all start from the same location
		Matrix arrow_cylinder_scale_matrix = MatrixScale(cylinder_x_scale, size, size) * MatrixTranslation({ cylinder_x_scale, 0.0f, 0.0f });

		Matrix general_translation = MatrixTranslation(axes->translation);

		float3 x_rotation = { axes->rotation.x, axes->rotation.y, axes->rotation.z };

		SetInstancedMatrix(
			instanced_data, 
			buffer_index, 
			MatrixMVPToGPU(
				general_translation,
				QuaternionRotationMatrix(x_rotation),
				arrow_cylinder_scale_matrix,
				camera_matrix
			)
		);

		// The Y axis - rotate around Z axis by 90 degrees
		float3 y_rotation = { axes->rotation.x, axes->rotation.y, axes->rotation.z + 90.0f };
		SetInstancedColor(instanced_data, buffer_index + 1, axes->color_y);

		SetInstancedMatrix(
			instanced_data, 
			buffer_index + 1, 
			MatrixMVPToGPU(
				general_translation,
				QuaternionRotationMatrix(y_rotation),
				arrow_cylinder_scale_matrix ,
				camera_matrix
			)
		);

		// The Z axis - rotate around Y axis by 90 degrees
		float3 z_rotation = { axes->rotation.x, axes->rotation.y - 90.0f, 0.0f };
		SetInstancedColor(
			instanced_data, 
			buffer_index + 2,
			axes->color_z
		);

		SetInstancedMatrix(
			instanced_data, 
			buffer_index + 2,
			MatrixMVPToGPU(
				general_translation,
				QuaternionRotationMatrix(z_rotation),
				arrow_cylinder_scale_matrix,
				camera_matrix
			)
		);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void ECS_VECTORCALL FillInstancedAxesArrowHead(void* instanced_data, size_t buffer_index, const DebugAxes* axes, Matrix camera_matrix) {
		// Magic constant to make it consistent in measurement units - if setting a sphere of radius 1.0f and axes of size 1.0f
		// then the axes should be radiuses for that sphere
		float size = axes->size * AXES_SIZE_REDUCE_FACTOR;
		
		// For all heads, rotate firstly to get the correct orientation and then translate and rotate with the given rotation
		// So, for all heads, interchange the translation before the rotation

		// The X axis
		SetInstancedColor(instanced_data, buffer_index, axes->color_x * ARROW_HEAD_DARKEN_COLOR);

		float cylinder_x_scale = CylinderXScale(size);
		Matrix general_translation = MatrixTranslation(axes->translation);
		Matrix local_translation = MatrixTranslation({ cylinder_x_scale, 0.0f, 0.0f });

		float3 x_rotation = axes->rotation;
		Matrix x_rotation_matrix = QuaternionRotationMatrix(x_rotation);

		Matrix arrow_head_scale = MatrixScale(float3::Splat(size));
		// This head doesn't need to be rotated before scaling
		SetInstancedMatrix(
			instanced_data, 
			buffer_index, 
			MatrixMVPToGPU(
				general_translation,
				x_rotation_matrix,
				arrow_head_scale * local_translation,
				camera_matrix
			)
		);

		// The Y axis
		float3 y_rotation = { axes->rotation.x, axes->rotation.y, axes->rotation.z + 90.0f };
		Matrix y_rotation_matrix = QuaternionRotationMatrix(y_rotation);
		SetInstancedColor(instanced_data, buffer_index + 1, axes->color_y * ARROW_HEAD_DARKEN_COLOR);
		SetInstancedMatrix(
			instanced_data, 
			buffer_index + 1,
			MatrixMVPToGPU(
				general_translation,
				y_rotation_matrix,
				arrow_head_scale * local_translation,
				camera_matrix
			)
		);

		// The Z axis
		float3 z_rotation = { axes->rotation.x, axes->rotation.y - 90.0f, 0.0f };
		Matrix z_rotation_matrix = QuaternionRotationMatrix(z_rotation);
		SetInstancedColor(instanced_data, buffer_index + 2, axes->color_z * ARROW_HEAD_DARKEN_COLOR);
		SetInstancedMatrix(
			instanced_data, 
			buffer_index + 2, 
			MatrixMVPToGPU(
				general_translation,
				z_rotation_matrix,
				arrow_head_scale * local_translation,
				camera_matrix
			)
		);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	DebugDrawer::DebugDrawer(MemoryManager* allocator, ResourceManager* resource_manager, size_t thread_count) {
		Initialize(allocator, resource_manager, thread_count);
	}

#pragma region Add to the queue - single threaded

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::AddLine(float3 start, float3 end, ColorFloat color, DebugDrawCallOptions options)
	{
		lines.Add({ start, end, color, options });
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::AddSphere(float3 position, float radius, ColorFloat color, DebugDrawCallOptions options)
	{
		spheres.Add({ position, radius, color, options });
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::AddPoint(float3 position, ColorFloat color, DebugDrawCallOptions options)
	{
		// Make wireframe false
		points.Add({ position, color, { false, options.ignore_depth, options.duration } });
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::AddRectangle(float3 corner0, float3 corner1, ColorFloat color, DebugDrawCallOptions options)
	{
		rectangles.Add({ corner0, corner1, color, options });
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::AddCross(float3 position, float3 rotation, float size, ColorFloat color, DebugDrawCallOptions options)
	{
		crosses.Add({ position, rotation, size, color, options });
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::AddCircle(float3 position, float3 rotation, float radius, ColorFloat color, DebugDrawCallOptions options)
	{
		circles.Add({ position, rotation, radius, color, options });
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::AddArrow(float3 start, float3 end, float size, ColorFloat color, DebugDrawCallOptions options)
	{
		float4 rotation_length = ArrowStartEndToRotation(start, end);
		arrows.Add({ start, {rotation_length.x, rotation_length.y, rotation_length.z}, rotation_length.w, size, color, options });
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::AddArrowRotation(float3 translation, float3 rotation, float length, float size, ColorFloat color, DebugDrawCallOptions options)
	{
		arrows.Add({ translation, rotation, length, size, color, options });
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::AddAxes(float3 translation, float3 rotation, float size, ColorFloat color_x, ColorFloat color_y, ColorFloat color_z, DebugDrawCallOptions options)
	{
		axes.Add({ translation, rotation, size, color_x, color_y, color_z, options });
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::AddTriangle(float3 point0, float3 point1, float3 point2, ColorFloat color, DebugDrawCallOptions options)
	{
		triangles.Add({ point0, point1, point2, color, options });
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::AddAABB(float3 min_coordinates, float3 max_coordinates, ColorFloat color, DebugDrawCallOptions options)
	{
		aabbs.Add({ min_coordinates, max_coordinates, color, options });
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::AddOOBB(float3 translation, float3 rotation, float3 scale, ColorFloat color, DebugDrawCallOptions options)
	{
		oobbs.Add({ translation, rotation, scale, color, options });
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::AddString(float3 position, float3 direction, float size, Stream<char> text, ColorFloat color, DebugDrawCallOptions options)
	{
		Stream<char> text_copy = function::StringCopy(Allocator(), text);
		strings.Add({ position, direction, size, text_copy, color, options });
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::AddStringRotation(float3 position, float3 rotation, float size, Stream<char> text, ColorFloat color, DebugDrawCallOptions options)
	{
		Stream<char> text_copy = function::StringCopy(Allocator(), text);
		float3 direction = GetRightVector(rotation);
		strings.Add({ position, direction, size, text_copy, color, options });
	}


	// ----------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Add to queue - multi threaded

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::AddLineThread(unsigned int thread_index, float3 start, float3 end, ColorFloat color, DebugDrawCallOptions options)
	{
		// Check to see if the queue is full
		// If it is, then a flush is needed to clear the queue
		if (thread_lines[thread_index].IsFull()) {
			FlushLine(thread_index);
		}
		thread_lines[thread_index].Add({ start, end, color, options });
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::AddSphereThread(unsigned int thread_index, float3 position, float radius, ColorFloat color, DebugDrawCallOptions options)
	{
		// Check to see if the queue is full
		// If it is, then a flush is needed to clear the queue
		if (thread_spheres[thread_index].IsFull()) {
			FlushSphere(thread_index);
		}
		thread_spheres[thread_index].Add({ position, radius, color });
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::AddPointThread(unsigned int thread_index, float3 position, ColorFloat color, DebugDrawCallOptions options)
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

	void DebugDrawer::AddRectangleThread(unsigned int thread_index, float3 corner0, float3 corner1, ColorFloat color, DebugDrawCallOptions options)
	{
		// Check to see if the queue is full
		// If it is, then a flush is needed to clear the queue
		if (thread_rectangles[thread_index].IsFull()) {
			FlushRectangle(thread_index);
		}
		thread_rectangles[thread_index].Add({ corner0, corner1, color, options });
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::AddCrossThread(unsigned int thread_index, float3 position, float3 rotation, float size, ColorFloat color, DebugDrawCallOptions options)
	{
		// Check to see if the queue is full
		// If it is, then a flush is needed to clear the queue
		if (thread_crosses[thread_index].IsFull()) {
			FlushCross(thread_index);
		}
		thread_crosses[thread_index].Add({ position, rotation, size, color, options });
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::AddCircleThread(unsigned int thread_index, float3 position, float3 rotation, float radius, ColorFloat color, DebugDrawCallOptions options)
	{
		// Check to see if the queue is full
		// If it is, then a flush is needed to clear the queue
		if (thread_circles[thread_index].IsFull()) {
			FlushCircle(thread_index);
		}
		thread_circles[thread_index].Add({ position, rotation, radius, color, options });
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::AddArrowThread(unsigned int thread_index, float3 start, float3 end, float size, ColorFloat color, DebugDrawCallOptions options)
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

	void DebugDrawer::AddArrowRotationThread(unsigned int thread_index, float3 translation, float3 rotation, float length, float size, ColorFloat color, DebugDrawCallOptions options)
	{
		// Check to see if the queue is full
		// If it is, then a flush is needed to clear the queue
		if (thread_arrows[thread_index].IsFull()) {
			FlushArrow(thread_index);
		}
		thread_arrows[thread_index].Add({ translation, rotation, length, size, color, options });
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::AddAxesThread(unsigned int thread_index, float3 translation, float3 rotation, float size, ColorFloat color_x, ColorFloat color_y, ColorFloat color_z, DebugDrawCallOptions options)
	{
		// Check to see if the queue is full
		// If it is, then a flush is needed to clear the queue
		if (thread_axes[thread_index].IsFull()) {
			FlushAxes(thread_index);
		}
		thread_axes[thread_index].Add({ translation, rotation, size, color_x, color_y, color_z, options });
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::AddTriangleThread(unsigned int thread_index, float3 point0, float3 point1, float3 point2, ColorFloat color, DebugDrawCallOptions options)
	{
		// Check to see if the queue is full
		// If it is, then a flush is needed to clear the queue
		if (thread_triangles[thread_index].IsFull()) {
			FlushTriangle(thread_index);
		}
		thread_triangles[thread_index].Add({ point0, point1, point2, color, options });
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::AddAABBThread(unsigned int thread_index, float3 min_coordinates, float3 max_coordinates, ColorFloat color, DebugDrawCallOptions options)
	{
		// Check to see if the queue is full
		// If it is, then a flush is needed to clear the queue
		if (thread_aabbs[thread_index].IsFull()) {
			FlushAABB(thread_index);
		}
		thread_aabbs[thread_index].Add({ min_coordinates, max_coordinates, color, options });
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::AddOOBBThread(unsigned int thread_index, float3 translation, float3 rotation, float3 scale, ColorFloat color, DebugDrawCallOptions options)
	{
		// Check to see if the queue is full
		// If it is, then a flush is needed to clear the queue
		if (thread_oobbs[thread_index].IsFull()) {
			FlushOOBB(thread_index);
		}
		thread_oobbs[thread_index].Add({ translation, rotation, scale, color, options });
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::AddStringThread(unsigned int thread_index, float3 position, float3 direction, float size, Stream<char> text, ColorFloat color, DebugDrawCallOptions options)
	{
		// Check to see if the queue is full
		// If it is, then a flush is needed to clear the queue
		if (thread_strings[thread_index].IsFull()) {
			FlushString(thread_index);
		}
		Stream<char> string_copy = function::StringCopy(AllocatorTs(), text);
		thread_strings[thread_index].Add({ position, direction, size, string_copy, color, options });
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::AddStringRotationThread(unsigned int thread_index, float3 position, float3 rotation, float size, Stream<char> text, ColorFloat color, DebugDrawCallOptions options)
	{
		// Check to see if the queue is full
		// If it is, then a flush is needed to clear the queue
		if (thread_strings[thread_index].IsFull()) {
			FlushString(thread_index);
		}
		Stream<char> string_copy = function::StringCopy(AllocatorTs(), text);
		thread_strings[thread_index].Add({ position, GetRightVector(rotation), size, string_copy, color, options });
	}

	// ----------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Draw immediately

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::DrawLine(float3 start, float3 end, ColorFloat color, DebugDrawCallOptions options)
	{
		float3* positions = MapPositions(this);
		InstancedTransformData* instanced = MapInstancedStructured(this);

		positions[0] = start;
		positions[1] = end;
		SetInstancedMatrix(instanced, 0, MatrixGPU(camera_matrix));
		SetInstancedColor(instanced, 0, color);

		UnmapAllStructured(this);

		SetLineState(options);
		BindSmallStructuredBuffers(this);

		DrawCallLine(1);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::DrawSphere(float3 position, float radius, ColorFloat color, DebugDrawCallOptions options)
	{
		InstancedTransformData* instanced = MapInstancedVertex(this);
		SetInstancedColor(instanced, 0, color);
		Matrix matrix = MatrixTS(MatrixTranslation(position), MatrixScale(float3::Splat(radius)));
		SetInstancedMatrix(instanced, 0, MatrixGPU(MatrixMVP(matrix, camera_matrix)));
		UnmapInstancedVertex(this);

		SetSphereState(options);
		BindSphereBuffers(instanced_small_vertex_buffer);

		DrawCallSphere(1);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::DrawPoint(float3 position, ColorFloat color, DebugDrawCallOptions options)
	{
		options.wireframe = false;

		InstancedTransformData* instanced = MapInstancedVertex(this);
		SetInstancedColor(instanced, 0, color);
		Matrix matrix = MatrixTS(MatrixTranslation(position), MatrixScale(float3::Splat(POINT_SIZE)));
		SetInstancedMatrix(instanced, 0, MatrixGPU(MatrixMVP(matrix, camera_matrix)));
		UnmapInstancedVertex(this);

		SetPointState(options);
		BindPointBuffers(instanced_small_vertex_buffer);

		DrawCallPoint(1);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	// Draw it double sided, so it appears on both sides
	void DebugDrawer::DrawRectangle(float3 corner0, float3 corner1, ColorFloat color, DebugDrawCallOptions options)
	{
		float3* positions = MapPositions(this);
		InstancedTransformData* instanced = MapInstancedStructured(this);

		SetRectanglePositionsFront(positions, corner0, corner1);

		SetInstancedColor(instanced, 0, color);
		SetInstancedMatrix(instanced, 0, MatrixGPU(camera_matrix));
		UnmapAllStructured(this);

		SetRectangleState(options);
		BindSmallStructuredBuffers(this);

		DrawCallRectangle(1);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::DrawCross(float3 position, float3 rotation, float size, ColorFloat color, DebugDrawCallOptions options)
	{
		options.wireframe = false;
		InstancedTransformData* instanced = MapInstancedVertex(this);

		Matrix matrix = MatrixMVPToGPU(
			MatrixTranslation(position),
			MatrixRotation(rotation),
			MatrixScale(float3::Splat(size)),
			camera_matrix
		);
		SetInstancedColor(instanced, 0, color);
		SetInstancedMatrix(instanced, 0, matrix);
		UnmapInstancedVertex(this);

		SetCrossState(options);
		BindCrossBuffers(instanced_small_vertex_buffer);

		DrawCallCross(1);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::DrawCircle(float3 position, float3 rotation, float radius, ColorFloat color, DebugDrawCallOptions options)
	{
		options.wireframe = false;
		InstancedTransformData* instanced = MapInstancedVertex(this);

		Matrix mvp_matrix = MatrixMVPToGPU(
			MatrixTranslation(position),
			MatrixRotation(rotation),
			MatrixScale(float3::Splat(radius)),
			camera_matrix
		);
		SetInstancedColor(instanced, 0, color);
		SetInstancedMatrix(instanced, 0, mvp_matrix);
		UnmapInstancedVertex(this);

		SetCircleState(options);
		BindCircleBuffers(instanced_small_vertex_buffer);

		DrawCallCircle(1);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::DrawArrow(float3 start, float3 end, float size, ColorFloat color, DebugDrawCallOptions options)
	{
		float4 rotation_length = ArrowStartEndToRotation(start, end);
		DrawArrowRotation(start, { rotation_length.x, rotation_length.y, rotation_length.z }, rotation_length.w, size, color, options);
	}

	void DebugDrawer::DrawArrowRotation(float3 translation, float3 rotation, float length, float size, ColorFloat color, DebugDrawCallOptions options)
	{
		InstancedTransformData* instanced = MapInstancedVertex(this);
		
		DebugArrow arrow = { translation, rotation, length, size, color, options };
		FillInstancedArrowCylinder(instanced, 0, &arrow, camera_matrix);

		UnmapInstancedVertex(this);
		SetArrowCylinderState(options);

		BindArrowCylinderBuffers(instanced_small_vertex_buffer);

		DrawCallArrowCylinder(1);

		// The instanced buffer must be mapped again
		instanced = MapInstancedVertex(this);
		FillInstancedArrowHead(instanced, 0, &arrow, camera_matrix);
		UnmapInstancedVertex(this);

		BindArrowHeadBuffers(instanced_small_vertex_buffer);

		SetArrowHeadState(options);

		DrawCallArrowHead(1);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::DrawAxes(float3 translation, float3 rotation, float size, ColorFloat color_x, ColorFloat color_y, ColorFloat color_z, DebugDrawCallOptions options)
	{
		// Draw 3 separate arrows - but batch the cylinder and head draws - in order to avoid 3 times the draw calls
		InstancedTransformData* instanced = MapInstancedVertex(this);

		DebugAxes axes = { translation, rotation, size, color_x, color_y, color_z, options };
		FillInstancedAxesArrowCylinders(instanced, 0, &axes, camera_matrix);

		UnmapInstancedVertex(this);
		SetArrowCylinderState(options);
		BindArrowCylinderBuffers(instanced_small_vertex_buffer);
		DrawCallArrowCylinder(3);

		// The arrow heads - The instanced buffer must be mapped again
		instanced = MapInstancedVertex(this);

		FillInstancedAxesArrowHead(instanced, 0, &axes, camera_matrix);
		
		UnmapInstancedVertex(this);
		SetArrowHeadState(options);
		BindArrowHeadBuffers(instanced_small_vertex_buffer);
		DrawCallArrowHead(3);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::DrawTriangle(float3 point0, float3 point1, float3 point2, ColorFloat color, DebugDrawCallOptions options)
	{
		float3* positions = MapPositions(this);
		InstancedTransformData* instanced = MapInstancedStructured(this);

		positions[0] = point0;
		positions[1] = point1;
		positions[2] = point2;

		SetInstancedColor(instanced, 0, color);
		SetInstancedMatrix(instanced, 0, MatrixGPU(camera_matrix));
		UnmapAllStructured(this);
	
		SetTriangleState(options);
		BindSmallStructuredBuffers(this);

		DrawCallTriangle(1);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	// It uses a scaled cube to draw it
	void DebugDrawer::DrawAABB(float3 translation, float3 scale, ColorFloat color, DebugDrawCallOptions options)
	{
		DrawOOBB(translation, float3::Splat(0.0f), scale, color, options);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::DrawOOBB(float3 translation, float3 rotation, float3 scale, ColorFloat color, DebugDrawCallOptions options)
	{
		InstancedTransformData* instanced = MapInstancedVertex(this);
		SetInstancedColor(instanced, 0, color);
		SetInstancedMatrix(instanced, 0, MatrixGPU(MatrixMVP(MatrixTransform(translation, rotation, scale), camera_matrix)));
		UnmapInstancedVertex(this);

		SetOOBBState(options);
		BindCubeBuffers(instanced_small_vertex_buffer);

		DrawCallOOBB(1);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::DrawString(float3 position, float3 direction, float size, Stream<char> text, ColorFloat color, DebugDrawCallOptions options)
	{
		ECS_ASSERT(text.size < 10'000);

		// Normalize the direction
		direction = Normalize(direction);

		float3 rotation_from_direction = DirectionToRotation(direction);
		// Invert the y axis 
		rotation_from_direction.y = -rotation_from_direction.y;
		float3 up_direction = GetUpVector(rotation_from_direction);

		// Splat these early
		float3 space_size = float3::Splat(STRING_SPACE_SIZE * size) * direction;
		float3 tab_size = float3::Splat(STRING_TAB_SIZE * size) * direction;
		float3 character_spacing = float3::Splat(STRING_CHARACTER_SPACING * size) * direction;
		float3 new_line_size = float3::Splat(STRING_NEW_LINE_SIZE * -size) * up_direction;

		Matrix rotation_matrix = MatrixRotation({rotation_from_direction.x, rotation_from_direction.y, rotation_from_direction.z});
		Matrix scale_matrix = MatrixScale(float3::Splat(size));
		Matrix scale_rotation = MatrixRS(rotation_matrix, scale_matrix);

		// Create the instanced vertex buffer
		VertexBuffer instanced_data_buffer = graphics->CreateVertexBuffer(sizeof(InstancedTransformData), text.size, true);

		// Keep track of already checked elements - the same principle as the Eratosthenes sieve
		bool* checked_characters = (bool*)ECS_STACK_ALLOC(sizeof(bool) * text.size);
		memset(checked_characters, 0, sizeof(bool) * text.size);

		// Horizontal offsets as in the offset alongside the direction
		float3* horizontal_offsets = (float3*)ECS_STACK_ALLOC(sizeof(float3) * text.size);
		// Vertical offsets as in the offset alongside the -up direction
		float3* vertical_offsets = (float3*)ECS_STACK_ALLOC(sizeof(float3) * text.size);

		// Create a mask of spaces and tabs offsets for each character

		// Do the first character separately
		horizontal_offsets[0] = { 0.0f, 0.0f, 0.0f };
		vertical_offsets[0] = { 0.0f, 0.0f, 0.0f };

		if (text[0] == ' ' || text[0] == '\n' || text[0] == '\t') {
			checked_characters[0] = true;
		}

		for (size_t index = 1; index < text.size; index++) {
			// Hoist the checking outside the if
			checked_characters[index] = text[index] == ' ' || text[index] == '\t' || text[index] == '\n';
			
			if (text[index - 1] == '\n') {
				vertical_offsets[index] = vertical_offsets[index - 1] + new_line_size;

				// If a new character has been found - set to 0 all the other offsets - the tab and the space
				horizontal_offsets[index] = { 0.0f, 0.0f, 0.0f };
			}
			else {
				// It doesn't matter if the current character is \n because it will reset the horizontal offset anyway
				unsigned int previous_alphabet_index = function::GetAlphabetIndex(text[index - 1]);
				unsigned int current_alphabet_index = function::GetAlphabetIndex(text[index]);

				float previous_offset = string_character_bounds[previous_alphabet_index].y;
				float current_offset = string_character_bounds[current_alphabet_index].x;

				// The current offset will be negative
				float3 character_span = float3::Splat((previous_offset - current_offset) * size) * direction;
				horizontal_offsets[index] = horizontal_offsets[index - 1] + character_spacing + character_span;
				vertical_offsets[index] = vertical_offsets[index - 1];
			}
		}

		SetStringState(options);
		BindStringBuffers(instanced_data_buffer);

		for (size_t index = 0; index < text.size; index++) {
			if (!checked_characters[index]) {
				// Map the vertex buffer
				void* instance_data = graphics->MapBuffer(instanced_data_buffer.buffer);

				unsigned int current_character_count = 0;
				// Do a search for the same character
				for (size_t subindex = index; subindex < text.size; subindex++) {
					if (text[subindex] == text[index]) {
						float3 translation = position + horizontal_offsets[subindex] + vertical_offsets[subindex];
						checked_characters[subindex] = true;
						
						SetInstancedColor(instance_data, current_character_count, color);
						SetInstancedMatrix(instance_data, current_character_count, MatrixGPU(MatrixMVP(scale_rotation * MatrixTranslation(translation), camera_matrix)));
						current_character_count++;
					}
				}

				// Draw the current characters
				graphics->UnmapBuffer(instanced_data_buffer.buffer);

				unsigned int alphabet_index = function::GetAlphabetIndex(text[index]);
				// Draw the current character
				graphics->DrawIndexedInstanced(string_mesh->submeshes[alphabet_index].index_count, current_character_count, string_mesh->submeshes[alphabet_index].index_buffer_offset);
			}
		}

		// Release the temporary buffer
		instanced_data_buffer.Release();
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::DrawStringRotation(
		float3 translation,
		float3 rotation,
		float size,
		Stream<char> text,
		ColorFloat color,
		DebugDrawCallOptions options
	) {
		DrawString(translation, GetRightVector(rotation), size, text, color, options);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DrawDebugVertexLine(
		DebugDrawer* drawer,
		VertexBuffer model_position,
		VertexBuffer attribute, 
		unsigned int float_step, 
		float size, 
		ColorFloat color,
		Matrix world_matrix, 
		DebugDrawCallOptions options
	) {
		// Draw them as lines
		Graphics* graphics = drawer->graphics;

		// Allocate temporary constant buffer and position buffers
		ConstantBuffer constant_buffer = graphics->CreateConstantBuffer(sizeof(InstancedTransformData), true);
		VertexBuffer line_position_buffer = graphics->CreateVertexBuffer(sizeof(float3), attribute.size * 2, true);

		// Create staging buffers for positions and normals
		VertexBuffer staging_normals = BufferToStaging(graphics, attribute);
		VertexBuffer staging_positions = BufferToStaging(graphics, model_position);

		float* attribute_data = (float*)graphics->MapBuffer(staging_normals.buffer, ECS_GRAPHICS_MAP_READ);
		float3* model_positions = (float3*)graphics->MapBuffer(staging_positions.buffer, ECS_GRAPHICS_MAP_READ);
		void* constant_data = graphics->MapBuffer(constant_buffer.buffer);
		float3* line_positions = (float3*)graphics->MapBuffer(line_position_buffer.buffer);

		SetInstancedColor(constant_data, 0, color);
		SetInstancedMatrix(constant_data, 0, MatrixGPU(MatrixMVP(world_matrix, drawer->camera_matrix)));

		// Set the line positions
		for (size_t index = 0; index < attribute.size; index++) {
			size_t offset = index * 2;
			size_t attribute_offset = index * float_step;
			float3 direction = { attribute_data[attribute_offset], attribute_data[attribute_offset + 1], attribute_data[attribute_offset + 2] };
			line_positions[offset] = model_positions[index];
			line_positions[offset + 1] = model_positions[index] + direction * size;
		}

		// Unmap the constant buffer and the staging ones
		graphics->UnmapBuffer(staging_normals.buffer);
		graphics->UnmapBuffer(staging_positions.buffer);
		graphics->UnmapBuffer(constant_buffer.buffer);
		graphics->UnmapBuffer(line_position_buffer.buffer);

		// Special shader
		unsigned int shader_type = ECS_DEBUG_SHADER_NORMAL_SINGLE_DRAW;
		drawer->BindShaders(shader_type);
		HandleOptions(drawer, options);
		graphics->BindTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

		// The positions will be taken from the model positions buffer
		graphics->BindVertexBuffer(line_position_buffer);
		graphics->BindVertexConstantBuffer(constant_buffer);

		graphics->Draw(attribute.size * 2);

		// Release the temporary buffer and the staging normals
		staging_normals.Release();
		staging_positions.Release();
		constant_buffer.Release();
		line_position_buffer.Release();
	}

	void DrawDebugVertexLine(
		DebugDrawer* drawer,
		VertexBuffer model_position,
		VertexBuffer attribute,
		unsigned int float_step,
		float size,
		ColorFloat color,
		Stream<Matrix> world_matrices,
		DebugDrawCallOptions options
	) {
		// Draw them as lines
		Graphics* graphics = drawer->graphics;

		// Allocate temporary instanced buffer and position buffers
		VertexBuffer instanced_buffer = graphics->CreateVertexBuffer(sizeof(InstancedTransformData), world_matrices.size, true);
		VertexBuffer line_position_buffer = graphics->CreateVertexBuffer(sizeof(float3), attribute.size * 2, true);

		// Create staging buffers for positions and normals
		VertexBuffer staging_normals = BufferToStaging(graphics, attribute);
		VertexBuffer staging_positions = BufferToStaging(graphics, model_position);

		float* attribute_data = (float*)graphics->MapBuffer(staging_normals.buffer, ECS_GRAPHICS_MAP_READ);
		float3* model_positions = (float3*)graphics->MapBuffer(staging_positions.buffer, ECS_GRAPHICS_MAP_READ);
		void* instanced_data = graphics->MapBuffer(instanced_buffer.buffer);
		float3* line_positions = (float3*)graphics->MapBuffer(line_position_buffer.buffer);

		// Set the line positions
		for (size_t index = 0; index < attribute.size; index++) {
			size_t offset = index * 2;
			size_t attribute_offset = index * float_step;
			float3 direction = { attribute_data[attribute_offset], attribute_data[attribute_offset + 1], attribute_data[attribute_offset + 2] };
			line_positions[offset] = model_positions[index];
			line_positions[offset + 1] = model_positions[index] + direction * size;
		}

		for (size_t index = 0; index < world_matrices.size; index++) {
			SetInstancedColor(instanced_data, index, color);
			SetInstancedMatrix(instanced_data, index, MatrixGPU(MatrixMVP(world_matrices[index], drawer->camera_matrix)));
		}

		// Unmap the constant buffer and the staging ones
		graphics->UnmapBuffer(staging_normals.buffer);
		graphics->UnmapBuffer(staging_positions.buffer);
		graphics->UnmapBuffer(instanced_buffer.buffer);
		graphics->UnmapBuffer(line_position_buffer.buffer);

		// Instanced draw
		unsigned int shader_type = ECS_DEBUG_SHADER_TRANSFORM;
		drawer->BindShaders(shader_type);
		HandleOptions(drawer, options);
		graphics->BindTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

		// The positions will be taken from the model positions buffer
		VertexBuffer vertex_buffers[2];
		vertex_buffers[0] = line_position_buffer;
		vertex_buffers[1] = instanced_buffer;
		graphics->BindVertexBuffers({ vertex_buffers, std::size(vertex_buffers) });

		graphics->DrawInstanced(attribute.size * 2, world_matrices.size);

		// Release the temporary buffer and the staging normals
		staging_normals.Release();
		staging_positions.Release();
		instanced_buffer.Release();
		line_position_buffer.Release();
	}

	void DebugDrawer::DrawNormals(
		VertexBuffer model_position,
		VertexBuffer model_normals, 
		float size,
		ColorFloat color, 
		Matrix world_matrix,
		DebugDrawCallOptions options
	)
	{
		DrawDebugVertexLine(this, model_position, model_normals, 3, size, color, world_matrix, options);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::DrawNormals(
		VertexBuffer model_position,
		VertexBuffer model_normals,
		float size,
		ColorFloat color,
		Stream<Matrix> world_matrices,
		DebugDrawCallOptions options
	) {
		DrawDebugVertexLine(this, model_position, model_normals, 3, size, color, world_matrices, options);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::DrawTangents(
		VertexBuffer model_position,
		VertexBuffer model_tangents,
		float size, 
		ColorFloat color, 
		Matrix world_matrix, 
		DebugDrawCallOptions options
	)
	{
		DrawDebugVertexLine(this, model_position, model_tangents, 4, size, color, world_matrix, options);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::DrawTangents(
		VertexBuffer model_position,
		VertexBuffer model_tangents, 
		float size,
		ColorFloat color,
		Stream<Matrix> world_matrices,
		DebugDrawCallOptions options
	)
	{
		DrawDebugVertexLine(this, model_position, model_tangents, 4, size, color, world_matrices, options);
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
			unsigned int instance_count = GetMaximumCount(counts);

			// Create temporary buffers to hold the data - each line has 2 endpoints
			VertexBuffer position_buffer = graphics->CreateVertexBuffer(sizeof(float3), instance_count * 2, true);
			StructuredBuffer instanced_buffer = graphics->CreateStructuredBuffer(sizeof(InstancedTransformData), instance_count, true);

			// Bind the vertex buffer and the structured buffer now
			graphics->BindVertexBuffer(position_buffer);
			ResourceView instanced_view = graphics->CreateBufferView(instanced_buffer, true);
			graphics->BindVertexResourceView(instanced_view);

			Matrix gpu_camera = MatrixGPU(camera_matrix);

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

			auto map_for = [&current_count, &current_indices, this, gpu_camera, deck_pointer](float3* positions, void* instanced_data) {
				for (size_t index = 0; index < current_count; index++) {
					DebugLine* line = &deck_pointer->buffers[current_indices[index].x][current_indices[index].y]; 
					positions[index * 2] = line->start; 
					positions[index * 2 + 1] = line->end; 
					SetInstancedColor(instanced_data, index, line->color); 
					SetInstancedMatrix(instanced_data, index, gpu_camera); 
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
					DrawCallLine(current_count);
				}
			};

			// Draw all the Wireframe depth elements - only solid draw
			draw_call({ false, false });

			// Draw all the Wireframe no depth elements - only solid draw
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

			// Release the temporary vertex buffer, structured buffer and the temporary allocation
			position_buffer.Release();
			instanced_view.Release();
			instanced_buffer.Release();
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
			unsigned int instance_count = GetMaximumCount(counts);

			// Create temporary buffers to hold the data
			VertexBuffer instanced_buffer = graphics->CreateVertexBuffer(sizeof(InstancedTransformData), instance_count, true);

			// Bind the vertex buffers now
			BindSphereBuffers(instanced_buffer);

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
					Matrix matrix = MatrixTS(MatrixTranslation(sphere->position), MatrixScale(float3::Splat(sphere->radius)));
					SetInstancedMatrix(instanced_data, index, MatrixGPU(matrix * camera_matrix));
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
					DrawCallSphere(current_count);
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
			instanced_buffer.Release();
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
			unsigned int instance_count = GetMaximumCount(counts);

			// Create temporary buffers to hold the data
			VertexBuffer instanced_buffer = graphics->CreateVertexBuffer(sizeof(InstancedTransformData), instance_count, true);

			// Bind the vertex buffers now
			BindPointBuffers(instanced_buffer);

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
					Matrix matrix = MatrixTS(MatrixTranslation(point->position), MatrixScale(float3::Splat(POINT_SIZE)));
					SetInstancedMatrix(instanced_data, index, MatrixGPU(matrix * camera_matrix));
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
					DrawCallPoint(current_count);
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
			instanced_buffer.Release();
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
			unsigned int instance_count = GetMaximumCount(counts);

			// Create temporary buffers to hold the data - each rectangles has 6 vertices
			VertexBuffer position_buffer = graphics->CreateVertexBuffer(sizeof(float3), instance_count * 6, true);
			StructuredBuffer instanced_buffer = graphics->CreateStructuredBuffer(sizeof(InstancedTransformData), instance_count, true);

			// Bind the vertex buffer and the structured buffer now
			graphics->BindVertexBuffer(position_buffer);
			ResourceView instanced_view = graphics->CreateBufferView(instanced_buffer, true);
			graphics->BindVertexResourceView(instanced_view);

			Matrix gpu_camera = MatrixGPU(camera_matrix);

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

			auto map_for = [&current_count, &current_indices, this, gpu_camera, deck_pointer](float3* positions, void* instanced_data) {
				for (size_t index = 0; index < current_count; index++) {
					DebugRectangle* rectangle = &deck_pointer->buffers[current_indices[index].x][current_indices[index].y];
					SetRectanglePositionsFront(positions + index * 6, rectangle->corner0, rectangle->corner1);
					SetInstancedColor(instanced_data, index, rectangle->color);
					SetInstancedMatrix(instanced_data, index, gpu_camera);
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
					DrawCallRectangle(current_count);
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
			position_buffer.Release();
			instanced_view.Release();
			instanced_buffer.Release();
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
			unsigned int instance_count = GetMaximumCount(counts);

			// Create temporary buffers to hold the data
			VertexBuffer instanced_buffer = graphics->CreateVertexBuffer(sizeof(InstancedTransformData), instance_count, true);

			// Bind the vertex buffers now
			BindCrossBuffers(instanced_buffer);

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
					Matrix matrix = MatrixTS(MatrixTranslation(cross->position), MatrixScale(float3::Splat(cross->size)));
					SetInstancedMatrix(instanced_data, index, MatrixGPU(MatrixMVP(matrix, camera_matrix)));
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
					DrawCallCross(current_count);
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
			instanced_buffer.Release();
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
			unsigned int instance_count = GetMaximumCount(counts);

			// Create temporary buffers to hold the data
			VertexBuffer instanced_buffer = graphics->CreateVertexBuffer(sizeof(InstancedTransformData), instance_count, true);

			// Bind the vertex buffers now
			BindCircleBuffers(instanced_buffer);

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
					Matrix matrix = MatrixTRS(MatrixTranslation(circle->position), MatrixRotation(circle->rotation), MatrixScale(float3::Splat(circle->radius)));
					SetInstancedMatrix(instanced_data, index, MatrixGPU(MatrixMVP(matrix, camera_matrix)));
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
					DrawCallCircle(current_count);
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
			instanced_buffer.Release();
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
			unsigned int instance_count = GetMaximumCount(counts);

			// Create temporary buffers to hold the data
			VertexBuffer instanced_buffer = graphics->CreateVertexBuffer(sizeof(InstancedTransformData), instance_count, true);

			// Bind the vertex buffers now
			BindArrowCylinderBuffers(instanced_buffer);

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
					FillInstancedArrowCylinder(instanced_data, index, arrow, camera_matrix);
				}
			};

			auto map_for_header = [&current_count, &current_indices, this, deck_pointer](void* instanced_data) {
				for (size_t index = 0; index < current_count; index++) {
					DebugArrow* arrow = &deck_pointer->buffers[current_indices[index].x][current_indices[index].y];
					FillInstancedArrowHead(instanced_data, index, arrow, camera_matrix);
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
					DrawCallArrowCylinder(current_count);
				}
			};

			auto draw_call_head = [&](DebugDrawCallOptions options) {
				if (current_count > 0) {
					// Map the buffers
					map_buffers(instanced_buffer, &instanced_data);

					// Fill the buffers
					map_for_header(instanced_data);

					// Unmap the buffers
					unmap_buffers(instanced_buffer);

					// Set the state
					SetArrowHeadState(options);

					DrawCallArrowHead(current_count);
				}
			};

			// Draw all the cylinder Wireframe depth elements
			draw_call_cylinder({ true, false });

			// Draw all the cylinder Wireframe no depth elements
			current_count = counts[WIREFRAME_NO_DEPTH];
			current_indices = indices[WIREFRAME_NO_DEPTH];
			draw_call_cylinder({ true, true });

			// Draw all the cylinder Solid depth elements
			current_count = counts[SOLID_DEPTH];
			current_indices = indices[SOLID_DEPTH];
			draw_call_cylinder({ false, false });

			// Draw all the cylinder Solid no depth elements
			current_count = counts[SOLID_NO_DEPTH];
			current_indices = indices[SOLID_NO_DEPTH];
			draw_call_cylinder({ false, true });

			// Bind the vertex buffers now
			BindArrowHeadBuffers(instanced_buffer);

			current_count = counts[WIREFRAME_DEPTH];
			current_count = counts[WIREFRAME_DEPTH];

			// Draw all the head Wireframe depth elements
			draw_call_head({ true, false });

			// Draw all the head Wireframe no depth elements
			current_count = counts[WIREFRAME_NO_DEPTH];
			current_indices = indices[WIREFRAME_NO_DEPTH];
			draw_call_head({ true, true });

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
			instanced_buffer.Release();
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
			unsigned int instance_count = GetMaximumCount(counts);

			// Create temporary buffers to hold the data
			VertexBuffer instanced_buffer = graphics->CreateVertexBuffer(sizeof(InstancedTransformData), instance_count * 3, true);

			unsigned int current_count = counts[WIREFRAME_DEPTH];
			ushort2* current_indices = indices[WIREFRAME_DEPTH];

			auto map_buffers = [this](VertexBuffer instanced_buffer, void** instanced_data) {
				*instanced_data = graphics->MapBuffer(instanced_buffer.buffer);
			};

			auto unmap_buffers = [this](VertexBuffer instanced_buffer) {
				graphics->UnmapBuffer(instanced_buffer.buffer);
			};

			auto map_cylinder_for = [&current_count, &current_indices, this, deck_pointer](void* instanced_data) {
				for (size_t index = 0; index < current_count; index++) {
					DebugAxes* axes = &deck_pointer->buffers[current_indices[index].x][current_indices[index].y];

					size_t buffer_index = index * 3;
					FillInstancedAxesArrowCylinders(instanced_data, buffer_index, axes, camera_matrix);
				}
			};

			auto map_head_for = [&current_count, &current_indices, this, deck_pointer](void* instanced_data) {
				for (size_t index = 0; index < current_count; index++) {
					DebugAxes* axes = &deck_pointer->buffers[current_indices[index].x][current_indices[index].y];

					size_t buffer_index = index * 3;
					FillInstancedAxesArrowHead(instanced_data, buffer_index, axes, camera_matrix);
				}
			};

			void* instanced_data = nullptr;

			auto draw_call_cylinder = [&](DebugDrawCallOptions options) {
				if (current_count > 0) {
					// Map the buffers
					map_buffers(instanced_buffer, &instanced_data);

					// Fill the buffers
					map_cylinder_for(instanced_data);

					// Unmap the buffers
					unmap_buffers(instanced_buffer);

					// Set the state
					SetArrowCylinderState(options);
					BindArrowCylinderBuffers(instanced_buffer);

					// Issue the draw call
					DrawCallArrowCylinder(current_count * 3);
				}
			};

			auto draw_call_head = [&](DebugDrawCallOptions options) {
				if (current_count > 0) {
					// Map the buffers
					map_buffers(instanced_buffer, &instanced_data);

					// Fill the buffers
					map_head_for(instanced_data);

					// Unmap the buffers
					unmap_buffers(instanced_buffer);

					// Set the state
					SetArrowHeadState(options);
					BindArrowHeadBuffers(instanced_buffer);

					// Issue the draw call
					DrawCallArrowHead(current_count * 3);
				}
			};

			// Draw all the Wireframe depth elements
			draw_call_cylinder({ true, false });
			draw_call_head({ true, false });

			// Draw all the Wireframe no depth elements
			current_count = counts[WIREFRAME_NO_DEPTH];
			current_indices = indices[WIREFRAME_NO_DEPTH];
			draw_call_cylinder({ true, true });
			draw_call_head({ true, true });

			// Draw all the Solid depth elements
			current_count = counts[SOLID_DEPTH];
			current_indices = indices[SOLID_DEPTH];
			draw_call_cylinder({ false, false });
			draw_call_head({ false, false });

			// Draw all the Solid no depth elements
			current_count = counts[SOLID_NO_DEPTH];
			current_indices = indices[SOLID_NO_DEPTH];
			draw_call_cylinder({ false, true });
			draw_call_head({ false, true });

			// Update the duration and remove those elements that expired
			UpdateElementDurations(deck_pointer, time_delta);

			// Release the temporary vertex buffers and the temporary allocation
			instanced_buffer.Release();
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
			unsigned int instance_count = GetMaximumCount(counts);

			// Create temporary buffers to hold the data - each triangle has 3 vertices
			VertexBuffer position_buffer = graphics->CreateVertexBuffer(sizeof(float3), instance_count * 3, true);
			StructuredBuffer instanced_buffer = graphics->CreateStructuredBuffer(sizeof(InstancedTransformData), instance_count, true);

			// Bind the vertex buffer and the structured buffer now
			graphics->BindVertexBuffer(position_buffer);
			ResourceView instanced_view = graphics->CreateBufferView(instanced_buffer, true);
			graphics->BindVertexResourceView(instanced_view);

			Matrix gpu_camera = MatrixGPU(camera_matrix);

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

			auto map_for = [&current_count, &current_indices, this, gpu_camera, deck_pointer](float3* positions, void* instanced_data) {
				for (size_t index = 0; index < current_count; index++) {
					DebugTriangle* triangle = &deck_pointer->buffers[current_indices[index].x][current_indices[index].y];
					positions[index * 3] = triangle->point0;
					positions[index * 3 + 1] = triangle->point1;
					positions[index * 3 + 2] = triangle->point2;
					SetInstancedColor(instanced_data, index, triangle->color);
					SetInstancedMatrix(instanced_data, index, gpu_camera);
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
					DrawCallTriangle(current_count);
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
			position_buffer.Release();
			instanced_view.Release();
			instanced_buffer.Release();
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
			unsigned int instance_count = GetMaximumCount(counts);

			// Create temporary buffers to hold the data
			VertexBuffer instanced_buffer = graphics->CreateVertexBuffer(sizeof(InstancedTransformData), instance_count, true);

			// Bind the vertex buffers now
			BindCubeBuffers(instanced_buffer);

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
					Matrix matrix = MatrixTS(MatrixTranslation(aabb->translation), MatrixScale(aabb->scale));
					SetInstancedMatrix(instanced_data, index, MatrixGPU(MatrixMVP(matrix, camera_matrix)));
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
					DrawCallAABB(current_count);
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
			instanced_buffer.Release();
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
			unsigned int instance_count = GetMaximumCount(counts);

			// Create temporary buffers to hold the data
			VertexBuffer instanced_buffer = graphics->CreateVertexBuffer(sizeof(InstancedTransformData), instance_count, true);

			// Bind the vertex buffers now
			BindCubeBuffers(instanced_buffer);

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
					SetInstancedMatrix(instanced_data, index, MatrixGPU(MatrixMVP(matrix, camera_matrix)));
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
					DrawCallOOBB(current_count);
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
			instanced_buffer.Release();
			allocator->Deallocate(type_indices);
		}
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::DrawStringDeck(float time_delta) {
		// Calculate the maximum amount of vertices needed for every type
		// and also get an integer mask to indicate the elements for each type
		unsigned int counts[ELEMENT_COUNT] = { 0 };
		ushort2* indices[ELEMENT_COUNT];
		auto deck_pointer = &strings;
		unsigned int total_count = deck_pointer->GetElementCount();

		if (total_count > 0) {
			// Allocate 4 times the memory needed to be sure that each type has enough indices
			ushort2* type_indices = CalculateElementTypeCounts(allocator, counts, indices, deck_pointer);

			// Get the maximum count of characters for each string
			size_t instance_count = 0;
			for (size_t index = 0; index < strings.buffers.size; index++) {
				for (size_t subindex = 0; subindex < strings.buffers[index].size; subindex++) {
					instance_count = std::max(strings.buffers[index][subindex].text.size, instance_count);
				}
			}
			// Sanity check
			ECS_ASSERT(instance_count < 10'000);

			// Create temporary buffers to hold the data
			VertexBuffer instanced_buffer = graphics->CreateVertexBuffer(sizeof(InstancedTransformData), instance_count, true);

			unsigned int current_count = counts[WIREFRAME_DEPTH];
			ushort2* current_indices = indices[WIREFRAME_DEPTH];

			bool* checked_characters = (bool*)allocator->Allocate(sizeof(bool) * instance_count);
			float3* horizontal_offsets = (float3*)allocator->Allocate(sizeof(float3) * instance_count);
			float3* vertical_offsets = (float3*)allocator->Allocate(sizeof(float3) * instance_count);

			auto map_for = [&]() {
				for (size_t index = 0; index < current_count; index++) {
					const DebugString* string = &deck_pointer->buffers[current_indices[index].x][current_indices[index].y];

					// Normalize the direction
					float3 direction = Normalize(string->direction);

					float3 rotation_from_direction = DirectionToRotation(direction);
					// Invert the y axis 
					rotation_from_direction.y = -rotation_from_direction.y;
					float3 up_direction = GetUpVector(rotation_from_direction);

					// Splat these early
					float3 space_size = float3::Splat(STRING_SPACE_SIZE * string->size) * direction;
					float3 tab_size = float3::Splat(STRING_TAB_SIZE * string->size) * direction;
					float3 character_spacing = float3::Splat(STRING_CHARACTER_SPACING * string->size) * direction;
					float3 new_line_size = float3::Splat(STRING_NEW_LINE_SIZE * -string->size) * up_direction;

					Matrix rotation_matrix = MatrixRotation({ rotation_from_direction.x, rotation_from_direction.y, rotation_from_direction.z });
					Matrix scale_matrix = MatrixScale(float3::Splat(string->size));
					Matrix scale_rotation = MatrixRS(rotation_matrix, scale_matrix);

					// Keep track of already checked elements - the same principle as the Eratosthenes sieve
					memset(checked_characters, 0, sizeof(bool) * string->text.size);

					// Create a mask of spaces and tabs offsets for each character

					// Do the first character separately
					horizontal_offsets[0] = { 0.0f, 0.0f, 0.0f };
					vertical_offsets[0] = { 0.0f, 0.0f, 0.0f };

					if (string->text[0] == ' ' || string->text[0] == '\n' || string->text[0] == '\t') {
						checked_characters[0] = true;
					}

					for (size_t text_index = 1; text_index < string->text.size; text_index++) {
						// Hoist the checking outside the if
						checked_characters[text_index] = string->text[text_index] == ' ' || string->text[text_index] == '\t' || string->text[text_index] == '\n';

						if (string->text[text_index - 1] == '\n') {
							vertical_offsets[text_index] = vertical_offsets[text_index - 1] + new_line_size;

							// If a new character has been found - set to 0 all the other offsets - the tab and the space
							horizontal_offsets[text_index] = { 0.0f, 0.0f, 0.0f };
						}
						else {
							// It doesn't matter if the current character is \n because it will reset the horizontal offset anyway
							unsigned int previous_alphabet_index = function::GetAlphabetIndex(string->text[text_index - 1]);
							unsigned int current_alphabet_index = function::GetAlphabetIndex(string->text[text_index]);

							float previous_offset = string_character_bounds[previous_alphabet_index].y;
							float current_offset = string_character_bounds[current_alphabet_index].x;

							// The current offset will be negative
							float3 character_span = float3::Splat((previous_offset - current_offset) * string->size) * direction;
							horizontal_offsets[text_index] = horizontal_offsets[text_index - 1] + character_spacing + character_span;
							vertical_offsets[text_index] = vertical_offsets[text_index - 1];
						}
					}

					for (size_t text_index = 0; text_index < string->text.size; text_index++) {
						if (!checked_characters[text_index]) {
							unsigned int current_character_count = 0;

							void* instanced_data = graphics->MapBuffer(instanced_buffer.buffer);

							// Do a search for the same character
							for (size_t subindex = text_index; subindex < string->text.size; subindex++) {
								if (string->text[subindex] == string->text[text_index]) {
									float3 translation = string->position + horizontal_offsets[subindex] + vertical_offsets[subindex];
									checked_characters[subindex] = true;

									SetInstancedColor(instanced_data, current_character_count, string->color);
									SetInstancedMatrix(instanced_data, current_character_count, MatrixGPU(MatrixMVP(
										scale_rotation * MatrixTranslation(translation), camera_matrix
									)));
									current_character_count++;
								}
							}

							graphics->UnmapBuffer(instanced_buffer.buffer);

							unsigned int alphabet_index = function::GetAlphabetIndex(string->text[text_index]);
							// Draw the current character
							graphics->DrawIndexedInstanced(string_mesh->submeshes[alphabet_index].index_count, current_character_count, string_mesh->submeshes[alphabet_index].index_buffer_offset);
						}
					}
				}
			};

			BindStringBuffers(instanced_buffer);

			auto draw_call = [&](DebugDrawCallOptions options) {
				if (current_count > 0) {
					// Set the state
					SetStringState(options);

					// Issues the draw calls once per string - too difficult to batch multiple strings at once
					map_for();
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

			// Update the duration and remove those elements that expired;
			// Also deallocate the string buffer
			for (size_t index = 0; index < deck_pointer->buffers.size; index++) {
				for (int64_t subindex = 0; subindex < deck_pointer->buffers[index].size; subindex++) {
					deck_pointer->buffers[index][subindex].options.duration -= time_delta;
					if (deck_pointer->buffers[index][subindex].options.duration < 0.0f) {
						allocator->Deallocate(deck_pointer->buffers[index][subindex].text.buffer);
						deck_pointer->buffers[index].RemoveSwapBack(subindex);
						subindex--;
					}
				}
			}
			deck_pointer->RecalculateFreeChunks();

			// Release the temporary vertex buffers and the temporary allocation
			instanced_buffer.Release();
			allocator->Deallocate(type_indices);
			allocator->Deallocate(horizontal_offsets);
			allocator->Deallocate(vertical_offsets);
			allocator->Deallocate(checked_characters);
		}
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
		graphics->BindTopology(D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP);
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
		unsigned int shader_type = ECS_DEBUG_SHADER_TRANSFORM;
		BindShaders(shader_type);
		HandleOptions(this, options);
		graphics->BindTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
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
		unsigned int shader_type = ECS_DEBUG_SHADER_TRANSFORM;
		HandleOptions(this, options);
		BindShaders(shader_type);
		graphics->BindTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	}

	// ----------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Initialize

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::Initialize(MemoryManager* _allocator, ResourceManager* resource_manager, size_t _thread_count) {
		allocator = _allocator;
		graphics = resource_manager->m_graphics;
		thread_count = _thread_count;

		// Initialize the small vertex buffers
		positions_small_vertex_buffer = graphics->CreateVertexBuffer(sizeof(float3), SMALL_VERTEX_BUFFER_CAPACITY);
		instanced_small_vertex_buffer = graphics->CreateVertexBuffer(sizeof(InstancedTransformData), SMALL_VERTEX_BUFFER_CAPACITY);
		instanced_small_structured_buffer = graphics->CreateStructuredBuffer(sizeof(InstancedTransformData), SMALL_VERTEX_BUFFER_CAPACITY);
		instanced_structured_view = graphics->CreateBufferView(instanced_small_structured_buffer);

		Stream<char> shader_source;
		Stream<void> byte_code;

#define REGISTER_SHADER(index, name) vertex_shaders[index] = resource_manager->LoadShaderImplementation(ECS_VERTEX_SHADER_SOURCE(name), ECS_SHADER_VERTEX, &shader_source, &byte_code); \
		pixel_shaders[index] = resource_manager->LoadShaderImplementation(ECS_PIXEL_SHADER_SOURCE(name), ECS_SHADER_PIXEL); \
		layout_shaders[index] = resource_manager->m_graphics->ReflectVertexShaderInput(shader_source, byte_code); \
		resource_manager->Deallocate(shader_source.buffer);

		// Initialize the shaders and input layouts
		REGISTER_SHADER(ECS_DEBUG_SHADER_TRANSFORM, DebugTransform);
		REGISTER_SHADER(ECS_DEBUG_SHADER_STRUCTURED_INSTANCED_DATA, DebugStructured);
		REGISTER_SHADER(ECS_DEBUG_SHADER_NORMAL_SINGLE_DRAW, DebugNormalSingleDraw);

#undef REGISTER_SHADER

		// Initialize the raster states

		// Wireframe
		D3D11_RASTERIZER_DESC rasterizer_desc = {};
		rasterizer_desc.AntialiasedLineEnable = TRUE;
		rasterizer_desc.CullMode = D3D11_CULL_NONE;
		rasterizer_desc.DepthClipEnable = TRUE;
		rasterizer_desc.FillMode = D3D11_FILL_WIREFRAME;
		rasterizer_states[ECS_DEBUG_RASTERIZER_WIREFRAME] = graphics->CreateRasterizerState(rasterizer_desc);

		// Solid - disable the back face culling
		rasterizer_desc.FillMode = D3D11_FILL_SOLID;
		rasterizer_desc.CullMode = D3D11_CULL_NONE;
		rasterizer_states[ECS_DEBUG_RASTERIZER_SOLID] = graphics->CreateRasterizerState(rasterizer_desc);

		// Solid - with back face culling
		rasterizer_desc.CullMode = D3D11_CULL_BACK;
		rasterizer_states[ECS_DEBUG_RASTERIZER_SOLID_CULL] = graphics->CreateRasterizerState(rasterizer_desc);

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
		total_memory += ECS_CACHE_LINE_SIZE * thread_count + sizeof(SpinLock*) * ECS_DEBUG_PRIMITIVE_COUNT;

		// The string character bounds
		total_memory += sizeof(float2) * (unsigned int)AlphabetIndex::Unknown;

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
		string_character_bounds = (float2*)buffer;

		AllocatorPolymorphic polymorphic_allocator = GetAllocatorPolymorphic(allocator);

		lines.Initialize(polymorphic_allocator, 1, DECK_CHUNK_SIZE, DECK_POWER_OF_TWO);
		spheres.Initialize(polymorphic_allocator, 1, DECK_CHUNK_SIZE, DECK_POWER_OF_TWO);
		points.Initialize(polymorphic_allocator, 1, DECK_CHUNK_SIZE, DECK_POWER_OF_TWO);
		rectangles.Initialize(polymorphic_allocator, 1, DECK_CHUNK_SIZE, DECK_POWER_OF_TWO);
		crosses.Initialize(polymorphic_allocator, 1, DECK_CHUNK_SIZE, DECK_POWER_OF_TWO);
		circles.Initialize(polymorphic_allocator, 1, DECK_CHUNK_SIZE, DECK_POWER_OF_TWO);
		arrows.Initialize(polymorphic_allocator, 1, DECK_CHUNK_SIZE, DECK_POWER_OF_TWO);
		axes.Initialize(polymorphic_allocator, 1, DECK_CHUNK_SIZE, DECK_POWER_OF_TWO);
		triangles.Initialize(polymorphic_allocator, 1, DECK_CHUNK_SIZE, DECK_POWER_OF_TWO);
		aabbs.Initialize(polymorphic_allocator, 1, DECK_CHUNK_SIZE, DECK_POWER_OF_TWO);
		oobbs.Initialize(polymorphic_allocator, 1, DECK_CHUNK_SIZE, DECK_POWER_OF_TWO);
		strings.Initialize(polymorphic_allocator, 1, DECK_CHUNK_SIZE, DECK_POWER_OF_TWO);

#pragma region Primitive buffers

		// The string meshes - in the gltf mesh they are stored in reverse order.
		// Change their order
		string_mesh = resource_manager->LoadCoalescedMesh<true>(STRING_MESH_FILE);

		size_t submesh_count = string_mesh->submeshes.size;
		// Change the order of the submeshes such that they mirror alphabet index
		unsigned int* string_submesh_mask = (unsigned int*)ECS_STACK_ALLOC(sizeof(unsigned int) * submesh_count);
		memset(string_submesh_mask, 0, sizeof(unsigned int) * submesh_count);

		// Blender export bug - " gets exported as \" - must account for that
		for (size_t index = 0; index < (unsigned int)AlphabetIndex::Space; index++) {
			for (size_t subindex = 0; subindex < submesh_count && string_submesh_mask[index] == 0; subindex++) {
				// If it is not the invalid character, " or backslash
				if (string_mesh->submeshes[subindex].name[1] == '\0') {
					unsigned int alphabet_index = function::GetAlphabetIndex(string_mesh->submeshes[subindex].name[0]);
					string_submesh_mask[index] = (alphabet_index == index) * subindex;
				}
			}
		}

		// Invalid character, " and \\ handled separately

		// "
		for (size_t index = 0; index < submesh_count; index++) {
			if (string_mesh->submeshes[index].name[1] == '\"') {
				string_submesh_mask[(unsigned int)AlphabetIndex::Quotes] = index;
				break;
			}
		}

		// backslash
		for (size_t index = 0; index < submesh_count; index++) {
			if (string_mesh->submeshes[index].name[1] == '\\') {
				string_submesh_mask[(unsigned int)AlphabetIndex::Backslash] = index;
				break;
			}
		}

		// Invalid character
		for (size_t index = 0; index < submesh_count; index++) {
			if (string_mesh->submeshes[index].name[1] != '\0' && string_mesh->submeshes[index].name[2] != '\0') {
				string_submesh_mask[submesh_count - 1] = index;
				break;
			}
		}

		// Swap the submeshes according to the mask
		Submesh* backup_submeshes = (Submesh*)ECS_STACK_ALLOC(sizeof(Submesh) * submesh_count);
		memcpy(backup_submeshes, string_mesh->submeshes.buffer, sizeof(Submesh) * submesh_count);
		for (size_t index = 0; index < submesh_count; index++) {
			string_mesh->submeshes[index] = backup_submeshes[string_submesh_mask[index]];
		}

		// Transform the vertex buffer to a staging buffer and read the values
		VertexBuffer staging_buffer = BufferToStaging(graphics, GetMeshVertexBuffer(string_mesh->mesh, ECS_MESH_POSITION));

		// Map the buffer
		float3* values = (float3*)graphics->MapBuffer(staging_buffer.buffer, ECS_GRAPHICS_MAP_READ);

		// Determine the string character spans
		for (size_t index = 0; index < submesh_count; index++) {
			float maximum = -10000.0f;
			float minimum = 10000.0f;
			// Walk through the vertices and record the minimum and maximum for the X axis
			for (size_t vertex_index = 0; vertex_index < string_mesh->submeshes[index].vertex_count; vertex_index++) {
				size_t offset = string_mesh->submeshes[index].vertex_buffer_offset + vertex_index;
				maximum = std::max(values[offset].x, maximum);
				minimum = std::min(values[offset].x, minimum);
			}

			// The character span is the difference between maximum and minimum
			string_character_bounds[index] = { minimum, maximum };
		}
		// Copy the unknown character to the last character
		string_character_bounds[(unsigned int)AlphabetIndex::Unknown - 1] = string_character_bounds[submesh_count - 1];

		// Place the space and tab dimensions
		string_character_bounds[(unsigned int)AlphabetIndex::Space] = { -STRING_SPACE_SIZE, STRING_SPACE_SIZE };
		string_character_bounds[(unsigned int)AlphabetIndex::Tab] = { -STRING_TAB_SIZE, STRING_TAB_SIZE };

		graphics->UnmapBuffer(staging_buffer.buffer);

		// Release the staging buffer
		staging_buffer.Release();

		for (size_t index = 0; index < ECS_DEBUG_VERTEX_BUFFER_COUNT; index++) {
			primitive_meshes[index] = ((Stream<Mesh>*)resource_manager->LoadMeshes<true>(ECS_DEBUG_PRIMITIVE_MESH_FILE[index]))->buffer;
		}

		float3 circle_positions[CIRCLE_TESSELATION + 1];

		// The circle
		float step = 2.0f * PI / CIRCLE_TESSELATION;
		size_t index = 0;
		float float_index = 0.0f;
		for (; index < CIRCLE_TESSELATION; index++) {
			float z = cos(float_index);
			float x = sin(float_index);

			circle_positions[index].x = x;
			circle_positions[index].y = 0.0f;
			circle_positions[index].z = z;

			float_index += step;
		}
		// Last point must be the same as the first
		circle_positions[index] = circle_positions[0];

		circle_buffer = graphics->CreateVertexBuffer(sizeof(float3), std::size(circle_positions), circle_positions);

#pragma endregion

	}

	// ----------------------------------------------------------------------------------------------------------------------

#pragma region Bindings

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::BindSphereBuffers(VertexBuffer instanced_data)
	{
		ECS_MESH_INDEX mesh_index = ECS_MESH_POSITION;
		graphics->BindMesh(*primitive_meshes[ECS_DEBUG_VERTEX_BUFFER_SPHERE], Stream<ECS_MESH_INDEX>(&mesh_index, 1));
		graphics->BindVertexBuffer(instanced_data, 1);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::BindPointBuffers(VertexBuffer instanced_data)
	{
		ECS_MESH_INDEX mesh_index = ECS_MESH_POSITION;
		graphics->BindMesh(*primitive_meshes[ECS_DEBUG_VERTEX_BUFFER_POINT], Stream<ECS_MESH_INDEX>(&mesh_index, 1));
		graphics->BindVertexBuffer(instanced_data, 1);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::BindCrossBuffers(VertexBuffer instanced_data)
	{
		ECS_MESH_INDEX mesh_index = ECS_MESH_POSITION;
		graphics->BindMesh(*primitive_meshes[ECS_DEBUG_VERTEX_BUFFER_CROSS], Stream<ECS_MESH_INDEX>(&mesh_index, 1));
		graphics->BindVertexBuffer(instanced_data, 1);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::BindCircleBuffers(VertexBuffer instanced_data)
	{
		VertexBuffer vertex_buffers[2];
		vertex_buffers[0] = circle_buffer;
		vertex_buffers[1] = instanced_data;
		graphics->BindVertexBuffers(Stream<VertexBuffer>(vertex_buffers, std::size(vertex_buffers)));
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::BindArrowCylinderBuffers(VertexBuffer instanced_data)
	{
		ECS_MESH_INDEX mesh_position = ECS_MESH_POSITION;
		Stream<ECS_MESH_INDEX> mesh_mapping(&mesh_position, 1);
		graphics->BindMesh(*primitive_meshes[ECS_DEBUG_VERTEX_BUFFER_ARROW_CYLINDER], mesh_mapping);
		graphics->BindVertexBuffer(instanced_data, 1);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::BindArrowHeadBuffers(VertexBuffer instanced_data)
	{
		ECS_MESH_INDEX mesh_position = ECS_MESH_POSITION;
		Stream<ECS_MESH_INDEX> mesh_mapping(&mesh_position, 1);
		graphics->BindMesh(*primitive_meshes[ECS_DEBUG_VERTEX_BUFFER_ARROW_HEAD], mesh_mapping);
		graphics->BindVertexBuffer(instanced_data, 1);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::BindCubeBuffers(VertexBuffer instanced_data)
	{
		ECS_MESH_INDEX mesh_index = ECS_MESH_POSITION;
		graphics->BindMesh(*primitive_meshes[ECS_DEBUG_VERTEX_BUFFER_CUBE], Stream<ECS_MESH_INDEX>(&mesh_index, 1));
		graphics->BindVertexBuffer(instanced_data, 1);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::BindStringBuffers(VertexBuffer instanced_data)
	{
		ECS_MESH_INDEX mesh_position = ECS_MESH_POSITION;
		graphics->BindMesh(string_mesh->mesh, Stream<ECS_MESH_INDEX>(&mesh_position, 1));
		graphics->BindVertexBuffer(instanced_data, 1);
	}

#pragma endregion

	// ----------------------------------------------------------------------------------------------------------------------

	AllocatorPolymorphic DebugDrawer::Allocator() const
	{
		return GetAllocatorPolymorphic(allocator);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	AllocatorPolymorphic DebugDrawer::AllocatorTs() const
	{
		return GetAllocatorPolymorphic(allocator, ECS_ALLOCATION_MULTI);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::BindShaders(unsigned int index)
	{
		graphics->BindVertexShader(vertex_shaders[index]);
		graphics->BindPixelShader(pixel_shaders[index]);
		graphics->BindInputLayout(layout_shaders[index]);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::CopyCharacterSubmesh(IndexBuffer index_buffer, unsigned int& buffer_offset, unsigned int alphabet_index) {
		CopyBufferSubresource(
			index_buffer,
			buffer_offset,
			string_mesh->mesh.index_buffer,
			string_mesh->submeshes[alphabet_index].index_buffer_offset,
			string_mesh->submeshes[alphabet_index].index_count,
			graphics->GetContext()
		);

		buffer_offset += string_mesh->submeshes[alphabet_index].index_count;
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::UpdateCameraMatrix(Matrix new_matrix)
	{
		camera_matrix = new_matrix;
	}

	// ----------------------------------------------------------------------------------------------------------------------

	GraphicsPipelineRenderState DebugDrawer::GetPreviousRenderState() const
	{
		return graphics->GetPipelineRenderState();
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::RestorePreviousRenderState(const GraphicsPipelineRenderState* state)
	{
		graphics->RestorePipelineRenderState(state);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	MemoryManager DebugDrawer::DefaultAllocator(GlobalMemoryManager* global_memory) {
		return MemoryManager(DefaultAllocatorSize(), ECS_KB * 16, DefaultAllocatorSize(), global_memory);
	}

	// ----------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Draw Calls

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::DrawCallLine(unsigned int instance_count)
	{
		graphics->DrawInstanced(2, instance_count);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::DrawCallSphere(unsigned int instance_count)
	{
		graphics->DrawIndexedInstanced(primitive_meshes[ECS_DEBUG_VERTEX_BUFFER_SPHERE]->index_buffer.count, instance_count);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::DrawCallPoint(unsigned int instance_count)
	{
		graphics->DrawIndexedInstanced(primitive_meshes[ECS_DEBUG_VERTEX_BUFFER_POINT]->index_buffer.count, instance_count);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::DrawCallRectangle(unsigned int instance_count)
	{
		graphics->DrawInstanced(6, instance_count);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::DrawCallCross(unsigned int instance_count)
	{
		graphics->DrawIndexedInstanced(primitive_meshes[ECS_DEBUG_VERTEX_BUFFER_CROSS]->index_buffer.count, instance_count);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::DrawCallCircle(unsigned int instance_count)
	{
		graphics->DrawInstanced(CIRCLE_TESSELATION + 1, instance_count);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::DrawCallArrowCylinder(unsigned int instance_count)
	{
		graphics->DrawIndexedInstanced(primitive_meshes[ECS_DEBUG_VERTEX_BUFFER_ARROW_CYLINDER]->index_buffer.count, instance_count);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::DrawCallArrowHead(unsigned int instance_count)
	{
		graphics->DrawIndexedInstanced(primitive_meshes[ECS_DEBUG_VERTEX_BUFFER_ARROW_HEAD]->index_buffer.count, instance_count);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::DrawCallTriangle(unsigned int instance_count)
	{
		graphics->DrawInstanced(3, instance_count);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::DrawCallAABB(unsigned int instance_count)
	{
		graphics->DrawIndexedInstanced(primitive_meshes[ECS_DEBUG_VERTEX_BUFFER_CUBE]->index_buffer.count, instance_count);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::DrawCallOOBB(unsigned int instance_count)
	{
		graphics->DrawIndexedInstanced(primitive_meshes[ECS_DEBUG_VERTEX_BUFFER_CUBE]->index_buffer.count, instance_count);
	}

	// ----------------------------------------------------------------------------------------------------------------------

#pragma endregion

	// ----------------------------------------------------------------------------------------------------------------------

	// ----------------------------------------------------------------------------------------------------------------------
}