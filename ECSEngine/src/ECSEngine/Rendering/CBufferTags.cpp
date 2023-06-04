#include "ecspch.h"
#include "CBufferTags.h"
#include "../Utilities/Reflection/ReflectionTypes.h"
#include "../Utilities/Function.h"
#include "../Utilities/FunctionInterfaces.h"
#include "../Rendering/RenderingStructures.h"

const char* INJECT_TAGS[] = {
	STRING(ECS_INJECT_CAMERA_POSITION),
	STRING(ECS_INJECT_MVP_MATRIX),
	STRING(ECS_INJECT_OBJECT_MATRIX)
};

namespace ECSEngine {

	// ------------------------------------------------------------------------------------------------------------

	void GetConstantBufferInjectTagFieldsFromType(const Reflection::ReflectionType* type, CapacityStream<unsigned int>* fields, bool reduce_expanded_fields)
	{
		for (size_t index = 0; index < type->fields.size; index++) {
			function::ForEach<true>(INJECT_TAGS, [&](const char* inject_tag) {
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
					fields->RemoveSwapBack(index + 1);
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

}
