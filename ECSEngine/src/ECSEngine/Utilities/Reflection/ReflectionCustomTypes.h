#pragma once
#include "../../Core.h"
#include "ReflectionTypes.h"

namespace ECSEngine {

	namespace Reflection {

		struct ReflectionManager;

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
			ECS_REFLECTION_CUSTOM_TYPE_HASH_TABLE,
			ECS_REFLECTION_CUSTOM_TYPE_DECK,
			ECS_REFLECTION_CUSTOM_TYPE_COUNT
		};

		// Must be kept in sync with the ECS_SERIALIZE_CUSTOM_TYPES
		extern ReflectionCustomTypeInterface* ECS_REFLECTION_CUSTOM_TYPES[];

		// This function will add the definition as a dependency unless the given definition is not a known basic type
		ECSENGINE_API void ReflectionCustomTypeAddDependentType(ReflectionCustomTypeDependentTypesData* data, Stream<char> definition);

		ECSENGINE_API void ReflectionCustomTypeDependentTypes_SingleTemplate(ReflectionCustomTypeDependentTypesData* data);

		// When the type has multiple template parameters, call this function, since it will handle all of them correctly
		ECSENGINE_API void ReflectionCustomTypeDependentTypes_MultiTemplate(ReflectionCustomTypeDependentTypesData* data);

		// E.g. for Template<Type> string should be Template
		ECSENGINE_API bool ReflectionCustomTypeMatchTemplate(ReflectionCustomTypeMatchData* data, const char* string);

		ECSENGINE_API Stream<char> ReflectionCustomTypeGetTemplateArgument(Stream<char> definition);

		// Fills in the template parameters for the given definition, while taking into consideration
		// That each individual parameter can contain nested templates
		ECSENGINE_API void ReflectionCustomTypeGetTemplateArguments(Stream<char> definition, CapacityStream<Stream<char>>& arguments);

		struct StreamCustomTypeInterface : public ReflectionCustomTypeInterface {
			bool Match(ReflectionCustomTypeMatchData* data) override;

			ulong2 GetByteSize(ReflectionCustomTypeByteSizeData* data) override;

			void GetDependentTypes(ReflectionCustomTypeDependentTypesData* data) override;

			bool IsBlittable(ReflectionCustomTypeIsBlittableData* data) override;

			void Copy(ReflectionCustomTypeCopyData* data) override;

			bool Compare(ReflectionCustomTypeCompareData* data) override;

			void Deallocate(ReflectionCustomTypeDeallocateData* data) override;

			size_t GetElementCount(ReflectionCustomTypeGetElementCountData* data) override;

			void* GetElement(ReflectionCustomTypeGetElementData* data) override;

			ReflectionCustomTypeGetElementIndexOrToken FindElement(ReflectionCustomTypeFindElementData* data) override;
		};

		struct SparseSetCustomTypeInterface : public ReflectionCustomTypeInterface {
			bool Match(ReflectionCustomTypeMatchData* data) override;

			ulong2 GetByteSize(ReflectionCustomTypeByteSizeData* data) override;

			void GetDependentTypes(ReflectionCustomTypeDependentTypesData* data) override;

			bool IsBlittable(ReflectionCustomTypeIsBlittableData* data) override;

			void Copy(ReflectionCustomTypeCopyData* data) override;

			bool Compare(ReflectionCustomTypeCompareData* data) override;

			void Deallocate(ReflectionCustomTypeDeallocateData* data) override;

			size_t GetElementCount(ReflectionCustomTypeGetElementCountData* data) override;

			void* GetElement(ReflectionCustomTypeGetElementData* data) override;

			ReflectionCustomTypeGetElementIndexOrToken FindElement(ReflectionCustomTypeFindElementData* data) override;
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

		// This interface handles AllocatorPolymorphic as well
		struct AllocatorCustomTypeInterface : public ReflectionCustomTypeInterface {
			bool Match(ReflectionCustomTypeMatchData* data) override;

			ulong2 GetByteSize(ReflectionCustomTypeByteSizeData* data) override;

			void GetDependentTypes(ReflectionCustomTypeDependentTypesData* data) override;

			bool IsBlittable(ReflectionCustomTypeIsBlittableData* data) override;

			void Copy(ReflectionCustomTypeCopyData* data) override;

			bool Compare(ReflectionCustomTypeCompareData* data) override;

			void Deallocate(ReflectionCustomTypeDeallocateData* data) override;
		};

		struct HashTableCustomTypeInterface : ReflectionCustomTypeInterface {
			bool Match(ReflectionCustomTypeMatchData* data) override;

			ulong2 GetByteSize(ReflectionCustomTypeByteSizeData* data) override;

			void GetDependentTypes(ReflectionCustomTypeDependentTypesData* data) override;

			bool IsBlittable(ReflectionCustomTypeIsBlittableData* data) override;

			void Copy(ReflectionCustomTypeCopyData* data) override;

			bool Compare(ReflectionCustomTypeCompareData* data) override;

			void Deallocate(ReflectionCustomTypeDeallocateData* data) override;

			size_t GetElementCount(ReflectionCustomTypeGetElementCountData* data) override;

			void* GetElement(ReflectionCustomTypeGetElementData* data) override;

			ReflectionCustomTypeGetElementIndexOrToken FindElement(ReflectionCustomTypeFindElementData* data) override;
		};

		struct DeckCustomTypeInterface : ReflectionCustomTypeInterface {
			bool Match(ReflectionCustomTypeMatchData* data) override;

			ulong2 GetByteSize(ReflectionCustomTypeByteSizeData* data) override;

			void GetDependentTypes(ReflectionCustomTypeDependentTypesData* data) override;

			bool IsBlittable(ReflectionCustomTypeIsBlittableData* data) override;

			void Copy(ReflectionCustomTypeCopyData* data) override;

			bool Compare(ReflectionCustomTypeCompareData* data) override;

			void Deallocate(ReflectionCustomTypeDeallocateData* data) override;

			size_t GetElementCount(ReflectionCustomTypeGetElementCountData* data) override;

			void* GetElement(ReflectionCustomTypeGetElementData* data) override;

			ReflectionCustomTypeGetElementIndexOrToken FindElement(ReflectionCustomTypeFindElementData* data) override;
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

		enum ECS_HASH_TABLE_CUSTOM_TYPE_ELEMENT_INDEX : unsigned char {
			ECS_HASH_TABLE_CUSTOM_TYPE_ELEMENT_VALUE,
			ECS_HASH_TABLE_CUSTOM_TYPE_ELEMENT_IDENTIFIER
		};

		// ---------------------------------------------------------------------------------------------------------------------

#pragma endregion

	}

}