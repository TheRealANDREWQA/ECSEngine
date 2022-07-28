#pragma once
#include "../Core.h"
#include <stdint.h>
#include <malloc.h>

namespace ECSEngine {

	struct Date {
		unsigned char month;
		unsigned char day;
		unsigned char hour;
		unsigned char minute;
		unsigned char seconds;
		unsigned short year;
		unsigned short milliseconds;
	};

	struct DebugInfo {
		const char* file;
		const char* function;
		int line;
	};

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

#pragma endregion

	template<typename T, void (*deallocate_function)(T)>
	struct StackScope {
		StackScope() {}
		StackScope(T _value) : value(_value) {}
		~StackScope() { deallocate_function(value); }

		StackScope(const StackScope& other) = default;
		StackScope& operator = (const StackScope& other) = default;

		T value;
	};

	inline void ReleaseMalloca(void* buffer) {
		ECS_FREEA(buffer);
	}

	using ScopedMalloca = StackScope<void*, ReleaseMalloca>;

	/* Minimalist wrapper for simple structs composed of 2 fundamental types
	* It overloads the common operators:
	* ==, !=, +, -, *, /
	* [] for access ease
	*/
	template<typename Base>
	struct Base2 {
		constexpr Base2() {}
		/*constexpr Base2(Base _x) : x(_x) {}*/
		constexpr Base2(Base _x, Base _y) : x(_x), y(_y) {}

		// assumes all components
		Base2(const Base* ptr) : x(*ptr), y(*(ptr + 1)) {}

		Base2(const Base2& other) = default;
		Base2& operator =(const Base2& other) = default;

		bool operator == (const Base2& other) const {
			return x == other.x && y == other.y;
		}

		bool operator != (const Base2& other) const {
			return x != other.x || y != other.y;
		}

		Base2 operator + (const Base2& other) const {
			return { x + other.x, y + other.y };
		}

		Base2& operator += (const Base2& other) {
			x += other.x;
			y += other.y;
			return *this;
		}

		Base2 operator - (const Base2& other) const {
			return { x - other.x, y - other.y };
		}

		Base2& operator -= (const Base2& other) {
			x -= other.x;
			y -= other.y;
			return *this;
		}

		Base2 operator * (const Base2& other) const {
			return { x * other.x, y * other.y };
		}

		Base2& operator *= (const Base2& other) {
			x *= other.x;
			y *= other.y;
			return *this;
		}

		Base2 operator * (float factor) const {
			return *this * Splat(factor);
		}

		Base2 operator *= (float factor) {
			x *= factor;
			y *= factor;
			return *this;
		}

		Base2 operator / (const Base2& other) const {
			return { x / other.x, y / other.y };
		}

		Base2& operator /= (const Base2& other) {
			x /= other.x;
			y /= other.y;
			return *this;
		}

		Base2 operator -() const {
			return { -x, -y };
		}

		Base& operator [](unsigned int index) {
			return *((Base*)this + index);
		}

		const Base& operator [](unsigned int index) const {
			return *((const Base*)this + index);
		}
		
		static Base2 Splat(Base value) {
			return Base2(value, value);
		}

		Base x;
		Base y;
	};

	/* Minimalist wrapper for simple structs composed of 3 fundamental types
	* It overloads the common operators:
	* ==, !=, +, -, *, /
	* [] for access ease
	*/
	template<typename Base>
	struct Base3 {
		constexpr Base3() {}
		//constexpr Base3(Base _x) : x(_x) {}
		//constexpr Base3(Base _x, Base _y) : x(_x), y(_y) {}
		constexpr Base3(Base _x, Base _y, Base _z) : x(_x), y(_y), z(_z) {}

		// assumes all components
		Base3(const Base* ptr) : x(*ptr), y(*(ptr + 1)), z(*(ptr + 2)) {}

		Base3(const Base3& other) = default;
		Base3& operator =(const Base3& other) = default;

		bool operator == (const Base3& other) const {
			return x == other.x && y == other.y && z == other.z;
		}

		bool operator != (const Base3& other) const {
			return x != other.x || y != other.y || z != other.z;
		}

		Base3 operator + (const Base3& other) const {
			return { x + other.x, y + other.y, z + other.z };
		}

		Base3& operator += (const Base3& other) {
			x += other.x;
			y += other.y;
			z += other.z;
			return *this;
		}

		Base3 operator - (const Base3& other) const {
			return { x - other.x, y - other.y, z - other.z };
		}

		Base3& operator -= (const Base3& other) {
			x -= other.x;
			y -= other.y;
			z -= other.z;
			return *this;
		}

		Base3 operator * (const Base3& other) const {
			return { x * other.x, y * other.y, z * other.z };
		}

		Base3& operator *= (const Base3& other) {
			x *= other.x;
			y *= other.y;
			z *= other.z;
			return *this;
		}

		Base3 operator * (float factor) const {
			return *this * Splat(factor);
		}

		Base3 operator *= (float factor) {
			x *= factor;
			y *= factor;
			z *= factor;
			return *this;
		}

		Base3 operator / (const Base3& other) const {
			return { x / other.x, y / other.y, z / other.z };
		}

		Base3& operator /= (const Base3& other) {
			x /= other.x;
			y /= other.y;
			z /= other.z;
			return *this;
		}

		Base3 operator -() const {
			return Base3(-x, -y, -z);
		}

		Base& operator [](unsigned int index) {
			return *((Base*)this + index);
		}

		const Base& operator[](unsigned int index) const {
			return *((const Base*)this + index);
		}

		static Base3 Splat(Base value) {
			return Base3(value, value, value);
		}

		Base x;
		Base y;
		Base z;
	};

	/* Minimalist wrapper for simple structs composed of 4 fundamental types
	* It overloads the common operators:
	* ==, !=, +, -, *, /
	* [] for access ease
	*/
	template<class Base>
	struct Base4 {
		constexpr  Base4() {}
		//constexpr Base4(Base _x) : x(_x) {}
		//constexpr Base4(Base _x, Base _y) : x(_x), y(_y) {}
		//constexpr Base4(Base _x, Base _y, Base _z) : x(_x), y(_y), z(_z) {}
		constexpr Base4(Base _x, Base _y, Base _z, Base _w) : x(_x), y(_y), z(_z), w(_w) {}

		// assumes all components
		Base4(const Base* ptr) : x(*ptr), y(*(ptr + 1)), z(*(ptr + 2)), w(*(ptr + 3)) {}

		Base4(const Base4& other) = default;
		Base4& operator =(const Base4& other) = default;

		bool operator == (const Base4& other) const {
			return x == other.x && y == other.y && z == other.z && w == other.w;
		}

		bool operator != (const Base4& other) const {
			return x != other.x || y != other.y || z != other.z || w != other.w;
		}

		Base4 operator + (const Base4& other) const {
			return { x + other.x, y + other.y, z + other.z, w + other.w };
		}

		Base4& operator += (const Base4& other) {
			x += other.x;
			y += other.y;
			z += other.z;
			w += other.w;
			return *this;
		}

		Base4 operator - (const Base4& other) const {
			return { x - other.x, y - other.y, z - other.z, w - other.w };
		}

		Base4& operator -= (const Base4& other) {
			x -= other.x;
			y -= other.y;
			z -= other.z;
			w -= other.w;
			return *this;
		}

		Base4 operator * (const Base4& other) const {
			return { x * other.x, y * other.y, z * other.z, w * other.w };
		}

		Base4& operator *= (const Base4& other) {
			x *= other.x;
			y *= other.y;
			z *= other.z;
			w *= other.w;
			return *this;
		}

		Base4 operator * (float factor) const {
			return *this * Splat(factor);
		}

		Base4 operator *= (float factor) {
			x *= factor;
			y *= factor;
			z *= factor;
			w *= factor;
			return *this;
		}

		Base4 operator / (const Base4& other) const {
			return { x / other.x, y / other.y, z / other.z, w / other.w };
		}

		Base4& operator /= (const Base4& other) {
			x /= other.x;
			y /= other.y;
			z /= other.z;
			w /= other.w;
			return *this;
		}

		Base4 operator -() const {
			return { -x, -y, -z, -w };
		}

		Base& operator [](unsigned int index) {
			return *((Base*)this + index);
		}

		const Base& operator[](unsigned int index) const {
			return *((const Base*)this + index);
		}

		static Base4 Splat(Base value) {
			return Base4(value, value, value, value);
		}

		Base x;
		Base y;
		Base z;
		Base w;
	};

	using bool2 = Base2<bool>;
	using char2 = Base2<char>;
	using uchar2 = Base2<unsigned char>;
	using short2 = Base2<short>;
	using ushort2 = Base2<unsigned short>;
	using int2 = Base2<int>;
	using uint2 = Base2<unsigned int>;
	using long2 = Base2<int64_t>;
	using ulong2 = Base2<uint64_t>;
	using float2 = Base2<float>;
	using double2 = Base2<double>;

	using bool3 = Base3<bool>;
	using char3 = Base3<char>;
	using uchar3 = Base3<unsigned char>;
	using short3 = Base3<short>;
	using ushort3 = Base3<unsigned short>;
	using int3 = Base3<int>;
	using uint3 = Base3<unsigned int>;
	using long3 = Base3<int64_t>;
	using ulong3 = Base3<uint64_t>;
	using float3 = Base3<float>;
	using double3 = Base3<double>;

	using bool4 = Base4<bool>;
	using char4 = Base4<char>;
	using uchar4 = Base4<unsigned char>;
	using short4 = Base4<short>;
	using ushort4 = Base4<unsigned short>;
	using int4 = Base4<int>;
	using uint4 = Base4<unsigned int>;
	using long4 = Base4<int64_t>;
	using ulong4 = Base4<uint64_t>;
	using float4 = Base4<float>;
	using double4 = Base4<double>;

}