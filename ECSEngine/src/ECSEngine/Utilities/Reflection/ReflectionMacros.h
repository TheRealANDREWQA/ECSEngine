#pragma once

#define ECS_REFLECT
#define ECS_ENUM_REFLECT

#define ECS_FIELDS_START_REFLECT
#define ECS_FIELDS_END_REFLECT

// Used to skip fields for determination of byte size and alignment
#define ECS_SKIP_REFLECTION(byte_size) byte_size;

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

// Used by the HasTag function of the ReflectionType
#define ECS_COMPONENT_TAG "COMPONENT"
// Used by the HasTag function of the ReflectionType
#define ECS_SHARED_COMPONENT_TAG "SHARED_COMPONENT"