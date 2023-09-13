#pragma once

#define ECS_REFLECT
#define ECS_ENUM_REFLECT

#define ECS_FIELDS_START_REFLECT
#define ECS_FIELDS_END_REFLECT

// Used to skip fields for determination of byte size and alignment
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

#define ECS_REFLECT_SETTINGS
#define ECS_REFLECT_COMPONENT
#define ECS_REFLECT_SHARED_COMPONENT
#define ECS_REFLECT_GLOBAL_COMPONENT
#define ECS_REFLECT_LINK_COMPONENT()
#define ECS_LINK_MODIFIER_FIELD
#define ECS_LINK_COMPONENT_NEEDS_BUTTON_FUNCTION "NeedsButton"

// Used to describe the type of element that the data pointer is used for
#define ECS_DATA_POINTER_TAG(a)

// When a type is changed and you want to keep the old data, use this tag in order
// tell the runtime what name it should map to. It will ignore this tag if it doesn't
// find the old name
#define ECS_MAP_FIELD_REFLECTION(old_name)

// Used with the IsTag function of the ReflectionType
#define ECS_COMPONENT_TAG "COMPONENT"
// Used with the IsTag function of the ReflectionType
#define ECS_SHARED_COMPONENT_TAG "SHARED_COMPONENT"
// Used with the IsTag function of the ReflectionType
#define ECS_GLOBAL_COMPONENT_TAG "GLOBAL_COMPONENT"
// Used with the HasTag function of the ReflectionType
#define ECS_LINK_COMPONENT_TAG "LINK_COMPONENT"

#define ECS_COMPONENT_ID_FUNCTION "ID"
#define ECS_COMPONENT_ALLOCATOR_SIZE_FUNCTION "AllocatorSize"
// Adding this function to a type tells the scene deserializer to not try and
// deserialize the data if the file contains a link type and the new type is not a link
// or the other way around
#define ECS_COMPONENT_STRICT_DESERIALIZE "StrictDeserialize"