#pragma once
#include "../Core.h"
#include <stdint.h>

namespace ECSEngine {

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