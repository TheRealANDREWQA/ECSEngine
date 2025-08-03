#include "ecspch.h"
#include "LinkComponents.h"
#include "../Utilities/Reflection/Reflection.h"
#include "../Resources/AssetDatabase.h"
#include "../Utilities/Serialization/SerializationHelpers.h"
#include "ComponentHelpers.h"
#include "../ECS/Entitymanager.h"
#include "ComponentAssetHandling.h"

#define LINK_COMPONENT_SUFFIX "Link"

namespace ECSEngine {

	// ----------------------------------------------------------------------------------------------------------------------------

	Stream<char> GetReflectionTypeLinkComponentTarget(const Reflection::ReflectionType* type)
	{
		Stream<char> opened_parenthese = FindFirstCharacter(type->tag, '(');
		if (opened_parenthese.size == 0) {
			return { nullptr, 0 };
		}

		opened_parenthese.buffer += 1;
		opened_parenthese.size -= 1;
		opened_parenthese = SkipWhitespace(opened_parenthese);

		if (opened_parenthese[opened_parenthese.size - 1] == ')') {
			opened_parenthese.size--;
		}
		opened_parenthese = SkipWhitespace(opened_parenthese, -1);
		return opened_parenthese;
	}

	// ----------------------------------------------------------------------------------------------------------------------------

	Stream<char> GetReflectionTypeLinkNameBase(Stream<char> name)
	{
		if (name.size > 0) {
			Stream<char> found_token = FindFirstToken(name, LINK_COMPONENT_SUFFIX);
			if (found_token.size > 0) {
				return { name.buffer, name.size - found_token.size };
			}
		}
		return name;
	}

	// ----------------------------------------------------------------------------------------------------------------------------

	Stream<char> GetReflectionTypeLinkComponentName(Stream<char> name, CapacityStream<char>& link_name)
	{
		link_name.CopyOther(name);
		link_name.AddStream(LINK_COMPONENT_SUFFIX);
		return link_name;
	}

	// ------------------------------------------------------------------------------------------------------------

	Stream<char> GetLinkComponentForTarget(const Reflection::ReflectionManager* reflection_manager, Stream<char> target, Stream<char> suffixed_name)
	{
		Stream<char> result;
		GetLinkComponentForTargets(reflection_manager, { &target, 1 }, &result, suffixed_name);
		return result;
	}

	// ------------------------------------------------------------------------------------------------------------

	void GetLinkComponentForTargets(
		const Reflection::ReflectionManager* reflection_manager, 
		Stream<Stream<char>> targets, 
		Stream<char>* link_names, 
		Stream<char> suffixed_name
	)
	{
		bool all_are_suffixed = true;
		if (suffixed_name.size > 0) {
			// Search for the suffixed variant first
			ECS_STACK_CAPACITY_STREAM(char, link_name, 1024);
			for (size_t index = 0; index < targets.size; index++) {
				link_name.CopyOther(targets[index]);
				link_name.AddStream(suffixed_name);

				const Reflection::ReflectionType* link_type = reflection_manager->TryGetType(link_name);
				if (link_type != nullptr) {
					link_names[index] = link_type->name;
				}
				else {
					link_names[index] = { nullptr, 0 };
					all_are_suffixed = false;
				}
			}
		}

		if (!all_are_suffixed) {
			Stream<char> tag[] = {
				ECS_LINK_COMPONENT_TAG
			};

			ECS_STACK_CAPACITY_STREAM(unsigned int, link_indices, 1024);
			// Get all types and search for LINK_COMPONENT tags
			Reflection::ReflectionManagerGetQuery get_query;
			get_query.indices = &link_indices;
			get_query.strict_compare = true;
			get_query.tags = { tag, ECS_COUNTOF(tag) };
			reflection_manager->GetHierarchyTypes(get_query);

			for (unsigned int index = 0; index < link_indices.size; index++) {
				const Reflection::ReflectionType* reflection_type = reflection_manager->GetType(link_indices[index]);
				Stream<char> link_target = GetReflectionTypeLinkComponentTarget(reflection_type);
				for (size_t subindex = 0; subindex < targets.size; subindex++) {
					if (link_names[subindex].size == 0 && link_target == targets[subindex]) {
						link_names[subindex] = reflection_type->name;
					}
				}
			}
		}
	}

	// ------------------------------------------------------------------------------------------------------------

	void ResetLinkComponent(
		const Reflection::ReflectionManager* reflection_manager,
		const Reflection::ReflectionType* type,
		void* link_component
	)
	{
		reflection_manager->SetInstanceDefaultData(type, link_component);
		// We don't have to reset the assets, since they will be defaulted to zeroing, which is their correct default value
	}

	// ------------------------------------------------------------------------------------------------------------

	void GetLinkComponentIndices(const Reflection::ReflectionManager* reflection_manager, CapacityStream<unsigned int>& indices) {
		Reflection::ReflectionManagerGetQuery query;
		query.indices = &indices;
		query.strict_compare = false;

		Stream<char> tags[] = {
			ECS_LINK_COMPONENT_TAG
		};
		query.tags = { tags, ECS_COUNTOF(tags) };
		reflection_manager->GetHierarchyTypes(query);
	}

	// ------------------------------------------------------------------------------------------------------------

	template<typename Functor>
	static void GetLinkComponentsImpl(const Reflection::ReflectionManager* reflection_manager, Functor&& functor) {
		ECS_STACK_CAPACITY_STREAM(unsigned int, link_type_indices, ECS_KB * 4);
		GetLinkComponentIndices(reflection_manager, link_type_indices);

		for (unsigned int index = 0; index < link_type_indices.size; index++) {
			const Reflection::ReflectionType* link_type = reflection_manager->GetType(link_type_indices[index]);
			Stream<char> target = GetReflectionTypeLinkComponentTarget(link_type);

			const Reflection::ReflectionType* target_type = reflection_manager->TryGetType(target);
			if (target_type != nullptr) {
				functor(link_type, target_type);
			}
		}
	}

	void GetUniqueLinkComponents(const Reflection::ReflectionManager* reflection_manager, CapacityStream<const Reflection::ReflectionType*>& link_types)
	{
		GetLinkComponentsImpl(reflection_manager, [&](const Reflection::ReflectionType* link_type, const Reflection::ReflectionType* target_type) {
			if (IsReflectionTypeComponent(target_type)) {
				link_types.AddAssert(link_type);
			}
		});
	}

	// ------------------------------------------------------------------------------------------------------------

	void GetSharedLinkComponents(const Reflection::ReflectionManager* reflection_manager, CapacityStream<const Reflection::ReflectionType*>& link_types)
	{
		GetLinkComponentsImpl(reflection_manager, [&](const Reflection::ReflectionType* link_type, const Reflection::ReflectionType* target_type) {
			if (IsReflectionTypeSharedComponent(target_type)) {
				link_types.AddAssert(link_type);
			}
		});
	}

	// ------------------------------------------------------------------------------------------------------------

	void GetGlobalLinkComponents(const Reflection::ReflectionManager* reflection_manager, CapacityStream<const Reflection::ReflectionType*>& link_types)
	{
		GetLinkComponentsImpl(reflection_manager, [&](const Reflection::ReflectionType* link_type, const Reflection::ReflectionType* target_type) {
			if (IsReflectionTypeGlobalComponent(target_type)) {
				link_types.AddAssert(link_type);
			}
		});
	}

	// ------------------------------------------------------------------------------------------------------------

	void GetAllLinkComponents(
		const Reflection::ReflectionManager* reflection_manager,
		CapacityStream<const Reflection::ReflectionType*>& unique_link_types,
		CapacityStream<const Reflection::ReflectionType*>& shared_link_types,
		CapacityStream<const Reflection::ReflectionType*>& global_link_types
	) {
		GetLinkComponentsImpl(reflection_manager, [&](const Reflection::ReflectionType* link_type, const Reflection::ReflectionType* target_type) {
			if (IsReflectionTypeComponent(target_type)) {
				unique_link_types.AddAssert(link_type);
			}
			else if (IsReflectionTypeSharedComponent(target_type)) {
				shared_link_types.AddAssert(link_type);
			}
			else if (IsReflectionTypeGlobalComponent(target_type)) {
				global_link_types.AddAssert(link_type);
			}
		});
	}

	// ------------------------------------------------------------------------------------------------------------

	bool ConvertFromTargetToLinkComponent(
		const ConvertToAndFromLinkBaseData* base_data,
		const void* target_data,
		void* link_data,
		const void* previous_target_data,
		const void* previous_link_data
	)
	{
		// Determine if the link component has a DLL function
		if (base_data->module_link.build_function == nullptr || base_data->module_link.reverse_function == nullptr) {
			// Fail
			return false;
		}

		// Use the function
		ModuleLinkComponentReverseFunctionData reverse_data;
		reverse_data.link_component = link_data;
		reverse_data.component = target_data;

		// These are forwarded
		reverse_data.previous_component = previous_target_data;
		reverse_data.previous_link_component = previous_link_data;
		base_data->module_link.reverse_function(&reverse_data);
		return true;
	}

	// ------------------------------------------------------------------------------------------------------------

	// The functor needs to handle the non asset fields
	template<typename Functor>
	bool ConvertLinkComponentToTargetImpl(
		const ConvertToAndFromLinkBaseData* base_data,
		const void* link_data,
		void* target_data,
		const void* previous_link_data,
		const void* previous_target_data,
		Functor&& functor
	) {
		// Determine if the link component has a DLL function
		if (base_data->module_link.build_function == nullptr || base_data->module_link.reverse_function == nullptr) {
			// Fail
			return false;
		}

		// Build it using the function
		ModuleLinkComponentFunctionData build_data;
		build_data.component = target_data;
		build_data.link_component = link_data;
		build_data.component_allocator = base_data->allocator;

		// These are forwarded
		build_data.previous_component = previous_target_data;
		build_data.previous_link_component = previous_link_data;
		base_data->module_link.build_function(&build_data);
		
		return true;
	}

	bool ConvertLinkComponentToTarget(
		const ConvertToAndFromLinkBaseData* base_data,
		const void* link_data,
		void* target_data,
		const void* previous_link_data,
		const void* previous_target_data
	)
	{
		auto copy_field = [&](size_t index) {
			if (!base_data->target_type->fields[index].Compare(base_data->link_type->fields[index])) {
				return false;
			}

			const void* source = OffsetPointer(link_data, base_data->link_type->fields[index].info.pointer_offset);
			void* destination = OffsetPointer(target_data, base_data->target_type->fields[index].info.pointer_offset);

			if (base_data->allocator.allocator != nullptr) {
				Reflection::CopyReflectionDataOptions copy_options;
				copy_options.allocator = base_data->allocator;
				copy_options.always_allocate_for_buffers = true;
				CopyReflectionTypeInstance(
					base_data->reflection_manager,
					base_data->target_type->fields[index].definition,
					source,
					destination,
					&copy_options
				);
			}
			else {
				memcpy(
					destination,
					source,
					base_data->target_type->fields[index].info.byte_size
				);
			}
			return true;
		};
		return ConvertLinkComponentToTargetImpl(base_data, link_data, target_data, previous_link_data, previous_target_data, copy_field);
	}

	// ------------------------------------------------------------------------------------------------------------

	bool ConvertLinkComponentToTargetAssetsOnly(
		const ConvertToAndFromLinkBaseData* base_data, 
		const void* link_data, 
		void* target_data,
		const void* previous_link_data,
		const void* previous_target_data
	)
	{
		return ConvertLinkComponentToTargetImpl(base_data, link_data, target_data, previous_link_data, previous_target_data, [](size_t index) { return true; });
	}

	// ------------------------------------------------------------------------------------------------------------

}