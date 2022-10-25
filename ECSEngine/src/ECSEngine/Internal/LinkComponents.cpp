#include "ecspch.h"
#include "LinkComponents.h"
#include "../Utilities/Reflection/Reflection.h"

namespace ECSEngine {

	// ------------------------------------------------------------------------------------------------------------

	void GetReflectionAssetFieldsFromLinkComponent(const Reflection::ReflectionType* type, CapacityStream<LinkComponentAssetField>& field_indices)
	{
		for (size_t index = 0; index < type->fields.size; index++) {

		}
	}

	// ------------------------------------------------------------------------------------------------------------

	AssetTargetFieldFromReflection GetAssetTargetFieldFromReflection(
		const Reflection::ReflectionType* type,
		unsigned int field,
		const void* data
	)
	{
		AssetTargetFieldFromReflection result;

		auto get_pointer_from_field = [data](const Reflection::ReflectionFieldInfo* info, const void** pointer) {
			if (info->stream_type == Reflection::ReflectionStreamFieldType::Pointer) {
				unsigned int indirection = Reflection::GetReflectionFieldPointerIndirection(*info);
				if (indirection > 1) {
					// Fail
					return false;
				}
				*pointer = *(void**)function::OffsetPointer(data, info->pointer_offset);
			}
			else if (info->stream_type == Reflection::ReflectionStreamFieldType::Basic) {
				*pointer = (void*)function::OffsetPointer(data, info->pointer_offset);
			}
			else {
				// Fail
				return false;
			}
			return true;
		};

		const void* pointer = nullptr;
		size_t pointer_size = 0;
		ECS_ASSET_TYPE asset_type = ECS_ASSET_TYPE_COUNT;

		// 2 means there is no match. It will be set to 1 if there is a match with a succesful
		// type or 0 if there is match but incorrect field type
		bool success = true;

		Stream<char> definition = type->fields[field].definition;
		const Reflection::ReflectionFieldInfo* info = &type->fields[field].info;
		
		size_t mapping_count = ECS_ASSET_TARGET_FIELD_NAMES_SIZE();
		for (size_t index = 0; index < mapping_count; index++) {
			if (function::CompareStrings(definition, ECS_ASSET_TARGET_FIELD_NAMES[index].name)) {
				if (data != nullptr) {
					success = get_pointer_from_field(info, &pointer);
				}
				else {
					success = true;
				}
				asset_type = ECS_ASSET_TARGET_FIELD_NAMES[index].asset_type;
				break;
			}
		}
		if (asset_type == ECS_ASSET_TEXTURE || asset_type == ECS_ASSET_GPU_SAMPLER || asset_type == ECS_ASSET_SHADER) {
			// The interface is one level deeper
			if (success) {
				pointer = *(const void**)pointer;
			}
		}
		else if (asset_type == ECS_ASSET_MISC) {
			if (success) {
				// Extract the size and the pointer is deeper
				pointer_size = *(size_t*)function::OffsetPointer(pointer, sizeof(void*));
				pointer = *(void**)pointer;
			}
		}

		if (success) {
			result.asset = { pointer, pointer_size };
			result.success = true;
		}
		else {
			result.success = false;
		}

		result.type = asset_type;
		return result;
	}

	// ------------------------------------------------------------------------------------------------------------

	ECS_SET_ASSET_TARGET_FIELD_RESULT SetAssetTargetFieldFromReflection(
		const Reflection::ReflectionType* type,
		unsigned int field,
		void* data,
		Stream<void> field_data,
		ECS_ASSET_TYPE field_type
	)
	{
		ECS_ASSET_TYPE current_field_type = FindAssetMetadataMacro(type->fields[field].definition);
		if (field_type != current_field_type) {
			return ECS_SET_ASSET_TARGET_FIELD_NONE;
		}

		auto copy_value = [&](void* ptr_to_copy, size_t copy_size) {
			memcpy(function::OffsetPointer(data, type->fields[field].info.pointer_offset), ptr_to_copy, copy_size);
		};

		auto set_pointer = [&](void* ptr_to_set) {
			void** ptr = (void**)function::OffsetPointer(data, type->fields[field].info.pointer_offset);
			*ptr = ptr_to_set;
		};

		auto copy_and_pointer = [&](size_t copy_size) {
			if (type->fields[field].info.stream_type == Reflection::ReflectionStreamFieldType::Basic) {
				// Copy the data
				copy_value(field_data.buffer, copy_size);
			}
			else if (type->fields[field].info.stream_type == Reflection::ReflectionStreamFieldType::Pointer) {
				if (Reflection::GetReflectionFieldPointerIndirection(type->fields[field].info) == 1) {
					set_pointer(field_data.buffer);
				}
				else {
					return ECS_SET_ASSET_TARGET_FIELD_FAILED;
				}
			}
			else {
				return ECS_SET_ASSET_TARGET_FIELD_FAILED;
			}
			return ECS_SET_ASSET_TARGET_FIELD_OK;
		};

		// Same type
		switch (field_type) {
		case ECS_ASSET_MESH:
		{
			return copy_and_pointer(sizeof(CoallescedMesh));
		}
		break;
		case ECS_ASSET_TEXTURE:
		case ECS_ASSET_GPU_SAMPLER:
		case ECS_ASSET_SHADER:
		{
			if (type->fields[field].info.stream_type == Reflection::ReflectionStreamFieldType::Basic) {
				set_pointer(field_data.buffer);
			}
			else {
				return ECS_SET_ASSET_TARGET_FIELD_FAILED;
			}
		}
		break;
		case ECS_ASSET_MATERIAL:
		{
			return copy_and_pointer(sizeof(Material));
		}
		break;
		case ECS_ASSET_MISC:
		{
			if (type->fields[field].info.stream_type == Reflection::ReflectionStreamFieldType::Basic) {
				// Copy the data
				copy_value(field_data.buffer, field_data.size);
			}
			else if (type->fields[field].info.stream_type == Reflection::ReflectionStreamFieldType::Pointer) {
				if (Reflection::GetReflectionFieldPointerIndirection(type->fields[field].info) == 1) {
					set_pointer(&field_data);
				}
				else {
					return ECS_SET_ASSET_TARGET_FIELD_FAILED;
				}
			}
			else {
				return ECS_SET_ASSET_TARGET_FIELD_FAILED;
			}
		}
		break;
		}

		return ECS_SET_ASSET_TARGET_FIELD_OK;
	}

	// ------------------------------------------------------------------------------------------------------------

}