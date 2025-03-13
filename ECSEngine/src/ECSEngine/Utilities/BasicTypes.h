// ECS_REFLECT
#pragma once
#include "../Core.h"
#include <stdint.h>
#include <malloc.h>
#include "Reflection/ReflectionMacros.h"
#include "Assert.h"

namespace ECSEngine {

	template<typename T>
	struct ReferenceCounted {
		T value;
		unsigned int reference_count;
	};

	// Useful for multithreading contexts where 
	template<typename T>
	struct CacheAligned {
		T value;
	private:
		char padding[(sizeof(T) % ECS_CACHE_LINE_SIZE) == 0 ? 0 : ECS_CACHE_LINE_SIZE - (sizeof(T) % ECS_CACHE_LINE_SIZE)];
	};

	// In order to not pull the <optional> header
	template<typename T>
	struct Optional {
		template<typename = std::enable_if_t<std::is_default_constructible_v<T>>>
		ECS_INLINE Optional() : has_value(false) {}
		ECS_INLINE Optional(Optional<T>&& other) : value(std::move(other.value)), has_value(other.has_value) {
			other.has_value = false;
		}
		ECS_INLINE Optional(T&& _value) : value(std::move(_value)), has_value(true) {}
		ECS_INLINE Optional(const T& _value) : value(_value), has_value(true) {}

		ECS_INLINE Optional<T>& operator= (const T& other) {
			value = other;
			has_value = true;
			return *this;
		}

		ECS_INLINE Optional<T>& operator= (T&& other) {
			value = std::move(other);
			has_value = true;
			return *this;
		}

		ECS_INLINE Optional<T>& operator=(const Optional<T>& other) {
			value = other.value;
			has_value = other.has_value;
			return *this;
		}

		ECS_INLINE Optional<T>& operator= (Optional<T>&& other) {
			value = std::move(other.value);
			has_value = other.has_value;
			other.has_value = false;
			return *this;
		}

		ECS_INLINE operator bool() const {
			return has_value;
		}

		T value;
		bool has_value;
	};

	// These are unsigned or signed, depending on your use case
	enum ECS_INT_TYPE : unsigned char {
		ECS_INT8,
		ECS_INT16,
		ECS_INT32,
		ECS_INT64,
		ECS_INT_TYPE_COUNT
	};

	enum ECS_CHANGE_SET_TYPE : unsigned char {
		ECS_CHANGE_SET_ADD,
		ECS_CHANGE_SET_REMOVE,
		ECS_CHANGE_SET_UPDATE
	};

	ECSENGINE_API size_t GetIntValueUnsigned(const void* ptr, ECS_INT_TYPE type);

	ECSENGINE_API int64_t GetIntValueSigned(const void* ptr, ECS_INT_TYPE type);

	ECSENGINE_API void SetIntValueUnsigned(void* ptr, ECS_INT_TYPE type, size_t value);

	ECSENGINE_API void SetIntValueSigned(void* ptr, ECS_INT_TYPE type, int64_t value);

	ECSENGINE_API bool IsUnsignedIntInRange(ECS_INT_TYPE type, size_t value);
	
	ECSENGINE_API bool IsSignedIntInRange(ECS_INT_TYPE type, int64_t value);

	ECS_INLINE size_t GetIntTypeByteSize(ECS_INT_TYPE type) {
		switch (type) {
		case ECS_INT8:
			return 1;
		case ECS_INT16:
			return 2;
		case ECS_INT32:
			return 4;
		case ECS_INT64:
			return 8;
		}

		ECS_ASSERT(false, "Invalid int type when retrieving the byte size");
		return 0;
	}

	struct Date {
		unsigned char month;
		unsigned char day;
		unsigned char hour;
		unsigned char minute;
		unsigned char seconds;
		unsigned short year;
		unsigned short milliseconds;
	};

	enum ECS_REFLECT ECS_AXIS : unsigned char {
		ECS_AXIS_X,
		ECS_AXIS_Y,
		ECS_AXIS_Z,
		ECS_AXIS_COUNT
	};

	ECSENGINE_API bool IsDateLater(Date first, Date second);

#pragma region Alphabet and character type

	// Helper Visualing enum
	enum class AlphabetIndex : unsigned char
	{
		ExclamationMark = 0x00,
		Quotes = 0x01,
		Tag = 0x02,
		Dollar = 0x03,
		Percent = 0x04,
		BinaryAnd = 0x05,
		SingleQuote = 0x06,
		OpenParanthesis = 0x07,
		ClosedParanthesis = 0x08,
		MultiplyStar = 0x09,
		Plus = 0x0A,
		Comma = 0x0B,
		Minus = 0x0C,
		Dot = 0x0D,
		ForwardSlash = 0x0E,
		Zero = 0x0F,
		One = 0x10,
		Two = 0x11,
		Three = 0x12,
		Four = 0x13,
		Five = 0x14,
		Six = 0x15,
		Seven = 0x16,
		Eight = 0x17,
		Nine = 0x18,
		Colon = 0x19,
		Semicolon = 0x1A,
		Less = 0x1B,
		Equal = 0x1C,
		Greater = 0x1D,
		QuestionMark = 0x1E,
		EmailSign = 0x1F,
		A = 0x20,
		B = 0x21,
		C = 0x22,
		D = 0x23,
		E = 0x24,
		F = 0x25,
		G = 0x26,
		H = 0x27,
		I = 0x28,
		J = 0x29,
		K = 0x2A,
		L = 0x2B,
		M = 0x2C,
		N = 0x2D,
		O = 0x2E,
		P = 0x2F,
		Q = 0x30,
		R = 0x31,
		S = 0x32,
		T = 0x33,
		U = 0x34,
		V = 0x35,
		W = 0x36,
		X = 0x37,
		Y = 0x38,
		Z = 0x39,
		OpenSquareParanthesis = 0x3A,
		Backslash = 0x3B,
		ClosedSquareParanthesis = 0x3C,
		Exponential = 0x3D,
		Underscore = 0x3E,
		AngledSingleQuote = 0x3F,
		a = 0x40,
		b = 0x41,
		c = 0x42,
		d = 0x43,
		e = 0x44,
		f = 0x45,
		g = 0x46,
		h = 0x47,
		i = 0x48,
		j = 0x49,
		k = 0x4A,
		l = 0x4B,
		m = 0x4C,
		n = 0x4D,
		o = 0x4E,
		p = 0x4F,
		q = 0x50,
		r = 0x51,
		s = 0x52,
		t = 0x53,
		u = 0x54,
		v = 0x55,
		w = 0x56,
		x = 0x57,
		y = 0x58,
		z = 0x59,
		OpenBracket = 0x5A,
		LogicalOr = 0x5B,
		ClosedBracket = 0x5C,
		Tilder = 0x5D,
		Space = 0x5E,
		Tab = 0x5F,
		Unknown = 0x60
	};

	enum class CharacterType : unsigned char {
		CapitalLetter,
		LowercaseLetter,
		Digit,
		Space,
		Symbol,
		Tab,
		Unknown
	};

	template<typename CharacterType>
	ECS_INLINE CharacterType Character(char character) {
		if constexpr (std::is_same_v<CharacterType, char>) {
			return character;
		}
		else if constexpr (std::is_same_v<CharacterType, wchar_t>) {
			return (wchar_t)((int)L'\0' + (int)character);
		}
	}

#pragma endregion

	/* Minimalist wrapper for simple structs composed of 2 fundamental types
	* It overloads the common operators:
	* ==, !=, +, -, *, /
	* [] for access ease
	*/
	template<typename T>
	struct Base2 {
		typedef T T;
		typedef Base2<bool> BooleanEquivalent;
		ECS_INLINE constexpr static size_t Count() {
			return 2;
		}
		

		ECS_INLINE constexpr Base2() {}
		/*constexpr Base2(Base _x) : x(_x) {}*/
		ECS_INLINE constexpr Base2(T _x, T _y) : x(_x), y(_y) {}

		template<typename OtherBase>
		ECS_INLINE constexpr Base2(Base2<OtherBase> xy) : x(xy.x), y(xy.y) {}

		// assumes all components
		ECS_INLINE Base2(const T* ptr) : x(*ptr), y(*(ptr + 1)) {}

		ECS_INLINE Base2(const Base2& other) = default;
		ECS_INLINE Base2& operator =(const Base2& other) = default;

		ECS_INLINE bool operator == (const Base2& other) const {
			return x == other.x && y == other.y;
		}

		ECS_INLINE bool operator != (const Base2& other) const {
			return x != other.x || y != other.y;
		}

		ECS_INLINE Base2 operator !() const {
			return Base2<Base>(!x, !y);
		}

		ECS_INLINE Base2 operator + (const Base2& other) const {
			return { x + other.x, y + other.y };
		}

		ECS_INLINE Base2& operator += (const Base2& other) {
			x += other.x;
			y += other.y;
			return *this;
		}

		ECS_INLINE Base2 operator - (const Base2& other) const {
			return { x - other.x, y - other.y };
		}

		ECS_INLINE Base2& operator -= (const Base2& other) {
			x -= other.x;
			y -= other.y;
			return *this;
		}

		ECS_INLINE Base2 operator * (const Base2& other) const {
			return { x * other.x, y * other.y };
		}

		ECS_INLINE Base2& operator *= (const Base2& other) {
			x *= other.x;
			y *= other.y;
			return *this;
		}

		ECS_INLINE Base2 operator * (float factor) const {
			return *this * Splat(factor);
		}

		ECS_INLINE Base2 operator *= (float factor) {
			x *= factor;
			y *= factor;
			return *this;
		}

		ECS_INLINE Base2 operator / (const Base2& other) const {
			return { x / other.x, y / other.y };
		}

		ECS_INLINE Base2& operator /= (const Base2& other) {
			x /= other.x;
			y /= other.y;
			return *this;
		}

		ECS_INLINE Base2 operator / (float factor) const {
			return *this * (1.0f / factor);
		}

		ECS_INLINE Base2& operator /= (float factor) {
			*this = *this / factor;
			return *this;
		}

		ECS_INLINE Base2 operator -() const {
			return { -x, -y };
		}

		ECS_INLINE T& operator [](unsigned int index) {
			return *((T*)this + index);
		}

		ECS_INLINE const T& operator [](unsigned int index) const {
			return *((const T*)this + index);
		}
		
		ECS_INLINE static Base2 Splat(T value) {
			return Base2(value, value);
		}

		T x;
		T y;
	};

	/* Minimalist wrapper for simple structs composed of 3 fundamental types
	* It overloads the common operators:
	* ==, !=, +, -, *, /
	* [] for access ease
	*/
	template<typename T>
	struct Base3 {
		typedef T T;
		typedef Base3<bool> BooleanEquivalent;
		ECS_INLINE constexpr static size_t Count() {
			return 3;
		}

		ECS_INLINE constexpr Base3() {}
		//constexpr Base3(Base _x) : x(_x) {}
		//constexpr Base3(Base _x, Base _y) : x(_x), y(_y) {}
		ECS_INLINE constexpr Base3(Base2<T> xy, T _z) : x(xy.x), y(xy.y), z(_z) {}
		ECS_INLINE constexpr Base3(T _x, T _y, T _z) : x(_x), y(_y), z(_z) {}

		template<typename OtherBase>
		ECS_INLINE constexpr Base3(Base3<OtherBase> xyz) : x(xyz.x), y(xyz.y), z(xyz.z) {}

		// assumes all components
		ECS_INLINE Base3(const T* ptr) : x(*ptr), y(*(ptr + 1)), z(*(ptr + 2)) {}

		Base3(const Base3& other) = default;
		Base3& operator =(const Base3& other) = default;

		ECS_INLINE bool operator == (const Base3& other) const {
			return x == other.x && y == other.y && z == other.z;
		}

		ECS_INLINE bool operator != (const Base3& other) const {
			return x != other.x || y != other.y || z != other.z;
		}

		ECS_INLINE Base3 operator !() const {
			return Base3<T>(!x, !y, !z);
		}

		ECS_INLINE Base3 operator + (const Base3& other) const {
			return { x + other.x, y + other.y, z + other.z };
		}

		ECS_INLINE Base3& operator += (const Base3& other) {
			x += other.x;
			y += other.y;
			z += other.z;
			return *this;
		}

		ECS_INLINE Base3 operator - (const Base3& other) const {
			return { x - other.x, y - other.y, z - other.z };
		}

		ECS_INLINE Base3& operator -= (const Base3& other) {
			x -= other.x;
			y -= other.y;
			z -= other.z;
			return *this;
		}

		ECS_INLINE Base3 operator * (const Base3& other) const {
			return { x * other.x, y * other.y, z * other.z };
		}

		ECS_INLINE Base3& operator *= (const Base3& other) {
			x *= other.x;
			y *= other.y;
			z *= other.z;
			return *this;
		}

		ECS_INLINE Base3 operator * (float factor) const {
			return *this * Splat(factor);
		}

		ECS_INLINE Base3& operator *= (float factor) {
			x *= factor;
			y *= factor;
			z *= factor;
			return *this;
		}

		ECS_INLINE Base3 operator / (const Base3& other) const {
			return { x / other.x, y / other.y, z / other.z };
		}

		ECS_INLINE Base3& operator /= (const Base3& other) {
			x /= other.x;
			y /= other.y;
			z /= other.z;
			return *this;
		}

		ECS_INLINE Base3 operator / (float factor) const {
			return *this * (1.0f / factor);
		}

		ECS_INLINE Base3& operator /= (float factor) {
			*this = *this / factor;
			return *this;
		}

		ECS_INLINE Base3 operator -() const {
			return Base3(-x, -y, -z);
		}

		ECS_INLINE T& operator [](unsigned int index) {
			return *((T*)this + index);
		}

		ECS_INLINE const T& operator[](unsigned int index) const {
			return *((const T*)this + index);
		}

		ECS_INLINE Base2<T> xy() const {
			return Base2<T>(x, y);
		}

		ECS_INLINE static Base3 Splat(T value) {
			return Base3(value, value, value);
		}

		T x;
		T y;
		T z;
	};

	/* Minimalist wrapper for simple structs composed of 4 fundamental types
	* It overloads the common operators:
	* ==, !=, +, -, *, /
	* [] for access ease
	*/
	template<class T>
	struct Base4 {
		typedef T T;
		typedef Base4<bool> BooleanEquivalent;
		ECS_INLINE constexpr static size_t Count() {
			return 4;
		}

		ECS_INLINE constexpr Base4() {}
		//constexpr Base4(Base _x) : x(_x) {}
		//constexpr Base4(Base _x, Base _y) : x(_x), y(_y) {}
		//constexpr Base4(Base _x, Base _y, Base _z) : x(_x), y(_y), z(_z) {}
		ECS_INLINE constexpr Base4(Base2<T> xy, Base2<T> zw) : x(xy.x), y(xy.y), z(zw.x), w(zw.y) {}
		ECS_INLINE constexpr Base4(Base3<T> xyz, T _w) : x(xyz.x), y(xyz.y), z(xyz.z), w(_w) {}
		ECS_INLINE constexpr Base4(T _x, T _y, T _z, T _w) : x(_x), y(_y), z(_z), w(_w) {}
		
		template<typename OtherBase>
		ECS_INLINE constexpr Base4(Base4<OtherBase> xyzw) : x(xyzw.x), y(xyzw.y), z(xyzw.z), w(xyzw.w) {}

		// assumes all components
		ECS_INLINE Base4(const T* ptr) : x(*ptr), y(*(ptr + 1)), z(*(ptr + 2)), w(*(ptr + 3)) {}

		Base4(const Base4& other) = default;
		Base4& operator =(const Base4& other) = default;

		ECS_INLINE bool operator == (const Base4& other) const {
			return x == other.x && y == other.y && z == other.z && w == other.w;
		}

		ECS_INLINE bool operator != (const Base4& other) const {
			return x != other.x || y != other.y || z != other.z || w != other.w;
		}

		ECS_INLINE Base4 operator !() const {
			return Base4<T>(!x, !y, !z, !w);
		}

		ECS_INLINE Base4 operator + (const Base4& other) const {
			return { x + other.x, y + other.y, z + other.z, w + other.w };
		}

		ECS_INLINE Base4& operator += (const Base4& other) {
			x += other.x;
			y += other.y;
			z += other.z;
			w += other.w;
			return *this;
		}

		ECS_INLINE Base4 operator - (const Base4& other) const {
			return { x - other.x, y - other.y, z - other.z, w - other.w };
		}

		ECS_INLINE Base4& operator -= (const Base4& other) {
			x -= other.x;
			y -= other.y;
			z -= other.z;
			w -= other.w;
			return *this;
		}

		ECS_INLINE Base4 operator * (const Base4& other) const {
			return { x * other.x, y * other.y, z * other.z, w * other.w };
		}

		ECS_INLINE Base4& operator *= (const Base4& other) {
			x *= other.x;
			y *= other.y;
			z *= other.z;
			w *= other.w;
			return *this;
		}

		ECS_INLINE Base4 operator * (float factor) const {
			return *this * Splat(factor);
		}

		ECS_INLINE Base4 operator *= (float factor) {
			x *= factor;
			y *= factor;
			z *= factor;
			w *= factor;
			return *this;
		}

		ECS_INLINE Base4 operator / (float factor) const {
			return *this * Splat(1.0f / factor);
		}

		ECS_INLINE Base4 operator /= (float factor) {
			float inverse_factor = 1.0f / factor;
			x *= inverse_factor;
			y *= inverse_factor;
			z *= inverse_factor;
			w *= inverse_factor;
			return *this;
		}

		ECS_INLINE Base4 operator / (const Base4& other) const {
			return { x / other.x, y / other.y, z / other.z, w / other.w };
		}

		ECS_INLINE Base4& operator /= (const Base4& other) {
			x /= other.x;
			y /= other.y;
			z /= other.z;
			w /= other.w;
			return *this;
		}

		ECS_INLINE Base4 operator -() const {
			return { -x, -y, -z, -w };
		}

		ECS_INLINE T& operator [](unsigned int index) {
			return *((T*)this + index);
		}

		ECS_INLINE const T& operator[](unsigned int index) const {
			return *((const T*)this + index);
		}

		ECS_INLINE Base2<T> xy() const {
			return Base2<T>(x, y);
		}

		ECS_INLINE Base2<T> zw() const {
			return Base2<T>(z, w);
		}

		ECS_INLINE Base3<T> xyz() const {
			return Base3<T>(x, y, z);
		}

		ECS_INLINE static Base4 Splat(T value) {
			return Base4(value, value, value, value);
		}

		T x;
		T y;
		T z;
		T w;
	};

	typedef Base2<bool> bool2;
	typedef Base2<char> char2;
	typedef Base2<unsigned char> uchar2;
	typedef Base2<short> short2;
	typedef Base2<unsigned short> ushort2;
	typedef Base2<int> int2;
	typedef Base2<unsigned int> uint2;
	typedef Base2<int64_t> long2;
	typedef Base2<uint64_t> ulong2;
	typedef Base2<float> float2;
	typedef Base2<double> double2;
	
	typedef Base3<bool> bool3;
	typedef Base3<char> char3;
	typedef Base3<unsigned char> uchar3;
	typedef Base3<short> short3;
	typedef Base3<unsigned short> ushort3;
	typedef Base3<int> int3;
	typedef Base3<unsigned int> uint3;
	typedef Base3<int64_t> long3;
	typedef Base3<uint64_t> ulong3;
	typedef Base3<float> float3;
	typedef Base3<double> double3;
	
	typedef Base4<bool> bool4;
	typedef Base4<char> char4;
	typedef Base4<unsigned char> uchar4;
	typedef Base4<short> short4;
	typedef Base4<unsigned short> ushort4;
	typedef Base4<int> int4;
	typedef Base4<unsigned int> uint4;
	typedef Base4<int64_t> long4;
	typedef Base4<uint64_t> ulong4;
	typedef Base4<float> float4;
	typedef Base4<double> double4;

	template<typename ValueType>
	struct Rectangle {
		typedef ValueType Value;

		ECS_INLINE ValueType GetScale() const {
			return bottom_right - top_left;
		}

		ECS_INLINE ValueType* AsValues() {
			return (ValueType*)this;
		}

		ECS_INLINE const ValueType* AsValues() const {
			return (const ValueType*)this;
		}

		ECS_INLINE constexpr static size_t Count() {
			return 2;
		}

		ECS_INLINE static Rectangle<ValueType> FromScale(ValueType position, ValueType scale) {
			return { position, position + scale };
		}

		ValueType top_left;
		ValueType bottom_right;
	};

	typedef Rectangle<float2> Rectangle2D;
	typedef Rectangle<float3> Rectangle3D;

	template<typename ValueType>
	struct FullRectangle {
		typedef ValueType Value;

		ECS_INLINE ValueType GetScale() const {
			return bottom_right - top_left;
		}

		ECS_INLINE ValueType* AsValues() {
			return (ValueType*)this;
		}

		ECS_INLINE const ValueType* AsValues() const {
			return (const ValueType*)this;
		}

		ECS_INLINE constexpr static size_t Count() {
			return 4;
		}

		ValueType top_left;
		ValueType top_right;
		ValueType bottom_right;
		ValueType bottom_left;
	};

	typedef FullRectangle<float2> FullRectangle2D;
	typedef FullRectangle<float3> FullRectangle3D;

	template<typename ValueType>
	struct Line {
		ValueType A;
		ValueType B;
	};

	typedef Line<float2> Line2D;
	typedef Line<float3> Line3D;

	// Allow these functions to receive a specified return type
	// such that we can do something like applying functors on floats
	// and generate booleans

	template<typename ReturnType, typename BasicType, typename Functor>
	ECS_INLINE ReturnType BasicType2Action(BasicType type, Functor&& functor) {
		return { (typename ReturnType::T)functor(type.x), (typename ReturnType::T)functor(type.y) };
	}

	template<typename ReturnType, typename BasicType, typename Functor>
	ECS_INLINE ReturnType BasicType3Action(BasicType type, Functor&& functor) {
		return { (typename ReturnType::T)functor(type.x), (typename ReturnType::T)functor(type.y), (typename ReturnType::T)functor(type.z) };
	}

	template<typename ReturnType, typename BasicType, typename Functor>
	ECS_INLINE ReturnType BasicType4Action(BasicType type, Functor&& functor) {
		return {
			(typename ReturnType::T)functor(type.x),
			(typename ReturnType::T)functor(type.y),
			(typename ReturnType::T)functor(type.z),
			(typename ReturnType::T)functor(type.w)
		};
	}

	template<typename ReturnType, typename BasicType, typename Functor>
	ECS_INLINE ReturnType BasicTypeAction(BasicType type, Functor&& functor) {
		// Check for fundamental types first since they don't have the Count() static function
		
		if constexpr (std::is_arithmetic_v<BasicType>) {
			// Allow this case for when we have types like bool and bool2 and we want
			// to apply a functor on both cases without having to write conditional checks
			return functor(type);
		}
		else {
			constexpr size_t base_count = BasicType::Count();
			if constexpr (base_count == 2) {
				return BasicType2Action<ReturnType>(type, functor);
			}
			else if constexpr (base_count == 3) {
				return BasicType3Action<ReturnType>(type, functor);
			}
			else if constexpr (base_count == 4) {
				return BasicType4Action<ReturnType>(type, functor);
			}
			else {
				static_assert(false, "Invalid Basic Type for Action");
			}
		}
	}

	// Apply a pair-wise function on basic type with 2 components
	template<typename ReturnType, typename BasicType, typename Functor>
	ECS_INLINE ReturnType BasicType2Action(BasicType first, BasicType second, Functor&& functor) {
		return { (typename ReturnType::T)functor(first.x, second.x), (typename ReturnType::T)functor(first.y, second.y) };
	}

	// Apply a pair-wise function on a baisc type with 3 components
	template<typename ReturnType, typename BasicType, typename Functor>
	ECS_INLINE ReturnType BasicType3Action(BasicType first, BasicType second, Functor&& functor) {
		return { 
			(typename ReturnType::T)functor(first.x, second.x),
			(typename ReturnType::T)functor(first.y, second.y),
			(typename ReturnType::T)functor(first.z, second.z)
		};
	}

	// Apply a pair-wise function ob a basic type with 4 components
	template<typename ReturnType, typename BasicType, typename Functor>
	ECS_INLINE ReturnType BasicType4Action(BasicType first, BasicType second, Functor&& functor) {
		return { 
			(typename ReturnType::T)functor(first.x, second.x),
			(typename ReturnType::T)functor(first.y, second.y),
			(typename ReturnType::T)functor(first.z, second.z),
			(typename ReturnType::T)functor(first.w, second.y)
		};
	}

	template<typename ReturnType, typename BasicType, typename Functor>
	ECS_INLINE ReturnType BasicTypeAction(BasicType first, BasicType second, Functor&& functor) {
		// Check for fundamental types first since they don't have the Count() static function
		if constexpr (std::is_arithmetic_v<BasicType>) {
			// Allow this case for when we have types like bool and bool2 and we want
			// to apply a functor on both cases without having to write conditional checks
			return functor(first, second);
		}
		else {
			constexpr size_t base_count = BasicType::Count();
			if constexpr (base_count == 2) {
				return BasicType2Action<ReturnType>(first, second, functor);
			}
			else if constexpr (base_count == 3) {
				return BasicType3Action<ReturnType>(first, second, functor);
			}
			else if constexpr (base_count == 4) {
				return BasicType4Action<ReturnType>(first, second, functor);
			}
			else {
				static_assert(false, "Invalid Basic Type for Action");
			}
		}
	}

	template<typename BasicType>
	ECS_INLINE BasicType BasicTypeAbsoluteDifference(BasicType first, BasicType second) {
		return BasicTypeAction<BasicType>(first, second, [](auto first, auto second) {
			return first > second ? first - second : second - first;
		});
	}

	template<typename BasicType>
	ECS_INLINE BasicType BasicTypeAbs(BasicType value) {
		return BasicTypeAction<BasicType>(value, [](auto value) {
			return abs(value);
		});
	}

	template<typename BasicType>
	ECS_INLINE BasicType BasicTypeDegToRad(BasicType angles) {
		return BasicTypeAction<BasicType>(angles, [](auto value) {
			return DegToRad((float)value);
		});
	}

	template<typename BasicType>
	ECS_INLINE BasicType BasicTypeRadToDeg(BasicType angles) {
		return BasicTypeAction<BasicType>(angles, [](auto value) {
			return RadToDeg((float)value);
		});
	}

	template<typename BasicType>
	ECS_INLINE BasicType BasicTypeClampMin(BasicType value, BasicType min) {
		return BasicTypeAction<BasicType>(value, min, [](auto value, auto min) {
			return value < min ? min : value;
		});
	}

	template<typename BasicType>
	ECS_INLINE BasicType BasicTypeClampMax(BasicType value, BasicType max) {
		return BasicTypeAction<BasicType>(value, max, [](auto value, auto max) {
			return value > max ? max : value;
		});
	}

	template<typename BasicType>
	ECS_INLINE BasicType BasicTypeClamp(BasicType value, BasicType min, BasicType max) {
		return BasicTypeClampMax(BasicTypeClampMin(value, min), max);
	}

	template<typename BasicType>
	ECS_INLINE BasicType BasicTypeMin(BasicType a, BasicType b) {
		return BasicTypeAction<BasicType>(a, b, [](auto a, auto b) {
			return a < b ? a : b;
		});
	}

	template<typename BasicType>
	ECS_INLINE BasicType BasicTypeMax(BasicType a, BasicType b) {
		return BasicTypeAction<BasicType>(a, b, [](auto a, auto b) {
			return a > b ? a : b;
		});
	}

	enum BasicTypeLogicOp {
		ECS_BASIC_TYPE_LOGIC_AND,
		ECS_BASIC_TYPE_LOGIC_OR,
	};

	// This function accepts normal arithmetic values like float, double
	// The functor must convert the given value into a boolean
	// Can choose whether or not to use early exiting to stop evaluating the members
	// When the condition result is known
	template<bool early_exit, BasicTypeLogicOp logic_op, typename Functor, typename BasicType>
	bool BasicTypeLogicalOpImplementation(BasicType value, Functor&& functor) {
		bool logic_values[4];
		size_t iterate_count = 0;

		if constexpr (std::is_arithmetic_v<BasicType>) {
			logic_values[0] = functor(value);
		}
		else {
			if constexpr (BasicType::Count() > 4) {
				static_assert(false, "Invalid basic type for logical op");
			}

			logic_values[0] = functor(value.x);
			if constexpr (early_exit) {
				if constexpr (logic_op == ECS_BASIC_TYPE_LOGIC_AND) {
					if (!logic_values[0]) {
						return false;
					}
				}
				else if constexpr (logic_op == ECS_BASIC_TYPE_LOGIC_OR) {
					if (logic_values[0]) {
						return true;
					}
				}
			}

			logic_values[1] = functor(value.y);
			if constexpr (early_exit) {
				if constexpr (logic_op == ECS_BASIC_TYPE_LOGIC_AND) {
					if (!logic_values[1]) {
						return false;
					}
				}
				else if constexpr (logic_op == ECS_BASIC_TYPE_LOGIC_OR) {
					if (logic_values[1]) {
						return true;
					}
				}
			}

			if constexpr (BasicType::Count() == 3) {
				logic_values[2] = functor(value.z);
				if constexpr (early_exit) {
					if constexpr (logic_op == ECS_BASIC_TYPE_LOGIC_AND) {
						if (!logic_values[2]) {
							return false;
						}
					}
					else if constexpr (logic_op == ECS_BASIC_TYPE_LOGIC_OR) {
						if (logic_values[2]) {
							return true;
						}
					}
				}
			}
			if constexpr (BasicType::Count() == 4) {
				logic_values[3] = functor(value.w);
				if constexpr (early_exit) {
					if constexpr (logic_op == ECS_BASIC_TYPE_LOGIC_AND) {
						if (!logic_values[3]) {
							return false;
						}
					}
					else if constexpr (logic_op == ECS_BASIC_TYPE_LOGIC_OR) {
						if (logic_values[3]) {
							return true;
						}
					}
				}
			}
			iterate_count = BasicType::Count() - 1;
		}

		for (size_t index = 0; index < iterate_count; index++) {
			if constexpr (logic_op == ECS_BASIC_TYPE_LOGIC_AND) {
				logic_values[0] &= logic_values[index + 1];
			}
			else if constexpr (logic_op == ECS_BASIC_TYPE_LOGIC_OR) {
				logic_values[0] |= logic_values[index + 1];
			}
			else {
				static_assert(false);
			}
		}
		return logic_values[0];
	}

	// This function accepts normal arithmetic values like float, double by reference
	// The functor must convert the given value into a boolean
	// Can choose whether or not to use early exiting to stop evaluating the members
	// When the condition result is known
	template<bool early_exit, BasicTypeLogicOp logic_op, typename Functor, typename BasicType>
	bool BasicTypeLogicalOpImplementationMutable(BasicType& value, Functor&& functor) {
		bool logic_values[4];
		size_t iterate_count = 0;

		if constexpr (std::is_arithmetic_v<BasicType>) {
			logic_values[0] = functor(value);
		}
		else {
			if constexpr (BasicType::Count() > 4) {
				static_assert(false, "Invalid basic type for logical op");
			}

			logic_values[0] = functor(value.x);
			if constexpr (early_exit) {
				if constexpr (logic_op == ECS_BASIC_TYPE_LOGIC_AND) {
					if (!logic_values[0]) {
						return false;
					}
				}
				else if constexpr (logic_op == ECS_BASIC_TYPE_LOGIC_OR) {
					if (logic_values[0]) {
						return true;
					}
				}
			}

			logic_values[1] = functor(value.y);
			if constexpr (early_exit) {
				if constexpr (logic_op == ECS_BASIC_TYPE_LOGIC_AND) {
					if (!logic_values[1]) {
						return false;
					}
				}
				else if constexpr (logic_op == ECS_BASIC_TYPE_LOGIC_OR) {
					if (logic_values[1]) {
						return true;
					}
				}
			}

			if constexpr (BasicType::Count() == 3) {
				logic_values[2] = functor(value.z);
				if constexpr (early_exit) {
					if constexpr (logic_op == ECS_BASIC_TYPE_LOGIC_AND) {
						if (!logic_values[2]) {
							return false;
						}
					}
					else if constexpr (logic_op == ECS_BASIC_TYPE_LOGIC_OR) {
						if (logic_values[2]) {
							return true;
						}
					}
				}
			}
			if constexpr (BasicType::Count() == 4) {
				logic_values[3] = functor(value.w);
				if constexpr (early_exit) {
					if constexpr (logic_op == ECS_BASIC_TYPE_LOGIC_AND) {
						if (!logic_values[3]) {
							return false;
						}
					}
					else if constexpr (logic_op == ECS_BASIC_TYPE_LOGIC_OR) {
						if (logic_values[3]) {
							return true;
						}
					}
				}
			}
			iterate_count = BasicType::Count() - 1;
		}

		for (size_t index = 0; index < iterate_count; index++) {
			if constexpr (logic_op == ECS_BASIC_TYPE_LOGIC_AND) {
				logic_values[0] &= logic_values[index + 1];
			}
			else if constexpr (logic_op == ECS_BASIC_TYPE_LOGIC_OR) {
				logic_values[0] |= logic_values[index + 1];
			}
			else {
				static_assert(false);
			}
		}
		return logic_values[0];
	}

	// This function accepts normal arithmetic values like float, double
	// The functor must convert the given value into a boolean
	// Can choose whether or not to use early exiting to stop evaluating the members
	// When the condition result is known
	template<bool early_exit, BasicTypeLogicOp logic_op, typename Functor, typename BasicType>
	bool BasicTypeLogicalOpImplementation(BasicType first, BasicType second, Functor&& functor) {
		bool logic_values[4];
		size_t iterate_count = 0;

		if constexpr (std::is_arithmetic_v<BasicType>) {
			logic_values[0] = functor(first, second);
		}
		else {
			if constexpr (BasicType::Count() > 4) {
				static_assert(false, "Invalid basic type for logical op");
			}

			logic_values[0] = functor(first.x, second.x);
			if constexpr (early_exit) {
				if constexpr (logic_op == ECS_BASIC_TYPE_LOGIC_AND) {
					if (!logic_values[0]) {
						return false;
					}
				}
				else if constexpr (logic_op == ECS_BASIC_TYPE_LOGIC_OR) {
					if (logic_values[0]) {
						return true;
					}
				}
			}

			logic_values[1] = functor(first.y, second.y);
			if constexpr (early_exit) {
				if constexpr (logic_op == ECS_BASIC_TYPE_LOGIC_AND) {
					if (!logic_values[1]) {
						return false;
					}
				}
				else if constexpr (logic_op == ECS_BASIC_TYPE_LOGIC_OR) {
					if (logic_values[1]) {
						return true;
					}
				}
			}

			if constexpr (BasicType::Count() == 3) {
				logic_values[2] = functor(first.z, second.z);
				if constexpr (early_exit) {
					if constexpr (logic_op == ECS_BASIC_TYPE_LOGIC_AND) {
						if (!logic_values[2]) {
							return false;
						}
					}
					else if constexpr (logic_op == ECS_BASIC_TYPE_LOGIC_OR) {
						if (logic_values[2]) {
							return true;
						}
					}
				}
			}
			if constexpr (BasicType::Count() == 4) {
				logic_values[3] = functor(first.w, second.w);
				if constexpr (early_exit) {
					if constexpr (logic_op == ECS_BASIC_TYPE_LOGIC_AND) {
						if (!logic_values[3]) {
							return false;
						}
					}
					else if constexpr (logic_op == ECS_BASIC_TYPE_LOGIC_OR) {
						if (logic_values[3]) {
							return true;
						}
					}
				}
			}
			iterate_count = BasicType::Count() - 1;
		}

		for (size_t index = 0; index < iterate_count; index++) {
			if constexpr (logic_op == ECS_BASIC_TYPE_LOGIC_AND) {
				logic_values[0] &= logic_values[index + 1];
			}
			else if constexpr (logic_op == ECS_BASIC_TYPE_LOGIC_OR) {
				logic_values[0] |= logic_values[index + 1];
			}
			else {
				static_assert(false);
			}
		}
		return logic_values[0];
	}

	// This function accepts normal arithmetic values like float, double by reference
	// The functor must convert the given value into a boolean
	// Can choose whether or not to use early exiting to stop evaluating the members
	// When the condition result is known
	template<bool early_exit, BasicTypeLogicOp logic_op, typename Functor, typename BasicType>
	bool BasicTypeLogicalOpImplementationMutable(BasicType& first, BasicType& second, Functor&& functor) {
		bool logic_values[4];
		size_t iterate_count = 0;

		if constexpr (std::is_arithmetic_v<BasicType>) {
			logic_values[0] = functor(first, second);
		}
		else {
			if constexpr (BasicType::Count() > 4) {
				static_assert(false, "Invalid basic type for logical op");
			}

			logic_values[0] = functor(first.x, second.x);
			if constexpr (early_exit) {
				if constexpr (logic_op == ECS_BASIC_TYPE_LOGIC_AND) {
					if (!logic_values[0]) {
						return false;
					}
				}
				else if constexpr (logic_op == ECS_BASIC_TYPE_LOGIC_OR) {
					if (logic_values[0]) {
						return true;
					}
				}
			}

			logic_values[1] = functor(first.y, second.y);
			if constexpr (early_exit) {
				if constexpr (logic_op == ECS_BASIC_TYPE_LOGIC_AND) {
					if (!logic_values[1]) {
						return false;
					}
				}
				else if constexpr (logic_op == ECS_BASIC_TYPE_LOGIC_OR) {
					if (logic_values[1]) {
						return true;
					}
				}
			}

			if constexpr (BasicType::Count() == 3) {
				logic_values[2] = functor(first.z, second.z);
				if constexpr (early_exit) {
					if constexpr (logic_op == ECS_BASIC_TYPE_LOGIC_AND) {
						if (!logic_values[2]) {
							return false;
						}
					}
					else if constexpr (logic_op == ECS_BASIC_TYPE_LOGIC_OR) {
						if (logic_values[2]) {
							return true;
						}
					}
				}
			}
			if constexpr (BasicType::Count() == 4) {
				logic_values[3] = functor(first.w, second.w);
				if constexpr (early_exit) {
					if constexpr (logic_op == ECS_BASIC_TYPE_LOGIC_AND) {
						if (!logic_values[3]) {
							return false;
						}
					}
					else if constexpr (logic_op == ECS_BASIC_TYPE_LOGIC_OR) {
						if (logic_values[3]) {
							return true;
						}
					}
				}
			}
			iterate_count = BasicType::Count() - 1;
		}

		for (size_t index = 0; index < iterate_count; index++) {
			if constexpr (logic_op == ECS_BASIC_TYPE_LOGIC_AND) {
				logic_values[0] &= logic_values[index + 1];
			}
			else if constexpr (logic_op == ECS_BASIC_TYPE_LOGIC_OR) {
				logic_values[0] |= logic_values[index + 1];
			}
			else {
				static_assert(false);
			}
		}
		return logic_values[0];
	}

	// This function accepts normal arithmetic values like float, double
	// The functor must convert the given value into a boolean
	template<BasicTypeLogicOp logic_op, typename Functor, typename BasicType>
	ECS_INLINE bool BasicTypeLogicalOp(BasicType value, Functor&& functor) {
		return BasicTypeLogicalOpImplementation<false, logic_op>(value, functor);
	}

	// This function accepts normal arithmetic values like float, double by reference
	// The functor must convert the given value into a boolean
	template<BasicTypeLogicOp logic_op, typename Functor, typename BasicType>
	ECS_INLINE bool BasicTypeLogicalOpMutable(BasicType& value, Functor&& functor) {
		return BasicTypeLogicalOpImplementationMutable<false, logic_op>(value, functor);
	}

	// This function accepts normal arithmetic values like float, double
	// The functor must convert the given value into a boolean
	template<BasicTypeLogicOp logic_op, typename Functor, typename BasicType>
	ECS_INLINE bool BasicTypeLogicalOpEarlyExit(BasicType value, Functor&& functor) {
		return BasicTypeLogicalOpImplementation<true, logic_op>(value, functor);
	}

	// This function accepts normal arithmetic values like float, double by reference
	// The functor must convert the given value into a boolean
	template<BasicTypeLogicOp logic_op, typename Functor, typename BasicType>
	ECS_INLINE bool BasicTypeLogicalOpEarlyExitMutable(BasicType& value, Functor&& functor) {
		return BasicTypeLogicalOpImplementationMutable<true, logic_op>(value, functor);
	}

	// This function accepts normal arithmetic values like float, double
	// The functor must convert the given value into a boolean
	template<BasicTypeLogicOp logic_op, typename Functor, typename BasicType>
	ECS_INLINE bool BasicTypeLogicalOp(BasicType first, BasicType second, Functor&& functor) {
		return BasicTypeLogicalOpImplementation<false, logic_op>(first, second, functor);
	}

	// This function accepts normal arithmetic values like float, double by reference
	// The functor must convert the given value into a boolean
	template<BasicTypeLogicOp logic_op, typename Functor, typename BasicType>
	ECS_INLINE bool BasicTypeLogicalOpMutable(BasicType& first, BasicType& second, Functor&& functor) {
		return BasicTypeLogicalOpImplementationMutable<false, logic_op>(first, second, functor);
	}

	// This function accepts normal arithmetic values like float, double
	// The functor must convert the given value into a boolean
	template<BasicTypeLogicOp logic_op, typename Functor, typename BasicType>
	ECS_INLINE bool BasicTypeLogicalOpEarlyExit(BasicType first, BasicType second, Functor&& functor) {
		return BasicTypeLogicalOpImplementation<true, logic_op>(first, second, functor);
	}

	// This function accepts normal arithmetic values like float, double by reference
	// The functor must convert the given value into a boolean
	template<BasicTypeLogicOp logic_op, typename Functor, typename BasicType>
	ECS_INLINE bool BasicTypeLogicalOpEarlyExitMutable(BasicType& first, BasicType& second, Functor&& functor) {
		return BasicTypeLogicalOpImplementationMutable<true, logic_op>(first, second, functor);
	}

	// This can only be applied to boolean types
	template<BasicTypeLogicOp logic_op, typename BasicType>
	ECS_INLINE bool BasicTypeLogicOpBoolean(BasicType value) {
		if constexpr (std::is_same_v<BasicType, bool>) {
			return value;
		}

		return BasicTypeLogicalOp<logic_op>(value, [](auto value) {
			return value;
		});
	}

	template<typename BasicType>
	ECS_INLINE bool BasicTypeLogicAndBoolean(BasicType value) {
		return BasicTypeLogicOpBoolean<ECS_BASIC_TYPE_LOGIC_AND>(value);
	}

	template<typename BasicType>
	ECS_INLINE bool BasicTypeLogicOrBoolean(BasicType value) {
		return BasicTypeLogicOpBoolean<ECS_BASIC_TYPE_LOGIC_OR>(value);
	}

	// Only booleans should be used here
	template<typename BasicType>
	ECS_INLINE BasicType BasicTypeLogicAnd(BasicType a, BasicType b) {
		return BasicTypeAction<BasicType>(a, b, [](auto a_value, auto b_value) {
			return a_value && b_value;
		});
	}

	// Only booleans should be used here
	template<typename BasicType>
	ECS_INLINE BasicType BasicTypeLogicOr(BasicType a, BasicType b) {
		return BasicTypeAction<BasicType>(a, b, [](auto a_value, auto b_value) {
			return a_value || b_value;
		});
	}

	template<typename BasicType>
	ECS_INLINE BasicType BasicTypeSign(BasicType element) {
		return BasicTypeAction<BasicType>(element, [](auto value) {
			return value < 0 ? -1 : 1;
		});
	}

	template<typename BasicType>
	ECS_INLINE BasicType BasicTypeFloatCompare(BasicType a, BasicType b, BasicType epsilon) {
		return BasicTypeLogicAnd(BasicTypeAction<BasicType>(AbsoluteDifference(a, b), epsilon, [](auto difference_value, auto epsilon_value) {
			return difference_value < epsilon_value;
		}));
	}

	template<typename BasicType>
	ECS_INLINE bool BasicTypeFloatCompareBoolean(BasicType a, BasicType b, BasicType epsilon) {
		return BasicTypeLogicAndBoolean(BasicTypeAction<BasicType>(AbsoluteDifference(a, b), epsilon, [](auto difference_value, auto epsilon_value) {
			return difference_value < epsilon_value;
		}));
	}

	template<typename BasicType>
	ECS_INLINE bool BasicTypeLess(BasicType a, BasicType b) {
		return BasicTypeLogicalOp<ECS_BASIC_TYPE_LOGIC_AND>(a, b, [](auto a, auto b) {
			return a < b;
		});
	}

	template<typename BasicType>
	ECS_INLINE bool BasicTypeLessEqual(BasicType a, BasicType b) {
		return BasicTypeLogicalOp<ECS_BASIC_TYPE_LOGIC_AND>(a, b, [](auto a, auto b) {
			return a <= b;
		});
	}

	template<typename BasicType>
	ECS_INLINE bool BasicTypeGreater(BasicType a, BasicType b) {
		return BasicTypeLogicalOp<ECS_BASIC_TYPE_LOGIC_AND>(a, b, [](auto a, auto b) {
			return a > b;
		});
	}

	template<typename BasicType>
	ECS_INLINE bool BasicTypeGreaterEqual(BasicType a, BasicType b) {
		return BasicTypeLogicalOp<ECS_BASIC_TYPE_LOGIC_AND>(a, b, [](auto a, auto b) {
			return a >= b;
		});
	}

	template<typename BasicType>
	ECS_INLINE bool BasicTypeEqual(BasicType a, BasicType b) {
		return BasicTypeLogicalOp<ECS_BASIC_TYPE_LOGIC_AND>(a, b, [](auto a, auto b) {
			return a == b;
		});
	}

	template<typename BasicType>
	ECS_INLINE bool BasicTypeNotEqual(BasicType a, BasicType b) {
		return BasicTypeLogicalOp<ECS_BASIC_TYPE_LOGIC_AND>(a, b, [](auto a, auto b) {
			return a != b;
		});
	}

	// Returns true if the type is a basic type of 2 unsigned integers
	template<typename T>
	constexpr bool IsUnsignedInteger2() {
		return std::is_same_v<T, uchar2> || std::is_same_v<T, ushort2> || std::is_same_v<T, uint2> || std::is_same_v<T, ulong2>;
	}

	// Returns true if the type is a basic type of 2 signed integers
	template<typename T>
	constexpr bool IsSignedInteger2() {
		return std::is_same_v<T, char2> || std::is_same_v<T, short2> || std::is_same_v<T, int2> || std::is_same_v<T, long2>;
	}

	// Returns true if the type is a basic type of 2 integers (unsigned or signed)
	template<typename T>
	constexpr bool IsInteger2() {
		return IsUnsignedInteger2<T>() || IsSignedInteger2<T>();
	}

	// Returns true if the type is a basic type of 3 unsigned integers
	template<typename T>
	constexpr bool IsUnsignedInteger3() {
		return std::is_same_v<T, uchar3> || std::is_same_v<T, ushort3> || std::is_same_v<T, uint3> || std::is_same_v<T, ulong3>;
	}

	// Returns true if the type is a basic type of 3 signed integers
	template<typename T>
	constexpr bool IsSignedInteger3() {
		return std::is_same_v<T, char3> || std::is_same_v<T, short3> || std::is_same_v<T, int3> || std::is_same_v<T, long3>;
	}

	// Returns true if the type is a basic type of 3 integers (unsigned or signed)
	template<typename T>
	constexpr bool IsInteger3() {
		return IsUnsignedInteger3<T>() || IsSignedInteger3<T>();
	}

	// Returns true if the type is a basic type of 4 unsigned integers
	template<typename T>
	constexpr bool IsUnsignedInteger4() {
		return std::is_same_v<T, uchar4> || std::is_same_v<T, ushort4> || std::is_same_v<T, uint4> || std::is_same_v<T, ulong4>;
	}

	// Returns true if the type is a basic type of 4 signed integers
	template<typename T>
	constexpr bool IsSignedInteger4() {
		return std::is_same_v<T, char4> || std::is_same_v<T, short4> || std::is_same_v<T, int4> || std::is_same_v<T, long4>;
	}

	// Returns true if the type is a basic type of 4 integers (unsigned or signed)
	template<typename T>
	constexpr bool IsInteger4() {
		return IsUnsignedInteger4<T>() || IsSignedInteger4<T>();
	}

	// Returns true if the type is a basic type of 2, 3 or 4 unsigned integers
	template<typename T>
	constexpr bool IsBasicTypeUnsignedInteger() {
		return IsUnsignedInteger2<T>() || IsUnsignedInteger3<T>() || IsUnsignedInteger4<T>();
	}

	// Returns true if the type is a basic type of 1, 2, 3 or 4 unsigned integers
	template<typename T>
	constexpr bool IsBasicTypeUnsignedIntegerWithScalar() {
		return std::is_unsigned_v<T> || IsBasicTypeUnsignedInteger<T>();
	}

	// Returns true if the type is a basic type of 2, 3 or 4 signed integers
	template<typename T>
	constexpr bool IsBasicTypeSignedInteger() {
		return IsSignedInteger2<T>() || IsSignedInteger3<T>() || IsSignedInteger4<T>();
	}

	// Returns true if the type is a basic type of 1, 2, 3 or 4 signed integers
	template<typename T>
	constexpr bool IsBasicTypeSignedIntegerWithScalar() {
		return std::is_signed_v<T> || IsBasicTypeSignedInteger<T>();
	}

	// Returns true if the template parameter is a basic type of 2, 3 or 4 integer components (unsigned or signed)
	template<typename T>
	constexpr bool IsBasicTypeInteger() {
		return IsBasicTypeUnsignedInteger<T>() || IsBasicTypeSignedInteger<T>();
	}

	// Returns true if the template parameter is a basic type of 2, 3 or 4 integer components (unsigned or signed)
	// Or it is a singled valued integer type
	template<typename T>
	constexpr bool IsBasicTypeIntegerWithScalar() {
		return std::is_integral_v<T> || IsBasicTypeInteger<T>();
	}

}