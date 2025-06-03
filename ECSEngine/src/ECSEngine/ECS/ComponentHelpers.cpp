#include "ecspch.h"
#include "ComponentHelpers.h"
#include "../Utilities/Reflection/Reflection.h"
#include "../Utilities/Reflection/ReflectionMacros.h"
#include "../Utilities/Reflection/ReflectionAllocatorHandling.h"
#include "../Allocators/ResizableLinearAllocator.h"
#include "../Resources/AssetMetadata.h"

#define MAX_COMPONENT_BUFFERS 7
#define MAX_COMPONENT_BLITTABLE_FIELDS 16
#define MAX_NESTED_TYPES 7

namespace ECSEngine {

	using namespace Reflection;

	// ----------------------------------------------------------------------------------------------------------------------------

	bool IsReflectionTypeComponent(const ReflectionType* type)
	{
		bool is_tag = type->IsTag(ECS_COMPONENT_TAG);
		if (is_tag) {
			double evaluation = type->GetEvaluation(ECS_COMPONENT_IS_SHARED_FUNCTION);
			return evaluation != DBL_MAX ? evaluation == 0.0 : false;
		}
		return false;
	}

	// ----------------------------------------------------------------------------------------------------------------------------

	bool IsReflectionTypeSharedComponent(const ReflectionType* type)
	{
		bool is_tag = type->IsTag(ECS_COMPONENT_TAG);
		if (is_tag) {
			double evaluation = type->GetEvaluation(ECS_COMPONENT_IS_SHARED_FUNCTION);
			return evaluation != DBL_MAX ? evaluation == 1.0 : false;
		}
		return false;
	}

	// ----------------------------------------------------------------------------------------------------------------------------

	bool IsReflectionTypeMaybeComponent(const Reflection::ReflectionType* type)
	{
		bool is_tag = type->IsTag(ECS_COMPONENT_TAG);
		if (is_tag && type->GetEvaluation(ECS_COMPONENT_IS_SHARED_FUNCTION) == DBL_MAX) {
			return true;
		}
		return false;
	}

	// ----------------------------------------------------------------------------------------------------------------------------

	bool IsReflectionTypeGlobalComponent(const Reflection::ReflectionType* type)
	{
		return type->IsTag(ECS_GLOBAL_COMPONENT_TAG) || type->IsTag(ECS_GLOBAL_COMPONENT_PRIVATE_TAG);
	}

	// ----------------------------------------------------------------------------------------------------------------------------

	bool IsReflectionTypeGlobalComponentPrivate(const Reflection::ReflectionType* type)
	{
		return type->IsTag(ECS_GLOBAL_COMPONENT_PRIVATE_TAG);
	}

	// ----------------------------------------------------------------------------------------------------------------------------

	bool IsReflectionTypeLinkComponent(const ReflectionType* type)
	{
		return type->HasTag(ECS_LINK_COMPONENT_TAG);
	}

	// ----------------------------------------------------------------------------------------------------------------------------

	bool IsReflectionTypeComponentType(const ReflectionType* type, ECS_COMPONENT_TYPE component_type)
	{
		if (component_type == ECS_COMPONENT_UNIQUE) {
			return IsReflectionTypeComponent(type);
		}
		else if (component_type == ECS_COMPONENT_SHARED) {
			return IsReflectionTypeSharedComponent(type);
		}
		else if (component_type == ECS_COMPONENT_GLOBAL) {
			return IsReflectionTypeGlobalComponent(type);
		}
		else {
			return IsReflectionTypeComponent(type) || IsReflectionTypeSharedComponent(type) || IsReflectionTypeGlobalComponent(type);
		}
	}
	
	// ----------------------------------------------------------------------------------------------------------------------------

	void FillReflectionComponentBlittableTypes(CapacityStream<Reflection::ReflectionCustomTypeCompareOptionBlittableType>& blittable_types) {
		size_t blittable_count = ECS_ASSET_TARGET_FIELD_NAMES_SIZE();
		// This will include the misc MiscAssetData with a pointer but that is no problem, shouldn't really happen in code
		for (size_t blittable_index = 0; blittable_index < blittable_count; blittable_index++) {
			blittable_types.AddAssert({ ECS_ASSET_TARGET_FIELD_NAMES[blittable_index].name, Reflection::ReflectionStreamFieldType::Pointer });
		}
		blittable_types.AddAssert({ STRING(MiscAssetData), Reflection::ReflectionStreamFieldType::Basic });
	}

	// ----------------------------------------------------------------------------------------------------------------------------
	
	// We use the copyable in order to copy the structures easier
	struct RuntimeComponentCopyDeallocateData : public Copyable {
		ECS_INLINE RuntimeComponentCopyDeallocateData() : Copyable(sizeof(*this)) {}

		size_t CopySizeImpl() const override {
			return type.CopySize();
		}

		void CopyImpl(const void* other, AllocatorPolymorphic allocator) override {
			const RuntimeComponentCopyDeallocateData* other_data = (const RuntimeComponentCopyDeallocateData*)other;
			type = other_data->type.CopyCoalesced(allocator);
			reflection_manager = other_data->reflection_manager;
			is_global_component = other_data->is_global_component;
		}

		void Deallocate(AllocatorPolymorphic allocator) override {
			type.DeallocateCoalesced(allocator);
		}

		// Cache this variable such that we don't spend time parsing the tag
		bool is_global_component;
		ReflectionType type;
		const ReflectionManager* reflection_manager;
	};

	// PERFORMANCE TODO: If the copy is too slow, we can determine
	// The buffer fields when registering the function and have a
	// Specialized function copy only those fields.

	static void ReflectionTypeRuntimeComponentCopy(ComponentCopyFunctionData* copy_data) {
		RuntimeComponentCopyDeallocateData* data = (RuntimeComponentCopyDeallocateData*)copy_data->function_data;
		//memcpy(copy_data->destination, copy_data->source, data->component_byte_size);
		CopyReflectionDataOptions options;
		options.allocator = copy_data->allocator;
		options.custom_options.deallocate_existing_data = copy_data->deallocate_previous;
		options.always_allocate_for_buffers = true;
		if (data->is_global_component) {
			// These 2 options are needed for global components
			options.custom_options.initialize_type_allocators = true;
			options.custom_options.use_field_allocators = true;
			// We need this option because this is the first initialization done for this instance
			options.custom_options.overwrite_resizable_allocators = true;
		}
		CopyReflectionTypeInstance(data->reflection_manager, &data->type, copy_data->source, copy_data->destination, &options);
	}

	static void ReflectionTypeRuntimeComponentDeallocate(ComponentDeallocateFunctionData* deallocate_data) {
		// TODO: Is it worth adding a Bulk processing parameter? Currently, that would be useful if an entire
		// Archetype base would be destroyed, which is not that often

		RuntimeComponentCopyDeallocateData* data = (RuntimeComponentCopyDeallocateData*)deallocate_data->function_data;
		// Do not reset the buffers, the component is destroyed anyways
		DeallocateReflectionTypeInstanceBuffers(data->reflection_manager, &data->type, deallocate_data->data, deallocate_data->allocator);
	}

	ComponentFunctions GetReflectionTypeRuntimeComponentFunctions(const ReflectionManager* reflection_manager, const ReflectionType* type, AllocatorPolymorphic allocator)
	{
		ComponentFunctions component_functions;
		if (!IsBlittableWithPointer(type)) {
			bool is_global_with_buffers = false;
			if (IsReflectionTypeGlobalComponent(type)) {
				// For global components, we have to check the buffers and allocator individually
				size_t overall_misc_index = GetReflectionTypeOverallAllocatorMiscIndex(type);
				if (overall_misc_index != -1) {
					is_global_with_buffers = true;
					component_functions.type_allocator_pointer_offset = type->misc_info[overall_misc_index].allocator_info.main_allocator_offset;
					component_functions.type_allocator_type = AllocatorTypeFromString(type->misc_info[overall_misc_index].allocator_info.main_allocator_definition);
				}
			}
			else {
				// If the allocator size is missing, then don't set this entry as having buffers
				component_functions.allocator_size = GetReflectionComponentAllocatorSize(type);
			}

			// If this is a global component
			if (component_functions.allocator_size > 0 || is_global_with_buffers) {
				component_functions.copy_function = ReflectionTypeRuntimeComponentCopy;
				component_functions.deallocate_function = ReflectionTypeRuntimeComponentDeallocate;

				RuntimeComponentCopyDeallocateData* copyable = AllocateAndConstruct<RuntimeComponentCopyDeallocateData>(allocator);

				copyable->type = type->CopyCoalesced(allocator);
				copyable->is_global_component = is_global_with_buffers;
				copyable->reflection_manager = reflection_manager;
				component_functions.data = copyable;
			}
		}

		return component_functions;
	}

	// ----------------------------------------------------------------------------------------------------------------------------

	typedef RuntimeComponentCopyDeallocateData RuntimeComponentCompareData;

	static bool ReflectionTypeRuntimeCompare(SharedComponentCompareFunctionData* data) {
		const RuntimeComponentCompareData* function_data = (const RuntimeComponentCompareData*)data->function_data;
		ECS_STACK_COMPONENT_BLITTABLE_TYPES(blittable_types);
		ReflectionCustomTypeCompareOptions options;
		options.blittable_types = blittable_types;
		return CompareReflectionTypeInstances(function_data->reflection_manager, &function_data->type, data->first, data->second, &options);
	}

	SharedComponentCompareEntry GetReflectionTypeRuntimeCompareEntry(
		const Reflection::ReflectionManager* reflection_manager,
		const Reflection::ReflectionType* type, 
		AllocatorPolymorphic allocator
	)
	{
		SharedComponentCompareEntry entry;

		if (!IsBlittableWithPointer(type)) {
			entry.function = ReflectionTypeRuntimeCompare;
			entry.data = nullptr;
			entry.use_copy_deallocate_data = true;
		}

		return entry;
	}

	// ----------------------------------------------------------------------------------------------------------------------------

	Component GetReflectionTypeComponent(const Reflection::ReflectionType* type)
	{
		double evaluation = type->GetEvaluation(ECS_COMPONENT_ID_FUNCTION);
		if (evaluation == DBL_MAX) {
			return Component{ -1 };
		}
		return Component{ (short)evaluation };
	}

	void GetHierarchyComponentTypes(
		const Reflection::ReflectionManager* reflection_manager, 
		unsigned int hierarchy_index, 
		CapacityStream<unsigned int>* unique_indices, 
		CapacityStream<unsigned int>* shared_indices,
		CapacityStream<unsigned int>* global_indices
	) 
	{
		ECS_STACK_CAPACITY_STREAM(unsigned, all_indices, ECS_KB * 16);
		Stream<char> tags[] = {
			ECS_COMPONENT_TAG,
			ECS_GLOBAL_COMPONENT_TAG,
			ECS_GLOBAL_COMPONENT_PRIVATE_TAG
		};

		ReflectionManagerGetQuery query_options;
		query_options.tags = { tags, ECS_COUNTOF(tags) };
		query_options.indices = &all_indices;
		query_options.strict_compare = true;

		reflection_manager->GetHierarchyTypes(query_options, hierarchy_index);

		for (unsigned int index = 0; index < all_indices.size; index++) {
			const Reflection::ReflectionType* reflection_type = reflection_manager->GetType(all_indices[index]);
			ECS_COMPONENT_TYPE component_type = GetReflectionTypeComponentType(reflection_type);

			if (component_type == ECS_COMPONENT_UNIQUE) {
				unique_indices->AddAssert(all_indices[index]);
			}
			else if (component_type == ECS_COMPONENT_SHARED) {
				shared_indices->AddAssert(all_indices[index]);
			}
			else if (component_type == ECS_COMPONENT_GLOBAL) {
				global_indices->AddAssert(all_indices[index]);
			}
			else {
				// It might be a maybe component
				ECS_ASSERT(IsReflectionTypeMaybeComponent(reflection_type));
			}
		}
	}

	// ----------------------------------------------------------------------------------------------------------------------------

	size_t GetReflectionComponentAllocatorSize(const Reflection::ReflectionType* type)
	{
		double evaluation = type->GetEvaluation(ECS_COMPONENT_ALLOCATOR_SIZE_FUNCTION);
		return evaluation == DBL_MAX ? 0 : (size_t)evaluation;
	}

	// ----------------------------------------------------------------------------------------------------------------------------

	ECS_COMPONENT_TYPE GetReflectionTypeComponentType(const Reflection::ReflectionType* type)
	{
		if (IsReflectionTypeComponent(type)) {
			return ECS_COMPONENT_UNIQUE;
		}
		else if (IsReflectionTypeSharedComponent(type)) {
			return ECS_COMPONENT_SHARED;
		}
		else if (IsReflectionTypeGlobalComponent(type)) {
			return ECS_COMPONENT_GLOBAL;
		}
		return ECS_COMPONENT_TYPE_COUNT;
	}

	// ----------------------------------------------------------------------------------------------------------------------------

	bool HasReflectionTypeComponentBuffers(const Reflection::ReflectionType* type)
	{
		return !IsBlittableWithPointer(type);
	}

	// ----------------------------------------------------------------------------------------------------------------------------

	ECS_VALIDATE_REFLECTION_TYPE_AS_COMPONENT ValidateReflectionTypeComponent(const Reflection::ReflectionType* type)
	{
		ECS_VALIDATE_REFLECTION_TYPE_AS_COMPONENT validate_value = ECS_VALIDATE_REFLECTION_TYPE_AS_COMPONENT_VALID;
		ECS_COMPONENT_TYPE component_type = GetReflectionTypeComponentType(type);
		if (component_type != ECS_COMPONENT_TYPE_COUNT) {
			// Verify that it has the ID function
			if (type->GetEvaluation(ECS_COMPONENT_ID_FUNCTION) == DBL_MAX) {
				validate_value |= ECS_VALIDATE_REFLECTION_TYPE_AS_COMPONENT_MISSING_ID_FUNCTION;
			}

			// Now check the buffers
			bool has_buffers = HasReflectionTypeComponentBuffers(type);
			size_t allocator_size = GetReflectionComponentAllocatorSize(type);
			if (has_buffers) {
				if (allocator_size == 0) {
					// For this case, if a main allocator is specified, then don't signal this
					// As an error, consider this entry as a system/global component which handles
					// Its own allocator
					if (GetReflectionTypeOverallAllocatorMiscIndex(type) == -1) {
						if (component_type == ECS_COMPONENT_GLOBAL) {
							validate_value |= ECS_VALIDATE_REFLECTION_TYPE_AS_COMPONENT_MISSING_TYPE_ALLOCATOR;
						}
						else {
							validate_value |= ECS_VALIDATE_REFLECTION_TYPE_AS_COMPONENT_MISSING_ALLOCATOR_SIZE_FUNCTION;
						}
					}
				}
			}

			if (allocator_size != 0 && !has_buffers) {
				validate_value |= ECS_VALIDATE_REFLECTION_TYPE_AS_COMPONENT_NO_BUFFERS_BUT_ALLOCATOR_SIZE_FUNCTION;
			}
		}

		// Now check if it has the ECS_REFLECT_COMPONENT tag and is missing the is_shared function
		if (component_type == ECS_COMPONENT_TYPE_COUNT) {
			if (IsReflectionTypeMaybeComponent(type)) {
				validate_value |= ECS_VALIDATE_REFLECTION_TYPE_AS_COMPONENT_MISSING_ID_FUNCTION;
			}
			else {
				validate_value |= ECS_VALIDATE_REFLECTION_TYPE_AS_COMPONENT_NOT_A_COMPONENT;
			}
		}

		return validate_value;
	}
	
	// ----------------------------------------------------------------------------------------------------------------------------

}