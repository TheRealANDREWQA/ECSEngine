#pragma once
#include "../../Core.h"
#include "ReflectionTypes.h"

namespace ECSEngine {

	namespace Reflection {


#pragma region Reflection Container Type functions

		enum ECS_REFLECTION_CUSTOM_TYPE_INDEX : unsigned char {
			ECS_REFLECTION_CUSTOM_TYPE_STREAM,
			ECS_REFLECTION_CUSTOM_TYPE_REFERENCE_COUNTED,
			ECS_REFLECTION_CUSTOM_TYPE_SPARSE_SET,
			ECS_REFLECTION_CUSTOM_TYPE_MATERIAL_ASSET,
			ECS_REFLECTION_CUSTOM_TYPE_DATA_POINTER,
			// Using a single entry for the allocator in order to reduce the interface bloat that is required,
			// This type will deal with all types of allocators
			ECS_REFLECTION_CUSTOM_TYPE_ALLOCATOR,
			ECS_REFLECTION_CUSTOM_TYPE_COUNT
		};

		// Must be kept in sync with the ECS_SERIALIZE_CUSTOM_TYPES
		extern ReflectionCustomTypeInterface* ECS_REFLECTION_CUSTOM_TYPES[];

		ECSENGINE_API void ReflectionCustomTypeDependentTypes_SingleTemplate(ReflectionCustomTypeDependentTypesData* data);

		// E.g. for Template<Type> string should be Template
		ECSENGINE_API bool ReflectionCustomTypeMatchTemplate(ReflectionCustomTypeMatchData* data, const char* string);

		ECSENGINE_API Stream<char> ReflectionCustomTypeGetTemplateArgument(Stream<char> definition);

		struct StreamCustomTypeInterface : public ReflectionCustomTypeInterface {
			bool Match(ReflectionCustomTypeMatchData* data) override;

			ulong2 GetByteSize(ReflectionCustomTypeByteSizeData* data) override;

			void GetDependentTypes(ReflectionCustomTypeDependentTypesData* data) override;

			bool IsBlittable(ReflectionCustomTypeIsBlittableData* data) override;

			void Copy(ReflectionCustomTypeCopyData* data) override;

			bool Compare(ReflectionCustomTypeCompareData* data) override;

			void Deallocate(ReflectionCustomTypeDeallocateData* data) override;
		};

		struct SparseSetCustomTypeInterface : public ReflectionCustomTypeInterface {
			bool Match(ReflectionCustomTypeMatchData* data) override;

			ulong2 GetByteSize(ReflectionCustomTypeByteSizeData* data) override;

			void GetDependentTypes(ReflectionCustomTypeDependentTypesData* data) override;

			bool IsBlittable(ReflectionCustomTypeIsBlittableData* data) override;

			void Copy(ReflectionCustomTypeCopyData* data) override;

			bool Compare(ReflectionCustomTypeCompareData* data) override;

			void Deallocate(ReflectionCustomTypeDeallocateData* data) override;
		};

		struct DataPointerCustomTypeInterface : public ReflectionCustomTypeInterface {
			bool Match(ReflectionCustomTypeMatchData* data) override;

			ulong2 GetByteSize(ReflectionCustomTypeByteSizeData* data) override;

			void GetDependentTypes(ReflectionCustomTypeDependentTypesData* data) override;

			bool IsBlittable(ReflectionCustomTypeIsBlittableData* data) override;

			void Copy(ReflectionCustomTypeCopyData* data) override;

			bool Compare(ReflectionCustomTypeCompareData* data) override;

			void Deallocate(ReflectionCustomTypeDeallocateData* data) override;
		};

		struct AllocatorCustomTypeInterface : public ReflectionCustomTypeInterface {
			bool Match(ReflectionCustomTypeMatchData* data) override;

			ulong2 GetByteSize(ReflectionCustomTypeByteSizeData* data) override;

			void GetDependentTypes(ReflectionCustomTypeDependentTypesData* data) override;

			bool IsBlittable(ReflectionCustomTypeIsBlittableData* data) override;

			void Copy(ReflectionCustomTypeCopyData* data) override;

			bool Compare(ReflectionCustomTypeCompareData* data) override;

			void Deallocate(ReflectionCustomTypeDeallocateData* data) override;
		};

		// ---------------------------------------------------------------------------------------------------------------------

		// Returns -1 if it is not matched
		ECSENGINE_API unsigned int FindReflectionCustomType(Stream<char> definition);

		// Returns nullptr if it doesn't find a match
		ECSENGINE_API ReflectionCustomTypeInterface* GetReflectionCustomType(Stream<char> definition);

		// Returns nullptr if the custom index is -1
		ECS_INLINE ReflectionCustomTypeInterface* GetReflectionCustomType(unsigned int custom_index) {
			return custom_index == -1 ? nullptr : ECS_REFLECTION_CUSTOM_TYPES[custom_index];
		}

		// ---------------------------------------------------------------------------------------------------------------------

#pragma endregion

	}

}