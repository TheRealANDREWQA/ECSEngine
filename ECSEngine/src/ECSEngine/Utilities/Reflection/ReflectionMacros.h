#pragma once

#define ECS_REFLECT
#define ECS_OMIT_FIELD_REFLECT(byte_size, ...) /* Can specify as second argument the alignment of this field, otherwise 8 will be assumed */
#define ECS_ENUM_REFLECT
#define ECS_POINTER_REFLECT(...)
//#define ECS_REFLECT_FIELD_SIZE(size)
#define ECS_EXPLICIT_TYPE_REFLECT(...) /* Pairs of extended type and the associated name */
#define ECS_REGISTER_ONLY_NAME_REFLECT(byte_size, ...) /* Can specify as second argument the alignment of this field, otherwise 8 will be assumed */
#define ECS_STREAM_REFLECT(byte_size)

#define ECS_REFLECT_COMPONENT