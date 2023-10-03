#include "ecspch.h"
#include "CBufferTags.h"
#include "../Utilities/Reflection/ReflectionTypes.h"
#include "../Rendering/RenderingStructures.h"
#include "../Rendering/Graphics.h"

const char* INJECT_TAGS[] = {
	STRING(ECS_INJECT_CAMERA_POSITION),
	STRING(ECS_INJECT_MVP_MATRIX),
	STRING(ECS_INJECT_OBJECT_MATRIX)
};

namespace ECSEngine {

	size_t INJECT_TAGS_DATA_SIZE[] = {
		sizeof(float3),
		sizeof(Matrix),
		sizeof(Matrix)
	};

	// ------------------------------------------------------------------------------------------------------------

	void GetConstantBufferInjectTagFieldsFromType(const Reflection::ReflectionType* type, CapacityStream<unsigned int>* fields, bool reduce_expanded_fields)
	{
		for (size_t index = 0; index < type->fields.size; index++) {
			ForEach<true>(INJECT_TAGS, [&](const char* inject_tag) {
				if (type->fields[index].Has(inject_tag)) {
					fields->AddAssert(index);
					return true;
				}
				return false;
			});
		}

		// Now reduce tags that are from expanded elements - like a matrix
		if (reduce_expanded_fields && fields->size > 1) {
			for (unsigned int index = 0; index < fields->size - 1; index++) {
				if (type->fields[fields->buffer[index]].tag == type->fields[fields->buffer[index + 1]].tag) {
					// Belongs to a compound element
					fields->Remove(index + 1);
					index--;
				}
			}
		}
	}

	// ------------------------------------------------------------------------------------------------------------

	ECS_INLINE uint2 GetConstantBufferTag(const Material* material, bool vertex_shader, Stream<char> tag) {
		return material->FindTag(tag, vertex_shader);
	}

	// ------------------------------------------------------------------------------------------------------------

	uint4 GetConstantBufferTag(const Material* material, Stream<char> tag) {
		uint2 vertex = material->FindTag(tag, true);
		if (vertex.x == -1) {
			uint2 pixel = material->FindTag(tag, false);
			return uint4(-1, -1, pixel.x, pixel.y);
		}
		return uint4(vertex.x, vertex.y, -1, -1);
	}

	// ------------------------------------------------------------------------------------------------------------

	uint2 GetConstantBufferInjectCamera(const Material* material, bool vertex_shader)
	{
		return GetConstantBufferTag(material, vertex_shader, STRING(ECS_INJECT_CAMERA_POSITION));
	}

	// ------------------------------------------------------------------------------------------------------------

	uint2 GetConstantBufferInjectMVPMatrix(const Material* material, bool vertex_shader)
	{
		return GetConstantBufferTag(material, vertex_shader, STRING(ECS_INJECT_MVP_MATRIX));
	}

	// ------------------------------------------------------------------------------------------------------------

	uint2 GetConstantBufferInjectObjectMatrix(const Material* material, bool vertex_shader)
	{
		return GetConstantBufferTag(material, vertex_shader, STRING(ECS_INJECT_OBJECT_MATRIX));
	}

	// ------------------------------------------------------------------------------------------------------------

	uint2 GetConstantBufferInjectTag(const Material* material, bool vertex_shader, ECS_CB_INJECT_TAG tag)
	{
		return GetConstantBufferTag(material, vertex_shader, INJECT_TAGS[tag]);
	}

	// ------------------------------------------------------------------------------------------------------------

	uint4 GetConstantBufferInjectCamera(const Material* material)
	{
		return GetConstantBufferTag(material, STRING(ECS_INJECT_CAMERA_POSITION));
	}

	// ------------------------------------------------------------------------------------------------------------

	uint4 GetConstantBufferInjectMVPMatrix(const Material* material)
	{
		return GetConstantBufferTag(material, STRING(ECS_INJECT_MVP_MATRIX));
	}

	// ------------------------------------------------------------------------------------------------------------

	uint4 GetConstantBufferInjectObjectMatrix(const Material* material)
	{
		return GetConstantBufferTag(material, STRING(ECS_INJECT_OBJECT_MATRIX));
	}

	// ------------------------------------------------------------------------------------------------------------

	uint4 GetConstantBufferInjectTag(const Material* material, ECS_CB_INJECT_TAG tag)
	{
		return GetConstantBufferTag(material, INJECT_TAGS[tag]);
	}

	// ------------------------------------------------------------------------------------------------------------

	void GetConstantBufferInjectedCBVertex(const Material* material, CapacityStream<MaterialInjectedCB>* injected_cbs)
	{
		GetConstantBufferInjectedCB(material, injected_cbs, true);
	}

	// ------------------------------------------------------------------------------------------------------------

	void GetConstantBufferInjectedCBPixel(const Material* material, CapacityStream<MaterialInjectedCB>* injected_cbs)
	{
		GetConstantBufferInjectedCB(material, injected_cbs, false);
	}

	// ------------------------------------------------------------------------------------------------------------

	void GetConstantBufferInjectedCB(const Material* material, CapacityStream<MaterialInjectedCB>* injected_cbs, bool vertex_shader)
	{
		for (size_t index = 0; index < ECS_CB_INJECT_TAG_COUNT; index++) {
			ECS_CB_INJECT_TAG inject_tag = (ECS_CB_INJECT_TAG)index;
			uint2 tag_values = GetConstantBufferInjectTag(material, vertex_shader, inject_tag);
			if (tag_values.x != -1) {
				injected_cbs->AddAssert({ inject_tag, (unsigned char)tag_values.x, (unsigned short)tag_values.y });
			}
		}
	}

	// ------------------------------------------------------------------------------------------------------------

	void BindConstantBufferInjectedTag(Stream<MaterialInjectedCB> injected_cbs, void* data, const void* data_to_write, size_t data_size, ECS_CB_INJECT_TAG tag) {
		size_t existing_index = FindConstantBufferInjectedTag(injected_cbs, tag);
		if (existing_index != -1) {
			void* tag_data = OffsetPointer(data, injected_cbs[existing_index].byte_offset);
			memcpy(tag_data, data_to_write, data_size);
		}
	}

	// ------------------------------------------------------------------------------------------------------------

	void BindConstantBufferInjectedCamera(Stream<MaterialInjectedCB> injected_cbs, void* data, float3 camera_position)
	{
		BindConstantBufferInjectedTag(injected_cbs, data, &camera_position, sizeof(camera_position), ECS_CB_INJECT_CAMERA_POSITION);
	}

	// ------------------------------------------------------------------------------------------------------------

	void BindConstantBufferInjectedMVPMatrix(Stream<MaterialInjectedCB> injected_cbs, void* data, const Matrix* mvp_matrix)
	{
		BindConstantBufferInjectedTag(injected_cbs, data, mvp_matrix, sizeof(*mvp_matrix), ECS_CB_INJECT_MVP_MATRIX);
	}

	// ------------------------------------------------------------------------------------------------------------

	void BindConstantBufferInjectedObjectMatrix(Stream<MaterialInjectedCB> injected_cbs, void* data, const Matrix* object_matrix)
	{
		BindConstantBufferInjectedTag(injected_cbs, data, object_matrix, sizeof(*object_matrix), ECS_CB_INJECT_OBJECT_MATRIX);
	}

	// ------------------------------------------------------------------------------------------------------------

	void BindConstantBufferInjectedCB(
		Graphics* graphics, 
		Material* material, 
		Stream<MaterialInjectedCB> injected_cbs,
		bool vertex_shader,
		const void** pending_values
	)
	{
		ECS_STACK_CAPACITY_STREAM_DYNAMIC(void*, mapped_buffers, injected_cbs.size);
		MapConstantBufferInjectedTags(graphics, material, injected_cbs, vertex_shader, mapped_buffers.buffer);

		for (size_t index = 0; index < injected_cbs.size; index++) {
			ECS_CB_INJECT_TAG tag = injected_cbs[index].type;
			BindConstantBufferInjectedTag(injected_cbs, mapped_buffers[index], pending_values[tag], INJECT_TAGS_DATA_SIZE[tag], tag);
		}

		UnmapConstantbufferInjectedTags(graphics, material, injected_cbs, vertex_shader);
	}

	// ------------------------------------------------------------------------------------------------------------

	void BindConstantBufferInjectedCB(Graphics* graphics, Material* material, const void** pending_values)
	{
		ECS_STACK_CAPACITY_STREAM(MaterialInjectedCB, injected_cbs, 64);
		GetConstantBufferInjectedCB(material, &injected_cbs, true);
		BindConstantBufferInjectedCB(graphics, material, injected_cbs, true, pending_values);

		injected_cbs.size = 0;
		GetConstantBufferInjectedCB(material, &injected_cbs, false);
		BindConstantBufferInjectedCB(graphics, material, injected_cbs, false, pending_values);
	}

	// ------------------------------------------------------------------------------------------------------------

	void MapConstantBufferInjectedTags(
		Graphics* graphics, 
		const Material* material, 
		Stream<MaterialInjectedCB> injected_cbs,
		bool vertex_shader,
		void** mapped_buffers
	)
	{
		const ConstantBuffer* buffers = vertex_shader ? material->v_buffers : material->p_buffers;
		for (size_t index = 0; index < injected_cbs.size; index++) {
			// Check to see if this buffer was already mapped
			size_t subindex = 0;
			for (; subindex < index; subindex++) {
				if (injected_cbs[index].buffer_index == injected_cbs[subindex].buffer_index) {
					break;
				}
			}
			if (subindex == index) {
				ConstantBuffer buffer = buffers[injected_cbs[index].buffer_index];
				mapped_buffers[index] = graphics->MapBuffer(buffer.buffer);
			}
			else {
				mapped_buffers[index] = mapped_buffers[subindex];
			}
		}
	}

	// ------------------------------------------------------------------------------------------------------------

	void UnmapConstantbufferInjectedTags(Graphics* graphics, const Material* material, Stream<MaterialInjectedCB> injected_cbs, bool vertex_shader)
	{
		const ConstantBuffer* buffers = vertex_shader ? material->v_buffers : material->p_buffers;
		for (size_t index = 0; index < injected_cbs.size; index++) {
			// Check to see if this buffer was already unmapped
			size_t subindex = 0;
			for (; subindex < index; subindex++) {
				if (injected_cbs[index].buffer_index == injected_cbs[subindex].buffer_index) {
					break;
				}
			}
			if (subindex == index) {
				ConstantBuffer buffer = buffers[injected_cbs[index].buffer_index];
				graphics->UnmapBuffer(buffer.buffer);
			}
			// Else nothing needs to be done
		}
	}

	// ------------------------------------------------------------------------------------------------------------

}
