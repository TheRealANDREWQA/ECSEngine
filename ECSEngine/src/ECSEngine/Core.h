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
constexpr size_t ECS_TB = ECS_GB * ECS_KB;

constexpr size_t ECS_KB_10 = 1'000;
constexpr size_t ECS_MB_10 = 1'000'000;
constexpr size_t ECS_GB_10 = 1'000'000'000;
constexpr size_t ECS_TB_10 = 1'000'000'000'000;

#define ECS_OS_PATH_SEPARATOR L'\\'
#define ECS_OS_PATH_SEPARATOR_ASCII '\\'
#define ECS_OS_PATH_SEPARATOR_REL L'/'
#define ECS_OS_PATH_SEPARATOR_ASCII_REL '/'

#define ECS_SIMD_BYTE_SIZE 256
#define ECS_SIMD_SIZE_T_SIZE 32

namespace ECSEngine {

	static inline float DegToRad(float angle) {
		return angle * DEG_TO_RAD_FACTOR;
	}

	static inline float RadToDeg(float angle) {
		return angle * RAD_TO_DEG_FACTOR;
	}

}

// This macro is used to give other macros that want a single argument
// but the argument is actually a list of values
#define FORWARD(...) __VA_ARGS__
#define STRING(s) #s
// Places the type followed by a comma with the stringified name
#define WITH_NAME(type) type, STRING(type)
#define ECS_CACHE_LINE_SIZE 64
#define ECS_VECTORCALL __vectorcall
#define ECS_INLINE __forceinline
#define ECS_NOINLINE __declspec(noinline)
#define ECS_RESTRICT __restrict
#define ECS_OPTIMIZE_START _Pragma("optimize(\"\", on )")
#define ECS_OPTIMIZE_END _Pragma("optimize(\"\", off )")

#define ECS_BIT(bit_index) (1 << bit_index)

#define ECS_FILE_LINE __FILE__, (unsigned int)__LINE__
#define ECS_LOCATION __builtin_FILE(), __builtin_FUNCTION(), __builtin_LINE()
// Default argument that takes the value of the calling function
#define ECS_FUNCTION __builtin_FUNCTION()

#define ECS_STACK_ALLOC(size) _alloca(size)
#define ECS_MALLOCA(size) _malloca(size)
#define ECS_FREEA(memory) _freea(memory)

#define ECS_PACK_START __pragma(pack(push, 1))
#define ECS_PACK_END __pragma(pack(pop))

#define ECS_PACK(struct_definition) ECS_PACK_START struct_definition ECS_PACK_END

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
#define ECS_FORWARD_STRUCT_MEMBERS_9(struct_name, field1, field2, field3, field4, field5, field6, field7, field8, field9) ECS_FORWARD_STRUCT_MEMBERS_8(struct_name, field1, field2, field3, field4, field5, field6, field7, field8); \
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

#define ECS_TEMPLATE_FUNCTION_INTEGER(function) function(char); function(int8_t); function(uint8_t); function(int16_t); function(uint16_t); \
function(int32_t); function(uint32_t); function(int64_t); function(uint64_t);

#define ECS_CLASS_DEFAULT_CONSTRUCTOR_AND_ASSIGNMENT(type) type(const type& other) = default; type& operator = (const type& other) = default;

#define ECS_ENUM_BITWISE_OPERATIONS(T) inline T operator~ (T a) { return (T)~(int)a; } \
inline T operator| (T a, T b) { return (T)((int)a | (int)b); } \
inline T operator& (T a, T b) { return (T)((int)a & (int)b); } \
inline T operator^ (T a, T b) { return (T)((int)a ^ (int)b); } \
inline T& operator|= (T& a, T b) { return (T&)((int&)a |= (int)b); } \
inline T& operator&= (T& a, T b) { return (T&)((int&)a &= (int)b); } \
inline T& operator^= (T& a, T b) { return (T&)((int&)a ^= (int)b); }

// Have faith in the optimizer that it will eliminate these dead branches
#define LOOP_UNROLL_4(count, function, pivot)	if (count >= pivot + 1) { function(pivot); } \
												if (count >= pivot + 2) { function(pivot + 1); } \
												if (count >= pivot + 3) { function(pivot + 2); } \
												if (count >= pivot + 4) { function(pivot + 3); } 


// It goes up to 16
#define LOOP_UNROLL(count, function)	LOOP_UNROLL_4(count, function, 0); \
										LOOP_UNROLL_4(count, function, 4); \
										LOOP_UNROLL_4(count, function, 8); \
										LOOP_UNROLL_4(count, function, 12); 
										//LOOP_UNROLL_4(count, function, 16); \
										//LOOP_UNROLL_4(count, function, 20); \
										//LOOP_UNROLL_4(count, function, 24); \
										//LOOP_UNROLL_4(count, function, 28); \