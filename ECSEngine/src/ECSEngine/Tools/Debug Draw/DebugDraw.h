#pragma once
#include "../../Core.h"
#include "../../Rendering/Graphics.h"
#include "../../Allocators/MemoryManager.h"
#include "../../Math/Matrix.h"
#include "../../Math/Quaternion.h"
#include "../../Containers/Deck.h"
#include "../../Multithreading/ConcurrentPrimitives.h"
#include "../../Rendering/RenderingStructures.h"
#include "DebugDrawTypes.h"

namespace ECSEngine {

#define ECS_DEBUG_DRAWER_STRUCTURED_OUTPUT_ID_THICKNESS 3

	struct ResourceManager;

	typedef float3 DebugVertex;

	ECS_INLINE Color AxisXColor() {
		return Color(255, 0, 0);
	}

	ECS_INLINE Color AxisYColor() {
		return Color(0, 255, 0);
	}

	ECS_INLINE Color AxisZColor() {
		return Color(0, 64, 255);
	}

	// If you don't want thickness, just assign the instance index directly
	struct DebugDrawOptions {
		bool wireframe = true;
		bool ignore_depth = false;
		float duration = 0.0f;
		unsigned int instance_thickness = (unsigned int)-1;
	};

	struct DebugLine {
		float3 start;
		float3 end;
		Color color;
		DebugDrawOptions options;
	};

	struct ECSENGINE_API DebugSphere {
		Matrix ECS_VECTORCALL GetMatrix(Matrix camera_matrix) const;

		float3 position;
		float radius;
		Color color;
		DebugDrawOptions options;
	};

	struct ECSENGINE_API DebugPoint {
		Matrix ECS_VECTORCALL GetMatrix(Matrix camera_matrix) const;
		
		float3 position;
		Color color;
		DebugDrawOptions options;
	};

	struct DebugRectangle {
		float3 corner0;
		float3 corner1;
		Color color;
		DebugDrawOptions options;
	};

	struct ECSENGINE_API DebugCross {
		Matrix ECS_VECTORCALL GetMatrix(Matrix camera_matrix) const;

		float3 position;
		QuaternionScalar rotation;
		float size;
		Color color;
		DebugDrawOptions options;
	};

	struct ECSENGINE_API DebugCircle {
		Matrix ECS_VECTORCALL GetMatrix(Matrix camera_matrix) const;

		float3 position;
		QuaternionScalar rotation;
		float radius;
		Color color;
		DebugDrawOptions options;
	};

	struct ECSENGINE_API DebugArrow {
		Matrix ECS_VECTORCALL GetMatrix(Matrix camera_matrix) const;

		float3 translation;
		QuaternionScalar rotation;
		float length;
		float size;
		Color color;
		DebugDrawOptions options;
	};

	struct DebugTriangle {
		float3 point0;
		float3 point1;
		float3 point2;
		Color color;
		DebugDrawOptions options;
	};

	struct ECSENGINE_API DebugAABB {
		Matrix ECS_VECTORCALL GetMatrix(Matrix camera_matrix) const;

		float3 translation;
		float3 scale;
		Color color;
		DebugDrawOptions options;
	};

	struct ECSENGINE_API DebugOOBB {
		Matrix ECS_VECTORCALL GetMatrix(Matrix camera_matrix) const;

		float3 translation;
		QuaternionScalar rotation;
		float3 scale;
		Color color;
		DebugDrawOptions options;
	};

	struct DebugString {
		float3 position;
		float3 direction;
		float size;
		Stream<char> text;
		Color color;
		DebugDrawOptions options;
	};

	// If you don't want thickness, just assign the index directly
	struct DebugAxesInfo {
		Color color_x = AxisXColor();
		Color color_y = AxisYColor();
		Color color_z = AxisZColor();
		unsigned int instance_thickness_x = (unsigned int)-1;
		unsigned int instance_thickness_y = (unsigned int)-1;
		unsigned int instance_thickness_z = (unsigned int)-1;
	};

	struct DebugOOBBCrossInfo {
		ECS_INLINE Color* GetColors() {
			return &color_x;
		}

		ECS_INLINE const Color* GetColors() const {
			return &color_x;
		}

		ECS_INLINE unsigned int* GetInstances() {
			return &instance_thickness_x;
		}

		ECS_INLINE const unsigned int* GetInstances() const {
			return &instance_thickness_x;
		}

		ECS_INLINE float* GetSizeOverrides() {
			return &size_override_x;
		}

		ECS_INLINE const float* GetSizeOverrides() const {
			return &size_override_x;
		}

		Color color_x = AxisXColor();
		Color color_y = AxisYColor();
		Color color_z = AxisZColor();

		unsigned int instance_thickness_x = (unsigned int)-1;
		unsigned int instance_thickness_y = (unsigned int)-1;
		unsigned int instance_thickness_z = (unsigned int)-1;

		// You can override the size for a specific axis
		float size_override_x = 0.0f;
		float size_override_y = 0.0f;
		float size_override_z = 0.0f;
	};

	typedef bool (*DrawDebugGridResidencyFunction)(uint3 index, void* data);

	struct ECSENGINE_API DebugGrid {
		// It will extract the resident cells now from the grid using the residency function
		void ExtractResidentCells(AllocatorPolymorphic allocator);

		uint3 dimensions;
		float3 cell_size;
		float3 translation;
		Color color;
		DebugDrawOptions options = {};
		// The residency function is used to iterate through all cells and draw
		// Only those that are considered to be resident
		DrawDebugGridResidencyFunction residency_function = nullptr;
		void* residency_data = nullptr;
		// You must set has_valid_cells to true in order for this to be considered
		DeckPowerOfTwo<uint3> valid_cells = {};
		bool has_valid_cells = false;
	};

	enum ECS_DEBUG_DRAWER_CALL_TYPE : unsigned char {
		ECS_DEBUG_DRAWER_CALL_IMMEDIATE,
		ECS_DEBUG_DRAWER_CALL_DEFERRED,
		ECS_DEBUG_DRAWER_CALL_DEFERRED_THREAD,
		ECS_DEBUG_DRAWER_CALL_TYPE_COUNT
	};

	struct DebugDrawCallType {
		ECS_DEBUG_DRAWER_CALL_TYPE type = ECS_DEBUG_DRAWER_CALL_IMMEDIATE;
		unsigned int thread_index = (unsigned int)-1;
	};

	struct ECSENGINE_API DebugDrawer {
		ECS_INLINE DebugDrawer() : allocator(nullptr), graphics(nullptr) {}
		DebugDrawer(MemoryManager* allocator, ResourceManager* manager, size_t thread_count);

		DebugDrawer(const DebugDrawer& other) = default;
		DebugDrawer& operator = (const DebugDrawer& other) = default;

#pragma region Add to the draw queue - single threaded

		void AddLine(float3 start, float3 end, Color color, DebugDrawOptions options = {});

		void AddLine(float3 translation, QuaternionScalar rotation, float size, Color color, DebugDrawOptions options = {});

		void AddSphere(float3 position, float radius, Color color, DebugDrawOptions options = {});

		void AddPoint(float3 position, float size, Color color, DebugDrawOptions options = {false});

		// Corner0 is the top left corner, corner1 is the bottom right corner
		void AddRectangle(float3 corner0, float3 corner1, Color color, DebugDrawOptions options = {});

		void AddCross(float3 position, QuaternionScalar rotation, float size, Color color, DebugDrawOptions options = {false});

		// If the start_from_same_point is set to true, it will mimick the Axes call, where the
		// arrows start from the same point. Else, it will draw like the normal cross
		void AddOOBBCross(
			float3 position, 
			QuaternionScalar rotation, 
			float length,
			float size, 
			bool start_from_same_point, 
			const DebugOOBBCrossInfo* info = {},
			DebugDrawOptions options = { false }
		);

		void AddCircle(float3 position, QuaternionScalar rotation, float radius, Color color, DebugDrawOptions options = {});

		void AddArrow(float3 start, float3 end, float size, Color color, DebugDrawOptions options = {false});
		
		void AddArrowRotation(float3 translation, QuaternionScalar rotation, float length, float size, Color color, DebugDrawOptions options = {false});

		void AddAxes(
			float3 translation, 
			QuaternionScalar rotation, 
			float size, 
			const DebugAxesInfo* info = {},
			DebugDrawOptions options = {false}
		);

		void AddTriangle(float3 point0, float3 point1, float3 point2, Color color, DebugDrawOptions options = {});

		void AddAABB(float3 translation, float3 scale, Color color, DebugDrawOptions options = {});

		void AddOOBB(float3 translation, QuaternionScalar rotation, float3 scale, Color color, DebugDrawOptions options = {});

		void AddString(float3 position, float3 direction, float size, Stream<char> text, Color color, DebugDrawOptions options = { false });

		void AddStringRotation(float3 position, QuaternionScalar rotation, float size, Stream<char> text, Color color, DebugDrawOptions options = { false });

		// If the retrieve entries now is set, it will get all the resident cells now
		// Instead of delaying the deduction and using the function
		void AddGrid(const DebugGrid* grid, bool retrieve_entries_now = false);

#pragma endregion

#pragma region Add to the draw queue - multi threaded

		void AddLineThread(unsigned int thread_index, float3 start, float3 end, Color color, DebugDrawOptions options = {});

		void AddLineThread(unsigned int thread_index, float3 translation, QuaternionScalar rotation, float size, Color color, DebugDrawOptions options = {});

		void AddSphereThread(unsigned int thread_index, float3 position, float radius, Color color, DebugDrawOptions options = {});

		void AddPointThread(unsigned int thread_index, float3 position, float size, Color color, DebugDrawOptions options = {false});

		// Corner0 is the top left corner, corner1 is the bottom right corner
		void AddRectangleThread(unsigned int thread_index, float3 corner0, float3 corner1, Color color, DebugDrawOptions options = {});

		void AddCrossThread(unsigned int thread_index, float3 position, QuaternionScalar rotation, float size, Color color, DebugDrawOptions options = {false});

		void AddOOBBCrossThread(
			unsigned int thread_index,
			float3 position,
			QuaternionScalar rotation,
			float length,
			float size,
			bool start_from_same_point,
			const DebugOOBBCrossInfo* info = {},
			DebugDrawOptions options = { false }
		);

		void AddCircleThread(unsigned int thread_index, float3 position, QuaternionScalar rotation, float radius, Color color, DebugDrawOptions options = {});

		void AddArrowThread(unsigned int thread_index, float3 start, float3 end, float size, Color color, DebugDrawOptions options = {false});

		void AddArrowRotationThread(unsigned int thread_index, float3 translation, QuaternionScalar rotation, float length, float size, Color color, DebugDrawOptions options = {false});

		void AddAxesThread(
			unsigned int thread_index, 
			float3 translation, 
			QuaternionScalar rotation, 
			float size, 
			const DebugAxesInfo* info = {},
			DebugDrawOptions options = {false}
		);

		void AddTriangleThread(unsigned int thread_index, float3 point0, float3 point1, float3 point2, Color color, DebugDrawOptions options = {});

		void AddAABBThread(unsigned int thread_index, float3 translation, float3 scale, Color color, DebugDrawOptions options = {});

		void AddOOBBThread(unsigned int thread_index, float3 translation, QuaternionScalar rotation, float3 scale, Color color, DebugDrawOptions options = {});

		// It must do an allocation from the memory manager under lock - possible expensive operation
		void AddStringThread(
			unsigned int thread_index,
			float3 position, 
			float3 direction,
			float size,
			Stream<char> text,
			Color color, 
			DebugDrawOptions options = { false }
		);

		// It must do an allocation from the memory manager under lock - possible expensive operation
		void AddStringRotationThread(
			unsigned int thread_index,
			float3 position,
			QuaternionScalar rotation,
			float size,
			Stream<char> text,
			Color color,
			DebugDrawOptions options = { false }
		);

		// If the retrieve entries now is set, it will get all the resident cells now
		// Instead of delaying the deduction and using the function
		void AddGridThread(unsigned int thread_index, const DebugGrid* grid, bool retrieve_entries_now = false);

#pragma endregion

#pragma region Dynamic Type Calls

		void CallLine(DebugDrawCallType call_type, float3 start, float3 end, Color color, DebugDrawOptions options = {});

		void CallLine(DebugDrawCallType call_type, float3 translation, QuaternionScalar rotation, float size, Color color, DebugDrawOptions options = {});

		void CallSphere(DebugDrawCallType call_type, float3 position, float radius, Color color, DebugDrawOptions options = { false });

		void CallPoint(DebugDrawCallType call_type, float3 position, float size, Color color, DebugDrawOptions options = { false });

		// Corner0 is the top left corner, corner1 is the bottom right corner
		void CallRectangle(DebugDrawCallType call_type, float3 corner0, float3 corner1, Color color, DebugDrawOptions options = {});
		
		void CallCross(DebugDrawCallType call_type, float3 position, QuaternionScalar rotation, float size, Color color, DebugDrawOptions options = { false });

		void CallOOBBCross(
			DebugDrawCallType call_type,
			float3 position,
			QuaternionScalar rotation,
			float length,
			float size,
			bool start_from_same_point,
			const DebugOOBBCrossInfo* info = {},
			DebugDrawOptions options = { false }
		);

		void CallCircle(DebugDrawCallType call_type, float3 position, QuaternionScalar rotation, float radius, Color color, DebugDrawOptions options = {});

		void CallArrow(DebugDrawCallType call_type, float3 start, float3 end, float size, Color color, DebugDrawOptions options = { false });

		// Rotation expressed as radians
		void CallArrowRotation(
			DebugDrawCallType call_type, 
			float3 translation, 
			QuaternionScalar rotation, 
			float length, 
			float size, 
			Color color, 
			DebugDrawOptions options = { false }
		);

		void CallAxes(
			DebugDrawCallType call_type,
			float3 translation,
			QuaternionScalar rotation,
			float size,
			const DebugAxesInfo* info = {},
			DebugDrawOptions options = { false }
		);

		void CallTriangle(DebugDrawCallType call_type, float3 point0, float3 point1, float3 point2, Color color, DebugDrawOptions options = {});

		void CallAABB(DebugDrawCallType call_type, float3 translation, float3 scale, Color color, DebugDrawOptions options = {});

		void CallOOBB(DebugDrawCallType call_type, float3 translation, QuaternionScalar rotation, float3 scale, Color color, DebugDrawOptions options = {});

		// Text rotation is the rotation alongside the X axis - rotates the text in order to be seen from below, above, or at a specified angle
		void CallString(
			DebugDrawCallType call_type,
			float3 translation,
			float3 direction,
			float size,
			Stream<char> text,
			Color color,
			DebugDrawOptions options = { false }
		);

		// Text rotation is the rotation alongside the X axis - rotates the text in order to be seen from below, above, or at a specified angle
		// The direction is specified as rotation in angles as degrees
		void CallStringRotation(
			DebugDrawCallType call_type,
			float3 translation,
			QuaternionScalar rotation,
			float size,
			Stream<char> text,
			Color color,
			DebugDrawOptions options = { false }
		);

#pragma endregion

#pragma region Dispatch Type Calls

		ECS_INLINE void DispatchLine(float3 start, float3 end, Color color, DebugDrawOptions options = {}) {
			CallLine(dispatch_call_type, start, end, color, options);
		}

		ECS_INLINE void DispatchLine(float3 translation, QuaternionScalar rotation, float size, Color color, DebugDrawOptions options = {}) {
			CallLine(dispatch_call_type, translation, rotation, size, color, options);
		}

		ECS_INLINE void DispatchSphere(float3 position, float radius, Color color, DebugDrawOptions options = { false }) {
			CallSphere(dispatch_call_type, position, radius, color, options);
		}

		ECS_INLINE void DispatchPoint(float3 position, float size, Color color, DebugDrawOptions options = { false }) {
			CallPoint(dispatch_call_type, position, size, color, options);
		}

		// Corner0 is the top left corner, corner1 is the bottom right corner
		ECS_INLINE void DispatchRectangle(float3 corner0, float3 corner1, Color color, DebugDrawOptions options = {}) {
			CallRectangle(dispatch_call_type, corner0, corner1, color, options);
		}

		ECS_INLINE void DispatchCross(float3 position, QuaternionScalar rotation, float size, Color color, DebugDrawOptions options = { false }) {
			CallCross(dispatch_call_type, position, rotation, size, color, options);
		}

		ECS_INLINE void DispatchOOBBCross(
			float3 position,
			QuaternionScalar rotation,
			float length,
			float size,
			bool start_from_same_point,
			const DebugOOBBCrossInfo* info = {},
			DebugDrawOptions options = { false }
		) {
			CallOOBBCross(dispatch_call_type, position, rotation, length, size, start_from_same_point, info, options);
		}

		ECS_INLINE void DispatchCircle(float3 position, QuaternionScalar rotation, float radius, Color color, DebugDrawOptions options = {}) {
			CallCircle(dispatch_call_type, position, rotation, radius, color, options);
		}

		ECS_INLINE void DispatchArrow(float3 start, float3 end, float size, Color color, DebugDrawOptions options = { false }) {
			CallArrow(dispatch_call_type, start, end, size, color, options);
		}

		// Rotation expressed as radians
		ECS_INLINE void DispatchArrowRotation(
			float3 translation,
			QuaternionScalar rotation,
			float length,
			float size,
			Color color,
			DebugDrawOptions options = { false }
		) {
			CallArrowRotation(dispatch_call_type, translation, rotation, length, size, color, options);
		}

		ECS_INLINE void DispatchAxes(
			float3 translation,
			QuaternionScalar rotation,
			float size,
			const DebugAxesInfo* info = {},
			DebugDrawOptions options = { false }
		) {
			CallAxes(dispatch_call_type, translation, rotation, size, info, options);
		}

		ECS_INLINE void DispatchTriangle(float3 point0, float3 point1, float3 point2, Color color, DebugDrawOptions options = {}) {
			CallTriangle(dispatch_call_type, point0, point1, point2, color, options);
		}

		ECS_INLINE void DispatchAABB(float3 translation, float3 scale, Color color, DebugDrawOptions options = {}) {
			CallAABB(dispatch_call_type, translation, scale, color, options);
		}

		ECS_INLINE void DispatchOOBB(float3 translation, QuaternionScalar rotation, float3 scale, Color color, DebugDrawOptions options = {}) {
			CallOOBB(dispatch_call_type, translation, rotation, scale, color, options);
		}

		// Text rotation is the rotation alongside the X axis - rotates the text in order to be seen from below, above, or at a specified angle
		ECS_INLINE void DispatchString(
			float3 translation,
			float3 direction,
			float size,
			Stream<char> text,
			Color color,
			DebugDrawOptions options = { false }
		) {
			CallString(dispatch_call_type, translation, direction, size, text, color, options);
		}

		// Text rotation is the rotation alongside the X axis - rotates the text in order to be seen from below, above, or at a specified angle
		// The direction is specified as rotation in angles as degrees
		ECS_INLINE void DispatchStringRotation(
			float3 translation,
			QuaternionScalar rotation,
			float size,
			Stream<char> text,
			Color color,
			DebugDrawOptions options = { false }
		) {
			CallStringRotation(dispatch_call_type, translation, rotation, size, text, color, options);
		}

#pragma endregion

#pragma region Draw immediately

		void DrawLine(float3 start, float3 end, Color color, DebugDrawOptions options = {});

		void DrawLine(float3 translation, QuaternionScalar rotation, float size, Color color, DebugDrawOptions options = {});

		void DrawSphere(float3 position, float radius, Color color, DebugDrawOptions options = {false});

		void DrawPoint(float3 position, float size, Color color, DebugDrawOptions options = {false});

		// Corner0 is the top left corner, corner1 is the bottom right corner
		void DrawRectangle(float3 corner0, float3 corner1, Color color, DebugDrawOptions options = {});

		void DrawCross(float3 position, QuaternionScalar rotation, float size, Color color, DebugDrawOptions options = {false});

		void DrawOOBBCross(
			float3 position, 
			QuaternionScalar rotation,
			float length,
			float size, 
			bool start_from_same_point, 
			const DebugOOBBCrossInfo* info = {},
			DebugDrawOptions options = {false}
		);

		void DrawCircle(float3 position, QuaternionScalar rotation, float radius, Color color, DebugDrawOptions options = {});

		void DrawArrow(float3 start, float3 end, float size, Color color, DebugDrawOptions options = {false});

		// Rotation expressed as radians
		void DrawArrowRotation(float3 translation, QuaternionScalar rotation, float length, float size, Color color, DebugDrawOptions options = {false});

		void DrawAxes(
			float3 translation, 
			QuaternionScalar rotation, 
			float size, 
			const DebugAxesInfo* info = {},
			DebugDrawOptions options = {false}
		);

		void DrawTriangle(float3 point0, float3 point1, float3 point2, Color color, DebugDrawOptions options = {});

		void DrawAABB(float3 translation, float3 scale, Color color, DebugDrawOptions options = {});

		void DrawOOBB(float3 translation, QuaternionScalar rotation, float3 scale, Color color, DebugDrawOptions options = {});

		// Text rotation is the rotation alongside the X axis - rotates the text in order to be seen from below, above, or at a specified angle
		void DrawString(
			float3 translation, 
			float3 direction, 
			float size, 
			Stream<char> text,
			Color color, 
			DebugDrawOptions options = {false}
		);

		// Text rotation is the rotation alongside the X axis - rotates the text in order to be seen from below, above, or at a specified angle
		// The direction is specified as rotation in angles as degrees
		void DrawStringRotation(
			float3 translation, 
			QuaternionScalar rotation, 
			float size, 
			Stream<char> text, 
			Color color, 
			DebugDrawOptions options = {false}
		);

		// Draws the normals for an object
		void DrawNormals(
			VertexBuffer model_position, 
			VertexBuffer model_normals,
			float size, 
			Color color, 
			Matrix world_matrix,
			DebugDrawOptions options = {}
		);

		// Draws the normals for multiple objects of the same type
		void DrawNormals(
			VertexBuffer model_position,
			VertexBuffer model_normals,
			float size,
			Color color,
			Stream<Matrix> world_matrices,
			DebugDrawOptions options = {}
		);

		// Draws the tangents for an object
		void DrawTangents(
			VertexBuffer model_position,
			VertexBuffer model_tangents,
			float size,
			Color color,
			Matrix world_matrix,
			DebugDrawOptions options = {}
		);

		// Draws the tangents for multiple objects of the same type
		void DrawTangents(
			VertexBuffer model_position,
			VertexBuffer model_tangents,
			float size,
			Color color,
			Stream<Matrix> world_matrices,
			DebugDrawOptions options = {}
		);

		void DrawGrid(const DebugGrid* grid, DebugShaderOutput shader_output);

#pragma endregion

#pragma region Draw Deck elements

		// Draws all combinations - Wireframe depth, wireframe no depth, solid depth, solid no depth
		// Elements that have their duration negative after substraction with time delta
		// will be removed. You can provide a custom source instead of the deck inside this drawer
		void DrawLineDeck(float time_delta, DebugShaderOutput shader_output = ECS_DEBUG_SHADER_OUTPUT_COLOR, DeckPowerOfTwo<DebugLine>* custom_source = nullptr);

		// Draws all combinations - Wireframe depth, wireframe no depth, solid depth, solid no depth
		// Elements that have their duration negative after substraction with time delta
		// will be removed. You can provide a custom source instead of the deck inside this drawer
		void DrawSphereDeck(float time_delta, DebugShaderOutput shader_output = ECS_DEBUG_SHADER_OUTPUT_COLOR, DeckPowerOfTwo<DebugSphere>* custom_source = nullptr);

		// Draws all combinations - Wireframe depth, wireframe no depth, solid depth, solid no depth
		// Elements that have their duration negative after substraction with time delta
		// will be removed. You can provide a custom source instead of the deck inside this drawer
		void DrawPointDeck(float time_delta, DebugShaderOutput shader_output = ECS_DEBUG_SHADER_OUTPUT_COLOR, DeckPowerOfTwo<DebugPoint>* custom_source = nullptr);

		// Draws all combinations - Wireframe depth, wireframe no depth, solid depth, solid no depth
		// Elements that have their duration negative after substraction with time delta
		// will be removed. You can provide a custom source instead of the deck inside this drawer
		void DrawRectangleDeck(float time_delta, DebugShaderOutput shader_output = ECS_DEBUG_SHADER_OUTPUT_COLOR, DeckPowerOfTwo<DebugRectangle>* custom_source = nullptr);

		// Draws all combinations - Wireframe depth, wireframe no depth, solid depth, solid no depth
		// Elements that have their duration negative after substraction with time delta
		// will be removed. You can provide a custom source instead of the deck inside this drawer
		void DrawCrossDeck(float time_delta, DebugShaderOutput shader_output = ECS_DEBUG_SHADER_OUTPUT_COLOR, DeckPowerOfTwo<DebugCross>* custom_source = nullptr);

		// Draws all combinations - Wireframe depth, wireframe no depth, solid depth, solid no depth
		// Elements that have their duration negative after substraction with time delta
		// will be removed. You can provide a custom source instead of the deck inside this drawer
		void DrawCircleDeck(float time_delta, DebugShaderOutput shader_output = ECS_DEBUG_SHADER_OUTPUT_COLOR, DeckPowerOfTwo<DebugCircle>* custom_source = nullptr);

		// Draws all combinations - Wireframe depth, wireframe no depth, solid depth, solid no depth
		// Elements that have their duration negative after substraction with time delta
		// will be removed. You can provide a custom source instead of the deck inside this drawer
		void DrawArrowDeck(float time_delta, DebugShaderOutput shader_output = ECS_DEBUG_SHADER_OUTPUT_COLOR, DeckPowerOfTwo<DebugArrow>* custom_source = nullptr);

		// Draws all combinations - Wireframe depth, wireframe no depth, solid depth, solid no depth
		// Elements that have their duration negative after substraction with time delta
		// will be removed. You can provide a custom source instead of the deck inside this drawer
		void DrawTriangleDeck(float time_delta, DebugShaderOutput shader_output = ECS_DEBUG_SHADER_OUTPUT_COLOR, DeckPowerOfTwo<DebugTriangle>* custom_source = nullptr);

		// Draws all combinations - Wireframe depth, wireframe no depth, solid depth, solid no depth
		// Elements that have their duration negative after substraction with time delta
		// will be removed. You can provide a custom source instead of the deck inside this drawer
		void DrawAABBDeck(float time_delta, DebugShaderOutput shader_output = ECS_DEBUG_SHADER_OUTPUT_COLOR, DeckPowerOfTwo<DebugAABB>* custom_source = nullptr);

		// Draws all combinations - Wireframe depth, wireframe no depth, solid depth, solid no depth
		// Elements that have their duration negative after substraction with time delta
		// will be removed. You can provide a custom source instead of the deck inside this drawer
		void DrawOOBBDeck(float time_delta, DebugShaderOutput shader_output = ECS_DEBUG_SHADER_OUTPUT_COLOR, DeckPowerOfTwo<DebugOOBB>* custom_source = nullptr);

		// Draws all combinations - Wireframe depth, wireframe no depth, solid depth, solid no depth
		// Elements that have their duration negative after substraction with time delta
		// will be removed. You can provide a custom source instead of the deck inside this drawer
		void DrawStringDeck(float time_delta, DebugShaderOutput shader_output = ECS_DEBUG_SHADER_OUTPUT_COLOR, DeckPowerOfTwo<DebugString>* custom_source = nullptr);

		// Draws all combinations - Wireframe depth, wireframe no depth, solid depth, solid no depth
		// Elements that have their duration negative after substraction with time delta
		// will be removed. You can provide a custom source instead of the deck inside this drawer
		void DrawGridDeck(float time_delta, DebugShaderOutput shader_output = ECS_DEBUG_SHADER_OUTPUT_COLOR, DeckPowerOfTwo<DebugGrid>* custom_source = nullptr);

		// Draws all combinations for all types - Wireframe depth, wireframe no depth, solid depth, solid no depth
		// Elements that have their duration negative after substraction with time delta
		// will be removed
		void DrawAll(float time_delta, DebugShaderOutput shader_output = ECS_DEBUG_SHADER_OUTPUT_COLOR);

#pragma endregion

#pragma region Output Instance Index

		// These functions output into a framebuffer which is of type R32_UINT
		// the instance index of 29 bits packed with 3 bits of pixel thickness
		// For all of these functions if the addition_stream is nullptr then it
		// will draw immediately, otherwise it will add the entry to the stream

		void OutputInstanceIndexLine(
			float3 start, 
			float3 end, 
			DebugDrawOptions options,
			AdditionStreamAtomic<DebugLine>* addition_stream
		);

		void OutputInstanceIndexLine(
			float3 translation,
			QuaternionScalar rotation,
			float size,
			DebugDrawOptions options,
			AdditionStreamAtomic<DebugLine>* addition_stream
		);

		void OutputInstanceIndexSphere(
			float3 translation,
			float radius,
			DebugDrawOptions options,
			AdditionStreamAtomic<DebugSphere>* addition_stream
		);

		void OutputInstanceIndexPoint(
			float3 translation,
			DebugDrawOptions options,
			AdditionStreamAtomic<DebugPoint>* addition_stream
		);

		void OutputInstanceIndexRectangle(
			float3 corner0, 
			float3 corner1,
			DebugDrawOptions options,
			AdditionStreamAtomic<DebugRectangle>* addition_stream
		);

		void OutputInstanceIndexCross(
			float3 position, 
			QuaternionScalar rotation, 
			float size,
			DebugDrawOptions options,
			AdditionStreamAtomic<DebugCross>* addition_stream
		);

		void OutputInstanceIndexOOBBCross(
			float3 position,
			QuaternionScalar rotation,
			float length,
			float size,
			bool start_from_same_point,
			const DebugOOBBCrossInfo* info,
			DebugDrawOptions options,
			AdditionStreamAtomic<DebugOOBB>* addition_stream
		);

		void OutputInstanceIndexCircle(
			float3 position, 
			QuaternionScalar rotation, 
			float radius,
			DebugDrawOptions options,
			AdditionStreamAtomic<DebugCircle>* addition_stream
		);

		void OutputInstanceIndexArrow(
			float3 start, 
			float3 end, 
			float size,
			DebugDrawOptions options,
			AdditionStreamAtomic<DebugArrow>* addition_stream
		);

		void OutputInstanceIndexArrowRotation(
			float3 translation,
			QuaternionScalar rotation,
			float length,
			float size,
			DebugDrawOptions options,
			AdditionStreamAtomic<DebugArrow>* addition_stream
		);

		void OutputInstanceIndexAxes(
			float3 translation,
			QuaternionScalar rotation,
			float size,
			unsigned int instance_thickness_x,
			unsigned int instance_thickness_y,
			unsigned int instance_thickness_z,
			DebugDrawOptions options,
			AdditionStreamAtomic<DebugArrow>* addition_stream
		);

		void OutputInstanceIndexTriangle(
			float3 point0, 
			float3 point1, 
			float3 point2,
			DebugDrawOptions options,
			AdditionStreamAtomic<DebugTriangle>* addition_stream
		);

		void OutputInstanceIndexAABB(
			float3 translation, 
			float3 scale,
			DebugDrawOptions options,
			AdditionStreamAtomic<DebugAABB>* addition_stream
		);

		void OutputInstanceIndexOOBB(
			float3 translation, 
			QuaternionScalar rotation, 
			float3 scale,
			DebugDrawOptions options,
			AdditionStreamAtomic<DebugOOBB>* addition_stream
		);

		// The allocator will be used to allocate the string
		void OutputInstanceIndexString(
			float3 translation,
			float3 direction,
			float size,
			Stream<char> text,
			DebugDrawOptions options,
			AdditionStreamAtomic<DebugString>* addition_stream,
			AllocatorPolymorphic allocator
		);

		// The allocator will be used to allocate the string
		void OutputInstanceIndexStringRotation(
			float3 translation,
			QuaternionScalar rotation,
			float size,
			Stream<char> text,
			DebugDrawOptions options,
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
		
		void FlushGrid(unsigned int thread_index);

#pragma endregion

#pragma region Set state

		// It does not set the vertex buffers. The topology is TRIANGLELIST
		void SetTransformShaderState(DebugDrawOptions options, DebugShaderOutput output);

		// It does not set the vertex buffers. The topology is TRIANGLELIST
		void SetStructuredShaderState(DebugDrawOptions options, DebugShaderOutput output);

#pragma endregion

#pragma region Draw Calls

		void DrawCallStructured(unsigned int vertex_count, unsigned int instance_count);

		void DrawCallTransform(unsigned int index_count, unsigned int instance_count);

#pragma endregion

		// Single Threaded
		// This activates the redirect for the entire structure
		// Returns true if the debug drawer was already in the redirect mode
		// Do not use the thread redirects if this redirect was activated!
		bool ActivateRedirect();

		// Multi Threaded
		// This activates the redirect for the specified thread
		// Returns true if the debug drawer was already in the redirect mode
		// For that thread. Do not use at the same time with the global redirect!
		bool ActivateRedirectThread(unsigned int thread_index);

		// Adds all stored entries to the other drawer
		// This function is not const because it calls FlushAll internally
		void AddTo(DebugDrawer* other);

		ECS_INLINE AllocatorPolymorphic Allocator() const {
			return allocator;
		}

		ECS_INLINE AllocatorPolymorphic AllocatorTs() const {
			return AllocatorPolymorphic{ allocator }.AsMulti();
		}

		void BindShaders(unsigned int index, DebugShaderOutput output);

		void Clear();

		// It also bumps forward the buffer offset
		void CopyCharacterSubmesh(IndexBuffer index_buffer, unsigned int& buffer_offset, unsigned int alphabet_index);

		// Single Threaded
		void DeactivateRedirect(bool previously_set);

		// Multi Threaded
		void DeactivateRedirectThread(unsigned int thread_index, bool previously_set);

		// If creates with a default constructor or allocated
		void Initialize(MemoryManager* allocator, ResourceManager* resource_manager, size_t thread_count);

		// This is a special initialize, that will initialize only the memory buffers, without
		// The GPU resources. This is intended for the case where this drawer will serve
		// Only as storage for the redirections, without being used to draw anything
		void InitializeRedirect(MemoryManager* allocator, size_t thread_count);

		void UpdateCameraMatrix(Matrix new_matrix);

		GraphicsPipelineRenderState GetPreviousRenderState() const;

		void RestorePreviousRenderState(const GraphicsPipelineRenderState* state);

		ECS_INLINE void SetCallType(DebugDrawCallType call_type) {
			dispatch_call_type = call_type;
		}

		// Initializes the first parameter with memory from the second parameter
		ECS_INLINE static void DefaultAllocator(MemoryManager* allocator, GlobalMemoryManager* global_memory) {
			new (allocator) MemoryManager(DefaultAllocatorSize(), ECS_KB * 16, DefaultAllocatorSize(), global_memory);
		}

		ECS_INLINE static size_t DefaultAllocatorSize() {
			return ECS_MB * 32;
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
		DeckPowerOfTwo<DebugGrid> grids;
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
		CapacityStream<DebugGrid>* thread_grids;
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

		VertexBuffer positions_large_vertex_buffer;
		VertexBuffer instanced_large_vertex_buffer;
		VertexBuffer output_large_matrix_vertex_buffer;
		VertexBuffer output_large_id_vertex_buffer;
		StructuredBuffer output_large_instanced_structured_buffer;
		StructuredBuffer output_large_matrix_structured_buffer;
		StructuredBuffer output_large_id_structured_buffer;
		ResourceView output_large_instanced_buffer_view;
		ResourceView output_large_matrix_buffer_view;
		ResourceView output_large_id_buffer_view;

		Mesh* primitive_meshes[ECS_DEBUG_VERTEX_BUFFER_COUNT];
		CoalescedMesh* string_mesh;
		VertexBuffer circle_buffer;
		VertexShader vertex_shaders[ECS_DEBUG_SHADER_COUNT][ECS_DEBUG_SHADER_OUTPUT_COUNT];
		PixelShader pixel_shaders[ECS_DEBUG_SHADER_COUNT][ECS_DEBUG_SHADER_OUTPUT_COUNT];
		InputLayout layout_shaders[ECS_DEBUG_SHADER_COUNT][ECS_DEBUG_SHADER_OUTPUT_COUNT];
		Matrix camera_matrix;
		float2* string_character_bounds;

		// Can be set by an external source such that functions
		// That are being called can use the debug drawer without
		// Knowing the type of the call it should use. This does not
		// Work with multithreading tho, since this is a global resource
		// But for single threading, which is the intended use case, it works
		DebugDrawCallType dispatch_call_type = {};

		// Important! The redirect works only for Add* class of functions
		// Not for the Draw* ones! Also, do not perform a DrawAll call!
		// We use counts for redirects such as to not force every Add*
		// Call make a check to see if it needs to be redirected. That
		// Would pessimize the common case, where there is no redirection
		// It this way, there is no overhead when there is no redirection,
		// And the entire overhead resides in the deactivate call, which 
		// Is a small overhead

		// This is a quick fix for the case that a module outputs to the game window
		// Some elements and these are to be be replicated in the scene window
		// These counts are used to know what elements to write to the redirect drawer
		// This flag is used for the entire structure
		bool is_redirect;
		unsigned int redirect_counts[ECS_DEBUG_PRIMITIVE_COUNT];
		// This is a base pointer, all the other entries for each thread are deduced
		// Simply as thread_index * ECS_DEBUG_PRIMITVE_COUNT
		unsigned int* redirect_thread_counts;
		bool* is_redirect_thread_count;
		DebugDrawer* redirect_drawer;
	};

	// Testing method that adds some primitives of each kind to see if they are displayed correctly
	ECSENGINE_API void DebugDrawerAddToDrawShapes(DebugDrawer* drawer);

	struct FrustumPoints;
	
	// Draws immediately
	ECSENGINE_API void DrawDebugFrustum(const FrustumPoints& frustum, DebugDrawer* drawer, Color color, DebugDrawOptions options = {});

	// Adds it to the main deck
	ECSENGINE_API void AddDebugFrustum(const FrustumPoints& frustum, DebugDrawer* drawer, Color color, DebugDrawOptions options = {});

	// Adds to the per thread draws
	ECSENGINE_API void AddDebugFrustumThread(const FrustumPoints& frustum, DebugDrawer* drawer, unsigned int thread_id, Color color, DebugDrawOptions options = {});

	// Draws immediately
	// The grid is assumed to be axis aligned
	ECSENGINE_API void DrawDebugGrid(const DebugGrid* grid, DebugDrawer* drawer, Color color, DebugDrawOptions options = {});

	// Adds the calls to the main deck
	// The grid is assumed to be axis aligned
	ECSENGINE_API void AddDebugGrid(const DebugGrid* grid, DebugDrawer* drawer, Color color, DebugDrawOptions options = {});

	// Adds to the per thread draws
	// The grid is assumed to be axis aligned
	ECSENGINE_API void AddDebugGridThread(
		const DebugGrid* grid,
		DebugDrawer* drawer, 
		unsigned int thread_id, 
		Color color, 
		DebugDrawOptions options = {}
	);
	
}