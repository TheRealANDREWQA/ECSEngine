#pragma once
#include "../../Core.h"
#include "../../Rendering/Graphics.h"
#include "../../Allocators/MemoryManager.h"
#include "../../Math/Matrix.h"
#include "../../Containers/Deck.h"
#include "../../Multithreading/ConcurrentPrimitives.h"
#include "../../Rendering/RenderingStructures.h"
#include "DebugDrawTypes.h"

namespace ECSEngine {

#define ECS_DEBUG_DRAWER_OUTPUT_INSTANCE_IMMEDIATE -1
#define ECS_DEBUG_DRAWER_OUTPUT_INSTANCE_ADD -2

	struct ResourceManager;

	typedef float3 DebugVertex;

	ECS_INLINE ColorFloat AxisXColor() {
		return ColorFloat(1.0f, 0.0f, 0.0f);
	}

	ECS_INLINE ColorFloat AxisYColor() {
		return ColorFloat(0.0f, 1.0f, 0.0f);
	}

	ECS_INLINE ColorFloat AxisZColor() {
		return ColorFloat(0.0f, 0.25f, 1.0f);
	}

	struct DebugDrawCallOptions {
		bool wireframe = true;
		bool ignore_depth = false;
		float duration = 0.0f;
	};

	union DebugDrawerOutput {
		ECS_INLINE DebugDrawerOutput() {}
		ECS_INLINE DebugDrawerOutput(ColorFloat _color) : color(_color) {}
		ECS_INLINE DebugDrawerOutput(unsigned int _instance_index) : instance_index(_instance_index) {}

		ColorFloat color;
		unsigned int instance_index;;
	};

	struct DebugLine {
		float3 start;
		float3 end;
		DebugDrawerOutput output;
		DebugDrawCallOptions options;
	};

	struct DebugSphere {
		float3 position;
		float radius;
		DebugDrawerOutput output;
		DebugDrawCallOptions options;
	};

	struct DebugPoint {
		float3 position;
		DebugDrawerOutput output;
		DebugDrawCallOptions options;
	};

	struct DebugRectangle {
		float3 corner0;
		float3 corner1;
		DebugDrawerOutput output;
		DebugDrawCallOptions options;
	};

	struct DebugCross {
		float3 position;
		float3 rotation;
		float size;
		DebugDrawerOutput output;
		DebugDrawCallOptions options;
	};

	struct DebugCircle {
		float3 position;
		float3 rotation;
		float radius;
		DebugDrawerOutput output;
		DebugDrawCallOptions options;
	};

	struct DebugArrow {
		float3 translation;
		float3 rotation;
		float length;
		float size;
		DebugDrawerOutput output;
		DebugDrawCallOptions options;
	};

	struct DebugAxes {
		float3 translation;
		float3 rotation;
		float size;
		DebugDrawerOutput output_x;
		DebugDrawerOutput output_y;
		DebugDrawerOutput output_z;
		DebugDrawCallOptions options;
	};

	struct DebugTriangle {
		float3 point0;
		float3 point1;
		float3 point2;
		DebugDrawerOutput output;
		DebugDrawCallOptions options;
	};

	struct DebugAABB {
		float3 translation;
		float3 scale;
		DebugDrawerOutput output;
		DebugDrawCallOptions options;
	};

	struct DebugOOBB {
		float3 translation;
		float3 rotation;
		float3 scale;
		DebugDrawerOutput output;
		DebugDrawCallOptions options;
	};

	struct DebugString {
		float3 position;
		float3 direction;
		float size;
		Stream<char> text;
		DebugDrawerOutput output;
		DebugDrawCallOptions options;
	};

	struct DebugDrawerOutputInstance {
		unsigned int instance_index;
		DebugDrawCallOptions options = {};
	};

	struct ECSENGINE_API DebugDrawer {
		DebugDrawer() : allocator(nullptr), graphics(nullptr) {}
		DebugDrawer(MemoryManager* allocator, ResourceManager* manager, size_t thread_count);

		DebugDrawer(const DebugDrawer& other) = default;
		DebugDrawer& operator = (const DebugDrawer& other) = default;

#pragma region Add to the draw queue - single threaded

		void AddLine(float3 start, float3 end, ColorFloat color, DebugDrawCallOptions options = {});

		void AddLine(float3 translation, float3 rotation, float size, ColorFloat color, DebugDrawCallOptions options = {});

		void AddSphere(float3 position, float radius, ColorFloat color, DebugDrawCallOptions options = {});

		void AddPoint(float3 position, ColorFloat color, DebugDrawCallOptions options = {false});

		// Corner0 is the top left corner, corner1 is the bottom right corner
		void AddRectangle(float3 corner0, float3 corner1, ColorFloat color, DebugDrawCallOptions options = {});

		void AddCross(float3 position, float3 rotation, float size, ColorFloat color, DebugDrawCallOptions options = {false});

		void AddCircle(float3 position, float3 rotation, float radius, ColorFloat color, DebugDrawCallOptions options = {});

		void AddArrow(float3 start, float3 end, float size, ColorFloat color, DebugDrawCallOptions options = {false});
		
		void AddArrowRotation(float3 translation, float3 rotation, float length, float size, ColorFloat color, DebugDrawCallOptions options = {false});

		void AddAxes(
			float3 translation, 
			float3 rotation, 
			float size, 
			ColorFloat color_x = AxisXColor(), 
			ColorFloat color_y = AxisYColor(), 
			ColorFloat color_z = AxisZColor(), 
			DebugDrawCallOptions options = {false}
		);

		void AddTriangle(float3 point0, float3 point1, float3 point2, ColorFloat color, DebugDrawCallOptions options = {});

		void AddAABB(float3 translation, float3 scale, ColorFloat color, DebugDrawCallOptions options = {});

		void AddOOBB(float3 translation, float3 rotation, float3 scale, ColorFloat color, DebugDrawCallOptions options = {});

		void AddString(float3 position, float3 direction, float size, Stream<char> text, ColorFloat color, DebugDrawCallOptions options = { false });

		void AddStringRotation(float3 position, float3 rotation, float size, Stream<char> text, ColorFloat color, DebugDrawCallOptions options = { false });

#pragma endregion

#pragma region Add to the draw queue - multi threaded

		void AddLineThread(unsigned int thread_index, float3 start, float3 end, ColorFloat color, DebugDrawCallOptions options = {});

		void AddLineThread(unsigned int thread_index, float3 translation, float3 rotation, float size, ColorFloat color, DebugDrawCallOptions options = {});

		void AddSphereThread(unsigned int thread_index, float3 position, float radius, ColorFloat color, DebugDrawCallOptions options = {});

		void AddPointThread(unsigned int thread_index, float3 position, ColorFloat color, DebugDrawCallOptions options = {false});

		// Corner0 is the top left corner, corner1 is the bottom right corner
		void AddRectangleThread(unsigned int thread_index, float3 corner0, float3 corner1, ColorFloat color, DebugDrawCallOptions options = {});

		void AddCrossThread(unsigned int thread_index, float3 position, float3 rotation, float size, ColorFloat color, DebugDrawCallOptions options = {});

		void AddCircleThread(unsigned int thread_index, float3 position, float3 rotation, float radius, ColorFloat color, DebugDrawCallOptions options = {});

		void AddArrowThread(unsigned int thread_index, float3 start, float3 end, float size, ColorFloat color, DebugDrawCallOptions options = {false});

		void AddArrowRotationThread(unsigned int thread_index, float3 translation, float3 rotation, float length, float size, ColorFloat color, DebugDrawCallOptions options = {false});

		void AddAxesThread(
			unsigned int thread_index, 
			float3 translation, 
			float3 rotation, 
			float size, 
			ColorFloat color_x = AxisXColor(), 
			ColorFloat color_y = AxisYColor(), 
			ColorFloat color_z = AxisZColor(), 
			DebugDrawCallOptions options = {false}
		);

		void AddTriangleThread(unsigned int thread_index, float3 point0, float3 point1, float3 point2, ColorFloat color, DebugDrawCallOptions options = {});

		void AddAABBThread(unsigned int thread_index, float3 translation, float3 scale, ColorFloat color, DebugDrawCallOptions options = {});

		void AddOOBBThread(unsigned int thread_index, float3 translation, float3 rotation, float3 scale, ColorFloat color, DebugDrawCallOptions options = {});

		// It must do an allocation from the memory manager under lock - possible expensive operation
		void AddStringThread(
			unsigned int thread_index,
			float3 position, 
			float3 direction,
			float size,
			Stream<char> text,
			ColorFloat color, 
			DebugDrawCallOptions options = { false }
		);

		// It must do an allocation from the memory manager under lock - possible expensive operation
		void AddStringRotationThread(
			unsigned int thread_index,
			float3 position,
			float3 direction,
			float size,
			Stream<char> text,
			ColorFloat color,
			DebugDrawCallOptions options = { false }
		);

#pragma endregion

#pragma region Draw immediately

		void DrawLine(float3 start, float3 end, ColorFloat color, DebugDrawCallOptions options = {});

		void DrawLine(float3 translation, float3 rotation, float size, ColorFloat color, DebugDrawCallOptions options = {});

		void DrawSphere(float3 position, float radius, ColorFloat color, DebugDrawCallOptions options = {});

		void DrawPoint(float3 position, ColorFloat color, DebugDrawCallOptions options = {});

		// Corner0 is the top left corner, corner1 is the bottom right corner
		void DrawRectangle(float3 corner0, float3 corner1, ColorFloat color, DebugDrawCallOptions options = {});

		void DrawCross(float3 position, float3 rotation, float size, ColorFloat color, DebugDrawCallOptions options = {});

		void DrawCircle(float3 position, float3 rotation, float radius, ColorFloat color, DebugDrawCallOptions options = {});

		void DrawArrow(float3 start, float3 end, float size, ColorFloat color, DebugDrawCallOptions options = {false});

		// Rotation expressed as radians
		void DrawArrowRotation(float3 translation, float3 rotation, float length, float size, ColorFloat color, DebugDrawCallOptions options = {false});

		void DrawAxes(
			float3 translation, 
			float3 rotation, 
			float size, 
			ColorFloat color_x = AxisXColor(), 
			ColorFloat color_y = AxisYColor(), 
			ColorFloat color_z = AxisZColor(), 
			DebugDrawCallOptions options = {false}
		);

		void DrawTriangle(float3 point0, float3 point1, float3 point2, ColorFloat color, DebugDrawCallOptions options = {});

		void DrawAABB(float3 translation, float3 scale, ColorFloat color, DebugDrawCallOptions options = {});

		void DrawOOBB(float3 translation, float3 rotation, float3 scale, ColorFloat color, DebugDrawCallOptions options = {});

		// Text rotation is the rotation alongside the X axis - rotates the text in order to be seen from below, above, or at a specified angle
		void DrawString(
			float3 translation, 
			float3 direction, 
			float size, 
			Stream<char> text,
			ColorFloat color, 
			DebugDrawCallOptions options = {false}
		);

		// Text rotation is the rotation alongside the X axis - rotates the text in order to be seen from below, above, or at a specified angle
		// The direction is specified as rotation in angles as degrees
		void DrawStringRotation(
			float3 translation, 
			float3 rotation, 
			float size, 
			Stream<char> text, 
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
			Stream<Matrix> world_matrices,
			DebugDrawCallOptions options = {}
		);

		// Draws the tangents for an object
		void DrawTangents(
			VertexBuffer model_position,
			VertexBuffer model_tangents,
			float size,
			ColorFloat color,
			Matrix world_matrix,
			DebugDrawCallOptions options = {}
		);

		// Draws the tangents for multiple objects of the same type
		void DrawTangents(
			VertexBuffer model_position,
			VertexBuffer model_tangents,
			float size,
			ColorFloat color,
			Stream<Matrix> world_matrices,
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

#pragma region Output Instance Index

		// These functions output into a framebuffer which is of type R32_UINT
		// the instance index of 29 bits packed with 3 bits of pixel thickness
		// For all of these functions if the addition_stream is nullptr then it
		// will draw immediately, otherwise it will add the entry to the stream

		void OutputInstanceIndexLine(
			float3 start, 
			float3 end, 
			const DebugDrawerOutputInstance* instance,
			AdditionStreamAtomic<DebugLine>* addition_stream
		);

		void OutputInstanceIndexLine(
			float3 translation,
			float3 rotation,
			float size,
			const DebugDrawerOutputInstance* instance,
			AdditionStreamAtomic<DebugLine>* addition_stream
		);

		void OutputInstanceIndexSphere(
			float3 translation,
			float radius,
			const DebugDrawerOutputInstance* instance,
			AdditionStreamAtomic<DebugSphere>* addition_stream
		);

		void OutputInstanceIndexPoint(
			float3 translation,
			const DebugDrawerOutputInstance* instance,
			AdditionStreamAtomic<DebugPoint>* addition_stream
		);

		void OutputInstanceIndexRectangle(
			float3 corner0, 
			float3 corner1,
			const DebugDrawerOutputInstance* instance,
			AdditionStreamAtomic<DebugRectangle>* addition_stream
		);

		void OutputInstanceIndexCross(
			float3 position, 
			float3 rotation, 
			float size,
			const DebugDrawerOutputInstance* instance,
			AdditionStreamAtomic<DebugCross>* addition_stream
		);

		void OutputInstanceIndexCircle(
			float3 position, 
			float3 rotation, 
			float radius,
			const DebugDrawerOutputInstance* instance,
			AdditionStreamAtomic<DebugCircle>* addition_stream
		);

		void OutputInstanceIndexArrow(
			float3 start, 
			float3 end, 
			float size,
			const DebugDrawerOutputInstance* instance,
			AdditionStreamAtomic<DebugArrow>* addition_stream
		);

		void OutputInstanceIndexArrowRotation(
			float3 translation,
			float3 rotation,
			float length,
			float size,
			const DebugDrawerOutputInstance* instance,
			AdditionStreamAtomic<DebugArrow>* addition_stream
		);

		void OutputInstanceIndexAxes(
			float3 translation,
			float3 rotation,
			float size,
			unsigned int instance_x,
			unsigned int instance_y,
			unsigned int instance_z,
			DebugDrawCallOptions options,
			AdditionStreamAtomic<DebugArrow>* addition_stream
		);

		void OutputInstanceIndexTriangle(
			float3 point0, 
			float3 point1, 
			float3 point2,
			const DebugDrawerOutputInstance* instance,
			AdditionStreamAtomic<DebugTriangle>* addition_stream
		);

		void OutputInstanceIndexAABB(
			float3 translation, 
			float3 scale,
			const DebugDrawerOutputInstance* instance,
			AdditionStreamAtomic<DebugAABB>* addition_stream
		);

		void OutputInstanceIndexOOBB(
			float3 translation, 
			float3 rotation, 
			float3 scale,
			const DebugDrawerOutputInstance* instance,
			AdditionStreamAtomic<DebugOOBB>* addition_stream
		);

		// The allocator will be used to allocate the string
		void OutputInstanceIndexString(
			float3 translation,
			float3 direction,
			float size,
			Stream<char> text,
			const DebugDrawerOutputInstance* instance,
			AdditionStreamAtomic<DebugString>* addition_stream,
			AllocatorPolymorphic allocator
		);

		// The allocator will be used to allocate the string
		void OutputInstanceIndexStringRotation(
			float3 translation,
			float3 rotation,
			float size,
			Stream<char> text,
			const DebugDrawerOutputInstance* instance,
			AdditionStreamAtomic<DebugString>* addition_stream,
			AllocatorPolymorphic allocator
		);

#pragma endregion

#pragma region Output Instance Bulk

		// The time delta is optional
		void OutputInstanceIndexLineBulk(
			const AdditionStreamAtomic<DebugLine>* addition_stream,
			float time_delta = 0.0f
		);

		// The time delta is optional
		void OutputInstanceIndexSphereBulk(
			const AdditionStreamAtomic<DebugSphere>* addition_stream,
			float time_delta = 0.0f
		);

		// The time delta is optional
		void OutputInstanceIndexPointBulk(
			const AdditionStreamAtomic<DebugPoint>* addition_stream,
			float time_delta = 0.0f
		);

		// The time delta is optional
		void OutputInstanceIndexRectangleBulk(
			const AdditionStreamAtomic<DebugRectangle>* addition_stream,
			float time_delta = 0.0f
		);

		// The time delta is optional
		void OutputInstanceIndexCrossBulk(
			const AdditionStreamAtomic<DebugCross>* addition_stream,
			float time_delta = 0.0f
		);

		// The time delta is optional
		void OutputInstanceIndexCircleBulk(
			const AdditionStreamAtomic<DebugCircle>* addition_stream,
			float time_delta = 0.0f
		);

		// The time delta is optional
		void OutputInstanceIndexArrowBulk(
			const AdditionStreamAtomic<DebugArrow>* addition_stream,
			float time_delta = 0.0f
		);

		// The time delta is optional
		void OutputInstanceIndexTriangleBulk(
			const AdditionStreamAtomic<DebugTriangle>* addition_stream,
			float time_delta = 0.0f
		);

		// The time delta is optional
		void OutputInstanceIndexAABBBulk(
			const AdditionStreamAtomic<DebugAABB>* addition_stream,
			float time_delta = 0.0f
		);

		// The time delta is optional
		void OutputInstanceIndexOOBBBulk(
			const AdditionStreamAtomic<DebugOOBB>* addition_stream,
			float time_delta = 0.0f
		);

		// The time delta is optional
		// The allocator will be used to allocate the string
		void OutputInstanceIndexStringBulk(
			const AdditionStreamAtomic<DebugString>* addition_stream,
			float time_delta = 0.0f
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

		void FlushArrow(unsigned int thread_index);

		void FlushTriangle(unsigned int thread_index);

		void FlushAABB(unsigned int thread_index);

		void FlushOOBB(unsigned int thread_index);

		void FlushString(unsigned int thread_index);

#pragma endregion

#pragma region Set state

		// It does not set the vertex buffers. The topology is TRIANGLELIST
		void SetTransformShaderState(DebugDrawCallOptions options, DebugShaderOutput output);

		// It does not set the vertex buffers. The topology is TRIANGLELIST
		void SetStructuredShaderState(DebugDrawCallOptions options, DebugShaderOutput output);

#pragma endregion

#pragma region Initialize

		// If creates with a default constructor or allocated
		void Initialize(MemoryManager* allocator, ResourceManager* resource_manager, size_t thread_count);

#pragma endregion

#pragma region Draw Calls

		void DrawCallStructured(unsigned int vertex_count, unsigned int instance_count);

		void DrawCallTransform(unsigned int index_count, unsigned int instance_count);

#pragma endregion

		AllocatorPolymorphic Allocator() const;

		AllocatorPolymorphic AllocatorTs() const;

		void BindShaders(unsigned int index, DebugShaderOutput output);

		void Clear();

		// It also bumps forward the buffer offset
		void CopyCharacterSubmesh(IndexBuffer index_buffer, unsigned int& buffer_offset, unsigned int alphabet_index);

		void UpdateCameraMatrix(Matrix new_matrix);

		GraphicsPipelineRenderState GetPreviousRenderState() const;

		void RestorePreviousRenderState(const GraphicsPipelineRenderState* state);

		ECS_INLINE static MemoryManager DefaultAllocator(GlobalMemoryManager* global_memory) {
			return MemoryManager(DefaultAllocatorSize(), ECS_KB * 16, DefaultAllocatorSize(), global_memory);
		}

		ECS_INLINE static size_t DefaultAllocatorSize() {
			return ECS_MB;
		}

		MemoryManager* allocator;
		Graphics* graphics;
		DeckPowerOfTwo<DebugLine> lines;
		DeckPowerOfTwo<DebugSphere> spheres;
		DeckPowerOfTwo<DebugPoint> points;
		DeckPowerOfTwo<DebugRectangle> rectangles;
		DeckPowerOfTwo<DebugCross> crosses;
		DeckPowerOfTwo<DebugCircle> circles;
		DeckPowerOfTwo<DebugArrow> arrows;
		DeckPowerOfTwo<DebugTriangle> triangles;
		DeckPowerOfTwo<DebugAABB> aabbs;
		DeckPowerOfTwo<DebugOOBB> oobbs;
		DeckPowerOfTwo<DebugString> strings;
		CapacityStream<DebugLine>* thread_lines;
		CapacityStream<DebugSphere>* thread_spheres;
		CapacityStream<DebugPoint>* thread_points;
		CapacityStream<DebugRectangle>* thread_rectangles;
		CapacityStream<DebugCross>* thread_crosses;
		CapacityStream<DebugCircle>* thread_circles;
		CapacityStream<DebugArrow>* thread_arrows;
		CapacityStream<DebugTriangle>* thread_triangles;
		CapacityStream<DebugAABB>* thread_aabbs;
		CapacityStream<DebugOOBB>* thread_oobbs;
		CapacityStream<DebugString>* thread_strings;
		SpinLock** thread_locks;
		unsigned int thread_count;
		RasterizerState rasterizer_states[ECS_DEBUG_RASTERIZER_COUNT];
		VertexBuffer positions_small_vertex_buffer;
		VertexBuffer instanced_small_transform_vertex_buffer;
		VertexBuffer output_instance_small_matrix_buffer;
		VertexBuffer output_instance_small_id_buffer;
		StructuredBuffer instanced_small_structured_buffer;
		ResourceView instanced_structured_view;
		StructuredBuffer output_instance_matrix_small_structured_buffer;
		ResourceView output_instance_matrix_structured_view;
		StructuredBuffer output_instance_id_small_structured_buffer;
		ResourceView output_instance_id_structured_view;
		Mesh* primitive_meshes[ECS_DEBUG_VERTEX_BUFFER_COUNT];
		CoalescedMesh* string_mesh;
		VertexBuffer circle_buffer;
		VertexShader vertex_shaders[ECS_DEBUG_SHADER_COUNT][ECS_DEBUG_SHADER_OUTPUT_COUNT];
		PixelShader pixel_shaders[ECS_DEBUG_SHADER_COUNT][ECS_DEBUG_SHADER_OUTPUT_COUNT];
		InputLayout layout_shaders[ECS_DEBUG_SHADER_COUNT][ECS_DEBUG_SHADER_OUTPUT_COUNT];
		Matrix camera_matrix;
		float2* string_character_bounds;
	};

}