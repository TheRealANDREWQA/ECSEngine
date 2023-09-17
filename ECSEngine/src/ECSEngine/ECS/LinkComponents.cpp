#include "ecspch.h"
#include "LinkComponents.h"
#include "../Utilities/Reflection/Reflection.h"
#include "../Resources/AssetDatabase.h"
#include "../Utilities/Serialization/SerializationHelpers.h"
#include "ComponentHelpers.h"

#define LINK_COMPONENT_SUFFIX "Link"
#define TAG_GENERATED_SUFFIX "_GENERATED"

/*
	For link types automatically created by the ECS engine
	Put an _GENERATED in the link target name such that we can distinguish from
	User link components without affecting other search functions or conventions
	We need the generated and user link components to have the same name ending
	And to use the same type tag LINK_COMPONENT()
*/

namespace ECSEngine {

	// ----------------------------------------------------------------------------------------------------------------------------

	bool GetReflectionTypeLinkComponentNeedsDLL(const Reflection::ReflectionType* type)
	{
		return function::FindFirstToken(type->tag, TAG_GENERATED_SUFFIX).size == 0;
	}

	// ----------------------------------------------------------------------------------------------------------------------------

	Stream<char> GetReflectionTypeLinkComponentTarget(const Reflection::ReflectionType* type)
	{
		Stream<char> opened_parenthese = function::FindFirstCharacter(type->tag, '(');
		if (opened_parenthese.size == 0) {
			return { nullptr, 0 };
		}

		opened_parenthese.buffer += 1;
		opened_parenthese.size -= 1;
		opened_parenthese = function::SkipWhitespace(opened_parenthese);

		Stream<char> generated_suffix = function::FindFirstToken(opened_parenthese, TAG_GENERATED_SUFFIX);
		if (generated_suffix.size > 0) {
			opened_parenthese.size = generated_suffix.buffer - opened_parenthese.buffer;
			opened_parenthese = function::SkipWhitespace(opened_parenthese, -1);
		}
		else {
			if (opened_parenthese[opened_parenthese.size - 1] == ')') {
				opened_parenthese.size--;
			}
			opened_parenthese = function::SkipWhitespace(opened_parenthese, -1);
		}
		return opened_parenthese;
	}

	// ----------------------------------------------------------------------------------------------------------------------------

	Stream<char> GetReflectionTypeLinkNameBase(Stream<char> name)
	{
		if (name.size > 0) {
			Stream<char> found_token = function::FindFirstToken(name, LINK_COMPONENT_SUFFIX);
			if (found_token.size > 0) {
				return { name.buffer, name.size - found_token.size };
			}
		}
		return name;
	}

	// ----------------------------------------------------------------------------------------------------------------------------

	Stream<char> GetReflectionTypeLinkComponentName(Stream<char> name, CapacityStream<char>& link_name)
	{
		link_name.Copy(name);
		link_name.AddStream(LINK_COMPONENT_SUFFIX);
		return link_name;
	}

	// ------------------------------------------------------------------------------------------------------------

	Reflection::ReflectionType CreateLinkTypeForComponent(
		const Reflection::ReflectionManager* reflection_manager,
		const Reflection::ReflectionType* type, 
		AllocatorPolymorphic allocator, 
		bool coallesced_allocation
	)
	{
		Reflection::ReflectionType result;

		result.folder_hierarchy_index = type->folder_hierarchy_index;

		ECS_STACK_CAPACITY_STREAM(LinkComponentAssetField, asset_fields, 512);
		ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(_stack_allocator, ECS_KB * 64, ECS_MB);
		AllocatorPolymorphic stack_allocator = GetAllocatorPolymorphic(&_stack_allocator);
		
		GetAssetFieldsFromLinkComponentTarget(type, asset_fields);

		AllocatorPolymorphic current_allocator = allocator;
		if (coallesced_allocation) {
			current_allocator = stack_allocator;		
		}

		result.fields.Initialize(current_allocator, type->fields.size);
		result.name.Initialize(current_allocator, type->name.size + sizeof(LINK_COMPONENT_SUFFIX) - 1);
		result.name.Copy(type->name);
		result.name.AddStream(LINK_COMPONENT_SUFFIX);
		// We also need to add the tag and the parentheses
		result.tag.Initialize(current_allocator, sizeof(ECS_LINK_COMPONENT_TAG) + type->name.size + 2);
		result.tag.Copy(ECS_LINK_COMPONENT_TAG);
		result.tag.Add('(');
		result.tag.AddStream(type->name);
		result.tag.AddStream(TAG_GENERATED_SUFFIX);
		result.tag.Add(')');

		result.evaluations = { nullptr, 0 };

		unsigned short current_pointer_offset = 0;

		for (unsigned int index = 0; index < (unsigned int)type->fields.size; index++) {
			// Check to see if it is an asset field
			unsigned int subindex = 0;
			for (; subindex < asset_fields.size; subindex++) {
				if (asset_fields[subindex].field_index == index) {
					break;
				}
			}

			if (subindex < asset_fields.size) {
				current_pointer_offset = function::AlignPointer(current_pointer_offset, alignof(unsigned int));

				result.fields[index].definition = "unsigned int";
				result.fields[index].info.basic_type = Reflection::ReflectionBasicFieldType::UInt32;
				result.fields[index].info.basic_type_count = 1;
				result.fields[index].info.stream_byte_size = 0;
				result.fields[index].info.default_uint = -1;
				result.fields[index].info.has_default_value = true;
				result.fields[index].info.stream_type = Reflection::ReflectionStreamFieldType::Basic;
				result.fields[index].info.pointer_offset = current_pointer_offset;
				result.fields[index].info.byte_size = sizeof(unsigned int);
				result.fields[index].name = coallesced_allocation ? type->fields[index].name : function::StringCopy(allocator, type->fields[index].name);

				current_pointer_offset += sizeof(unsigned int);

#define TAG(handle_name) result.fields[index].tag = coallesced_allocation ? Stream<char>(STRING(handle_name)) : function::StringCopy(allocator, Stream<char>(STRING(handle_name)));

				// It is an asset field
				switch (asset_fields[subindex].type.type) {
				case ECS_ASSET_MESH:
					TAG(ECS_MESH_HANDLE);
					break;
				case ECS_ASSET_TEXTURE:
					TAG(ECS_TEXTURE_HANDLE);
					break;
				case ECS_ASSET_GPU_SAMPLER:
					TAG(ECS_GPU_SAMPLER_HANDLE);
					break;
				case ECS_ASSET_SHADER:
					switch (asset_fields[subindex].type.shader_type) {
					case ECS_SHADER_VERTEX:
						TAG(ECS_VERTEX_SHADER_HANDLE);
						break;
					case ECS_SHADER_PIXEL:
						TAG(ECS_PIXEL_SHADER_HANDLE);
						break;
					case ECS_SHADER_COMPUTE:
						TAG(ECS_COMPUTE_SHADER_HANDLE);
						break;
					case ECS_SHADER_TYPE_COUNT:
						TAG(ECS_SHADER_HANDLE);
						break;
					}
					break;
				case ECS_ASSET_MATERIAL:
					TAG(ECS_MATERIAL_HANDLE);
					break;
				case ECS_ASSET_MISC:
					TAG(ECS_MISC_HANDLE);
					break;
				default:
					ECS_ASSERT(false, "Invalid asset type");
				}
			}
			else {
				// Normal field, just copy it
				size_t alignment = GetFieldTypeAlignmentEx(reflection_manager, type->fields[index]);
				current_pointer_offset = function::AlignPointer(current_pointer_offset, alignment);

				if (coallesced_allocation) {
					result.fields[index] = type->fields[index];
				}
				else {
					result.fields[index] = type->fields[index].Copy(allocator);
				}
				result.fields[index].info.pointer_offset = current_pointer_offset;
				current_pointer_offset += result.fields[index].info.byte_size;
			}
		}

#undef TAG

		if (coallesced_allocation) {
			size_t copy_size = result.CopySize();
			void* allocation = Allocate(allocator, copy_size);
			uintptr_t ptr = (uintptr_t)allocation;
			result = result.CopyTo(ptr);
		}

		result.byte_size = Reflection::CalculateReflectionTypeByteSize(reflection_manager, &result);
		result.alignment = Reflection::CalculateReflectionTypeAlignment(reflection_manager, &result);
		result.is_blittable = Reflection::SearchIsBlittable(reflection_manager, &result);
		result.is_blittable_with_pointer = Reflection::SearchIsBlittableWithPointer(reflection_manager, &result);

		return result;
	}

	// ------------------------------------------------------------------------------------------------------------

	bool IsLinkTypeDefaultGeneratedForComponent(
		const Reflection::ReflectionType* target_type, 
		const Reflection::ReflectionType* link_type
	)
	{
		if (link_type->fields.size != target_type->fields.size) {
			return false;
		}

		ECS_STACK_CAPACITY_STREAM(LinkComponentAssetField, target_asset_fields, 512);
		ECS_STACK_CAPACITY_STREAM(LinkComponentAssetField, link_asset_fields, 512);
		
		GetAssetFieldsFromLinkComponentTarget(target_type, target_asset_fields);
		GetAssetFieldsFromLinkComponent(link_type, link_asset_fields);

		if (target_asset_fields.size != link_asset_fields.size) {
			return false;
		}

		// Same asset fields
		for (unsigned int index = 0; index < target_asset_fields.size; index++) {
			if (target_asset_fields[index].field_index != link_asset_fields[index].field_index ||
				target_asset_fields[index].type.type != link_asset_fields[index].type.type) {
				return false;
			}
		}

		// Now check the other fields
		for (size_t index = 0; index < target_type->fields.size; index++) {
			unsigned int subindex = 0;
			for (; subindex < target_asset_fields.size; subindex++) {
				if (target_asset_fields[subindex].field_index == (unsigned int)index) {
					break;
				}
			}

			if (subindex == target_asset_fields.size) {
				// Test the fields to be the same
				if (target_type->fields[index].definition != link_type->fields[index].definition) {
					return false;
				}
			}
		}

		return true;
	}

	// ------------------------------------------------------------------------------------------------------------

	void CreateLinkTypesForComponents(Reflection::ReflectionManager* reflection_manager, unsigned int folder_index, CreateLinkTypesForComponentsOptions* options)
	{
		ECS_STACK_CAPACITY_STREAM(unsigned int, unique_indices, 1024);
		ECS_STACK_CAPACITY_STREAM(unsigned int, shared_indices, 1024);
		ECS_STACK_CAPACITY_STREAM(unsigned int, global_indices, 1024);

		GetHierarchyComponentTypes(reflection_manager, folder_index, &unique_indices, &shared_indices, &global_indices);

		// Determine the unique and shared components that actually require a link type
		auto reduce_loop = [=](CapacityStream<unsigned int>& indices) {
			ECS_STACK_CAPACITY_STREAM(LinkComponentAssetField, asset_fields, 128);
			for (unsigned int index = 0; index < indices.size; index++) {
				asset_fields.size = 0;
				const Reflection::ReflectionType* type = reflection_manager->GetType(indices[index]);
				bool success = GetAssetFieldsFromLinkComponentTarget(type, asset_fields);

				// It has no asset fields
				if (success && asset_fields.size == 0) {
					indices.RemoveSwapBack(index);
					index--;
				}
			}
		};

		reduce_loop(unique_indices);
		reduce_loop(shared_indices);
		reduce_loop(global_indices);

		unsigned int total_count = unique_indices.size + shared_indices.size + global_indices.size;
		ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 64, ECS_MB);
		Stream<char>* target_names = (Stream<char>*)stack_allocator.Allocate(sizeof(Stream<char>) * total_count);
		Stream<char>* link_names = (Stream<char>*)stack_allocator.Allocate(sizeof(Stream<char>) * total_count);

		unsigned int target_name_count = 0;
		
		Stream<Stream<char>> exceptions = { nullptr, 0 };
		if (options != nullptr) {
			exceptions = options->exceptions;
		}

		auto register_names = [&](Stream<unsigned int> indices) {
			for (unsigned int index = 0; index < indices.size; index++) {
				target_names[target_name_count] = reflection_manager->GetType(indices[index])->name;
				if (exceptions.size == 0 || function::FindString(target_names[target_name_count], exceptions) == -1) {
					target_name_count++;
				}
			}
		};

		register_names(unique_indices);
		register_names(shared_indices);
		register_names(global_indices);

		GetLinkComponentForTargets(reflection_manager, { target_names, target_name_count }, link_names, LINK_COMPONENT_SUFFIX);
		AllocatorPolymorphic allocator = reflection_manager->Allocator();
		if (options != nullptr && options->allocator.allocator != nullptr) {
			allocator = options->allocator;
		}

		for (unsigned int index = 0; index < target_name_count; index++) {
			if (link_names[index].size == 0) {
				// There is no link and needs asset fields
				Reflection::ReflectionType link_type = CreateLinkTypeForComponent(
					reflection_manager,
					reflection_manager->GetType(target_names[index]),
					allocator,
					true
				);

				reflection_manager->AddTypeToHierarchy(&link_type, folder_index, allocator, true);
			}
		}
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
				link_name.Copy(targets[index]);
				link_name.AddStream(suffixed_name);

				Reflection::ReflectionType link_type;
				if (reflection_manager->TryGetType(link_name, link_type)) {
					link_names[index] = link_type.name;
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
			get_query.tags = { tag, std::size(tag) };
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

	// The functor takes as parameter a size_t for the field index and must return AssetTypeEx
	template<typename Functor>
	void GetLinkComponentMissingAssetFieldsImplementation(
		const Reflection::ReflectionType* previous_type,
		const Reflection::ReflectionType* new_type,
		CapacityStream<LinkComponentAssetField>* missing_fields,
		Functor&& functor
	) {
		ECS_STACK_CAPACITY_STREAM(size_t, missing_field_indices, 512);
		Reflection::GetReflectionTypesDifferentFields(previous_type, new_type, &missing_field_indices);

		for (unsigned int index = 0; index < missing_field_indices.size; index++) {
			AssetTypeEx asset_type = functor(missing_field_indices[index]);
			if (asset_type.type != ECS_ASSET_TYPE_COUNT) {
				missing_fields->AddAssert({ (unsigned int)missing_field_indices[index], asset_type });
			}
		}
	}

	void GetLinkComponentMissingAssetFields(
		const Reflection::ReflectionType* previous_type, 
		const Reflection::ReflectionType* new_type, 
		CapacityStream<LinkComponentAssetField>* missing_fields
	)
	{
		GetLinkComponentMissingAssetFieldsImplementation(previous_type, new_type, missing_fields, [previous_type](size_t field_index) {
			return FindAssetMetadataMacro(previous_type->fields[field_index].tag);
		});
	}

	void GetLinkComponentTargetMissingAssetFields(
		const Reflection::ReflectionType* previous_type, 
		const Reflection::ReflectionType* new_type, 
		CapacityStream<LinkComponentAssetField>* missing_fields
	)
	{
		GetLinkComponentMissingAssetFieldsImplementation(previous_type, new_type, missing_fields, [previous_type](size_t field_index) {
			AssetTargetFieldFromReflection asset_field = GetAssetTargetFieldFromReflection(previous_type, field_index, nullptr);
			AssetTypeEx missing_type_ex;
			missing_type_ex.type = ECS_ASSET_TYPE_COUNT;
			return asset_field.success ? asset_field.type : missing_type_ex;
		});
	}

	// ------------------------------------------------------------------------------------------------------------

	void GetAssetFieldsFromLinkComponent(const Reflection::ReflectionType* type, CapacityStream<LinkComponentAssetField>& field_indices)
	{
		for (size_t index = 0; index < type->fields.size; index++) {
			AssetTypeEx asset_type = FindAssetMetadataMacro(type->fields[index].tag);
			if (asset_type.type != ECS_ASSET_TYPE_COUNT) {
				field_indices.AddAssert({ (unsigned int)index, asset_type });
			}
		}
	}

	// ------------------------------------------------------------------------------------------------------------

	bool GetAssetFieldsFromLinkComponentTarget(const Reflection::ReflectionType* type, CapacityStream<LinkComponentAssetField>& field_indices)
	{
		for (size_t index = 0; index < type->fields.size; index++) {
			AssetTargetFieldFromReflection asset_field = GetAssetTargetFieldFromReflection(type, index, nullptr);

			if (!asset_field.success) {
				return false;
			}
			else if (asset_field.type.type != ECS_ASSET_TYPE_COUNT) {
				field_indices.Add({ (unsigned int)index, asset_field.type });
			}
		}
		return true;
	}

	// ------------------------------------------------------------------------------------------------------------

	bool HasAssetFieldsLinkComponent(const Reflection::ReflectionType* type)
	{
		for (size_t index = 0; index < type->fields.size; index++) {
			AssetTypeEx asset_type = FindAssetMetadataMacro(type->fields[index].tag);
			if (asset_type.type != ECS_ASSET_TYPE_COUNT) {
				return true;
			}
		}
		return false;
	}

	// ------------------------------------------------------------------------------------------------------------

	bool HasAssetFieldsTargetComponent(const Reflection::ReflectionType* type)
	{
		for (size_t index = 0; index < type->fields.size; index++) {
			AssetTargetFieldFromReflection asset_field = GetAssetTargetFieldFromReflection(type, index, nullptr);

			if (!asset_field.success) {
				return false;
			}
			else if (asset_field.type.type != ECS_ASSET_TYPE_COUNT) {
				return true;
			}
		}
		return true;
	}

	// ------------------------------------------------------------------------------------------------------------

	AssetTargetFieldFromReflection GetAssetTargetFieldFromReflection(
		const Reflection::ReflectionType* type,
		unsigned int field,
		const void* data
	)
	{
		AssetTargetFieldFromReflection result;

		result.type.type = ECS_ASSET_TYPE_COUNT;

		// 2 means there is no match. It will be set to 1 if there is a match with a succesful
		// type or 0 if there is match but incorrect field type
		bool success = true;

		Stream<char> definition = type->fields[field].definition;
		const Reflection::ReflectionFieldInfo* info = &type->fields[field].info;
		
		size_t mapping_count = ECS_ASSET_TARGET_FIELD_NAMES_SIZE();
		for (size_t index = 0; index < mapping_count; index++) {
			if (function::CompareStrings(definition, ECS_ASSET_TARGET_FIELD_NAMES[index].name)) {
				result.type = ECS_ASSET_TARGET_FIELD_NAMES[index].type;
				if (data != nullptr) {
					result.asset = GetAssetTargetFieldFromReflection(type, field, data, result.type.type);
					success = result.asset.size != -1;
				}
				else {
					success = true;
				}
				break;
			}
		}
		
		result.success = success;
		return result;
	}

	// ------------------------------------------------------------------------------------------------------------

	Stream<void> GetAssetTargetFieldFromReflection(const Reflection::ReflectionType* type, unsigned int field, const void* data, ECS_ASSET_TYPE asset_type)
	{
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
		size_t pointer_size = -1;

		bool success = get_pointer_from_field(&type->fields[field].info, &pointer);

		if (success) {
			if (asset_type == ECS_ASSET_TEXTURE || asset_type == ECS_ASSET_GPU_SAMPLER || asset_type == ECS_ASSET_SHADER) {
				// The interface is one level deeper - unless the pointer is nullptr
				pointer = *(const void**)pointer;
			}
			else if (asset_type == ECS_ASSET_MISC) {
				// Extract the size and the pointer is deeper
				pointer_size = *(size_t*)function::OffsetPointer(pointer, sizeof(void*));
				pointer = *(void**)pointer;
			}

			// Materials and meshes should have the pointer extraction correctly applied
			
			if (asset_type != ECS_ASSET_MISC) {
				// Indicate that the retrieval was successfull
				pointer_size = 0;
			}
		}

		return { pointer, pointer_size };
	}

	// ------------------------------------------------------------------------------------------------------------

	void GetLinkComponentTargetHandles(
		const Reflection::ReflectionType* type, 
		const AssetDatabase* asset_database,
		const void* data,
		Stream<LinkComponentAssetField> asset_fields, 
		unsigned int* handles
	)
	{
		for (size_t index = 0; index < asset_fields.size; index++) {
			Stream<void> field_data = GetAssetTargetFieldFromReflection(type, asset_fields[index].field_index, data, asset_fields[index].type.type);
			unsigned int handle = asset_database->FindAssetEx(field_data, asset_fields[index].type.type);
			handles[index] = handle;
		}
	}

	// ------------------------------------------------------------------------------------------------------------

	template<typename Comparator>
	ECS_SET_ASSET_TARGET_FIELD_RESULT SetAssetTargetFieldFromReflectionImpl(
		const Reflection::ReflectionType* type,
		unsigned int field,
		void* data,
		Stream<void> field_data,
		ECS_ASSET_TYPE field_type,
		Comparator&& comparator
	) {
		ECS_ASSET_TYPE current_field_type = FindAssetTargetField(type->fields[field].definition).type;
		if (field_type != current_field_type) {
			return ECS_SET_ASSET_TARGET_FIELD_NONE;
		}

		void* ptr_to_write = function::OffsetPointer(data, type->fields[field].info.pointer_offset);

		auto copy_value = [&](const void* ptr_to_copy, size_t copy_size) {
			if (!comparator(ptr_to_write)) {
				return ECS_SET_ASSET_TARGET_FIELD_OK;
			}

			if ((unsigned int)ptr_to_copy >= ECS_ASSET_RANDOMIZED_ASSET_LIMIT) {
				memcpy(ptr_to_write, ptr_to_copy, copy_size);
			}
			else {
				memset(ptr_to_write, 0, copy_size);
				unsigned int* randomized_value = (unsigned int*)ptr_to_write;
				*randomized_value = (unsigned int)ptr_to_copy;
			}
			return ECS_SET_ASSET_TARGET_FIELD_MATCHED;
		};

		auto set_pointer = [&](void* ptr_to_set) {
			void** ptr = (void**)ptr_to_write;
			if (!comparator(*ptr)) {
				return ECS_SET_ASSET_TARGET_FIELD_OK;
			}
			*ptr = ptr_to_set;
			return ECS_SET_ASSET_TARGET_FIELD_MATCHED;
		};

		auto copy_and_pointer = [&](size_t copy_size) {
			if (type->fields[field].info.stream_type == Reflection::ReflectionStreamFieldType::Basic) {
				// Copy the data
				return copy_value(field_data.buffer, copy_size);
			}
			else if (type->fields[field].info.stream_type == Reflection::ReflectionStreamFieldType::Pointer) {
				if (Reflection::GetReflectionFieldPointerIndirection(type->fields[field].info) == 1) {
					return set_pointer(field_data.buffer);
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
		case ECS_ASSET_MATERIAL:
		{
			if (type->fields[field].info.stream_type == Reflection::ReflectionStreamFieldType::Pointer) {
				return set_pointer(field_data.buffer);
			}
			else {
				return ECS_SET_ASSET_TARGET_FIELD_FAILED;
			}
		}
		break;
		case ECS_ASSET_TEXTURE:
		case ECS_ASSET_GPU_SAMPLER:
		case ECS_ASSET_SHADER:
		{
			if (type->fields[field].info.stream_type == Reflection::ReflectionStreamFieldType::Basic) {
				return set_pointer(field_data.buffer);
			}
			else {
				return ECS_SET_ASSET_TARGET_FIELD_FAILED;
			}
		}
		break;
		case ECS_ASSET_MISC:
		{
			if (type->fields[field].info.stream_type != Reflection::ReflectionStreamFieldType::Pointer) {
				// It is a Stream<void>, copy into it
				return copy_value(field_data.buffer, field_data.size);
			}
			else if (type->fields[field].info.stream_type == Reflection::ReflectionStreamFieldType::Pointer) {
				if (Reflection::GetReflectionFieldPointerIndirection(type->fields[field].info) == 1) {
					return set_pointer(&field_data);
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
		default:
			ECS_ASSERT(false, "Invalid asset type");
		}

		return ECS_SET_ASSET_TARGET_FIELD_OK;
	}

	ECS_SET_ASSET_TARGET_FIELD_RESULT SetAssetTargetFieldFromReflection(
		const Reflection::ReflectionType* type,
		unsigned int field,
		void* data,
		Stream<void> field_data,
		ECS_ASSET_TYPE field_type
	)
	{
		return SetAssetTargetFieldFromReflectionImpl(type, field, data, field_data, field_type, [](const void* ptr) { return true; });
	}

	// ------------------------------------------------------------------------------------------------------------

	ECS_SET_ASSET_TARGET_FIELD_RESULT SetAssetTargetFieldFromReflectionIfMatches(
		const Reflection::ReflectionType* type, 
		unsigned int field, 
		void* data, 
		Stream<void> field_data, 
		ECS_ASSET_TYPE field_type, 
		Stream<void> comparator
	)
	{
		return SetAssetTargetFieldFromReflectionImpl(type, field, data, field_data, field_type, [=](void* ptr) {
			switch (field_type) {
			case ECS_ASSET_MESH:
			case ECS_ASSET_MATERIAL:
			case ECS_ASSET_TEXTURE: 
			case ECS_ASSET_GPU_SAMPLER:
			case ECS_ASSET_SHADER: 
			{
				return ptr == comparator.buffer;
			}
			case ECS_ASSET_MISC:
			{
				Stream<void> current_ptr = *(Stream<void>*)ptr;
				return current_ptr.buffer == comparator.buffer && current_ptr.size == comparator.size;
			}
			break;
			}

			// Shouldn't reach here
			return false;
		});
	}

	// ------------------------------------------------------------------------------------------------------------

	template<typename Functor>
	void GetLinkComponentHandlesImpl(
		const Reflection::ReflectionType* type, 
		const void* link_components, 
		size_t count, 
		CapacityStream<unsigned int>& handles, 
		Functor&& functor
	) {
		for (size_t index = 0; index < count; index++) {
			unsigned int field = functor(index);
			handles.AddAssert((unsigned int*)function::OffsetPointer(link_components, type->fields[field].info.pointer_offset));
		}
	}

	void GetLinkComponentHandles(
		const Reflection::ReflectionType* type,
		const void* link_component, 
		Stream<unsigned int> field_indices, 
		CapacityStream<unsigned int>& handles
	)
	{
		GetLinkComponentHandlesImpl(type, link_component, field_indices.size, handles, [field_indices](size_t index) {
			return field_indices[index];
		});
	}

	// ------------------------------------------------------------------------------------------------------------

	void GetLinkComponentHandles(
		const Reflection::ReflectionType* type,
		const void* link_component, 
		Stream<LinkComponentAssetField> field_indices, 
		CapacityStream<unsigned int>& handles
	)
	{
		GetLinkComponentHandlesImpl(type, link_component, field_indices.size, handles, [field_indices](size_t index) {
			return field_indices[index].field_index;
		});
	}

	// ------------------------------------------------------------------------------------------------------------

	template<typename Functor>
	void GetLinkComponentHandlePtrsImpl(
		const Reflection::ReflectionType* type,
		const void* link_component,
		size_t count,
		CapacityStream<unsigned int*>& pointers,
		Functor&& functor
	) {
		for (size_t index = 0; index < count; index++) {
			unsigned int field_index = functor(index);
			pointers.AddAssert((unsigned int*)function::OffsetPointer(link_component, type->fields[field_index].info.pointer_offset));
		}
	}

	void GetLinkComponentHandlePtrs(
		const Reflection::ReflectionType* type,
		const void* link_component,
		Stream<unsigned int> field_indices,
		CapacityStream<unsigned int*>& pointers
	) {
		GetLinkComponentHandlePtrsImpl(type, link_component, field_indices.size, pointers, [field_indices](size_t index) {
			return field_indices[index];
		});
	}

	// ------------------------------------------------------------------------------------------------------------

	void GetLinkComponentHandlePtrs(
		const Reflection::ReflectionType* type,
		const void* link_component,
		Stream<LinkComponentAssetField> field_indices,
		CapacityStream<unsigned int*>& pointers
	) {
		GetLinkComponentHandlePtrsImpl(type, link_component, field_indices.size, pointers, [field_indices](size_t index) {
			return field_indices[index].field_index;
		});
	}

	// ------------------------------------------------------------------------------------------------------------

	void GetLinkComponentAssetData(
		const Reflection::ReflectionType* type,
		const void* link_component,
		const AssetDatabase* database, 
		Stream<LinkComponentAssetField> asset_fields,
		CapacityStream<Stream<void>>* field_data
	)
	{
		constexpr size_t static_data_size = std::max(std::max(std::max(std::max(std::max(sizeof(MeshMetadata), sizeof(TextureMetadata)), 
			sizeof(GPUSamplerMetadata)), sizeof(ShaderMetadata)), sizeof(MaterialAsset)), sizeof(MiscAsset));
		static const char empty_metadata[static_data_size] = { 0 };

		ECS_ASSERT(field_data->capacity - field_data->size >= asset_fields.size);

		for (size_t index = 0; index < asset_fields.size; index++) {
			unsigned int handle = *(unsigned int*)function::OffsetPointer(link_component, type->fields[asset_fields[index].field_index].info.pointer_offset);
			const void* metadata = nullptr;
			if (handle == -1) {
				metadata = empty_metadata;
			}
			else {
				metadata = database->GetAssetConst(handle, asset_fields[index].type.type);
			}
			Stream<void> asset_data = GetAssetFromMetadata(metadata, asset_fields[index].type.type);
			field_data->Add(asset_data);
		}
	}

	// ------------------------------------------------------------------------------------------------------------

	void GetLinkComponentAssetDataForTarget(
		const Reflection::ReflectionType* type, 
		const void* target, 
		Stream<LinkComponentAssetField> asset_fields, 
		CapacityStream<Stream<void>>* field_data
	)
	{
		ECS_ASSERT(field_data->capacity - field_data->size >= asset_fields.size);

		for (size_t index = 0; index < asset_fields.size; index++) {
			AssetTargetFieldFromReflection target_field = GetAssetTargetFieldFromReflection(type, asset_fields[index].field_index, target);
			ECS_ASSERT(target_field.type.type == asset_fields[index].type.type && target_field.success);
			field_data->Add(target_field.asset);
		}
	}

	// ------------------------------------------------------------------------------------------------------------

	void GetLinkComponentAssetDataForTargetDeep(
		const Reflection::ReflectionType* type,
		const void* target,
		Stream<LinkComponentAssetField> asset_fields,
		const AssetDatabase* database,
		ECS_ASSET_TYPE asset_type,
		CapacityStream<Stream<void>>* field_data
	) {
		for (size_t index = 0; index < asset_fields.size; index++) {
			AssetTargetFieldFromReflection target_field = GetAssetTargetFieldFromReflection(type, asset_fields[index].field_index, target);
			ECS_ASSERT(target_field.type.type == asset_fields[index].type.type && target_field.success);
			if (asset_fields[index].type.type == asset_type) {
				field_data->AddAssert(target_field.asset);
			}
			else {
				// If the asset type is a texture or gpu sampler or shader, it can be referenced by a material
				if (IsAssetTypeReferenceable(asset_type) && IsAssetTypeWithDependencies(asset_fields[index].type.type)) {
					unsigned int handle = database->FindAssetEx(target_field.asset, target_field.type.type);
					if (handle != -1) {
						// Normally, if the asset is created, all handles should be valid (aka -1)
						// But let's be conservative and still check for it. Also if it is set and the asset
						// doesn't exist in the database skip it
						ECS_STACK_CAPACITY_STREAM(AssetTypedHandle, dependencies, 512);
						GetAssetDependencies(database->GetAssetConst(handle, target_field.type.type), target_field.type.type, &dependencies);

						for (unsigned int subindex = 0; subindex < dependencies.size; subindex++) {
							unsigned int current_handle = dependencies[subindex].handle;
							if (dependencies[subindex].type == asset_type && current_handle != -1) {
								if (database->Exists(current_handle, asset_type)) {
									field_data->AddAssert(GetAssetFromMetadata(database->GetAssetConst(current_handle, asset_type), asset_type));
								}
							}
						}
					}
				}
			}
		}
	}

	// ------------------------------------------------------------------------------------------------------------

	bool ValidateLinkComponent(const Reflection::ReflectionType* link_type, const Reflection::ReflectionType* target_type)
	{
		bool needs_dll = GetReflectionTypeLinkComponentNeedsDLL(link_type);
		if (needs_dll) {
			// If user supplied then assume true
			return true;
		}

		if (link_type->fields.size != target_type->fields.size) {
			return false;
		}

		for (size_t index = 0; index < link_type->fields.size; index++) {
			AssetTypeEx asset_type = FindAssetMetadataMacro(link_type->fields[index].tag);
			if (asset_type.type != ECS_ASSET_TYPE_COUNT) {
				// We have an asset handle - verify that the corresponding asset is in the target
				AssetTargetFieldFromReflection target_field = GetAssetTargetFieldFromReflection(target_type, index, nullptr);
				if (!target_field.success || target_field.type.type != asset_type.type) {
					return false;
				}
			}
			else {
				// Not an asset. Verify that they are identical.
				if (!link_type->fields[index].Compare(target_type->fields[index])) {
					return false;
				}
			}
		}

		return true;
	}

	// ------------------------------------------------------------------------------------------------------------

	bool ValidateLinkComponentTarget(const Reflection::ReflectionType* link_type, ModuleLinkComponentTarget target)
	{
		bool has_modifier_fields = false;
		for (size_t index = 0; index < link_type->fields.size && !has_modifier_fields; index++) {
			if (link_type->fields[index].GetTag(STRING(ECS_LINK_MODIFIER_FIELD)).size > 0) {
				has_modifier_fields = true;
			}
		}

		if (has_modifier_fields && target.apply_modifier == nullptr) {
			return false;
		}
		return true;
	}

	// ------------------------------------------------------------------------------------------------------------

	void ResetLinkComponent(
		const Reflection::ReflectionManager* reflection_manager,
		const Reflection::ReflectionType* type,
		void* link_component
	)
	{
		reflection_manager->SetInstanceDefaultData(type, link_component);
		// Make all handles -1
		for (size_t index = 0; index < type->fields.size; index++) {
			if (FindAssetMetadataMacro(type->fields[index].tag).type != ECS_ASSET_TYPE_COUNT) {
				// It is a handle
				unsigned int* handle = (unsigned int*)function::OffsetPointer(link_component, type->fields[index].info.pointer_offset);
				*handle = -1;
			}
		}
	}

	// ------------------------------------------------------------------------------------------------------------

	void GetLinkComponentIndices(const Reflection::ReflectionManager* reflection_manager, CapacityStream<unsigned int>& indices) {
		Reflection::ReflectionManagerGetQuery query;
		query.indices = &indices;
		query.strict_compare = false;

		Stream<char> tags[] = {
			ECS_LINK_COMPONENT_TAG
		};
		query.tags = { tags, std::size(tags) };
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

			Reflection::ReflectionType target_type;
			if (reflection_manager->TryGetType(target, target_type)) {
				functor(link_type, &target_type);
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
		bool needs_dll = GetReflectionTypeLinkComponentNeedsDLL(base_data->link_type);
		if (needs_dll && (base_data->module_link.build_function == nullptr || base_data->module_link.reverse_function == nullptr)) {
			// Fail
			return false;
		}

		bool success = true;
		// Extract the handles of the target
		ECS_STACK_CAPACITY_STREAM(LinkComponentAssetField, asset_fields, 512);
		if (base_data->asset_fields.size == 0) {
			success = GetAssetFieldsFromLinkComponentTarget(base_data->target_type, asset_fields);
			if (!success) {
				return false;
			}

			if (!needs_dll) {
				// Verify that the link type has the same assets
				ECS_STACK_CAPACITY_STREAM(LinkComponentAssetField, link_asset_fields, 512);
				GetAssetFieldsFromLinkComponent(base_data->link_type, link_asset_fields);

				if (link_asset_fields.size != asset_fields.size) {
					return false;
				}

				for (size_t index = 0; index < asset_fields.size; index++) {
					if (link_asset_fields[index].field_index != asset_fields[index].field_index 
						|| link_asset_fields[index].type.type != asset_fields[index].type.type) {
						return false;
					}
				}
			}
		}
		else {
			asset_fields = base_data->asset_fields;
		}

		ECS_STACK_CAPACITY_STREAM(unsigned int, asset_handles, 512);
		GetLinkComponentTargetHandles(base_data->target_type, base_data->asset_database, target_data, asset_fields, asset_handles.buffer);
		asset_handles.size = asset_fields.size;

		// Needs DLL but the functions are present
		if (needs_dll) {
			// Use the function
			ModuleLinkComponentReverseFunctionData reverse_data;
			reverse_data.asset_handles = asset_handles;
			reverse_data.link_component = link_data;
			reverse_data.component = target_data;

			// These are forwarded
			reverse_data.previous_component = previous_target_data;
			reverse_data.previous_link_component = previous_link_data;
			base_data->module_link.reverse_function(&reverse_data);
		}
		else {
			const size_t MAX_LINK_SIZE = ECS_KB * 4;
			size_t link_type_size = Reflection::GetReflectionTypeByteSize(base_data->link_type);
			ECS_ASSERT(link_type_size < MAX_LINK_SIZE);
			size_t link_type_storage[MAX_LINK_SIZE / sizeof(size_t)];
			memcpy(link_type_storage, link_data, link_type_size);

			struct RestoreLinkData {
				void operator()() {
					if (link_type_size > 0) {
						memcpy(link_data, link_initial_storage, link_type_size);
					}
				}

				void* link_data;
				void* link_initial_storage;
				size_t link_type_size;
			};

			StackScope<RestoreLinkData> restore_link({ link_data, link_type_storage, link_type_size });

			// Fail if they have a different number of fields
			if (base_data->target_type->fields.size != base_data->link_type->fields.size) {
				return false;
			}

			//// Create map of the fields that should be restored - if the types are changed and some fields
			//// are missing, then copy only those that are matching and the rest default initialize
			//ECS_STACK_CAPACITY_STREAM(unsigned int, same_field_mask, 512);

			//for (size_t index = 0; index < base_data->link_type->fields.size; index++) {
			//	// Determine which fields are the same
			//	unsigned int target_index = base_data->target_type->FindField(base_data->link_type->fields[index].name);
			//	if (target_index != -1) {
			//		same_field_mask.AddAssert(target_index);
			//	}
			//}

			// Go through the link target and try to copy the fields
			ECS_STACK_CAPACITY_STREAM(unsigned int*, link_handle_ptrs, 512);
			GetLinkComponentHandlePtrs(base_data->link_type, link_data, asset_fields, link_handle_ptrs);
			for (unsigned int index = 0; index < asset_fields.size; index++) {
				*link_handle_ptrs[index] = asset_handles[index];
			}

			for (unsigned int index = 0; index < (unsigned int)base_data->link_type->fields.size; index++) {
				// Check to see if it an asset field
				unsigned int subindex = 0;
				for (; subindex < asset_fields.size; subindex++) {
					if (asset_fields[subindex].field_index == index) {
						break;
					}
				}

				if (subindex != asset_fields.size) {
					continue;
				}

				// Not an asset. Check to see that they have identical types
				if (!base_data->link_type->fields[index].Compare(base_data->target_type->fields[index])) {
					return false;
				}

				const void* source = function::OffsetPointer(target_data, base_data->target_type->fields[index].info.pointer_offset);
				void* destination = function::OffsetPointer(link_data, base_data->link_type->fields[index].info.pointer_offset);
				if (base_data->allocator.allocator != nullptr) {
					CopyReflectionType(
						base_data->reflection_manager,
						base_data->link_type->fields[index].definition,
						source,
						destination,
						base_data->allocator
					);
				}
				else {
					memcpy(
						destination,
						source,
						base_data->link_type->fields[index].info.byte_size
					);
				}
			}

			// Disable the restore
			restore_link.deallocator.link_type_size = 0;
		}

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
		bool use_apply_modifier_function,
		Functor&& functor
	) {
		// Determine if the link component has a DLL function
		bool needs_dll = GetReflectionTypeLinkComponentNeedsDLL(base_data->link_type);
		if (needs_dll && (base_data->module_link.build_function == nullptr || base_data->module_link.reverse_function == nullptr)) {
			// Fail
			return false;
		}

		ECS_STACK_CAPACITY_STREAM(LinkComponentAssetField, asset_fields, 512);
		if (base_data->asset_fields.size == 0) {
			GetAssetFieldsFromLinkComponent(base_data->link_type, asset_fields);

			if (!needs_dll) {
				ECS_STACK_CAPACITY_STREAM(LinkComponentAssetField, target_asset_fields, 512);
				bool success = GetAssetFieldsFromLinkComponentTarget(base_data->target_type, target_asset_fields);
				if (!success) {
					return false;
				}

				if (target_asset_fields.size != asset_fields.size) {
					return false;
				}

				for (unsigned int index = 0; index < asset_fields.size; index++) {
					if (asset_fields[index].field_index != target_asset_fields[index].field_index ||
						asset_fields[index].type.type != target_asset_fields[index].type.type) {
						return false;
					}
				}
			}
		}
		else {
			asset_fields = base_data->asset_fields;
		}

		ECS_STACK_CAPACITY_STREAM(Stream<void>, assets, 512);
		GetLinkComponentAssetData(base_data->link_type, link_data, base_data->asset_database, asset_fields, &assets);

		if (needs_dll) {
			if (use_apply_modifier_function) {
				// Use the apply modifier function
				ModuleLinkComponentApplyModifierFieldsFunctionData apply_data;
				apply_data.asset_handles = assets;
				apply_data.component = target_data;
				apply_data.link_component = link_data;

				// These are forwarded
				apply_data.previous_component = previous_target_data;
				apply_data.previous_link_component = previous_link_data;
				if (base_data->module_link.apply_modifier != nullptr) {
					base_data->module_link.apply_modifier(&apply_data);
				}
				else {
					return false;
				}
			}
			else {
				// Build it using the function
				ModuleLinkComponentFunctionData build_data;
				build_data.assets = assets;
				build_data.component = target_data;
				build_data.link_component = link_data;

				// These are forwarded
				build_data.previous_component = previous_target_data;
				build_data.previous_link_component = previous_link_data;
				base_data->module_link.build_function(&build_data);
			}
		}
		else {
			if (base_data->link_type->fields.size != base_data->target_type->fields.size) {
				return false;
			}

			const size_t MAX_TARGET_SIZE = ECS_KB * 4;
			size_t target_size = Reflection::GetReflectionTypeByteSize(base_data->target_type);
			size_t target_data_storage[MAX_TARGET_SIZE / sizeof(size_t)];
			memcpy(target_data_storage, target_data, target_size);

			struct RestoreTarget {
				void operator()() {
					if (copy_size > 0) {
						memcpy(target_data, target_storage, copy_size);
					}
				}

				void* target_data;
				const void* target_storage;
				size_t copy_size;
			};

			StackScope<RestoreTarget> restore_stack({ target_data, target_data_storage, target_size });

			for (unsigned int index = 0; index < asset_fields.size; index++) {
				ECS_SET_ASSET_TARGET_FIELD_RESULT result = SetAssetTargetFieldFromReflection(
					base_data->target_type,
					asset_fields[index].field_index, 
					target_data, 
					assets[index], 
					asset_fields[index].type.type
				);
				if (result != ECS_SET_ASSET_TARGET_FIELD_OK && result != ECS_SET_ASSET_TARGET_FIELD_MATCHED) {
					return false;
				}
			}

			for (unsigned int index = 0; index < (unsigned int)base_data->link_type->fields.size; index++) {
				// Check to see that the field is not an asset field
				unsigned int subindex = 0;
				for (; subindex < asset_fields.size; subindex++) {
					if (asset_fields[subindex].field_index == index) {
						break;
					}
				}

				if (subindex != asset_fields.size) {
					continue;
				}

				if (!functor(index)) {
					return false;
				}
			}

			restore_stack.deallocator.copy_size = 0;
		}

		return true;
	}

	bool ConvertLinkComponentToTarget(
		const ConvertToAndFromLinkBaseData* base_data,
		const void* link_data,
		void* target_data,
		const void* previous_link_data,
		const void* previous_target_data,
		bool apply_modifier_function
	)
	{
		auto copy_field = [&](size_t index) {
			if (!base_data->target_type->fields[index].Compare(base_data->link_type->fields[index])) {
				return false;
			}

			const void* source = function::OffsetPointer(link_data, base_data->link_type->fields[index].info.pointer_offset);
			void* destination = function::OffsetPointer(target_data, base_data->target_type->fields[index].info.pointer_offset);

			if (base_data->allocator.allocator != nullptr) {
				CopyReflectionType(
					base_data->reflection_manager,
					base_data->target_type->fields[index].definition,
					source,
					destination,
					base_data->allocator
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
		return ConvertLinkComponentToTargetImpl(base_data, link_data, target_data, previous_link_data, previous_target_data, apply_modifier_function, copy_field);
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
		return ConvertLinkComponentToTargetImpl(base_data, link_data, target_data, previous_link_data, previous_target_data, false, [](size_t index) { return true; });
	}

	// ------------------------------------------------------------------------------------------------------------

	void GetAssetReferenceCountsFromEntities(
		const EntityManager* entity_manager, 
		const Reflection::ReflectionManager* reflection_manager, 
		AssetDatabase* asset_database
	)
	{
		// Allocate space for the reference counts and initialize them to 0
		ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 128, ECS_MB);
		
		ECS_STACK_CAPACITY_STREAM(Stream<unsigned int>, asset_reference_counts, ECS_ASSET_TYPE_COUNT);
		for (size_t index = 0; index < ECS_ASSET_TYPE_COUNT; index++) {
			asset_reference_counts[index].Initialize(&stack_allocator, asset_database->GetAssetCount((ECS_ASSET_TYPE)index));
			memset(asset_reference_counts[index].buffer, 0, asset_reference_counts[index].MemoryOf(asset_reference_counts[index].size));
		}

		// Get the reference counts
		GetAssetReferenceCountsFromEntities(entity_manager, reflection_manager, asset_database, asset_reference_counts.buffer);

		unsigned int max_asset_count = asset_database->GetMaxAssetCount();

		CapacityStream<unsigned int> handles_to_be_removed;
		handles_to_be_removed.Initialize(&stack_allocator, 0, max_asset_count);

		// Perform the reference count change for those assets that are still in use
		// Those that need to be removed cache them in separate buffer and perform the removal afterwards
		for (size_t asset_type_index = 0; asset_type_index < ECS_ASSET_TYPE_COUNT; asset_type_index++) {
			ECS_ASSET_TYPE asset_type = (ECS_ASSET_TYPE)asset_type_index;
			handles_to_be_removed.size = 0;
			for (unsigned int index = 0; index < asset_reference_counts[asset_type].size; index++) {
				unsigned int handle = asset_database->GetAssetHandleFromIndex(index, asset_type);

				// Get the standalone reference count - subtract the difference between these 2 counts
				unsigned int standalone_reference_count = asset_database->GetReferenceCountStandalone(handle, asset_type);
				unsigned int difference = standalone_reference_count - asset_reference_counts[asset_type][index];

				if (difference == standalone_reference_count) {
					unsigned int reference_count = asset_database->GetReferenceCount(handle, asset_type);
					if (reference_count == standalone_reference_count) {
						// Add it to the removal buffer - can be safely removed
						handles_to_be_removed.Add(handle);
					}
					else {
						// Reduce the reference count
						asset_database->SetAssetReferenceCount(handle, asset_type, reference_count - difference);
					}
				}
				else {
					// Update the reference count
					asset_database->SetAssetReferenceCount(handle, asset_type, asset_reference_counts[asset_type][index]);
				}
			}

			// Remove the handles now
			AssetDatabaseRemoveInfo remove_info;
			remove_info.remove_dependencies = true;
			for (unsigned int index = 0; index < handles_to_be_removed.size; index++) {
				asset_database->RemoveAssetForced(handles_to_be_removed[index], asset_type, &remove_info);
			}
		}
	}

	// ------------------------------------------------------------------------------------------------------------

	void GetAssetReferenceCountsFromEntities(
		const EntityManager* entity_manager, 
		const Reflection::ReflectionManager* reflection_manager, 
		const AssetDatabase* asset_database, 
		Stream<unsigned int>* asset_fields_reference_count,
		GetAssetReferenceCountsFromEntitiesOptions options
	)
	{
		// Make sure that there are enough slots for each asset type
		for (size_t index = 0; index < ECS_ASSET_TYPE_COUNT; index++) {
			ECS_ASSERT(asset_fields_reference_count[index].size >= asset_database->GetAssetCount((ECS_ASSET_TYPE)index));
		}

		ECS_STACK_CAPACITY_STREAM(unsigned int, unique_types, ECS_KB * 16);
		ECS_STACK_CAPACITY_STREAM(unsigned int, shared_types, ECS_KB * 16);
		ECS_STACK_CAPACITY_STREAM(unsigned int, global_types, ECS_KB * 4);

		GetHierarchyComponentTypes(reflection_manager, -1, &unique_types, &shared_types, &global_types);

		auto increment_reference_counts = [&](
			Stream<LinkComponentAssetField> asset_fields, 
			const Reflection::ReflectionType* reflection_type, 
			const void* data, 
			unsigned int increment_count
		) {
			ECS_STACK_CAPACITY_STREAM(unsigned int, handles, 512);
			GetLinkComponentTargetHandles(reflection_type, asset_database, data, asset_fields, handles.buffer);

			// For each handle increase the reference count for that asset type
			for (unsigned int index = 0; index < asset_fields.size; index++) {
				if (handles[index] != -1) {
					unsigned int asset_index = asset_database->GetIndexFromAssetHandle(handles[index], asset_fields[index].type.type);
					asset_fields_reference_count[asset_fields[index].type.type][asset_index] += increment_count;
				}
			}
		};

		auto get_corresponding_reflection_type = [&](Component component, Stream<unsigned int> type_indices) {
			const Reflection::ReflectionType* reflection_type = nullptr;
			for (unsigned int index = 0; index < type_indices.size; index++) {
				const Reflection::ReflectionType* current_reflection_type = reflection_manager->GetType(type_indices[index]);
				Component reflection_component = GetReflectionTypeComponent(current_reflection_type);
				if (reflection_component == component) {
					reflection_type = current_reflection_type;
					break;
				}
			}

			return reflection_type;
		};

		// Unique components
		entity_manager->ForEachComponent([&](Component component) {
			// For each component determine its reflection type and the asset fields
			const Reflection::ReflectionType* reflection_type = get_corresponding_reflection_type(component, unique_types);

			ECS_ASSERT(reflection_type != nullptr);

			ECS_STACK_CAPACITY_STREAM(LinkComponentAssetField, asset_fields, 512);
			GetAssetFieldsFromLinkComponentTarget(reflection_type, asset_fields);

			if (asset_fields.size > 0) {
				// If it has asset fields, then retrieve them and keep the count of reference counts
				entity_manager->ForEachEntityComponent(component, [&](Entity entity, const void* data) {
					increment_reference_counts(asset_fields, reflection_type, data, 1);
				});
			}
		});

		// Shared components
		entity_manager->ForEachSharedComponent([&](Component component) {
			// For each component determine its reflection type and the asset fields
			const Reflection::ReflectionType* reflection_type = get_corresponding_reflection_type(component, shared_types);
			ECS_ASSERT(reflection_type != nullptr);

			ECS_STACK_CAPACITY_STREAM(LinkComponentAssetField, asset_fields, 512);
			GetAssetFieldsFromLinkComponentTarget(reflection_type, asset_fields);

			if (asset_fields.size > 0) {
				// Now go for each shared instance
				entity_manager->ForEachSharedInstance(component, [&](SharedInstance instance) {
					const void* data = entity_manager->GetSharedData(component, instance);
					unsigned int increase_count = 1;
					if (!options.shared_instance_only_once) {
						increase_count = entity_manager->GetNumberOfEntitiesForSharedInstance(component, instance);
					}
					// If the options is activated, count the number of entities that reference this instance
					increment_reference_counts(asset_fields, reflection_type, data, increase_count);
				});
			}
		});

		// Global components
		entity_manager->ForAllGlobalComponents([&](const void* global_data, Component component) {
			// For each component determine its reflection type and the asset fields
			const Reflection::ReflectionType* reflection_type = get_corresponding_reflection_type(component, global_types);

			ECS_ASSERT(reflection_type != nullptr);

			ECS_STACK_CAPACITY_STREAM(LinkComponentAssetField, asset_fields, 512);
			GetAssetFieldsFromLinkComponentTarget(reflection_type, asset_fields);

			if (asset_fields.size > 0) {
				// If it has asset fields, then retrieve them and keep the count of reference counts
				increment_reference_counts(asset_fields, reflection_type, global_data, 1);
			}
		});

		if (options.add_reference_counts_to_dependencies) {
			// After we retrieved the reference counts for all assets from the scene, we need to increase reference counts
			// for assets that are being referenced by other assets
			function::ForEach(ECS_ASSET_TYPES_WITH_DEPENDENCIES, [&](ECS_ASSET_TYPE asset_type) {
				ECS_STACK_CAPACITY_STREAM(AssetTypedHandle, asset_dependencies, 512);
				for (size_t index = 0; index < asset_fields_reference_count[asset_type].size; index++) {
					asset_dependencies.size = 0;
					asset_database->GetDependencies(asset_database->GetAssetHandleFromIndex(index, asset_type), asset_type, &asset_dependencies);
					for (unsigned int subindex = 0; subindex < asset_dependencies.size; subindex++) {
						unsigned int asset_index = asset_database->GetIndexFromAssetHandle(asset_dependencies[subindex].handle, asset_dependencies[subindex].type);
						asset_fields_reference_count[asset_dependencies[subindex].type][asset_index]++;
					}
				}
			});
		}
	}

	// ------------------------------------------------------------------------------------------------------------

	bool HasModifierFieldsLinkComponent(const Reflection::ReflectionType* link_type)
	{
		for (size_t index = 0; index < link_type->fields.size; index++) {
			if (link_type->fields[index].Has(STRING(ECS_LINK_MODIFIER_FIELD))) {
				return true;
			}
		}
		return false;
	}

	// ------------------------------------------------------------------------------------------------------------

	bool NeedsButtonModifierFieldsLinkComponent(const Reflection::ReflectionType* link_type)
	{
		return link_type->GetEvaluation(ECS_LINK_COMPONENT_NEEDS_BUTTON_FUNCTION) != DBL_MAX;
	}

	// ------------------------------------------------------------------------------------------------------------

	void CopyModifierFieldsLinkComponent(const Reflection::ReflectionType* link_type, void* destination, const void* source)
	{
		for (size_t index = 0; index < link_type->fields.size; index++) {
			if (link_type->fields[index].Has(STRING(ECS_LINK_MODIFIER_FIELD))) {
				memcpy(
					function::OffsetPointer(destination, link_type->fields[index].info.pointer_offset),
					function::OffsetPointer(source, link_type->fields[index].info.pointer_offset),
					link_type->fields[index].info.byte_size
				);
			}
		}
	}

	// ------------------------------------------------------------------------------------------------------------

	void ResetModifierFieldsLinkComponent(const Reflection::ReflectionType* link_type, void* link_component)
	{
		for (size_t index = 0; index < link_type->fields.size; index++) {
			if (link_type->fields[index].Has(STRING(ECS_LINK_MODIFIER_FIELD))) {
				void* value = function::OffsetPointer(link_component, link_type->fields[index].info.pointer_offset);
				if (link_type->fields[index].info.has_default_value) {
					memcpy(value, &link_type->fields[index].info.default_bool, link_type->fields[index].info.byte_size);
				}
				else {
					memset(value, 0, link_type->fields[index].info.byte_size);
				}
			}
		}
	}

	// ------------------------------------------------------------------------------------------------------------

}