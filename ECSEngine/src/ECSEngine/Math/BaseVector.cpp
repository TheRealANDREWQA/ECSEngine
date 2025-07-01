#include "ecspch.h"
#include "BaseVector.h"
#include "../Utilities/PointerUtilities.h"
#include "../Utilities/StringUtilities.h"

namespace ECSEngine {

	// We could use templated functions like these to generate the implementations, but
	// It is faster to write the hardcoded version

	/*template<typename Vector>
	ECS_INLINE static bool ECS_VECTORCALL OperatorEquals(Vector a, Vector b) {
		Vec8fb result = a.x == b.x;
		for (size_t index = 1; index < Vector::Count(); index++) {
			result &= a[index] == b[index];
		}
		return horizontal_and(result);
	}

	template<typename Vector>
	ECS_INLINE static Vector ECS_VECTORCALL OperatorPlus(Vector a, Vector b) {
		Vector result;
		for (size_t index = 0; index < Vector::Count(); index++) {
			result[index] = a[index] + b[index];
		}
		return result;
	}

	template<typename Vector>
	ECS_INLINE static Vector ECS_VECTORCALL OperatorMinus(Vector a, Vector b) {
		Vector result;
		for (size_t index = 0; index < Vector::Count(); index++) {
			result[index] = a[index] - b[index];
		}
		return result;
	}*/

	bool ECS_VECTORCALL Vector3::operator==(Vector3 other) const
	{
		return horizontal_and(x == other.x && y == other.y && z == other.z);
	}

	bool ECS_VECTORCALL Vector3::operator!=(Vector3 other) const
	{
		return !(*this == other);
	}

	Vector3 ECS_VECTORCALL Vector3::operator+(const Vector3 other) const
	{
		return { x + other.x, y + other.y, z + other.z };
	}

	Vector3 ECS_VECTORCALL Vector3::operator-(const Vector3 other) const
	{
		return { x - other.x, y - other.y, z - other.z };
	}

	Vector3 ECS_VECTORCALL Vector3::operator-() const
	{
		return { -x, -y, -z };
	}

	Vector3 ECS_VECTORCALL Vector3::operator*(const Vector3 other) const
	{
		return { x * other.x, y * other.y, z * other.z };
	}

	Vector3 ECS_VECTORCALL Vector3::operator/(const Vector3 other) const
	{
		return { x / other.x, y / other.y, z / other.z };
	}

	Vector3 ECS_VECTORCALL Vector3::operator*(const Vec8f other) const
	{
		return *this * Splat(other);
	}

	Vector3 ECS_VECTORCALL Vector3::operator/(const Vec8f other) const
	{
		return *this * Splat(1.0f / other);
	}

	Vector3 ECS_VECTORCALL Vector3::operator*(float value) const
	{
		return *this * Splat(value);
	}

	Vector3 ECS_VECTORCALL Vector3::operator/(float value) const
	{
		return *this * Splat(1.0f / value);
	}

	Vector3& ECS_VECTORCALL Vector3::operator+=(const Vector3 other)
	{
		*this = *this + other;
		return *this;
	}

	Vector3& ECS_VECTORCALL Vector3::operator-=(const Vector3 other)
	{
		*this = *this - other;
		return *this;
	}

	Vector3& ECS_VECTORCALL Vector3::operator*=(const Vector3 other)
	{
		*this = *this * other;
		return *this;
	}

	Vector3& ECS_VECTORCALL Vector3::operator/=(const Vector3 other)
	{
		*this = *this / other;
		return *this;
	}

	Vector3& ECS_VECTORCALL Vector3::operator *= (const Vec8f other) {
		*this = *this * Splat(other);
		return *this;
	}

	Vector3& ECS_VECTORCALL Vector3::operator /= (const Vec8f other) {
		*this = *this * Splat(1.0f / other);
		return *this;
	}

	Vector3& Vector3::operator*=(float _value)
	{
		*this = *this * _value;
		return *this;
	}

	Vector3& Vector3::operator/=(float _value)
	{
		*this = *this / _value;
		return *this;
	}

	float3 ECS_VECTORCALL Vector3::At(size_t index) const
	{
		return {
			VectorAt(x, index),
			VectorAt(y, index),
			VectorAt(z, index)
		};
	}

	Vector3& Vector3::LoadEntry(const void* data) {
		LoadEntry(data, ElementCount());
		return *this;
	}

	Vector3& Vector3::LoadEntry(const void* data, size_t load_count) {
		Load(data, OffsetPointer(data, sizeof(float) * load_count), OffsetPointer(data, sizeof(float) * load_count * 2));
		return *this;
	}

	Vector3& Vector3::Load(const void** source_data)
	{
		return Load(source_data[0], source_data[1], source_data[2]);
	}

	Vector3& Vector3::LoadAligned(const void** source_data)
	{
		return LoadAligned(source_data[0], source_data[1], source_data[2]);
	}

	Vector3& Vector3::LoadPartial(const void** source_data, size_t count)
	{
		return LoadPartial(source_data[0], source_data[1], source_data[2], count);
	}

	Vector3& Vector3::Load(const void* source_x, const void* source_y, const void* source_z)
	{
		x.load((const float*)source_x);
		y.load((const float*)source_y);
		z.load((const float*)source_z);
		return *this;
	}

	Vector3& Vector3::LoadAligned(const void* source_x, const void* source_y, const void* source_z)
	{
		x.load_a((const float*)source_x);
		y.load_a((const float*)source_y);
		z.load_a((const float*)source_z);
		return *this;
	}

	Vector3& Vector3::LoadPartial(const void* source_x, const void* source_y, const void* source_z, size_t load_count)
	{
		x.load_partial(load_count, (const float*)source_x);
		y.load_partial(load_count, (const float*)source_y);
		z.load_partial(load_count, (const float*)source_z);
		return *this;
	}

	Vector3& Vector3::LoadAdjacent(const void* source_data, size_t offset, size_t capacity)
	{
		ECSEngine::LoadAdjacent<3>((Vec8f*)this, source_data, offset, capacity);
		return *this;
	}

	Vector3& Vector3::LoadAlignedAdjacent(const void* source_data, size_t offset, size_t capacity)
	{
		ECSEngine::LoadAlignedAdjacent<3>((Vec8f*)this, source_data, offset, capacity);
		return *this;
	}

	Vector3& Vector3::LoadPartialAdjacent(const void* source_data, size_t offset, size_t capacity, size_t count)
	{
		ECSEngine::LoadPartialAdjacent<3>((Vec8f*)this, source_data, offset, capacity, count);
		return *this;
	}

	void Vector3::Set(float3 value, size_t index)
	{
		// Broadcast the values and then blend
		Vec8f broadcast_x = value.x;
		Vec8f broadcast_y = value.y;
		Vec8f broadcast_z = value.z;

		x = BlendSingleSwitch(x, broadcast_x, index);
		y = BlendSingleSwitch(y, broadcast_y, index);
		z = BlendSingleSwitch(z, broadcast_z, index);
	}

	Vector3 Vector3::Splat(float3 value)
	{
		Vector3 result;
		result.x = value.x;
		result.y = value.y;
		result.z = value.z;
		return result;
	}

	Vector3 Vector3::Splat(Vec8f value)
	{
		return Vector3(value, value, value);
	}

	void Vector3::StoreEntry(void* destination) const {
		StoreEntry(destination, ElementCount());
	}

	void Vector3::StoreEntry(void* destination, size_t store_count) const {
		StorePartial(destination, OffsetPointer(destination, sizeof(float) * store_count), OffsetPointer(destination, sizeof(float) * store_count * 2), store_count);
	}

	void Vector3::Store(void** destination) const
	{
		Store(destination[0], destination[1], destination[2]);
	}

	void Vector3::StoreAligned(void** destination) const
	{
		StoreAligned(destination[0], destination[1], destination[2]);
	}

	void Vector3::StoreStreamed(void** destination) const
	{
		StoreStreamed(destination[0], destination[1], destination[2]);
	}

	void Vector3::StorePartial(void** destination, size_t count) const
	{
		StorePartial(destination[0], destination[1], destination[2], count);
	}

	void Vector3::Store(void* destination_x, void* destination_y, void* destination_z) const
	{
		x.store((float*)destination_x);
		y.store((float*)destination_y);
		z.store((float*)destination_z);
	}

	void Vector3::StoreAligned(void* destination_x, void* destination_y, void* destination_z) const
	{
		x.store_a((float*)destination_x);
		y.store_a((float*)destination_y);
		z.store_a((float*)destination_z);
	}

	void Vector3::StoreStreamed(void* destination_x, void* destination_y, void* destination_z) const
	{
		x.store_nt((float*)destination_x);
		y.store_nt((float*)destination_y);
		z.store_nt((float*)destination_z);
	}

	void Vector3::StorePartial(void* destination_x, void* destination_y, void* destination_z, size_t store_count) const
	{
		x.store_partial(store_count, (float*)destination_x);
		y.store_partial(store_count, (float*)destination_y);
		z.store_partial(store_count, (float*)destination_z);
	}

	void Vector3::StoreAdjacent(void* destination, size_t offset, size_t capacity) const
	{
		ECSEngine::StoreAdjacent<3>((const Vec8f*)this, destination, offset, capacity);
	}

	void Vector3::StoreAlignedAdjacent(void* destination, size_t offset, size_t capacity) const
	{
		ECSEngine::StoreAlignedAdjacent<3>((const Vec8f*)this, destination, offset, capacity);
	}

	void Vector3::StoreStreamedAdjacent(void* destination, size_t offset, size_t capacity) const
	{
		ECSEngine::StoreStreamedAdjacent<3>((const Vec8f*)this, destination, offset, capacity);
	}

	void Vector3::StorePartialAdjacent(void* destination, size_t offset, size_t capacity, size_t count) const
	{
		ECSEngine::StorePartialAdjacent<3>((const Vec8f*)this, destination, offset, capacity, count);
	}

	Vector3& Vector3::Gather(const void* source_data)
	{
		x = ECSEngine::GatherStride<3, 0>(source_data);
		y = ECSEngine::GatherStride<3, 1>(source_data);
		z = ECSEngine::GatherStride<3, 2>(source_data);
		return *this;
	}

	Vector3& Vector3::GatherPartial(const void* source_data, size_t load_count)
	{
		x = ECSEngine::GatherStride<3, 0>(source_data, load_count);
		y = ECSEngine::GatherStride<3, 1>(source_data, load_count);
		z = ECSEngine::GatherStride<3, 2>(source_data, load_count);
		return *this;
	}

	Vector3& Vector3::GatherPartialMasked(const void* source_data, size_t load_count)
	{
		x = ECSEngine::GatherStrideMasked<3, 0>(source_data, load_count);
		y = ECSEngine::GatherStrideMasked<3, 1>(source_data, load_count);
		z = ECSEngine::GatherStrideMasked<3, 2>(source_data, load_count);
		return *this;
	}

	void Vector3::Scatter(void* destination) const
	{
		ECSEngine::ScatterStride<3, 0>(x, destination);
		ECSEngine::ScatterStride<3, 1>(y, destination);
		ECSEngine::ScatterStride<3, 2>(z, destination);
	}

	void Vector3::ScatterPartial(void* destination, size_t store_count) const
	{
		ECSEngine::ScatterStride<3, 0>(x, destination, store_count);
		ECSEngine::ScatterStride<3, 1>(y, destination, store_count);
		ECSEngine::ScatterStride<3, 2>(z, destination, store_count);
	}

	void Vector3::ToString(CapacityStream<char>& characters, size_t precision) const
	{
		alignas(Vec8f) float x_values[ElementCount()];
		alignas(Vec8f) float y_values[ElementCount()];
		alignas(Vec8f) float z_values[ElementCount()];

		x.store_a(x_values);
		y.store_a(y_values);
		z.store_a(z_values);

		// Write the values as a each logical float3 element
		for (size_t index = 0; index < ElementCount(); index++) {
			// This is what we are doing, but to make it faster, avoid using format FormatString(characters, "{#} {#} {#}\n");
			ConvertFloatToChars(characters, x_values[index], precision);
			characters.AddAssert(' ');
			ConvertFloatToChars(characters, y_values[index], precision);
			characters.AddAssert(' ');
			ConvertFloatToChars(characters, z_values[index], precision);
			characters.AddAssert('\n');
		}
	}

	bool ECS_VECTORCALL Vector4::operator==(Vector4 other) const
	{
		return horizontal_and(x == other.x && y == other.y && z == other.z && w == other.w);
	}

	bool ECS_VECTORCALL Vector4::operator!=(Vector4 other) const
	{
		return !(*this == other);
	}

	Vector4 ECS_VECTORCALL Vector4::operator+(const Vector4 other) const
	{
		return { x + other.x, y + other.y, z + other.z, w + other.w };
	}

	Vector4 ECS_VECTORCALL Vector4::operator-(const Vector4 other) const
	{
		return { x - other.x , y - other.y, z - other.z, w - other.w };
	}

	Vector4 ECS_VECTORCALL Vector4::operator-() const
	{
		return { -x, -y, -z, -w };
	}

	Vector4 ECS_VECTORCALL Vector4::operator*(const Vector4 other) const
	{
		return { x * other.x, y * other.y, z * other.z, w * other.w };
	}

	Vector4 ECS_VECTORCALL Vector4::operator/(const Vector4 other) const
	{
		return { x / other.x, y / other.y, z / other.z, w / other.w };
	}

	Vector4 ECS_VECTORCALL Vector4::operator*(const Vec8f other) const
	{
		return *this * Splat(other);
	}

	Vector4 ECS_VECTORCALL Vector4::operator/(const Vec8f other) const
	{
		return *this * Splat(1.0f / other);
	}

	Vector4 ECS_VECTORCALL Vector4::operator*(float value) const
	{
		return *this * Splat(value);
	}

	Vector4 ECS_VECTORCALL Vector4::operator/(float value) const
	{
		return *this * Splat(1.0f / value);
	}

	Vector4& ECS_VECTORCALL Vector4::operator+=(const Vector4 other)
	{
		*this = *this + other;
		return *this;
	}

	Vector4& ECS_VECTORCALL Vector4::operator-=(const Vector4 other)
	{
		*this = *this - other;
		return *this;
	}

	Vector4& ECS_VECTORCALL Vector4::operator*=(const Vector4 other)
	{
		*this = *this * other;
		return *this;
	}

	Vector4& ECS_VECTORCALL Vector4::operator/=(const Vector4 other)
	{
		*this = *this / other;
		return *this;
	}

	Vector4& ECS_VECTORCALL Vector4::operator*=(const Vec8f other)
	{
		*this = *this * Splat(other);
		return *this;
	}

	Vector4& ECS_VECTORCALL Vector4::operator/=(const Vec8f other)
	{
		*this = *this * Splat(1.0f / other);
		return *this;
	}

	Vector4& Vector4::operator*=(float _value)
	{
		*this = *this * _value;
		return *this;
	}

	Vector4& Vector4::operator/=(float _value)
	{
		*this = *this / _value;
		return *this;
	}

	float4 ECS_VECTORCALL Vector4::At(size_t index) const
	{
		return {
			VectorAt(x, index),
			VectorAt(y, index),
			VectorAt(z, index),
			VectorAt(w, index)
		};
	}

	Vector4& Vector4::LoadEntry(const void* data) {
		LoadEntry(data, ElementCount());
		return *this;
	}

	Vector4& Vector4::LoadEntry(const void* data, size_t load_count) {
		Load(data, OffsetPointer(data, sizeof(float) * ElementCount()), OffsetPointer(data, sizeof(float) * ElementCount() * 2), 
			OffsetPointer(data, sizeof(float) * ElementCount() * 3));
		return *this;
	}

	Vector4& Vector4::Load(const void** source_data)
	{
		Load(source_data[0], source_data[1], source_data[2], source_data[3]);
		return *this;
	}

	Vector4& Vector4::LoadAligned(const void** source_data)
	{
		LoadAligned(source_data[0], source_data[1], source_data[2], source_data[3]);
		return *this;
	}

	Vector4& Vector4::LoadPartial(const void** source_data, size_t count)
	{
		LoadPartial(source_data[0], source_data[1], source_data[2], source_data[3], count);
		return *this;
	}

	Vector4& Vector4::Load(const void* source_x, const void* source_y, const void* source_z, const void* source_w)
	{
		x.load((const float*)source_x);
		y.load((const float*)source_y);
		z.load((const float*)source_z);
		w.load((const float*)source_w);
		return *this;
	}

	Vector4& Vector4::LoadAligned(const void* source_x, const void* source_y, const void* source_z, const void* source_w)
	{
		x.load_a((const float*)source_x);
		y.load_a((const float*)source_y);
		z.load_a((const float*)source_z);
		w.load_a((const float*)source_w);
		return *this;
	}

	Vector4& Vector4::LoadPartial(const void* source_x, const void* source_y, const void* source_z, const void* source_w, size_t load_count)
	{
		x.load_partial(load_count, (const float*)source_x);
		y.load_partial(load_count, (const float*)source_y);
		z.load_partial(load_count, (const float*)source_z);
		w.load_partial(load_count, (const float*)source_w);
		return *this;
	}

	Vector4& Vector4::LoadAdjacent(const void* source_data, size_t offset, size_t capacity)
	{
		ECSEngine::LoadAdjacent<4>((Vec8f*)this, source_data, offset, capacity);
		return *this;
	}

	Vector4& Vector4::LoadAlignedAdjacent(const void* source_data, size_t offset, size_t capacity)
	{
		ECSEngine::LoadAlignedAdjacent<4>((Vec8f*)this, source_data, offset, capacity);
		return *this;
	}

	Vector4& Vector4::LoadPartialAdjacent(const void* source_data, size_t offset, size_t capacity, size_t count)
	{
		ECSEngine::LoadPartialAdjacent<4>((Vec8f*)this, source_data, offset, capacity, count);
		return *this;
	}

	void Vector4::Set(float4 value, size_t index)
	{
		// Broadcast the values and then blend
		Vec8f broadcast_x = value.x;
		Vec8f broadcast_y = value.y;
		Vec8f broadcast_z = value.z;
		Vec8f broadcast_w = value.w;

		x = BlendSingleSwitch(x, broadcast_x, index);
		y = BlendSingleSwitch(y, broadcast_y, index);
		z = BlendSingleSwitch(z, broadcast_z, index);
		w = BlendSingleSwitch(w, broadcast_w, index);
	}

	Vector4& Vector4::Splat(float4 value)
	{
		x = value.x;
		y = value.y;
		z = value.z;
		w = value.w;
		return *this;
	}

	Vector4 Vector4::Splat(Vec8f value)
	{
		return Vector4(value, value, value, value);
	}

	void Vector4::StoreEntry(void* destination) const {
		StoreEntry(destination, ElementCount());
	}

	void Vector4::StoreEntry(void* destination, size_t load_count) const {
		Store(destination, OffsetPointer(destination, sizeof(float) * load_count), OffsetPointer(destination, sizeof(float) * load_count * 2),
			OffsetPointer(destination, sizeof(float) * load_count * 3));
	}

	void Vector4::Store(void** destination) const
	{
		Store(destination[0], destination[1], destination[2], destination[3]);
	}

	void Vector4::StoreAligned(void** destination) const
	{
		StoreAligned(destination[0], destination[1], destination[2], destination[3]);
	}

	void Vector4::StoreStreamed(void** destination) const
	{
		StoreStreamed(destination[0], destination[1], destination[2], destination[3]);
	}

	void Vector4::StorePartial(void** destination, size_t count) const
	{
		StorePartial(destination[0], destination[1], destination[2], destination[3], count);
	}

	void Vector4::Store(void* destination_x, void* destination_y, void* destination_z, void* destination_w) const
	{
		x.store((float*)destination_x);
		y.store((float*)destination_y);
		z.store((float*)destination_z);
		w.store((float*)destination_w);
	}

	void Vector4::StoreAligned(void* destination_x, void* destination_y, void* destination_z, void* destination_w) const
	{
		x.store_a((float*)destination_x);
		y.store_a((float*)destination_y);
		z.store_a((float*)destination_z);
		w.store_a((float*)destination_w);
	}

	void Vector4::StoreStreamed(void* destination_x, void* destination_y, void* destination_z, void* destination_w) const
	{
		x.store_nt((float*)destination_x);
		y.store_nt((float*)destination_y);
		z.store_nt((float*)destination_z);
		w.store_nt((float*)destination_w);
	}

	void Vector4::StorePartial(void* destination_x, void* destination_y, void* destination_z, void* destination_w, size_t store_count) const
	{
		x.store_partial(store_count, (float*)destination_x);
		y.store_partial(store_count, (float*)destination_y);
		z.store_partial(store_count, (float*)destination_z);
		w.store_partial(store_count, (float*)destination_w);
	}

	void Vector4::StoreAdjacent(void* destination, size_t offset, size_t capacity) const
	{
		ECSEngine::StoreAdjacent<4>((const Vec8f*)this, destination, offset, capacity);
	}

	void Vector4::StoreAlignedAdjacent(void* destination, size_t offset, size_t capacity) const
	{
		ECSEngine::StoreAlignedAdjacent<4>((const Vec8f*)this, destination, offset, capacity);
	}

	void Vector4::StoreStreamedAdjacent(void* destination, size_t offset, size_t capacity) const
	{
		ECSEngine::StoreStreamedAdjacent<4>((const Vec8f*)this, destination, offset, capacity);
	}

	void Vector4::StorePartialAdjacent(void* destination, size_t offset, size_t capacity, size_t count) const
	{
		ECSEngine::StorePartialAdjacent<4>((const Vec8f*)this, destination, offset, capacity, count);
	}

	Vector4& Vector4::Gather(const void* source_data)
	{
		x = ECSEngine::GatherStride<4, 0>(source_data);
		y = ECSEngine::GatherStride<4, 1>(source_data);
		z = ECSEngine::GatherStride<4, 2>(source_data);
		w = ECSEngine::GatherStride<4, 3>(source_data);
		return *this;
	}

	Vector4& Vector4::GatherPartial(const void* source_data, size_t load_count)
	{
		x = ECSEngine::GatherStride<4, 0>(source_data, load_count);
		y = ECSEngine::GatherStride<4, 1>(source_data, load_count);
		z = ECSEngine::GatherStride<4, 2>(source_data, load_count);
		w = ECSEngine::GatherStride<4, 3>(source_data, load_count);
		return *this;
	}

	Vector4& Vector4::GatherPartialMasked(const void* source_data, size_t load_count)
	{
		x = ECSEngine::GatherStrideMasked<4, 0>(source_data, load_count);
		y = ECSEngine::GatherStrideMasked<4, 1>(source_data, load_count);
		z = ECSEngine::GatherStrideMasked<4, 2>(source_data, load_count);
		w = ECSEngine::GatherStrideMasked<4, 3>(source_data, load_count);
		return *this;
	}

	void Vector4::Scatter(void* destination) const
	{
		ECSEngine::ScatterStride<4, 0>(x, destination);
		ECSEngine::ScatterStride<4, 1>(y, destination);
		ECSEngine::ScatterStride<4, 2>(z, destination);
		ECSEngine::ScatterStride<4, 3>(w, destination);
	}

	void Vector4::ScatterPartial(void* destination, size_t store_count) const
	{
		ECSEngine::ScatterStride<4, 0>(x, destination, store_count);
		ECSEngine::ScatterStride<4, 1>(y, destination, store_count);
		ECSEngine::ScatterStride<4, 2>(z, destination, store_count);
		ECSEngine::ScatterStride<4, 3>(w, destination, store_count);
	}

	void Vector4::ToString(CapacityStream<char>& characters, size_t precision) const
	{
		alignas(Vec8f) float x_values[ElementCount()];
		alignas(Vec8f) float y_values[ElementCount()];
		alignas(Vec8f) float z_values[ElementCount()];
		alignas(Vec8f) float w_values[ElementCount()];
		
		x.store_a(x_values);
		y.store_a(y_values);
		z.store_a(z_values);
		w.store_a(w_values);

		// Write the values as a each logical float4 element
		for (size_t index = 0; index < ElementCount(); index++) {
			// This is what we are doing, but to make it faster, avoid using format FormatString(characters, "{#} {#} {#} {#}\n");
			ConvertFloatToChars(characters, x_values[index], precision);
			characters.AddAssert(' ');
			ConvertFloatToChars(characters, y_values[index], precision);
			characters.AddAssert(' ');
			ConvertFloatToChars(characters, z_values[index], precision);
			characters.AddAssert(' ');
			ConvertFloatToChars(characters, w_values[index], precision);
			characters.AddAssert('\n');
		}
	}

	VectorMask::VectorMask(SIMDVectorMask vector_mask)
	{
		value = to_bits(vector_mask);
	}

	bool VectorMask::AreAllSet(int count) const
	{
		if (count == Vec8f::size()) {
			return value == UCHAR_MAX;
		}
		for (int index = 0; index < count; index++) {
			if (!Get(index)) {
				return false;
			}
		}
		return true;
	}

	bool VectorMask::AreAllCleared(int count) const
	{
		for (int index = 0; index < count; index++) {
			if (Get(index)) {
				return false;
			}
		}
		return true;
	}

	bool VectorMask::IsAnySet(int count) const
	{
		if (count == Vec8f::size()) {
			return value != 0;
		}
		for (int index = 0; index < count; index++) {
			if (Get(index)) {
				return true;
			}
		}
		return false;
	}

	void VectorMask::WriteBooleans(bool* values, int count) const {
		int bit = 1;
		for (int index = 0; index < count; index++) {
			values[index] = (value & bit) != 0;
			bit <<= 1;
		}
	}

}