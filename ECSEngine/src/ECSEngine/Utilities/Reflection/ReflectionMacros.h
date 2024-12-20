#pragma once

#pragma warning(disable: 4003)

#define ECS_REFLECT

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
// Exists and corresponds to an actual allocator
#define ECS_FIELD_ALLOCATOR(allocator_field_name)

// Should be placed for an allocator field - when specified, it indicates that all further allocations for an instance
// Should be made from this allocator, unless that field has an allocator override, and the user specified that type
// Allocators are enabled
#define ECS_MAIN_ALLOCATOR

// When specified, it indicates that this allocator should not be initialized as a standalone entry, but rather
// It should reference the allocator that is passed in.
#define ECS_REFERENCE_ALLOCATOR

#define ECS_REFLECT_SETTINGS
#define ECS_REFLECT_COMPONENT
#define ECS_REFLECT_GLOBAL_COMPONENT
#define ECS_REFLECT_LINK_COMPONENT(a)

// TODO: The link modifier specifiers are no longer in use
// Have a thought if these should stick around or they should be removed
// Together with the link modifier code
#define ECS_LINK_MODIFIER_FIELD
#define ECS_LINK_COMPONENT_NEEDS_BUTTON_FUNCTION "NeedsButton"

// When a type is changed and you want to keep the old data, use this tag in order
// tell the runtime what name it should map to. It will ignore this tag if it doesn't
// find the old name
#define ECS_MAP_FIELD_REFLECTION(old_name)

// Used with the IsTag function of the ReflectionType
#define ECS_COMPONENT_TAG "COMPONENT"
// Used with the IsTag function of the ReflectionType
#define ECS_GLOBAL_COMPONENT_TAG "GLOBAL_COMPONENT"
// Used with the HasTag function of the ReflectionType
#define ECS_LINK_COMPONENT_TAG "LINK_COMPONENT"

#define ECS_COMPONENT_ID_FUNCTION "ID"
#define ECS_COMPONENT_IS_SHARED_FUNCTION "IsShared"
#define ECS_COMPONENT_ALLOCATOR_SIZE_FUNCTION "AllocatorSize"
// Adding this function to a type tells the scene deserializer to not try and
// deserialize the data if the file contains a link type and the new type is not a link
// or the other way around
#define ECS_COMPONENT_STRICT_DESERIALIZE "StrictDeserialize"