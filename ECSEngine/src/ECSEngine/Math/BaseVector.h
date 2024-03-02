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
		typedef Vec8f T;

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
		float3 ECS_VECTORCALL At(size_t index) const;

		// Loads a stored vector in the XXX YYY ZZZ format with SIMD_WIDTH entries
		Vector3& LoadEntry(const void* data);

		// Loads a stored vector in the XXX YYY ZZZ format with load count entries
		Vector3& LoadEntry(const void* data, size_t load_count);

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
		static Vector3 Splat(float3 value);

		static Vector3 Splat(Vec8f value);

		void StoreEntry(void* destination) const;

		void StoreEntry(void* destination, size_t store_count) const;

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

	// For a memory based vector value, it will write the values from the float3 in a SoA
	// Order such that you can use the vector as is in the calculations
	ECS_INLINE void WriteToVector3StorageSoA(Vector3* vector_value, float3 value, size_t index) {
		float* float_interpretation = (float*)vector_value;
		float_interpretation[index] = value.x;
		float_interpretation[index + Vector3::ElementCount()] = value.y;
		float_interpretation[index + Vector3::ElementCount() * 2] = value.z;
	}

	struct ECSENGINE_API Vector4 {
		typedef Vec8f T;

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

		float4 ECS_VECTORCALL At(size_t index) const;

		// Loads a stored vector in the XXX YYY ZZZ WWW format with SIMD_WIDTH entries
		Vector4& LoadEntry(const void* data);

		// Loads a stored vector in the XXX YYY ZZZ WWW format with load count entries
		Vector4& LoadEntry(const void* data, size_t load_count);

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

		void StoreEntry(void* destination) const;
		
		void StoreEntry(void* destination, size_t load_count) const;

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

	// For a memory based vector value, it will write the values from the float3 in a SoA
	// Order such that you can use the vector as is in the calculations
	ECS_INLINE void WriteToVector4StorageSoA(Vector4* vector_value, float4 value, size_t index) {
		float* float_interpretation = (float*)vector_value;
		float_interpretation[index] = value.x;
		float_interpretation[index + Vector4::ElementCount()] = value.y;
		float_interpretation[index + Vector4::ElementCount() * 2] = value.z;
		float_interpretation[index + Vector4::ElementCount() * 3] = value.w;
	}
	
	typedef Vec8fb SIMDVectorMask;

	ECS_INLINE bool ECS_VECTORCALL IsAnySet(SIMDVectorMask mask) {
		return horizontal_or(mask);
	}

	ECS_INLINE bool ECS_VECTORCALL AreAllSet(SIMDVectorMask mask) {
		return horizontal_and(mask);
	}

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

		// Returns true if a range of consecutive bits are set
		template<int count, int offset>
		ECS_INLINE bool GetRange() const {
			int mask = 0;
			if constexpr (count == 1) {
				mask = 1;
			}
			else if constexpr (count == 2) {
				mask = 3;
			}
			else if constexpr (count == 3) {
				mask = 7;
			}
			else if constexpr (count == 4) {
				mask = 15;
			}
			else if constexpr (count == 5) {
				mask = 31;
			}
			else if constexpr (count == 6) {
				mask = 63;
			}
			else if constexpr (count == 7) {
				mask = 127;
			}
			else if constexpr (count == 8) {
				mask = 255;
			}
			else {
				static_assert(false, "Invalid count for VectorMask GetRange");
			}
			static_assert(count <= Vector3::ElementCount(), "Invalid offset for VectorMask GetRange");

			mask <<= offset;
			return (value & mask) == mask;
		}

		ECS_INLINE void Set(int index, bool new_value) {
			if (new_value) {
				value |= 1 << index;
			}
			else {
				value &= ~(1 << index);
			}
		}

		// Write the bits into bytes, for a given count
		void WriteBooleans(bool* values, int count) const;
		
		// Writes all the bits as a compressed byte
		ECS_INLINE void WriteCompressedMask(unsigned char* byte_value) const {
			*byte_value = value;
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