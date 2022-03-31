#pragma once

#define ECS_REFLECT
#define ECS_ENUM_REFLECT

#define ECS_OMIT_FIELD_REFLECT(byte_size, ...) /* Can specify as second argument the alignment of this field, otherwise 8 will be assumed */
#define ECS_OMIT_FIELD_BY_TYPE_REFLECT /* It will search for the type in order to determine its byte size */

#define ECS_POINTER_REFLECT(...)
//#define ECS_REFLECT_FIELD_SIZE(size)
#define ECS_EXPLICIT_TYPE_REFLECT(...) /* Pairs of extended type and the associated name */
#define ECS_STREAM_REFLECT(byte_size)

#define ECS_REFLECT_COMPONENT