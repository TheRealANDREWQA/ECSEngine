#include "ecspch.h"
#include "ReflectionAllocatorHandling.h"
#include "ReflectionCustomTypes.h"

namespace ECSEngine {

	namespace Reflection {

		bool IsReflectionTypeOverallAllocatorByTag(const ReflectionType* type, size_t field_index) {
			return type->fields[field_index].Has(STRING(ECS_TYPE_MAIN_ALLOCATOR));
		}

		bool IsReflectionTypeFieldAllocator(const ReflectionType* type, size_t field_index) {
			ReflectionCustomTypeMatchData match_data;
			match_data.definition = type->fields[field_index].definition;
			return ECS_REFLECTION_CUSTOM_TYPES[ECS_REFLECTION_CUSTOM_TYPE_ALLOCATOR]->Match(&match_data);
		}

		bool IsReflectionTypeFieldAllocatorAsReference(const ReflectionType* type, size_t field_index) {
			return type->fields[field_index].Has(STRING(ECS_TYPE_REFERENCE_ALLOCATOR));
		}

		Stream<char> GetReflectionTypeFieldAllocatorFromTag(const ReflectionType* type, size_t field_index) {
			Stream<char> field_tag = type->fields[field_index].GetTag(STRING(ECS_FIELD_ALLOCATOR));
			if (field_tag.size > 0) {
				return GetStringParameter(field_tag);
			}
			return {};
		}

		size_t GetReflectionTypeAllocatorMiscIndex(const ReflectionType* type) {
			for (size_t index = 0; index < type->misc_info.size; index++) {
				if (type->misc_info[index].type == ECS_REFLECTION_TYPE_MISC_INFO_ALLOCATOR) {
					return index;
				}
			}
			return -1;
		}

		AllocatorPolymorphic ConvertReflectionTypeFieldToAllocator(const ReflectionType* type, size_t field_index, const void* instance) {
			const void* field = type->GetField(instance, field_index);
			ECS_ALLOCATOR_TYPE allocator_type = AllocatorTypeFromString(type->fields[field_index].definition);
			return ConvertPointerToAllocatorPolymorphicEx(field, allocator_type);
		}

		AllocatorPolymorphic GetReflectionTypeOverallAllocator(const ReflectionType* type, const void* instance, AllocatorPolymorphic fallback_allocator) {
			size_t misc_allocator_index = GetReflectionTypeAllocatorMiscIndex(type);
			if (misc_allocator_index != -1) {
				return ConvertReflectionTypeFieldToAllocator(type, type->misc_info[misc_allocator_index].allocator_info.field_index, instance);
			}
			return fallback_allocator;
		}

		AllocatorPolymorphic GetReflectionTypeFieldAllocator(const ReflectionType* type, size_t field_index, const void* instance, AllocatorPolymorphic fallback_allocator) {
			Stream<char> field_allocator_name = GetReflectionTypeFieldAllocatorFromTag(type, field_index);
			if (field_allocator_name.size > 0) {
				unsigned int allocator_field_index = type->FindField(field_allocator_name);
				return ConvertReflectionTypeFieldToAllocator(type, allocator_field_index, instance);
			}
			return fallback_allocator;
		}

	}

}