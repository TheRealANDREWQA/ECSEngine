#pragma once

#pragma warning(disable: 4003)

#define ECS_REFLECT
// When attached to a struct, it indicates that this structure might appear in other structures
// And it should be considered as a valid dependency, without having its proper definition registered
// It only looks at the token to the right of it and it adds to the valid dependencies array
#define ECS_REFLECT_VALID_DEPENDENCY

#define ECS_FIELDS_START_REFLECT
#define ECS_FIELDS_END_REFLECT

// Used to skip fields for determination of byte size and alignment
// If the type can be deduced, you don't need to specify the static assert
// Use a static assert in order to be sure that the size/alignment is correct
// The alignment is optional - it is assumed to be the maximal alignment of the platform
// if left unspecified
#define ECS_SKIP_REFLECTION(static_assert_byte_size, ...) static_assert_byte_size; __VA_ARGS__;

// Used to omit fields which are user defined types but don't want to reflect these
// but at the same time want to keep them. Use static assert in order to be
// sure that the size is correct. Can optionally give the alignment, otherwise
// it is assumed to be 8
#define ECS_GIVE_SIZE_REFLECTION(byte_size, ...) byte_size;

// The value should be placed in its parenthesis. Used in conjuction with a macro
#define ECS_CONSTANT_REFLECT(value) value
// Place it at the beginning of the line of the evaluation that you want to perform
// Only functions with constant expressions can be evaluated (including any macro value with ECS_CONSTANT_REFLECT).
// Can use sizeof(Type) or alignof(Type) if the type is reflected. For these types if some fields are skipped
// you need to make sure programmatically that the byte size and the alignment are set accordingly.
#define ECS_EVALUATE_FUNCTION_REFLECT

// With this macro you can specify SoA fields. The fields need to be specified by their
// Name, with the size field being the first one mentioned, with an optional capacity field
// In order to skip the capacity field if there is none, you simply need to use "". The first
// Argument is a name that you can give for the overarching SoA stream - in case of aggregate streams, 
// like for a float3* being split into 3 float*. You can specify empty "" if this is not an aggregate SoA
// There can be a maximum of 13 parallel streams (should suffice for all intents and purposes)
#define ECS_SOA_REFLECT(name, size_field, capacity_field, ...)
//// With this specifier, you can tell other systems to treat multiple SoA streams as a single
//// Stream of the aggregate type. Works for basic types, like float3, float4. You need to list
//// The names of the fields here
//#define ECS_SOA_REFLECT_AGGREGATE(...)

// When placed for a field - it describes from which field the allocations should be made from
// If the deserialize/copy function specified per field allocator usage. It will validate that the field name
// Exists and corresponds to an actual allocator. It can be used inside an ECS_SOA_REFLECT as the last parameter, like this
// ECS_SOA_REFLECT(name, size_field, capacity_field, field0, field1, ECS_FIELD_ALLOCATOR(allocator_name))
#define ECS_FIELD_ALLOCATOR(allocator_field_name)

// Should be placed for an allocator field - when specified, it indicates that all further allocations for an instance
// Should be made from this allocator, unless that field has an allocator override, and the user specified that type
// Allocators are enabled. This type of allocator can be a reference as well, but this means that it must be of type
// AllocatorPolymorphic, as strongly typed allocators cannot reference any arbitrary allocator input.
#define ECS_MAIN_ALLOCATOR

// When specified, it indicates that this allocator should not be initialized as a standalone entry, but rather
// It should reference the allocator that is passed in. At the moment, it works only on AllocatorPolymorphic entries,
// Not on MemoryManager or MemoryManager* entries. If you want it to reference a specific field from the type, use
// ECS_FIELD_ALLOCATOR to designate that field. Can be combined with ECS_MAIN_ALLOCATOR
#define ECS_REFERENCE_ALLOCATOR

// This tag can be added to pointers only. It indicates that during a reflection Copy call, this entry should not
// Be separately allocated, but instead it should reference the existing pointer, from the source. In case this
// Field should reference another field from the destination during the copy, you can specify in the parentheses
// A named key which will be used to resolve the reference using the data from a destination field. This tag
// Must appear after the ECS_POINTER_KEY_REFERENCE_TARGET, otherwise the connection will not be seen.
// Supports as second argument ECS_CUSTOM_TYPE_ELEMENT when a key is specified
#define ECS_POINTER_AS_REFERENCE(...)

// Used in conjunction with ECS_POINTER_AS_REFERENCE, it indicates that this field contains the pointers which
// Should be referenced by the field which asked for references. It must appear before the ECS_POINTER_AS_REFERENCE
// Tag when starting from the top type, first field, since this information is gathered in a DFS manner. Supports
// A second optional argument ECS_CUSTOM_TYPE_ELEMENT
#define ECS_POINTER_KEY_REFERENCE_TARGET(key_name, ...)

// It should be applied to custom type fields only, which can have multiple types of elements in them, like a hash table
// It is used to specify tag options that should be applied to that element type only, not to the entire custom type
// To check the element name values that are valid for each custom type, check ReflectionCustomTypes.h. The macros
// Are defined there, but they don't have any definition assigned, you should use them as STRING(macro_name) like
// STRING(ECS_HASH_TABLE_CUSTOM_TYPE_ELEMENT_VALUE) when in C++ code, and when in reflection tags, it should be just
// The macro itself. This field should be embedded into the tags that supports this feature as an optional last field. These are:
//	- ECS_POINTER_AS_REFERENCE
//	- ECS_POINTER_KEY_REFERENCE_TARGET
// (When a new entry is added, the function GetReflectionCustomTypeElementOptions should be modified)
#define ECS_CUSTOM_TYPE_ELEMENT(element_name)

#define ECS_REFLECT_SETTINGS
#define ECS_REFLECT_COMPONENT
#define ECS_REFLECT_GLOBAL_COMPONENT
// This tag indicates that a component is global, but handled privately,
// And won't appear in the entities window
#define ECS_REFLECT_GLOBAL_COMPONENT_PRIVATE
#define ECS_REFLECT_LINK_COMPONENT(link_target)

// When a type is changed and you want to keep the old data, use this tag in order
// tell the runtime what name it should map to. It will ignore this tag if it doesn't
// find the old name
#define ECS_MAP_FIELD_REFLECTION(old_name)

// Used with the IsTag function of the ReflectionType
#define ECS_COMPONENT_TAG "COMPONENT"
// Used with the IsTag function of the ReflectionType
#define ECS_GLOBAL_COMPONENT_TAG "GLOBAL_COMPONENT"
// Used with the IsTag function of the ReflectionType
#define ECS_GLOBAL_COMPONENT_PRIVATE_TAG "GLOBAL_COMPONENT_PRIVATE"
// Used with the HasTag function of the ReflectionType
#define ECS_LINK_COMPONENT_TAG "LINK_COMPONENT"

#define ECS_COMPONENT_ID_FUNCTION "ID"
#define ECS_COMPONENT_IS_SHARED_FUNCTION "IsShared"
#define ECS_COMPONENT_ALLOCATOR_SIZE_FUNCTION "AllocatorSize"
// Adding this function to a type tells the scene deserializer to not try and
// deserialize the data if the file contains a link type and the new type is not a link
// or the other way around
#define ECS_COMPONENT_STRICT_DESERIALIZE "StrictDeserialize"

namespace ECSEngine {
	
	// Enums for custom types should go here, such that they can be readily used when ReflectionMacros.h is included
	
	enum ECS_HASH_TABLE_CUSTOM_TYPE_ELEMENT_INDEX : unsigned char {
		ECS_HASH_TABLE_CUSTOM_TYPE_ELEMENT_VALUE,
		ECS_HASH_TABLE_CUSTOM_TYPE_ELEMENT_IDENTIFIER
	};

}