#pragma once
#include "../Core.h"
#include "VCLExtensions.h"
#include "../Utilities/BasicTypes.h"

namespace ECSEngine {

	/*
		The reason for which we dropped a templated BaseVector class that would implement
		Most operations is that MSVC with __vectorcall doesn't generate optimized code
		If a structure contains a union or any other struct type. This is a bit sad, but
		The speed reduction from this is considerable that it cannot be ignored
	*/

	struct ECSENGINE_API Vector3 {
		ECS_INLINE Vector3() {}
		ECS_INLINE Vector3(Vec8f _x, Vec8f _y, Vec8f _z) : x(_x), y(_y), z(_z) {}

		ECS_INLINE Vector3(const Vector3& other) = default;
		ECS_INLINE Vector3& ECS_VECTORCALL operator = (const Vector3& other) = default;

		bool ECS_VECTORCALL operator == (Vector3 other) const;

		bool ECS_VECTORCALL operator != (Vector3 other) const;

		Vector3 ECS_VECTORCALL operator + (const Vector3 other) const;

		Vector3 ECS_VECTORCALL operator - (const Vector3 other) const;

		Vector3 ECS_VECTORCALL operator -() const;

		Vector3 ECS_VECTORCALL operator * (const Vector3 other) const;

		Vector3 ECS_VECTORCALL operator / (const Vector3 other) const;

		Vector3 ECS_VECTORCALL operator * (const Vec8f other) const;

		Vector3 ECS_VECTORCALL operator / (const Vec8f other) const;

		Vector3 ECS_VECTORCALL operator * (float value) const;

		Vector3 ECS_VECTORCALL operator / (float value) const;

		Vector3& ECS_VECTORCALL operator += (const Vector3 other);

		Vector3& ECS_VECTORCALL operator -= (const Vector3 other);

		Vector3& ECS_VECTORCALL operator *= (const Vector3 other);

		Vector3& ECS_VECTORCALL operator /= (const Vector3 other);

		Vector3& ECS_VECTORCALL operator *= (const Vec8f other);

		Vector3& ECS_VECTORCALL operator /= (const Vec8f other);

		Vector3& operator *= (float _value);

		Vector3& operator /= (float _value);

		// Use this only to retrieve a single value, since it is expensive
		float3 At(size_t index) const;

		Vector3& Load(const void** source_data);

		Vector3& LoadAligned(const void** source_data);

		Vector3& LoadPartial(const void** source_data, size_t count);

		Vector3& Load(const void* source_x, const void* source_y, const void* source_z);

		Vector3& LoadAligned(const void* source_x, const void* source_y, const void* source_z);

		Vector3& LoadPartial(const void* source_x, const void* source_y, const void* source_z, size_t load_count);

		// Load data from a buffer that is stored contiguously like XXX YYY ZZZ
		Vector3& LoadAdjacent(const void* source_data, size_t offset, size_t capacity);

		// Load data from a buffer that is stored contiguously like XXX YYY ZZZ
		Vector3& LoadAlignedAdjacent(const void* source_data, size_t offset, size_t capacity);

		// Load data from a buffer that is stored contiguously like XXX YYY ZZZ
		Vector3& LoadPartialAdjacent(const void* source_data, size_t offset, size_t capacity, size_t count);

		// Changes a single set of values
		void Set(float3 value, size_t index);

		// The value will be replicated across all slots
		Vector3& Splat(float3 value);

		static Vector3 Splat(Vec8f value);

		void Store(void** destination) const;

		void StoreAligned(void** destination) const;

		// The address needs to be aligned to a ECS_SIMD_BYTE_SIZE byte boundary
		void StoreStreamed(void** destination) const;

		void StorePartial(void** destination, size_t store_count) const;

		void Store(void* destination_x, void* destination_y, void* destination_z) const;

		void StoreAligned(void* destination_x, void* destination_y, void* destination_z) const;

		// The address needs to be aligned to a ECS_SIMD_BYTE_SIZE byte boundary
		void StoreStreamed(void* destination_x, void* destination_y, void* destination_z) const;

		void StorePartial(void* destination_x, void* destination_y, void* destination_z, size_t store_count) const;

		// Store the data for a buffer that is stored contiguously like XXX YYY ZZZ
		void StoreAdjacent(void* destination, size_t offset, size_t capacity) const;

		// Store the data for a buffer that is stored contiguously like XXX YYY ZZZ
		void StoreAlignedAdjacent(void* destination, size_t offset, size_t capacity) const;

		// Store the data for a buffer that is stored contiguously like XXX YYY ZZZ
		// The address needs to be aligned to a ECS_SIMD_BYTE_SIZE byte boundary
		void StoreStreamedAdjacent(void* destination, size_t offset, size_t capacity) const;

		// Store the data for a buffer that is stored contiguously like XXX YYY ZZZ
		void StorePartialAdjacent(void* destination, size_t offset, size_t capacity, size_t count) const;

		Vector3& Gather(const void* source_data);

		// This version will set the empty slots to the first gathered element
		Vector3& GatherPartial(const void* source_data, size_t load_count);

		// This version will set the empty slots to 0.0f
		Vector3& GatherPartialMasked(const void* source_data, size_t load_count);

		void Scatter(void* destination) const;

		void ScatterPartial(void* destination, size_t store_count) const;

		ECS_INLINE Vec8f& ECS_VECTORCALL operator [](size_t index) {
			Vec8f* values = (Vec8f*)this;
			return values[index];
		}

		ECS_INLINE const Vec8f& ECS_VECTORCALL operator [](size_t index) const {
			const Vec8f* values = (const Vec8f*)this;
			return values[index];
		}

		ECS_INLINE constexpr static size_t Count() {
			return 3;
		}

		ECS_INLINE constexpr static size_t ElementCount() {
			return Vec8f::size();
		}

		Vec8f x;
		Vec8f y;
		Vec8f z;
	};

	struct ECSENGINE_API Vector4 {
		ECS_INLINE Vector4() {}
		ECS_INLINE Vector4(float4 single_value) : x(single_value.x), y(single_value.y), z(single_value.z), w(single_value.w) {}
		ECS_INLINE Vector4(Vector3 xyz, Vec8f _w) : x(xyz.x), y(xyz.y), z(xyz.z), w(_w) {}
		ECS_INLINE Vector4(Vec8f _x, Vec8f _y, Vec8f _z, Vec8f _w) : x(_x), y(_y), z(_z), w(_w) {}

		ECS_INLINE Vector4(const Vector4& other) = default;
		ECS_INLINE Vector4& ECS_VECTORCALL operator = (const Vector4& other) = default;

		bool ECS_VECTORCALL operator == (Vector4 other) const;

		bool ECS_VECTORCALL operator != (Vector4 other) const;

		Vector4 ECS_VECTORCALL operator + (const Vector4 other) const;

		Vector4 ECS_VECTORCALL operator - (const Vector4 other) const;

		Vector4 ECS_VECTORCALL operator -() const;

		Vector4 ECS_VECTORCALL operator * (const Vector4 other) const;

		Vector4 ECS_VECTORCALL operator / (const Vector4 other) const;

		Vector4 ECS_VECTORCALL operator * (const Vec8f other) const;

		Vector4 ECS_VECTORCALL operator / (const Vec8f other) const;

		Vector4 ECS_VECTORCALL operator * (float value) const;

		Vector4 ECS_VECTORCALL operator / (float value) const;

		Vector4& ECS_VECTORCALL operator += (const Vector4 other);

		Vector4& ECS_VECTORCALL operator -= (const Vector4 other);

		Vector4& ECS_VECTORCALL operator *= (const Vector4 other);

		Vector4& ECS_VECTORCALL operator /= (const Vector4 other);

		Vector4& ECS_VECTORCALL operator *= (const Vec8f other);

		Vector4& ECS_VECTORCALL operator /= (const Vec8f other);

		Vector4& operator *= (float _value);

		Vector4& operator /= (float _value);

		ECS_INLINE Vector3 AsVector3() const {
			return { x, y, z };
		}

		float4 At(size_t index) const;

		Vector4& Load(const void** source_data);

		Vector4& LoadAligned(const void** source_data);

		Vector4& LoadPartial(const void** source_data, size_t count);

		Vector4& Load(const void* source_x, const void* source_y, const void* source_z, const void* source_w);

		Vector4& LoadAligned(const void* source_x, const void* source_y, const void* source_z, const void* source_w);

		Vector4& LoadPartial(const void* source_x, const void* source_y, const void* source_z, const void* source_w, size_t load_count);

		// Load data from a buffer that is stored contiguously like XXX YYY ZZZ
		Vector4& LoadAdjacent(const void* source_data, size_t offset, size_t capacity);

		// Load data from a buffer that is stored contiguously like XXX YYY ZZZ
		Vector4& LoadAlignedAdjacent(const void* source_data, size_t offset, size_t capacity);

		// Load data from a buffer that is stored contiguously like XXX YYY ZZZ
		Vector4& LoadPartialAdjacent(const void* source_data, size_t offset, size_t capacity, size_t count);

		// Changes a single set of values
		void Set(float4 value, size_t index);

		// The value will be replicated across all slots
		Vector4& Splat(float4 value);

		static Vector4 Splat(Vec8f value);

		void Store(void** destination) const;

		void StoreAligned(void** destination) const;

		// The address needs to be aligned to a ECS_SIMD_BYTE_SIZE byte boundary
		void StoreStreamed(void** destination) const;

		void StorePartial(void** destination, size_t store_count) const;

		void Store(void* destination_x, void* destination_y, void* destination_z, void* destination_w) const;

		void StoreAligned(void* destination_x, void* destination_y, void* destination_z, void* destination_w) const;

		// The address needs to be aligned to a ECS_SIMD_BYTE_SIZE byte boundary
		void StoreStreamed(void* destination_x, void* destination_y, void* destination_z, void* destination_w) const;

		void StorePartial(void* destination_x, void* destination_y, void* destination_z, void* destination_w, size_t store_count) const;

		// Store the data for a buffer that is stored contiguously like XXX YYY ZZZ
		void StoreAdjacent(void* destination, size_t offset, size_t capacity) const;

		// Store the data for a buffer that is stored contiguously like XXX YYY ZZZ
		void StoreAlignedAdjacent(void* destination, size_t offset, size_t capacity) const;

		// Store the data for a buffer that is stored contiguously like XXX YYY ZZZ
		// The address needs to be aligned to a 32 byte boundary
		void StoreStreamedAdjacent(void* destination, size_t offset, size_t capacity) const;

		// Store the data for a buffer that is stored contiguously like XXX YYY ZZZ
		void StorePartialAdjacent(void* destination, size_t offset, size_t capacity, size_t count) const;

		Vector4& Gather(const void* source_data);

		// This version will set the empty slots to the first gathered element
		Vector4& GatherPartial(const void* source_data, size_t load_count);

		// This version will set the empty slots to 0.0f
		Vector4& GatherPartialMasked(const void* source_data, size_t load_count);

		void Scatter(void* destination) const;

		void ScatterPartial(void* destination, size_t store_count) const;

		ECS_INLINE Vec8f& ECS_VECTORCALL operator [](size_t index) {
			Vec8f* values = (Vec8f*)this;
			return values[index];
		}

		ECS_INLINE const Vec8f& ECS_VECTORCALL operator [](size_t index) const {
			const Vec8f* values = (Vec8f*)this;
			return values[index];
		}

		ECS_INLINE constexpr static size_t Count() {
			return 4;
		}

		ECS_INLINE constexpr static size_t ElementCount() {
			return Vec8f::size();
		}

		Vec8f x;
		Vec8f y;
		Vec8f z;
		Vec8f w;
	};
	
	typedef Vec8fb SIMDVectorMask;

	struct ECSENGINE_API VectorMask {
		ECS_INLINE VectorMask() {}
		ECS_INLINE VectorMask(int _value) : value(_value) {}
		VectorMask(SIMDVectorMask vector_mask);

		// Returns true if the first given count of bits are set else false
		bool AreAllSet(int count) const;

		// Returns true if the first given count of bits are cleared else false
		bool AreAllCleared(int count) const;

		// Returns true if a bit from the first given count is set else false
		bool IsAnySet(int count) const;

		ECS_INLINE bool Get(int index) const {
			return value & (1 << index);
		}

		ECS_INLINE void Set(int index, bool new_value) {
			if (new_value) {
				value |= 1 << index;
			}
			else {
				value &= ~(1 << index);
			}
		}

		int value;
	};

	/*
		We need templates for these functions since conversions would ruin the functions
	*/

	// Mask should be bool or SIMDVectorMask
	template<typename Mask>
	ECS_INLINE bool SIMDMaskAreAllSet(Mask value, size_t count) {
		if constexpr (std::is_same_v<Mask, bool>) {
			return value;
		}
		else if constexpr (std::is_same_v<Mask, SIMDVectorMask> || std::is_same_v<Mask, Vec8f>) {
			return VectorMask{ value }.AreAllSet(count);
		}
	}

	// Mask should be bool or SIMDVectorMask
	template<typename Mask>
	ECS_INLINE bool SIMDMaskAreAllCleared(Mask value, size_t count) {
		if constexpr (std::is_same_v<Mask, bool>) {
			return !value;
		}
		else if constexpr (std::is_same_v<Mask, SIMDVectorMask> || std::is_same_v<Mask, Vec8f>) {
			return VectorMask{ value }.AreAllCleared(count);
		}
	}

}