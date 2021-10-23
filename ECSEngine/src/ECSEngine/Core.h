#pragma once

#ifdef ECSENGINE_PLATFORM_WINDOWS
	#ifdef ECSENGINE_BUILD_DLL
		#define	ECSENGINE_API __declspec(dllexport)
	#else
		#define ECSENGINE_API __declspec(dllimport)
	#endif 
	#define ECSENGINE_DIRECTX11
#else
	#error Windows is the only platform
#endif

constexpr float PI = 3.14159265358979323846f;
constexpr float PI_INVERSE = 1.0f / PI;
constexpr float DEG_TO_RAD_FACTOR = PI / 180.0f;
constexpr float RAD_TO_DEG_FACTOR = 180.0f / PI;
constexpr size_t ECS_KB = 1 << 10;
constexpr size_t ECS_MB = ECS_KB * ECS_KB;
constexpr size_t ECS_GB = ECS_MB * ECS_KB;

namespace ECSEngine {

	static inline float DegToRad(float angle) {
		return angle * DEG_TO_RAD_FACTOR;
	}

	static inline float RadToDeg(float angle) {
		return angle * RAD_TO_DEG_FACTOR;
	}

}

#define STRING(s) #s
#define ECS_CACHE_LINE_SIZE 64
#define ECS_MICROSOFT_WRL using namespace Microsoft::WRL
#define ECS_CONTAINERS using namespace ECSEngine::containers
#define ECS_VECTORCALL __vectorcall
#define ECS_INLINE __forceinline
#define ECS_NOINLINE __declspec(noinline)
#define ECS_RESTRICT __restrict

#define ECS_FORWARD_STRUCT_MEMBERS_1(struct_name, field) struct_name.field = field;
#define ECS_FORWARD_STRUCT_MEMBERS_2(struct_name, field1, field2) ECS_FORWARD_STRUCT_MEMBERS_1(struct_name, field1); \
struct_name.field2 = field2;
#define ECS_FORWARD_STRUCT_MEMBERS_3(struct_name, field1, field2, field3) ECS_FORWARD_STRUCT_MEMBERS_2(struct_name, field1, field2); \
struct_name.field3 = field3;
#define ECS_FORWARD_STRUCT_MEMBERS_4(struct_name, field1, field2, field3, field4) ECS_FORWARD_STRUCT_MEMBERS_3(struct_name, field1, field2, field3); \
struct_name.field4 = field4;
#define ECS_FORWARD_STRUCT_MEMBERS_5(struct_name, field1, field2, field3, field4, field5) ECS_FORWARD_STRUCT_MEMBERS_4(struct_name, field1, field2, field3, field4); \
struct_name.field5 = field5;
#define ECS_FORWARD_STRUCT_MEMBERS_6(struct_name, field1, field2, field3, field4, field5, field6) ECS_FORWARD_STRUCT_MEMBERS_5(struct_name, field1, field2, field3, field4, field5); \
struct_name.field6 = field6;
#define ECS_FORWARD_STRUCT_MEMBERS_7(struct_name, field1, field2, field3, field4, field5, field6, field7) ECS_FORWARD_STRUCT_MEMBERS_6(struct_name, field1, field2, field3, field4, field5, field6); \
struct_name.field7 = field7;
#define ECS_FORWARD_STRUCT_MEMBERS_8(struct_name, field1, field2, field3, field4, field5, field6, field7, field8) ECS_FORWARD_STRUCT_MEMBERS_7(struct_name, field1, field2, field3, field4, field5, field6, field7); \
struct_name.field8 = field8;
#define ECS_FORWARD_STRUCT_MEMBERS_9(struct_name, field1, field2, field3, field4, field5, field6, filed7, field8, field9) ECS_FORWARD_STRUCT_MEMBERS_8(struct_name, field1, field2, field3, field4, field5, field6, field7, field8); \
struct_name.field9 = field9;
#define ECS_FORWARD_STRUCT_MEMBERS_10(struct_name, field1, field2, field3, field4, field5, field6, field7, field8, field9, field10) ECS_FORWARD_STRUCT_MEMBERS_9(struct_name, field1, field2, field3, field4, field5, field6, field7, field8, field9); \
struct_name.field10 = field10;

#define ECS_TEMPLATE_FUNCTION(return_type, function_name, ...) template ECSENGINE_API return_type function_name(__VA_ARGS__)

#define ECS_TEMPLATE_FUNCTION_2_BEFORE(return_type, function_name, first, second, ...) ECS_TEMPLATE_FUNCTION(return_type, function_name, first, __VA_ARGS__); \
ECS_TEMPLATE_FUNCTION(return_type, function_name, second, __VA_ARGS__)

#define ECS_TEMPLATE_FUNCTION_2_AFTER(return_type, function_name, first, second, ...) ECS_TEMPLATE_FUNCTION(return_type, function_name, __VA_ARGS__, first); \
 ECS_TEMPLATE_FUNCTION(return_type, function_name, __VA_ARGS__, second)

#define ECS_TEMPLATE_FUNCTION_3_BEFORE(return_type, function_name, first, second, third, ...) ECS_TEMPLATE_FUNCTION(return_type, function_name, first, __VA_ARGS__); \
 ECS_TEMPLATE_FUNCTION(return_type, function_name, second, __VA_ARGS__); \
 ECS_TEMPLATE_FUNCTION(return_type, function_name, third, __VA_ARGS__)

#define ECS_TEMPLATE_FUNCTION_3_AFTER(return_type, function_name, first, second, third, ...) ECS_TEMPLATE_FUNCTION(return_type, function_name, __VA_ARGS__, first); \
ECS_TEMPLATE_FUNCTION(return_type, function_name, __VA_ARGS__, second); ECS_TEMPLATE_FUNCTION(return_type, function_name, __VA_ARGS__, third)

#define ECS_TEMPLATE_FUNCTION_4_BEFORE(return_type, function_name, first, second, third, fourth, ...) ECS_TEMPLATE_FUNCTION_2_BEFORE(return_type, function_name, first, second, __VA_ARGS__); ECS_TEMPLATE_FUNCTION_2_BEFORE(return_type, function_name, third, fourth, __VA_ARGS__)

#define ECS_TEMPLATE_FUNCTION_4_AFTER(return_type, function_name, first, second, third, fourth, ...) ECS_TEMPLATE_FUNCTION_2_AFTER(return_type, function_name, first, second, __VA_ARGS__); ECS_TEMPLATE_FUNCTION_2_AFTER(return_type, function_name, third, fourth, __VA_ARGS__)

#define ECS_TEMPLATE_FUNCTION_BOOL(return_type, function_name, ...) template ECSENGINE_API return_type function_name<false>(__VA_ARGS__); template ECSENGINE_API return_type function_name<true>(__VA_ARGS__)
#define ECS_TEMPLATE_FUNCTION_BOOL_NO_API(return_type, function_name, ...) template return_type function_name<false>(__VA_ARGS__); template return_type function_name<true>(__VA_ARGS__);
#define ECS_TEMPLATE_FUNCTION_BOOL_CONST(return_type, function_name, ...) template ECSENGINE_API return_type function_name<false>(__VA_ARGS__) const; template ECSENGINE_API return_type function_name<true>(__VA_ARGS__) const;
#define ECS_TEMPLATE_FUNCTION_BOOL_CONST_NO_API(return_type, function_name, ...) template return_type function_name<false>(__VA_ARGS__) const; template return_type function_name<true>(__VA_ARGS__) const;

#define ECS_TEMPLATE_FUNCTION_DOUBLE_BOOL(return_type, function_name, ...) template ECSENGINE_API return_type function_name<false, false>(__VA_ARGS__); template ECSENGINE_API return_type function_name<true, false>(__VA_ARGS__); \
template ECSENGINE_API return_type function_name<false, true>(__VA_ARGS__); template ECSENGINE_API return_type function_name<true, true>(__VA_ARGS__);

#define ECS_TEMPLATE_FUNCTION_DOUBLE_BOOL_NO_API(return_type, function_name, ...) template return_type function_name<false, false>(__VA_ARGS__); template return_type function_name<true, false>(__VA_ARGS__); \
template return_type function_name<false, true>(__VA_ARGS__); template return_type function_name<true, true>(__VA_ARGS__);

#define ECS_TEMPLATE_FUNCTION_DOUBLE_BOOL_CONST(return_type, function_name, ...) template ECSENGINE_API return_type function_name<false, false>(__VA_ARGS__) const; template ECSENGINE_API return_type function_name<true, false>(__VA_ARGS__) const; \
template ECSENGINE_API return_type function_name<false, true>(__VA_ARGS__) const; template ECSENGINE_API return_type function_name<true, true>(__VA_ARGS__) const;

#define ECS_TEMPLATE_FUNCTION_DOUBLE_BOOL_CONST_NO_API(return_type, function_name, ...) template return_type function_name<false, false>(__VA_ARGS__) const; template return_type function_name<true, false>(__VA_ARGS__) const; \
template return_type function_name<false, true>(__VA_ARGS__) const; template return_type function_name<true, true>(__VA_ARGS__) const;

#define ECS_TEMPLATE_FUNCTION_ALLOCATOR_API(return_type, function_name, ...) template ECSENGINE_API return_type function_name(LinearAllocator*, __VA_ARGS__); \
template ECSENGINE_API return_type function_name(StackAllocator*, __VA_ARGS__); \
template ECSENGINE_API return_type function_name(PoolAllocator*, __VA_ARGS__); \
template ECSENGINE_API return_type function_name(MultipoolAllocator*, __VA_ARGS__); \
template ECSENGINE_API return_type function_name(MemoryManager*, __VA_ARGS__); \
template ECSENGINE_API return_type function_name(GlobalMemoryManager*, __VA_ARGS__); \
template ECSENGINE_API return_type function_name(MemoryArena*, __VA_ARGS__); \
template ECSENGINE_API return_type function_name(ResizableMemoryArena*, __VA_ARGS__);

#define ECS_TEMPLATE_FUNCTION_ALLOCATOR(return_type, function_name, ...) template return_type function_name(LinearAllocator*, __VA_ARGS__); \
template return_type function_name(StackAllocator*, __VA_ARGS__); \
template return_type function_name(PoolAllocator*, __VA_ARGS__); \
template return_type function_name(MultipoolAllocator*, __VA_ARGS__); \
template return_type function_name(MemoryManager*, __VA_ARGS__); \
template return_type function_name(GlobalMemoryManager*, __VA_ARGS__); \
template return_type function_name(MemoryArena*, __VA_ARGS__); \
template return_type function_name(ResizableMemoryArena*, __VA_ARGS__);

#define ECS_TEMPLATE_FUNCTION_INTEGER(function) function(int8_t); function(uint8_t); function(int16_t); function(uint16_t); \
function(int32_t); function(uint32_t); function(int64_t); function(uint64_t);

#define ECS_CLASS_DEFAULT_CONSTRUCTOR_AND_ASSIGNMENT(type) type(const type& other) = default; type& operator = (const type& other) = default;