#include "ecspch.h"
#include "ReflectionAllocatorHandling.h"
#include "ReflectionCustomTypes.h"

namespace ECSEngine {

	namespace Reflection {

		bool IsReflectionTypeOverallAllocatorByTag(const ReflectionType* type, size_t field_index) {
			return type->fields[field_index].Has(STRING(ECS_MAIN_ALLOCATOR));
		}

		bool IsReflectionTypeFieldAllocator(const ReflectionType* type, size_t field_index) {
			ReflectionCustomTypeMatchData match_data;
			match_data.definition = type->fields[field_index].definition;
			return ECS_REFLECTION_CUSTOM_TYPES[ECS_REFLECTION_CUSTOM_TYPE_ALLOCATOR]->Match(&match_data);
		}

		bool IsReflectionTypeFieldAllocatorFromMisc(const ReflectionType* type, size_t field_index) {
			for (size_t index = 0; index < type->misc_info.size; index++) {
				if (type->misc_info[index].type == ECS_REFLECTION_TYPE_MISC_INFO_ALLOCATOR) {
					if (type->misc_info[index].allocator_info.field_index == field_index) {
						return true;
					}
				}
			}
			return false;
		}

		bool IsReflectionTypeFieldAllocatorAsReference(const ReflectionType* type, size_t field_index) {
			return type->fields[field_index].Has(STRING(ECS_REFERENCE_ALLOCATOR));
		}

		Stream<char> GetReflectionTypeFieldAllocatorFromTag(const ReflectionType* type, size_t field_index) {
			Stream<char> field_tag = type->fields[field_index].GetTag(STRING(ECS_FIELD_ALLOCATOR));
			if (field_tag.size > 0) {
				return GetStringParameter(field_tag);
			}
			return {};
		}

		size_t GetReflectionTypeFieldAllocatorFromTagAsIndex(const ReflectionType* type, size_t field_index) {
			Stream<char> field_tag = GetReflectionTypeFieldAllocatorFromTag(type, field_index);
			return field_tag.size == 0 ? -1 : type->FindField(field_tag);
		}

		size_t GetReflectionTypeOverallAllocatorMiscIndex(const ReflectionType* type) {
			for (size_t index = 0; index < type->misc_info.size; index++) {
				if (type->misc_info[index].type == ECS_REFLECTION_TYPE_MISC_INFO_ALLOCATOR) {
					if (type->misc_info[index].allocator_info.modifier == ECS_REFLECTION_TYPE_MISC_ALLOCATOR_MODIFIER_MAIN) {
						return index;
					}
				}
			}
			return -1;
		}

		AllocatorPolymorphic ConvertReflectionTypeFieldToAllocator(const ReflectionType* type, size_t field_index, const void* instance) {
			const void* field = type->GetField(instance, field_index);
			ECS_ALLOCATOR_TYPE allocator_type = AllocatorTypeFromString(type->fields[field_index].definition);
			return ConvertPointerToAllocatorPolymorphicEx(field, allocator_type);
		}

		AllocatorPolymorphic ConvertReflectionTypeFieldToAllocatorWithMainHandling(const ReflectionType* type, size_t field_index, const void* instance) {
			// Firstly, determine if this is a main allocator field index with a nested allocator
			size_t main_allocator_soa_index = GetReflectionTypeOverallAllocatorMiscIndex(type);
			if (main_allocator_soa_index != -1) {
				const ReflectionTypeMiscAllocator* main_allocator = &type->misc_info[main_allocator_soa_index].allocator_info;
				if (main_allocator->field_index == field_index) {
					// Handle this case manually
					return main_allocator->GetMainAllocatorForInstance(instance);
				}
			}

			return ConvertReflectionTypeFieldToAllocator(type, field_index, instance);
		}

		AllocatorPolymorphic GetReflectionTypeOverallAllocator(const ReflectionType* type, const void* instance, AllocatorPolymorphic fallback_allocator) {
			size_t misc_allocator_index = GetReflectionTypeOverallAllocatorMiscIndex(type);
			if (misc_allocator_index != -1) {
				// This is a more special case to handle. We must make use of the extra information from the misc info, cannot use
				// The normal ConvertReflectionTypeFieldToAllocator to get the allocator
				const ReflectionTypeMiscAllocator* misc_allocator = &type->misc_info[misc_allocator_index].allocator_info;
				return misc_allocator->GetMainAllocatorForInstance(instance);
			}
			return fallback_allocator;
		}

		AllocatorPolymorphic GetReflectionTypeFieldAllocator(const ReflectionType* type, size_t field_index, const void* instance, AllocatorPolymorphic fallback_allocator) {
			Stream<char> field_allocator_name = GetReflectionTypeFieldAllocatorFromTag(type, field_index);
			if (field_allocator_name.size > 0) {
				unsigned int allocator_field_index = type->FindField(field_allocator_name);
				return ConvertReflectionTypeFieldToAllocatorWithMainHandling(type, allocator_field_index, instance);
			}
			return fallback_allocator;
		}

		AllocatorPolymorphic GetReflectionTypeFieldAllocatorForSoa(const ReflectionType* type, const ReflectionTypeMiscSoa* soa, const void* instance, AllocatorPolymorphic fallback_allocator) {
			if (soa->field_allocator_index != UCHAR_MAX) {
				return ConvertReflectionTypeFieldToAllocatorWithMainHandling(type, soa->field_allocator_index, instance);
			}
			return fallback_allocator;
		}

		void SetReflectionTypeFieldAllocatorReference(const ReflectionType* type, const ReflectionTypeMiscAllocator* allocator_info, void* instance, AllocatorPolymorphic reference_allocator) {
			// Ensure that the field is a AllocatorPolymorphic
			ECS_ASSERT(type->fields[allocator_info->field_index].definition == STRING(AllocatorPolymorphic), "Reference allocator must of type AllocatorPolymorphic!");
			AllocatorPolymorphic* allocator = (AllocatorPolymorphic*)type->GetField(instance, allocator_info->field_index);
			*allocator = reference_allocator;
		}

		// The index extractor is a functor that receives as arguments (size_t soa_index) and it should return a size_t which indicates
		// What index it should use
		template<typename IndexExtractor>
		static void GetReflectionTypeAllocatorInitializeOrderImpl(const ReflectionType* type, CapacityStream<unsigned int>& allocator_indices, IndexExtractor&& extractor) {
			for (size_t index = 0; index < type->misc_info.size; index++) {
				if (type->misc_info[index].type == ECS_REFLECTION_TYPE_MISC_INFO_ALLOCATOR) {
					size_t allocator_field_index = type->misc_info[index].allocator_info.field_index;
					size_t reference_index = GetReflectionTypeFieldAllocatorFromTagAsIndex(type, allocator_field_index);

					size_t allocator_index_to_use = extractor(index);
					if (reference_index == -1) {
						// We can insert it right at the beginning
						allocator_indices.Insert(0, (unsigned int)allocator_index_to_use);
					}
					else {
						// Find its dependency, if its already in the allocator_field_indices, and insert it right after it,
						// Else (if the dependency was not added yet) add it the end of the array
						unsigned int inside_array_index = allocator_indices.Find((unsigned)reference_index);
						if (inside_array_index == -1) {
							allocator_indices.AddAssert((unsigned int)allocator_index_to_use);
						}
						else {
							allocator_indices.Insert(inside_array_index + 1, (unsigned int)allocator_index_to_use);
						}
					}
				}
			}
		}

		void GetReflectionTypeAllocatorInitializeOrderSoaIndices(const ReflectionType* type, CapacityStream<unsigned int>& allocator_soa_indices) {
			GetReflectionTypeAllocatorInitializeOrderImpl(type, allocator_soa_indices, [type](size_t soa_index) {
				return soa_index;
			});
		}

		void GetReflectionTypeAllocatorPointerAndDefinition(const ReflectionType* type, size_t field_index, const void* instance, void*& allocator_pointer, Stream<char>& definition) {
			size_t main_allocator_soa_index = GetReflectionTypeOverallAllocatorMiscIndex(type);
			// Set the default value here, and overwrite it only for the unusual case
			allocator_pointer = type->GetField(instance, field_index);
			definition = type->fields[field_index].definition;
			
			if (main_allocator_soa_index != -1) {
				const ReflectionTypeMiscAllocator* main_allocator = &type->misc_info[main_allocator_soa_index].allocator_info;
				if (main_allocator->field_index == field_index) {
					allocator_pointer = OffsetPointer(instance, main_allocator->main_allocator_offset);
					definition = main_allocator->main_allocator_definition;
				}
			}
		}

	}

}