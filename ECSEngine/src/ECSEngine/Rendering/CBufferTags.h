#pragma once
#include "Shaders/CBufferTags.hlsli"
#include "../Core.h"
#include "../Containers/Stream.h"
#include "../Utilities/BasicTypes.h"
#include "../Math/Matrix.h"

namespace ECSEngine {

	// This is mirrored in CBufferTags.cpp by INJECT_TAGS
	enum ECS_CB_INJECT_TAG : unsigned char {
		ECS_CB_INJECT_CAMERA_POSITION,
		ECS_CB_INJECT_MVP_MATRIX,
		ECS_CB_INJECT_OBJECT_MATRIX,
		ECS_CB_INJECT_TAG_COUNT
	};

	struct MaterialInjectedCB {
		ECS_CB_INJECT_TAG type;
		unsigned char buffer_index;
		unsigned short byte_offset;
	};

	namespace Reflection {
		struct ReflectionType;
	}

	struct Material;
	struct Graphics;

	// This will retrieve the fields from the reflection type field tags
	// The reduce_expanded_fields is used to remove fields that come from expanded fields (like rows of a matrix,
	// if this is set to true, it will only return the first row, else all rows)
	ECSENGINE_API void GetConstantBufferInjectTagFieldsFromType(
		const Reflection::ReflectionType* type, 
		CapacityStream<unsigned int>* fields, 
		bool reduce_expanded_fields
	);

	// Returns { buffer_index, byte_offset } if it finds the inject tag, else { -1, -1 }
	ECSENGINE_API uint2 GetConstantBufferInjectCamera(const Material* material, bool vertex_shader);

	// Returns { buffer_index, byte_offset } if it finds the inject tag, else { -1, -1 }
	ECSENGINE_API uint2 GetConstantBufferInjectMVPMatrix(const Material* material, bool vertex_shader);

	// Returns { buffer_index, byte_offset } if it finds the inject tag, else { -1, -1 }
	ECSENGINE_API uint2 GetConstantBufferInjectObjectMatrix(const Material* material, bool vertex_shader);

	// Returns { buffer_index, byte_offset } if it finds the inject tag, else { -1, -1 }
	ECSENGINE_API uint2 GetConstantBufferInjectTag(const Material* material, bool vertex_shader, ECS_CB_INJECT_TAG tag);

	// Returns in the x, y the result for vertex shader and in the z, w the pixel shader result
	ECSENGINE_API uint4 GetConstantBufferInjectCamera(const Material* material);

	// Returns in the x, y the result for vertex shader and in the z, w the pixel shader result
	ECSENGINE_API uint4 GetConstantBufferInjectMVPMatrix(const Material* material);

	// Returns in the x, y the result for vertex shader and in the z, w the pixel shader result
	ECSENGINE_API uint4 GetConstantBufferInjectObjectMatrix(const Material* material);

	// Returns in the x, y the result for vertex shader and in the z, w the pixel shader result
	ECSENGINE_API uint4 GetConstantBufferInjectTag(const Material* material, ECS_CB_INJECT_TAG tag);

	// Fills in the stream with the buffer indices and their respective types that have injected fields
	ECSENGINE_API void GetConstantBufferInjectedCBVertex(const Material* material, CapacityStream<MaterialInjectedCB>* injected_cbs);

	// Fills in the stream with the buffer indices and their respective types that have injected fields
	ECSENGINE_API void GetConstantBufferInjectedCBPixel(const Material* material, CapacityStream<MaterialInjectedCB>* injected_cbs);

	// Fills in the stream with the buffer indices and their respective types that have injected fields
	ECSENGINE_API void GetConstantBufferInjectedCB(const Material* material, CapacityStream<MaterialInjectedCB>* injected_cbs, bool vertex_shader);

	// Returns the index inside the injected_cbs where the tag was found, if it is missing it returns -1
	ECS_INLINE size_t FindConstantBufferInjectedTag(Stream<MaterialInjectedCB> injected_cbs, ECS_CB_INJECT_TAG tag) {
		return injected_cbs.Find(tag, [](MaterialInjectedCB cb) {
			return cb.type;
		});
	}

	ECSENGINE_API void BindConstantBufferInjectedCamera(Stream<MaterialInjectedCB> injected_cbs, void* data, float3 camera_position);

	ECSENGINE_API void BindConstantBufferInjectedMVPMatrix(Stream<MaterialInjectedCB> injected_cbs, void* data, const Matrix* mvp_matrix);

	ECSENGINE_API void BindConstantBufferInjectedObjectMatrix(Stream<MaterialInjectedCB> injected_cbs, void* data, const Matrix* object_matrix);

	ECSENGINE_API void BindConstantBufferInjectedTag(
		Stream<MaterialInjectedCB> injected_cbs, 
		void* data, 
		const void* data_to_write, 
		size_t data_size, 
		ECS_CB_INJECT_TAG tag
	);

	// It uses the immediate context to map the constant buffers
	// Pending values needs to be an array with all the possible inject values
	// The entries should be populated using ECS_CB_INJECT_TAG
	ECSENGINE_API void BindConstantBufferInjectedCB(
		Graphics* graphics, 
		Material* material,
		Stream<MaterialInjectedCB> injected_cbs,
		bool vertex_shader,
		const void** pending_values
	);

	// It uses the immediate context to map the constant buffers
	// Pending values needs to be an array with all the possible inject values
	// The entries should be populated using ECS_CB_INJECT_TAG
	ECSENGINE_API void BindConstantBufferInjectedCB(
		Graphics* graphics,
		Material* material,
		const void** pending_values
	);

	// It uses the immediate context to map the constant buffers
	// There needs to be injected_cbs.size elements in mapped_buffers
	ECSENGINE_API void MapConstantBufferInjectedTags(
		Graphics* graphics,
		const Material* material,
		Stream<MaterialInjectedCB> injected_cbs,
		bool vertex_shader,
		void** mapped_buffers
	);

	// It uses the immediate context to map the constant buffers
	ECSENGINE_API void UnmapConstantbufferInjectedTags(
		Graphics* graphics,
		const Material* material,
		Stream<MaterialInjectedCB> injected_cbs,
		bool vertex_shader
	);

}