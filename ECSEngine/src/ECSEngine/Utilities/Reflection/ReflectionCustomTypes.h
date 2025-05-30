#pragma once
#include "../../Core.h"
#include "ReflectionTypes.h"
// This include is not referenced directly here, but it is added to make it easier
// For consumers of the API to reference that one as well
#include "ReflectionCustomTypesEnum.h"

namespace ECSEngine {

	namespace Reflection {

		struct ReflectionManager;

#pragma region Reflection Container Type functions

		// Must be kept in sync with the ECS_SERIALIZE_CUSTOM_TYPES
		extern ReflectionCustomTypeInterface* ECS_REFLECTION_CUSTOM_TYPES[];

		// This function will add the definition as a dependency unless the given definition is not a known basic type
		ECSENGINE_API void ReflectionCustomTypeAddDependentType(ReflectionCustomTypeDependenciesData* data, Stream<char> definition);

		ECSENGINE_API void ReflectionCustomTypeDependentTypes_SingleTemplate(ReflectionCustomTypeDependenciesData* data);

		// When the type has multiple template parameters, call this function, since it will handle all of them correctly
		ECSENGINE_API void ReflectionCustomTypeDependentTypes_MultiTemplate(ReflectionCustomTypeDependenciesData* data);

		// E.g. for Template<Type> string should be Template
		ECSENGINE_API bool ReflectionCustomTypeMatchTemplate(ReflectionCustomTypeMatchData* data, const char* string);

		ECSENGINE_API Stream<char> ReflectionCustomTypeGetTemplateArgument(Stream<char> definition);

		// Fills in the template parameters for the given definition, while taking into consideration
		// That each individual parameter can contain nested templates
		ECSENGINE_API void ReflectionCustomTypeGetTemplateArguments(Stream<char> definition, CapacityStream<Stream<char>>& arguments);

		struct StreamCustomTypeInterface : public ReflectionCustomTypeInterface {
			bool Match(ReflectionCustomTypeMatchData* data) override;

			ulong2 GetByteSize(ReflectionCustomTypeByteSizeData* data) override;

			void GetDependencies(ReflectionCustomTypeDependenciesData* data) override;

			bool IsBlittable(ReflectionCustomTypeIsBlittableData* data) override;

			void Copy(ReflectionCustomTypeCopyData* data) override;

			bool Compare(ReflectionCustomTypeCompareData* data) override;

			void Deallocate(ReflectionCustomTypeDeallocateData* data) override;

			size_t GetElementCount(ReflectionCustomTypeGetElementCountData* data) override;

			void* GetElement(ReflectionCustomTypeGetElementData* data) override;

			ReflectionCustomTypeGetElementIndexOrToken FindElement(ReflectionCustomTypeFindElementData* data) override;

			bool ValidateTags(ReflectionCustomTypeValidateTagData* data) override {
				// Nothing to validate for this type
				return true;
			}
		};

		struct SparseSetCustomTypeInterface : public ReflectionCustomTypeInterface {
			bool Match(ReflectionCustomTypeMatchData* data) override;

			ulong2 GetByteSize(ReflectionCustomTypeByteSizeData* data) override;

			void GetDependencies(ReflectionCustomTypeDependenciesData* data) override;

			bool IsBlittable(ReflectionCustomTypeIsBlittableData* data) override;

			void Copy(ReflectionCustomTypeCopyData* data) override;

			bool Compare(ReflectionCustomTypeCompareData* data) override;

			void Deallocate(ReflectionCustomTypeDeallocateData* data) override;

			size_t GetElementCount(ReflectionCustomTypeGetElementCountData* data) override;

			void* GetElement(ReflectionCustomTypeGetElementData* data) override;

			ReflectionCustomTypeGetElementIndexOrToken FindElement(ReflectionCustomTypeFindElementData* data) override;

			bool ValidateTags(ReflectionCustomTypeValidateTagData* data) override {
				// Nothing to validate for this type
				return true;
			}
		};

		struct DataPointerCustomTypeInterface : public ReflectionCustomTypeInterface {
			bool Match(ReflectionCustomTypeMatchData* data) override;

			ulong2 GetByteSize(ReflectionCustomTypeByteSizeData* data) override;

			void GetDependencies(ReflectionCustomTypeDependenciesData* data) override;

			bool IsBlittable(ReflectionCustomTypeIsBlittableData* data) override;

			void Copy(ReflectionCustomTypeCopyData* data) override;

			bool Compare(ReflectionCustomTypeCompareData* data) override;

			void Deallocate(ReflectionCustomTypeDeallocateData* data) override;

			bool ValidateTags(ReflectionCustomTypeValidateTagData* data) override {
				// Nothing to validate for this type
				return true;
			}
		};

		// This interface handles AllocatorPolymorphic as well
		struct AllocatorCustomTypeInterface : public ReflectionCustomTypeInterface {
			bool Match(ReflectionCustomTypeMatchData* data) override;

			ulong2 GetByteSize(ReflectionCustomTypeByteSizeData* data) override;

			void GetDependencies(ReflectionCustomTypeDependenciesData* data) override;

			bool IsBlittable(ReflectionCustomTypeIsBlittableData* data) override;

			void Copy(ReflectionCustomTypeCopyData* data) override;

			bool Compare(ReflectionCustomTypeCompareData* data) override;

			void Deallocate(ReflectionCustomTypeDeallocateData* data) override;

			bool ValidateTags(ReflectionCustomTypeValidateTagData* data) override {
				// Nothing to validate for this type
				return true;
			}
		};

		struct HashTableCustomTypeInterface : ReflectionCustomTypeInterface {
			bool Match(ReflectionCustomTypeMatchData* data) override;

			ulong2 GetByteSize(ReflectionCustomTypeByteSizeData* data) override;

			void GetDependencies(ReflectionCustomTypeDependenciesData* data) override;

			bool IsBlittable(ReflectionCustomTypeIsBlittableData* data) override;

			void Copy(ReflectionCustomTypeCopyData* data) override;

			bool Compare(ReflectionCustomTypeCompareData* data) override;

			void Deallocate(ReflectionCustomTypeDeallocateData* data) override;

			size_t GetElementCount(ReflectionCustomTypeGetElementCountData* data) override;

			void* GetElement(ReflectionCustomTypeGetElementData* data) override;

			ReflectionCustomTypeGetElementIndexOrToken FindElement(ReflectionCustomTypeFindElementData* data) override;

			bool ValidateTags(ReflectionCustomTypeValidateTagData* data) override;
		};

		struct DeckCustomTypeInterface : ReflectionCustomTypeInterface {
			bool Match(ReflectionCustomTypeMatchData* data) override;

			ulong2 GetByteSize(ReflectionCustomTypeByteSizeData* data) override;

			void GetDependencies(ReflectionCustomTypeDependenciesData* data) override;

			bool IsBlittable(ReflectionCustomTypeIsBlittableData* data) override;

			void Copy(ReflectionCustomTypeCopyData* data) override;

			bool Compare(ReflectionCustomTypeCompareData* data) override;

			void Deallocate(ReflectionCustomTypeDeallocateData* data) override;

			size_t GetElementCount(ReflectionCustomTypeGetElementCountData* data) override;

			void* GetElement(ReflectionCustomTypeGetElementData* data) override;

			ReflectionCustomTypeGetElementIndexOrToken FindElement(ReflectionCustomTypeFindElementData* data) override;
		
			bool ValidateTags(ReflectionCustomTypeValidateTagData* data) override {
				// Nothing to validate for this type
				return true;
			}
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

		// Describes the type of hash table definition
		enum HASH_TABLE_TYPE : unsigned char {
			HASH_TABLE_NORMAL,
			HASH_TABLE_DEFAULT,
			HASH_TABLE_EMPTY
		};

		struct HashTableTemplateArguments {
			Stream<char> value_type;
			Stream<char> identifier_type;
			Stream<char> hash_function_type;
			bool is_soa = false;
			HASH_TABLE_TYPE table_type;
		};

		HashTableTemplateArguments HashTableExtractTemplateArguments(Stream<char> definition);

		// Returns a definition info for the value template type, while taking into account the special case of HashTableEmpty
		ReflectionDefinitionInfo HashTableGetValueDefinitionInfo(const ReflectionManager* reflection_manager, const HashTableTemplateArguments& template_arguments);

		// Returns a definition info for the identifier template type, while taking into account the special case of ResourceIdentifier
		ReflectionDefinitionInfo HashTableGetIdentifierDefinitionInfo(const ReflectionManager* reflection_manager, HashTableTemplateArguments& template_arguments);

		// Returns in the x component the byte size of the pair, while in the y component the offset that must be added to the value byte size
		// In order to correctly address the identifier inside the pair
		ulong2 HashTableComputePairByteSizeAndAlignmentOffset(const ReflectionDefinitionInfo& value_definition_info, const ReflectionDefinitionInfo& identifier_definition_info);

		// For an untyped hash table, it will allocate an allocation and set the table buffers accordingly
		// Returns the number of bytes needed for the allocation
		size_t HashTableCustomTypeAllocateAndSetBuffers(void* hash_table, size_t capacity, AllocatorPolymorphic allocator, bool is_soa, const ReflectionDefinitionInfo& value_definition_info, const ReflectionDefinitionInfo& identifier_definition_info);

		// ---------------------------------------------------------------------------------------------------------------------

#pragma endregion

	}

}