#pragma once
#include "../Core.h"
#include "ecspch.h"
#include "../Utilities/BasicTypes.h"
#include "VCLExtensions.h"
#include "Vector.h"

constexpr float ECS_QUATERNION_EPSILON = 0.00005f;

namespace ECSEngine {

	namespace VectorGlobals {
		inline Vector4 QUATERNION_EPSILON_4(ECS_QUATERNION_EPSILON);
		inline Vector8 QUATERNION_EPSILON_8(ECS_QUATERNION_EPSILON);
	}

	struct Quaternion {
		ECS_INLINE Quaternion() {}
		ECS_INLINE Quaternion(float x, float y, float z, float w) : value(x, y, z, w) {}
		ECS_INLINE Quaternion(const float* values) {
			value.load(values);
		}
		ECS_INLINE ECS_VECTORCALL Quaternion(Vec4f _value) : value(_value) {}
		ECS_INLINE ECS_VECTORCALL Quaternion(Vector4 _value) : value(_value.value) {}

		ECS_INLINE Quaternion(const Quaternion& other) = default;
		ECS_INLINE Quaternion& ECS_VECTORCALL operator = (const Quaternion& other) = default;

		ECS_INLINE bool ECS_VECTORCALL operator == (Quaternion other) const {
			return horizontal_and(abs(value - other.value) < VectorGlobals::QUATERNION_EPSILON_4);
		}
		
		ECS_INLINE bool ECS_VECTORCALL operator != (Quaternion other) const {
			return !(*this == other);
		}

		ECS_INLINE float ECS_VECTORCALL operator [](unsigned int index) const {
			return value.extract(index);
		}

		ECS_INLINE Quaternion ECS_VECTORCALL operator + (Quaternion other) const {
			return value + other.value;
		}

		ECS_INLINE Quaternion ECS_VECTORCALL operator - (Quaternion other) const {
			return value - other.value;
		}

		ECS_INLINE Quaternion ECS_VECTORCALL operator * (float scalar) const {
			return value * Vec4f(scalar);
		}

		ECS_INLINE Quaternion ECS_VECTORCALL operator -() const {
			return -value;
		}

		ECS_INLINE Quaternion& ECS_VECTORCALL operator += (Quaternion other) {
			value += other.value;
			return *this;
		}

		ECS_INLINE Quaternion& ECS_VECTORCALL operator -= (Quaternion other) {
			value -= other.value;
			return *this;
		}
		
		ECS_INLINE Quaternion& ECS_VECTORCALL operator *= (float scalar) {
			value *= Vec4f(scalar);
			return *this;
		}

		Vec4f value;
	};

	struct PackedQuaternion {
		ECS_INLINE PackedQuaternion() {}
		ECS_INLINE PackedQuaternion(float x1, float y1, float z1, float w1, float x2, float y2, float z2, float w2) 
		: value(x1, y1, z1, w1, x2, y2, z2, w2) {}
		ECS_INLINE PackedQuaternion(const float* values) {
			value.load(values);
		}
		ECS_INLINE ECS_VECTORCALL PackedQuaternion(Vec8f _value) : value(_value) {}
		ECS_INLINE ECS_VECTORCALL PackedQuaternion(Vector8 _value) : value(_value.value) {}
		ECS_INLINE ECS_VECTORCALL PackedQuaternion(Quaternion first, Quaternion second) : value(first.value, second.value) {}

		ECS_INLINE PackedQuaternion(const PackedQuaternion& other) = default;
		ECS_INLINE PackedQuaternion& ECS_VECTORCALL operator = (const PackedQuaternion& other) = default;

		ECS_INLINE bool2 ECS_VECTORCALL operator == (PackedQuaternion other) const {
			auto bool_vector = abs(value - other.value) < VectorGlobals::QUATERNION_EPSILON_8;
			return { horizontal_and(bool_vector.get_low()), horizontal_and(bool_vector.get_high()) };
		}

		ECS_INLINE bool2 ECS_VECTORCALL operator != (PackedQuaternion other) const {
			bool2 is_equal = *this == other;
			return { !is_equal.x, !is_equal.y };
		}

		ECS_INLINE float ECS_VECTORCALL operator [](unsigned int index) const {
			return value.extract(index);
		}

		ECS_INLINE PackedQuaternion ECS_VECTORCALL operator + (PackedQuaternion other) const {
			return value + other.value;
		}

		ECS_INLINE PackedQuaternion ECS_VECTORCALL operator - (PackedQuaternion other) const {
			return value - other.value;
		}

		ECS_INLINE PackedQuaternion ECS_VECTORCALL operator * (float scalar) const {
			return value * Vec8f(scalar);
		}

		ECS_INLINE PackedQuaternion ECS_VECTORCALL operator -() const {
			return -value;
		}

		ECS_INLINE PackedQuaternion& ECS_VECTORCALL operator += (PackedQuaternion other) {
			value += other.value;
			return *this;
		}

		ECS_INLINE PackedQuaternion& ECS_VECTORCALL operator -= (PackedQuaternion other) {
			value -= other.value;
			return *this;
		}

		ECS_INLINE PackedQuaternion& ECS_VECTORCALL operator *= (float scalar) {
			value *= Vec8f(scalar);
			return *this;
		}

		ECS_INLINE Quaternion ECS_VECTORCALL Low() const {
			return Quaternion(value.get_low());
		}

		ECS_INLINE Quaternion ECS_VECTORCALL High() const {
			return Quaternion(value.get_high());
		}

		Vec8f value;
	};

#pragma region Identity

	// ---------------------------------------------------------------------------------------------------------------

	ECS_INLINE Quaternion ECS_VECTORCALL QuaternionIdentity() {
		return VectorGlobals::QUATERNION_IDENTITY_4;
	}

	ECS_INLINE PackedQuaternion ECS_VECTORCALL PackedQuaternionIdentity() {
		return VectorGlobals::QUATERNION_IDENTITY_8;
	}

	// ---------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region From Rotation

	// ---------------------------------------------------------------------------------------------------------------

	ECS_INLINE Quaternion ECS_VECTORCALL QuaternionFromRotation(float3 rotation) {
		Quaternion quaternion;

		Vector4 angles(rotation);
		angles = DegToRad(angles);

		Vector4 half(0.5f);
		angles *= half;

		Vec4f cos;
		Vector4 sin = sincos(&cos, angles);

		// Quaternion:
		// x = cos(x) * cos(y) * cos(z) + sin(x) * sin(y) * sin(z)
		// y = sin(x) * cos(y) * cos(z) - cos(x) * sin(y) * sin(z)
		// z = cos(x) * sin(y) * cos(z) + sin(x) * cos(y) * cos(z)
		// w = cos(x) * cos(y) * sin(z) - sin(x) * sin(y) * cos(z)

		Vector4 first_permutation_left = blend4<0, 4, 0, 0>(cos, sin);
		Vector4 second_permutation_left = blend4<1, 1, 5, 1>(cos, sin);
		Vector4 third_permutation_left = blend4<2, 2, 2, 6>(cos, sin);

		Vector4 first_permutation_right = permute4<1, 0, 1, 1>(first_permutation_left);
		Vector4 second_permutation_right = permute4<2, 2, 0, 2>(second_permutation_left);
		Vector4 third_permutation_right = permute4<3, 3, 0, 0>(third_permutation_left);

		Vector4 left = first_permutation_left * second_permutation_left * third_permutation_left;
		Vector4 right = first_permutation_right * second_permutation_right * third_permutation_right;
		right = change_sign<0, 1, 0, 1>(right);

		quaternion.value = (left + right).value;
		return quaternion;
	}

	ECS_INLINE PackedQuaternion ECS_VECTORCALL QuaternionFromRotation(float3 rotation1, float3 rotation2) {
		PackedQuaternion quaternion;

		Vector8 angles(rotation1, rotation2);
		angles = DegToRad(angles);

		Vector8 half(0.5f);
		angles *= half;

		Vec8f cos;
		Vector8 sin = sincos(&cos, angles);

		// Quaternion:
		// x = cos(x) * cos(y) * cos(z) + sin(x) * sin(y) * sin(z)
		// y = sin(x) * cos(y) * cos(z) - cos(x) * sin(y) * sin(z)
		// z = cos(x) * sin(y) * cos(z) + sin(x) * cos(y) * cos(z)
		// w = cos(x) * cos(y) * sin(z) - sin(x) * sin(y) * cos(z)

		Vector8 first_permutation_left = blend8<0, 8, 0, 0, 4, 12, 4, 4>(cos, sin);
		Vector8 second_permutation_left = blend8<1, 1, 9, 1, 5, 5, 13, 5>(cos, sin);
		Vector8 third_permutation_left = blend8<2, 2, 2, 10, 6, 6, 6, 14>(cos, sin);

		Vector8 first_permutation_right = permute8<1, 0, 1, 1, 5, 4, 5, 5>(first_permutation_left);
		Vector8 second_permutation_right = permute8<2, 2, 0, 2, 6, 6, 4, 6>(second_permutation_left);
		Vector8 third_permutation_right = permute8<3, 3, 0, 0, 7, 7, 4, 4>(third_permutation_left);

		Vector8 left = first_permutation_left * second_permutation_left * third_permutation_left;
		Vector8 right = first_permutation_right * second_permutation_right * third_permutation_right;
		right = change_sign<0, 1, 0, 1, 0, 1, 0, 1>(right);

		quaternion.value = (left + right).value;
		return quaternion;
	}

	// ---------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Angle From Axis

	// ---------------------------------------------------------------------------------------------------------------

	// angle must be expressed as radians
	ECS_INLINE Quaternion ECS_VECTORCALL QuaternionAngleFromAxis(float angle_radians, float3 axis) {
		Vector4 vector_axis(axis);
		vector_axis = Normalize3(vector_axis);

		angle_radians *= 0.5f;
		Vec4f cos;
		Vector4 sin = sincos(&cos, angle_radians);
		vector_axis *= sin;
		vector_axis = blend4<0, 1, 2, 7>(vector_axis, cos);
		return Quaternion(vector_axis.value);
	}

	// angle must be expressed as radians
	ECS_INLINE PackedQuaternion ECS_VECTORCALL QuaternionAngleFromAxis(float angle_radians1, float3 axis1, float angle_radians2, float3 axis2) {
		Vector8 vector_axis(axis1, axis2);
		vector_axis = Normalize3(vector_axis);

		angle_radians1 *= 0.5f;
		angle_radians2 *= 0.5f;

		Vec8f cos;
		Vector8 sin = sincos(&cos, Vec8f(angle_radians1, angle_radians1, angle_radians1, angle_radians1, angle_radians2, angle_radians2, angle_radians2, angle_radians2));
		vector_axis *= sin;
		vector_axis = blend8<0, 1, 2, 11, 5, 6, 7, 15>(vector_axis, cos);
		return PackedQuaternion(vector_axis.value);
	}

	// ---------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region From Vectors

	// ---------------------------------------------------------------------------------------------------------------

	namespace SIMDHelpers {
		// Expects the from vector and the to vector to be normalized
		ECS_INLINE Quaternion ECS_VECTORCALL QuaternionFromVectorsHelper(Vector4 from_vector, Vector4 to_vector) {
			// If they are equal return an identity quaternion
			if (from_vector == to_vector) {
				return QuaternionIdentity();
			}
			// else check if they are opposite 
			// If they are, return the most orthogonal axis of the from vector
			// that can be used to create a pure quaternion
			else if (from_vector == Vector4(-to_vector)) {
				Vector4 no_abs_from_vector = from_vector;
				from_vector = abs(from_vector);
				alignas(16) float4 values;
				from_vector.StoreAligned(&values);

				Vector4 one = VectorGlobals::ONE_4;
				Vector4 zero = ZeroVector4();
				Vector4 orthogonal = blend4<4, 0, 0, 0>(zero, one);
				if (values.y < values.x) {
					orthogonal = blend4<0, 4, 0, 0>(zero, one);
				}
				else if (values.z < values.y && values.z < values.x) {
					orthogonal = blend4<0, 0, 4, 0>(zero, one);
				}
				Vector4 axis = Normalize3(Cross(no_abs_from_vector, orthogonal));
				return Quaternion(blend4<0, 1, 2, 4>(axis, zero));
			}

			// else create the quaternion from the half vector
			Vector4 half = Normalize3(from_vector + to_vector);
			Vector4 axis = Cross(from_vector, half);
			Vector4 dot = Dot3(from_vector, half);
			return Quaternion(blend4<0, 1, 2, 4>(axis, dot));
		}

		// Expects the from vector and the to vector to be normalized
		ECS_INLINE PackedQuaternion ECS_VECTORCALL QuaternionFromVectorsHelper(Vector8 from_vector, Vector8 to_vector) {
			// If they are equal return an identity quaternion
			if (from_vector == to_vector) {
				return PackedQuaternionIdentity();
			}
			// else check if they are opposite 
			// If they are, return the most orthogonal axis of the from_vector vector
			// that can be used to create a pure quaternion
			else if (from_vector == Vector8(-to_vector)) {
				Vector8 no_abs_from_vector_vector = from_vector;
				from_vector = abs(from_vector);
				alignas(32) float4 values[2];
				from_vector.StoreAligned(&values);

				Vector8 one = VectorGlobals::ONE_8;
				Vector8 zero = ZeroVector8();
				Vector8 orthogonal = blend8<8, 0, 0, 0, 8, 0, 0, 0>(zero, one);

				// first assignment - no need to do a second blend to keep the original values
				if (values[0].y < values[0].x) {
					orthogonal = blend8<0, 8, 0, 0, 8, 0, 0, 0>(zero, one);
				}
				else if (values[0].z < values[0].y && values[0].z < values[0].x) {
					orthogonal = blend8<0, 0, 8, 0, 8, 0, 0, 0>(zero, one);
				}

				// second assignment - original values must be kept so a second blend must be done
				if (values[1].y < values[1].x) {
					// only the first 4 values of the zero one blend must be correct
					orthogonal = blend8<0, 1, 2, 3, 8, 9, 10, 11>(blend8<0, 8, 0, 0, 0, 0, 0, 0>(zero, one), orthogonal);
				}
				else if (values[1].z < values[1].y && values[1].z < values[1].x) {
					orthogonal = blend8<0, 1, 2, 3, 8, 9, 10, 11>(blend8<0, 0, 8, 0, 0, 0, 0, 0>(zero, one), orthogonal);
				}

				Vector8 axis = Normalize(Cross(no_abs_from_vector_vector, orthogonal));
				return PackedQuaternion(blend8<0, 1, 2, 8, 4, 5, 6, 8>(axis, zero));
			}

			// can't be sure which branch each half must take
			// so split them in 2 Vector4's and do each assignment
			// and then merge
			Vector4 from_vector_vector_low = from_vector.value.get_low();
			Vector4 from_vector_vector_high = from_vector.value.get_high();

			Vector4 to_vector_low = to_vector.value.get_low();
			Vector4 to_vector_high = to_vector.value.get_high();

			Quaternion quaternion_low = SIMDHelpers::QuaternionFromVectorsHelper(from_vector_vector_low, to_vector_low);
			Quaternion quaternion_high = SIMDHelpers::QuaternionFromVectorsHelper(from_vector_vector_high, to_vector_high);

			return PackedQuaternion(quaternion_low, quaternion_high);
		}
	}

	// If the vectors are already normalized you can call directly
	// SIMDHelpers::QuaternionFromVectorsHelper
	ECS_INLINE Quaternion ECS_VECTORCALL QuaternionFromVectors(Vector4 from, Vector4 to) {
		// Normalize the vectors
		from = Normalize3(from);
		to = Normalize3(to);

		return SIMDHelpers::QuaternionFromVectorsHelper(from, to);
	}

	// If the vectors are already normalized you can call directly
	// SIMDHelpers::QuaternionFromVectorsHelper
	ECS_INLINE Quaternion ECS_VECTORCALL QuaternionFromVectors(float3 from, float3 to) {
		Vector4 from_vector(from);
		Vector4 to_vector(to);
		
		return QuaternionFromVectors(from_vector, to_vector);
	}

	// If the vectors are already normalized you can call directly
	// SIMDHelpers::QuaternionFromVectorsHelper
	ECS_INLINE PackedQuaternion ECS_VECTORCALL QuaternionFromVectors(Vector8 from, Vector8 to) {
		// Normalize the vectors
		from = Normalize3(from);
		to = Normalize3(to);

		return SIMDHelpers::QuaternionFromVectorsHelper(from, to);
	}

	// If the vectors are already normalized you can call directly
	// SIMDHelpers::QuaternionFromVectorsHelper
	ECS_INLINE PackedQuaternion ECS_VECTORCALL QuaternionFromVectors(Vector4 from1, Vector4 to1, Vector4 from2, Vector4 to2) {
		Vector8 from(from1, from2);
		Vector8 to(to1, to2);

		return QuaternionFromVectors(from, to);
	}

	ECS_INLINE PackedQuaternion ECS_VECTORCALL QuaternionFromVectors(float3 from1, float3 to1, float3 from2, float3 to2) {
		Vector8 from_vector(from1, from2);
		Vector8 to_vector(to1, to2);

		return QuaternionFromVectors(from_vector, to_vector);
	}

	// ---------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Get Axis

	// ---------------------------------------------------------------------------------------------------------------

	// Will do a partial store on the axis, this is useful if packed buffers must be filled in
	ECS_INLINE void ECS_VECTORCALL QuaternionAxis(Quaternion quaternion, float3& axis) {
		Vector4 vector_axis(axis);
		vector_axis = Normalize3(vector_axis);
		vector_axis.StorePartialConstant<3>(&axis);
	}

	// Can do a normal store on float4 and the use only the 3 components; useful for stack variable
	ECS_INLINE void ECS_VECTORCALL QuaternionAxis(Quaternion quaternion, float4& axis) {
		Vector4 vector_axis(axis);
		vector_axis = Normalize3(vector_axis);
		vector_axis.Store(&axis);
	}

	// Will do a partial store on each axis by using the high and the low part
	ECS_INLINE void ECS_VECTORCALL QuaternionAxis(PackedQuaternion quaternion, float3& first_axis, float3& second_axis) {
		Quaternion low = quaternion.Low();
		Quaternion high = quaternion.High();

		QuaternionAxis(low, first_axis);
		QuaternionAxis(high, second_axis);
	}

	// Will do a normal store on each axis by using the high and the low part; useful for stack variables
	ECS_INLINE void ECS_VECTORCALL QuaternionAxis(PackedQuaternion quaternion, float4& first_axis, float4& second_axis) {
		Quaternion low = quaternion.Low();
		Quaternion high = quaternion.High();

		QuaternionAxis(low, first_axis);
		QuaternionAxis(high, second_axis);
	}

	// ---------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Get Angle

	// ---------------------------------------------------------------------------------------------------------------

	ECS_INLINE float ECS_VECTORCALL QuaternionAngle(Quaternion quaternion) {
		return 2.0f * quaternion[3];
	}

	ECS_INLINE float2 ECS_VECTORCALL QuaternionAngle(PackedQuaternion quaternion) {
		alignas(32) float values[8];
		quaternion.value.store_a(values);
		return { 2.0f * values[0], 2.0f * values[7] };
	}

	// ---------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Same Orientation

	// ---------------------------------------------------------------------------------------------------------------

	ECS_INLINE bool ECS_VECTORCALL QuaternionSameOrientation(Quaternion a, Quaternion b) {
		return horizontal_and(abs(a.value - b.value) < VectorGlobals::QUATERNION_EPSILON_4 || horizontal_and(abs(a.value + b.value) < VectorGlobals::QUATERNION_EPSILON_4));
	}

	ECS_INLINE bool2 ECS_VECTORCALL QuaternionSameOrientation(PackedQuaternion a, PackedQuaternion b) {
		auto negative_bool = abs(a.value - b.value) < VectorGlobals::QUATERNION_EPSILON_8;
		auto positive_bool = abs(a.value + b.value) < VectorGlobals::QUATERNION_EPSILON_8;
		return { horizontal_and(negative_bool.get_low()) || horizontal_and(positive_bool.get_low()), horizontal_and(negative_bool.get_high()) || horizontal_and(positive_bool.get_high()) };
	}

	// ---------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Dot

	// ---------------------------------------------------------------------------------------------------------------

	// The value is replicated accross all register values
	ECS_INLINE Quaternion ECS_VECTORCALL QuaternionDot(Quaternion a, Quaternion b) {
		return Dot(a.value, b.value);
	}

	// The value is replicated accross all 4 register values inside the low and high parts
	ECS_INLINE PackedQuaternion ECS_VECTORCALL QuaternionDot(PackedQuaternion a, PackedQuaternion b) {
		return Dot(a.value, b.value);
	}

	// ---------------------------------------------------------------------------------------------------------------

#pragma region

#pragma region Squared Length

	// ---------------------------------------------------------------------------------------------------------------

	ECS_INLINE Quaternion ECS_VECTORCALL QuaternionSquaredLength(Quaternion a) {
		return QuaternionDot(a, a);
	}

	ECS_INLINE PackedQuaternion ECS_VECTORCALL QuaternionSquaredLength(PackedQuaternion a) {
		return QuaternionDot(a, a);
	}

	// ---------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Length

	// ---------------------------------------------------------------------------------------------------------------

	ECS_INLINE Quaternion ECS_VECTORCALL QuaternionLength(Quaternion quaternion) {
		return sqrt(QuaternionSquaredLength(quaternion).value);
	}

	ECS_INLINE PackedQuaternion ECS_VECTORCALL QuaternionLength(PackedQuaternion quaternion) {
		return sqrt(QuaternionSquaredLength(quaternion).value);
	}

	// ---------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Normalize

	// ---------------------------------------------------------------------------------------------------------------

	ECS_INLINE Quaternion ECS_VECTORCALL QuaternionNormalize(Quaternion quaternion) {
		Vector4 vector(quaternion.value);
		return Normalize(vector);
	}

	ECS_INLINE PackedQuaternion ECS_VECTORCALL QuaternionNormalize(PackedQuaternion quaternion) {
		Vector8 vector(quaternion.value);
		return Normalize(vector);
	}

	// ---------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Conjugate

	// ---------------------------------------------------------------------------------------------------------------

	// Invert the axis
	ECS_INLINE Quaternion ECS_VECTORCALL QuaternionConjugate(Quaternion quaternion) {
		return Quaternion(blend4<0, 1, 2, 7>(-quaternion.value, quaternion.value));
	}

	// Invert the axis 
	ECS_INLINE PackedQuaternion ECS_VECTORCALL QuaternionConjugate(PackedQuaternion quaternion) {
		return PackedQuaternion(blend8<0, 1, 2, 11, 4, 5, 6, 15>(-quaternion.value, quaternion.value));
	}

	// ---------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Inverse

	// ---------------------------------------------------------------------------------------------------------------

	// Proper inverse is the conjugate divided by the squared length
	ECS_INLINE Quaternion ECS_VECTORCALL QuaternionInverse(Quaternion quaternion) {
		Vector4 reciprocal = VectorGlobals::ONE_4 / Vector4(QuaternionSquaredLength(quaternion).value);
		return Quaternion(Vector4(QuaternionConjugate(quaternion).value) * reciprocal);
	}

	// Proper inverse is the conjugate divided by the squared length
	ECS_INLINE PackedQuaternion ECS_VECTORCALL QuaternionInverse(PackedQuaternion quaternion) {
		Vector8 reciprocal = VectorGlobals::ONE_8 / Vector8(QuaternionSquaredLength(quaternion).value);
		return PackedQuaternion(Vector8(QuaternionConjugate(quaternion).value) * reciprocal);
	}

	// The conjugate and the inverse are the same if the quaternion is normalized;
	// Avoids a division, a dot product and a multiplication
	ECS_INLINE Quaternion ECS_VECTORCALL QuaternionNormalizedInverse(Quaternion quaternion) {
		return QuaternionConjugate(quaternion);
	}

	// The conjugate and the inverse are the same if the quaternion is normalized;
	// Avoids a division, a dot product and a multiplication
	ECS_INLINE PackedQuaternion ECS_VECTORCALL QuaternionNormalizedInverse(PackedQuaternion quaternion) {
		return QuaternionConjugate(quaternion);
	}

	// ---------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Multiplication

	// ---------------------------------------------------------------------------------------------------------------

	ECS_INLINE Quaternion ECS_VECTORCALL QuaternionMultiply(Quaternion a, Quaternion b) {
		// x = b.x * a.w + b.y * a.z - b.z * a.y + b.w * a.x
		// y = -b.x * a.z + b.y * a.w + b.z * a.x + b.w * a.y
		// z = b.x * a.y - b.y * a.x + b.z * a.w + b.w * a.z
		// w = -b.x * a.x - b.y * a.y - b.z * a.z + b.w * a.w

		Vector4 a_vector = a.value;
		Vector4 first_permutation = permute4<3, 2, 1, 0>(a_vector);
		Vector4 second_permutation = permute4<2, 3, 0, 1>(a_vector);
		Vector4 third_permutation = permute4<1, 0, 3, 2>(a_vector);
		
		// Negate the correct values and the use the dot product to get the values
		first_permutation = change_sign<0, 0, 1, 0>(first_permutation);
		second_permutation = change_sign<1, 0, 0, 0>(second_permutation);
		third_permutation = change_sign<0, 1, 0, 0>(third_permutation);
		Vector4 fourth_permutation = change_sign<1, 1, 1, 0>(a_vector);

		Vector4 x = Dot(b.value, first_permutation);
		Vector4 y = Dot(b.value, second_permutation);
		Vector4 z = Dot(b.value, third_permutation);
		Vector4 w = Dot(b.value, fourth_permutation);

		Vector4 xy = blend4<0, 4, 2, 3>(x, y);
		Vector4 zw = blend4<0, 0, 0, 4>(z, w);
		return blend4<0, 1, 4, 5>(xy, zw);
	}

	ECS_INLINE PackedQuaternion ECS_VECTORCALL QuaternionMultiply(PackedQuaternion a, PackedQuaternion b) {
		// x = b.x * a.w + b.y * a.z - b.z * a.y + b.w * a.x
		// y = -b.x * a.z + b.y * a.w + b.z * a.x + b.w * a.y
		// z = b.x * a.y - b.y * a.x + b.z * a.w + b.w * a.z
		// w = -b.x * a.x - b.y * a.y - b.z * a.z + b.w * a.w

		Vector8 a_vector = a.value;
		Vector8 first_permutation = permute8<3, 2, 1, 0, 7, 6, 5, 4>(a_vector);
		Vector8 second_permutation = permute8<2, 3, 0, 1, 6, 7, 4, 5>(a_vector);
		Vector8 third_permutation = permute8<1, 0, 3, 2, 5, 4, 7, 6>(a_vector);

		// Negate the correct values and the use the dot product to get the values
		first_permutation = change_sign<0, 0, 1, 0, 0, 0, 1, 0>(first_permutation);
		second_permutation = change_sign<1, 0, 0, 0, 1, 0, 0, 0>(second_permutation);
		third_permutation = change_sign<0, 1, 0, 0, 0, 1, 0, 0>(third_permutation);
		Vector8 fourth_permutation = change_sign<1, 1, 1, 0, 1, 1, 1, 0>(a_vector);

		Vector8 x = Dot(b.value, first_permutation);
		Vector8 y = Dot(b.value, second_permutation);
		Vector8 z = Dot(b.value, third_permutation);
		Vector8 w = Dot(b.value, fourth_permutation);

		Vector8 xy = blend8<0, 8, 2, 3, 4, 12, 6, 7>(x, y);
		Vector8 zw = blend8<0, 1, 2, 8, 4, 5, 6, 15>(z, w);
		return blend8<0, 1, 10, 11, 4, 5, 14, 15>(xy, zw);
	}

	// ---------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Vector multiply

	// ---------------------------------------------------------------------------------------------------------------

	ECS_INLINE Vector4 ECS_VECTORCALL QuaternionVectorMultiply(Quaternion quaternion, Vector4 vector) {
		// 2.0f * Dot3(quaternion.xyz, vector.xyz) * quaternion.xyz + vector * 
		// (quaternion.w * quaternion.w - Dot(quaternion.xyz, quaternion.xyz)) + Cross(quaternion.xyz, vector.xyz) * 2.0f * quaternion.w

		Vector4 quat_vec(quaternion.value);
		// Splat the w component
		Vector4 quat_w = permute4<3, 3, 3, 3>(quat_vec);
		Vector4 w_squared = quat_w * quat_w;
		Vector4 dot = Dot3(quat_vec, vector);
		Vector4 cross = Cross(quat_vec, vector);
		Vector4 two = Vector4(2.0f);

		Vector4 quaternion_dot = Dot3(quat_vec, quat_vec);

		Vector4 first_term = two * dot * quat_vec;
		Vector4 second_term = vector * (w_squared - quaternion_dot);
		Vector4 third_term = cross * two * quat_w;

		return first_term + second_term + third_term;
	}

	ECS_INLINE Vector8 ECS_VECTORCALL QuaternionVectorMultiply(PackedQuaternion quaternion, Vector8 vector) {
		// 2.0f * Dot3(quaternion.xyz, vector.xyz) * quaternion.xyz + vector * 
		// (quaternion.w * quaternion.w - Dot(quaternion.xyz, quaternion.xyz)) + Cross(quaternion.xyz, vector.xyz) * 2.0f * quaternion.w

		Vector8 quat_vec(quaternion.value);
		// Splat the w component
		Vector8 quat_w = permute8<3, 3, 3, 3, 7, 7, 7, 7>(quat_vec);
		Vector8 w_squared = quat_w * quat_w;
		Vector8 dot = Dot3(quat_vec, vector);
		Vector8 cross = Cross(quat_vec, vector);
		Vector8 two = Vector8(2.0f);

		Vector8 quaternion_dot = Dot3(quat_vec, quat_vec);

		Vector8 first_term = two * dot * quat_vec;
		Vector8 second_term = vector * (w_squared - quaternion_dot);
		Vector8 third_term = cross * two * quat_w;

		return first_term + second_term + third_term;
	}

	// ---------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Neighbourhood

	// ---------------------------------------------------------------------------------------------------------------

	// If the dot product between the 2 quaternions is negative, negate b and return it
	ECS_INLINE Quaternion ECS_VECTORCALL QuaternionNeighbour(Quaternion quat_a, Quaternion quat_b) {
		Quaternion dot = QuaternionDot(quat_a, quat_b);
		float value = Vector4(dot.value).First();
		if (value < 0.0f) {
			return -quat_b;
		}
		return quat_b;
	}

	// If the dot product between the 2 quaternions is negative, negate b and return it
	ECS_INLINE PackedQuaternion ECS_VECTORCALL QuaternionNeighbour(PackedQuaternion quat_a, PackedQuaternion quat_b) {
		PackedQuaternion dot = QuaternionDot(quat_a, quat_b);
		float value1 = Vector8(dot.value).First();
		float value2 = Vector8(dot.value).FirstHigh();

		PackedQuaternion negated = -quat_b;
		PackedQuaternion result = quat_b;
		if (value1 < 0.0f) {
			result = Vector8(blend8<8, 9, 10, 11, 4, 5, 6, 7>(quat_b.value, negated.value));
		}
		if (value2 < 0.0f) {
			result = Vector8(blend8<0, 1, 2, 3, 12, 13, 14, 15>(result.value, negated.value));
		}
		return result;
	}

	// ---------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Mix

	// ---------------------------------------------------------------------------------------------------------------

	// If inputs are unit length, output will be the same; assumes neighbour quaternions
	// Similar to a lerp
	ECS_INLINE Quaternion ECS_VECTORCALL QuaternionMix(Quaternion from, Quaternion to, float percentage) {
		Vector4 vector_percentage(percentage);
		Vector4 one_minus_vector = VectorGlobals::ONE_4 - vector_percentage;
		return Quaternion(Vector4(Vector4(from.value) * one_minus_vector + Vector4(to.value) * vector_percentage).value);
	}

	// If inputs are unit length, output will be the same; assumes neighbour quaternions
	// Similar to a lerp; applies the percentage to both parts - high and low
	ECS_INLINE PackedQuaternion ECS_VECTORCALL QuaternionMix(PackedQuaternion from, PackedQuaternion to, float percentage) {
		Vector8 vector_percentage(percentage);
		Vector8 one_minus_vector = VectorGlobals::ONE_8 - vector_percentage;
		return PackedQuaternion(Vector8(Vector8(from.value) * one_minus_vector + Vector8(to.value) * vector_percentage).value);
	}

	// If inputs are unit length, output will be the same; assumes neighbour quaternions
	// Similar to a lerp; applies different percentages to high and low
	ECS_INLINE PackedQuaternion ECS_VECTORCALL QuaternionMix(PackedQuaternion from, PackedQuaternion to, float percentage1, float percentage2) {
		Vector8 vector_percentage{ Vector4(percentage1), Vector4(percentage2) };
		Vector8 one_minus_vector = VectorGlobals::ONE_8 - vector_percentage;
		return PackedQuaternion(Vector8(Vector8(from.value) * one_minus_vector + Vector8(to.value) * vector_percentage).value);
	}

	// If inputs are unit length, output will be the same; makes neighbour correction as needed
	// Similar to a lerp
	ECS_INLINE Quaternion ECS_VECTORCALL QuaternionMixNeighbour(Quaternion from, Quaternion to, float percentage) {
		to = QuaternionNeighbour(from, to);
		return QuaternionMix(from, to, percentage);
	}

	// If inputs are unit length, output will be the same; makes neighbour correction as needed
	// Similar to a lerp; applies the percentage to both parts - high and low
	ECS_INLINE PackedQuaternion ECS_VECTORCALL QuaternionMixNeighbour(PackedQuaternion from, PackedQuaternion to, float percentage) {
		to = QuaternionNeighbour(from, to);
		return QuaternionMix(from, to, percentage);
	}

	// If inputs are unit length, output will be the same; makes neighbour correction as needed
	// Similar to a lerp; applies different percentages to high and low
	ECS_INLINE PackedQuaternion ECS_VECTORCALL QuaternionMixNeighbour(PackedQuaternion from, PackedQuaternion to, float percentage1, float percentage2) {
		to = QuaternionNeighbour(from, to);
		return QuaternionMix(from, to, percentage1, percentage2);
	}

	// ---------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region NLerp

	// ---------------------------------------------------------------------------------------------------------------

	// Assumes neighbour quaternions
	ECS_INLINE Quaternion ECS_VECTORCALL QuaternionNLerp(Quaternion from, Quaternion to, float percentage) {
		return QuaternionNormalize(from + (to - from) * percentage);
	}

	// Assumes neighbour quaternions; percentage1 for low, percentage2 for high
	ECS_INLINE PackedQuaternion ECS_VECTORCALL QuaternionNLerp(PackedQuaternion from, PackedQuaternion to, float percentage1, float percentage2) {
		Vector8 to_from = (to - from).value;
		return QuaternionNormalize(from + PackedQuaternion(to_from * Vector8{ Vector4(percentage1), Vector4(percentage2) }));
	}

	// Assumes neighbour quaternions; percentage for low and high
	ECS_INLINE PackedQuaternion ECS_VECTORCALL QuaternionNLerp(PackedQuaternion from, PackedQuaternion to, float percentage) {
		return QuaternionNormalize(from + (to - from) * percentage);
	}

	// Makes neighbour correction
	ECS_INLINE Quaternion ECS_VECTORCALL QuaternionNLerpNeighbour(Quaternion from, Quaternion to, float percentage) {
		to = QuaternionNeighbour(from, to);
		return QuaternionNLerp(from, to, percentage);
	}

	// Makes neighbour correction; percentage1 for low, percentage2 for high
	ECS_INLINE PackedQuaternion ECS_VECTORCALL QuaternionNLerpNeighbour(PackedQuaternion from, PackedQuaternion to, float percentage1, float percentage2) {
		to = QuaternionNeighbour(from, to);
		return QuaternionNLerp(from, to, percentage1, percentage2);
	}

	// Makes neighbour correction; percentage for low and high
	ECS_INLINE PackedQuaternion ECS_VECTORCALL QuaternionNLerpNeighbour(PackedQuaternion from, PackedQuaternion to, float percentage) {
		to = QuaternionNeighbour(from, to);
		return QuaternionNLerp(from, to, percentage);
	}

	// ---------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region To Power

	// ---------------------------------------------------------------------------------------------------------------

	ECS_INLINE Quaternion ECS_VECTORCALL QuaternionToPower(Quaternion quaternion, float power) {
		float quat_angle = QuaternionAngle(quaternion);
		float angle = acosf(quat_angle);
		Vector4 axis = Normalize3(quaternion.value);
		
		Vec4f cos;
		Vector4 sin = sincos(&cos, power * angle);
		axis *= sin;

		return Quaternion(blend4<0, 1, 2, 4>(axis, cos));
	}

	ECS_INLINE PackedQuaternion ECS_VECTORCALL QuaternionToPower(PackedQuaternion quaternion, float power) {
		Vector8 power_vector(power);
		Vector8 axis = Normalize3(quaternion.value);

		Vector8 angle = acos(quaternion.value);

		Vec8f cos;
		Vector8 sin = sincos(&cos, angle * power_vector);
		axis *= sin;

		return PackedQuaternion(blend8<0, 1, 2, 11, 4, 5, 6, 15>(axis, cos));
	}

	// ---------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region SLerp

	// ---------------------------------------------------------------------------------------------------------------

	// Assumes neighbour quaternions
	ECS_INLINE Quaternion ECS_VECTORCALL QuaternionSLerp(Quaternion from, Quaternion to, float percentage) {
		Quaternion dot = QuaternionDot(from, to);
		Vector4 abs_dot = abs(Vector4(dot.value));
		Vector4 one_epsilon = VectorGlobals::ONE_4 - VectorGlobals::QUATERNION_EPSILON_4;
		if (horizontal_and(abs_dot.value > one_epsilon.value)) {
			return QuaternionNLerp(from, to, percentage);
		}
		Quaternion delta = QuaternionMultiply(QuaternionInverse(from), to);
		return QuaternionNormalize(QuaternionMultiply(QuaternionToPower(delta, percentage), from));
	}

	// Assumes neighbour quaternions; applies the same percentage to low and high
	ECS_INLINE PackedQuaternion ECS_VECTORCALL QuaternionSLerp(PackedQuaternion from, PackedQuaternion to, float percentage) {
		Quaternion from_low = from.Low();
		Quaternion from_high = from.High();
		Quaternion to_low = to.Low();
		Quaternion to_high = to.High();

		return PackedQuaternion(QuaternionSLerp(from_low, to_low, percentage), QuaternionSLerp(from_high, to_high, percentage));
	}

	// Assumes neighbour quaternions; applies different percentages to low and high
	ECS_INLINE PackedQuaternion ECS_VECTORCALL QuaternionSLerp(PackedQuaternion from, PackedQuaternion to, float percentage1, float percentage2) {
		Quaternion from_low = from.Low();
		Quaternion from_high = from.High();
		Quaternion to_low = to.Low();
		Quaternion to_high = to.High();

		return PackedQuaternion(QuaternionSLerp(from_low, to_low, percentage1), QuaternionSLerp(from_high, to_high, percentage2));
	}

	// Makes the neighbour adjustment
	ECS_INLINE Quaternion ECS_VECTORCALL QuaternionSLerpNeighbour(Quaternion from, Quaternion to, float percentage) {
		to = QuaternionNeighbour(from, to);
		return QuaternionSLerp(from, to, percentage);
	}

	// Makes the neighbour adjustment; applies the same percentage to low and high
	ECS_INLINE PackedQuaternion ECS_VECTORCALL QuaternionSLerpNeighbour(PackedQuaternion from, PackedQuaternion to, float percentage) {
		to = QuaternionNeighbour(from, to);
		return QuaternionSLerp(from, to, percentage);
	}

	// Makes the neighbour adjustment; applies different percentages to low and high
	ECS_INLINE PackedQuaternion ECS_VECTORCALL QuaternionSLerpNeighbour(PackedQuaternion from, PackedQuaternion to, float percentage1, float percentage2) {
		to = QuaternionNeighbour(from, to);
		return QuaternionSLerp(from, to, percentage1, percentage2);
	}

	// ---------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Look Rotation

	// ---------------------------------------------------------------------------------------------------------------

	ECS_INLINE Quaternion ECS_VECTORCALL QuaternionLookRotation(Vector4 direction, Vector4 up) {
		Vector4 normalized_direction = Normalize3(direction); // Object Forward
		Vector4 normalized_up = Normalize3(up); // Desired Up
		Vector4 normalized_right = Cross(normalized_up, normalized_direction); // Object Right

		Vector4 object_up = Cross(normalized_direction, normalized_right); // Object Up
		// From world forward to object forward

		Vector4 default_up = VectorGlobals::UP_4;
		Vector4 default_forward = VectorGlobals::FORWARD_4;
		Quaternion world_to_object = QuaternionFromVectors(default_forward, normalized_direction);

		// What direction is the new object up?
		Vector4 objectUp = QuaternionVectorMultiply(world_to_object, default_up);

		// From object up to desired up
		Quaternion object_up_to_desired_up = QuaternionFromVectors(objectUp, object_up);

		// Rotate to forward direction first then twist to correct up
		Quaternion result = QuaternionMultiply(world_to_object, object_up_to_desired_up);

		// Normalize the result
		return QuaternionNormalize(result);
	}

	ECS_INLINE Quaternion ECS_VECTORCALL QuaternionLookRotation(float3 direction, float3 up) {
		// Find orthonormal basis vectors
		Vector4 direction_vector(direction);
		Vector4 up_vector(up);

		return QuaternionLookRotation(direction_vector, up_vector);
	}

	ECS_INLINE PackedQuaternion ECS_VECTORCALL QuaternionLookRotation(Vector8 direction, Vector8 up) {
		Vector8 normalized_direction = Normalize3(direction); // Object Forward
		Vector8 normalized_up = Normalize3(up); // Desired Up
		Vector8 normalized_right = Cross(normalized_up, normalized_direction); // Object Right

		Vector8 object_up = Cross(normalized_direction, normalized_right); // Object Up
		// From world forward to object forward

		Vector8 default_up = VectorGlobals::UP_8;
		Vector8 default_forward = VectorGlobals::FORWARD_8;
		PackedQuaternion world_to_object = QuaternionFromVectors(default_forward, normalized_direction);

		// What direction is the new object up?
		Vector8 objectUp = QuaternionVectorMultiply(world_to_object, default_up);

		// From object up to desired up
		PackedQuaternion object_up_to_desired_up = QuaternionFromVectors(objectUp, object_up);

		// Rotate to forward direction first then twist to correct up
		PackedQuaternion result = QuaternionMultiply(world_to_object, object_up_to_desired_up);

		// Normalize the result
		return QuaternionNormalize(result);
	}

	ECS_INLINE PackedQuaternion ECS_VECTORCALL QuaternionLookRotation(float3 direction1, float3 up1, float3 direction2, float3 up2) {
		// Find orthonormal basis vectors
		Vector8 direction_vector(direction1, direction2);
		Vector8 up_vector(up1, up2);

		return QuaternionLookRotation(direction_vector, up_vector);
	}

	// ---------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Rotate Vector

	// ---------------------------------------------------------------------------------------------------------------

	ECS_INLINE Vector4 ECS_VECTORCALL RotateVector(Vector4 vector, Quaternion quaternion) {
		return QuaternionVectorMultiply(quaternion, vector);
	}

	ECS_INLINE Vector8 ECS_VECTORCALL RotateVector(Vector8 vector, PackedQuaternion quaternion) {
		return QuaternionVectorMultiply(quaternion, vector);
	}

	// ---------------------------------------------------------------------------------------------------------------

#pragma region


}