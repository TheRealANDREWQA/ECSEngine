#pragma once
#include "../Core.h"
#include "ecspch.h"
#include "../Utilities/BasicTypes.h"
#include "VCLExtensions.h"
#include "Vector.h"

#define ECS_QUATERNION_EPSILON (0.00005f)

namespace ECSEngine {

	namespace VectorGlobals {
		inline Vector8 QUATERNION_EPSILON(ECS_QUATERNION_EPSILON);
	}

	struct Quaternion;

	Quaternion ECS_VECTORCALL QuaternionMultiply(Quaternion a, Quaternion b);
	Quaternion ECS_VECTORCALL QuaternionFromEuler(float3 rotation);
	Quaternion ECS_VECTORCALL QuaternionFromEuler(float3 rotation0, float3 rotation1);
	float3 ECS_VECTORCALL QuaternionToEulerLow(Quaternion quaternion);
	float3 ECS_VECTORCALL QuaternionToEulerHigh(Quaternion quaternion);

	// When wanting to store a quaternion value into structures but you don't want
	// to store the SIMD quaternion is this value
	typedef float4 QuaternionStorage;

	struct Quaternion {
		typedef Vector8 VectorType;

		ECS_INLINE Quaternion() {}
		ECS_INLINE Quaternion(float x1, float y1, float z1, float w1, float x2, float y2, float z2, float w2)
		: value(x1, y1, z1, w1, x2, y2, z2, w2) {}
		ECS_INLINE Quaternion(const float* values) {
			value.load(values);
		}
		ECS_INLINE Quaternion(float3 rotation) {
			*this = QuaternionFromEuler(rotation);
		}
		ECS_INLINE Quaternion(QuaternionStorage storage_value) {
			value = Vector8(storage_value).value;
		}
		ECS_INLINE Quaternion(float3 rotation0, float3 rotation1) {
			*this = QuaternionFromEuler(rotation0, rotation1);
		}
		ECS_INLINE Quaternion(QuaternionStorage value0, QuaternionStorage value1) {
			value = Vector8(value0, value1).value;
		}
		ECS_INLINE Quaternion(Vec8f _value) : value(_value) {}
		ECS_INLINE Quaternion(Vector8 _value) : value(_value.value) {}

		ECS_INLINE Quaternion(const Quaternion& other) = default;
		ECS_INLINE Quaternion& ECS_VECTORCALL operator = (const Quaternion& other) = default;

		ECS_INLINE ECS_VECTORCALL operator Vec8f() const {
			return value;
		}

		ECS_INLINE ECS_VECTORCALL operator __m256() const {
			return value.operator __m256();
		}

		ECS_INLINE bool2 ECS_VECTORCALL operator == (Quaternion other) const {
			auto bool_vector = abs(value - other.value) < VectorGlobals::QUATERNION_EPSILON;
			return { horizontal_and(bool_vector.get_low()), horizontal_and(bool_vector.get_high()) };
		}

		ECS_INLINE bool2 ECS_VECTORCALL operator != (Quaternion other) const {
			bool2 is_equal = *this == other;
			return { !is_equal.x, !is_equal.y };
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
			return value * Vec8f(scalar);
		}

		ECS_INLINE Quaternion ECS_VECTORCALL operator * (Quaternion other) const {
			return QuaternionMultiply(*this, other);
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
			value *= Vec8f(scalar);
			return *this;
		}

		ECS_INLINE Quaternion& ECS_VECTORCALL operator *= (Quaternion other) {
			*this = QuaternionMultiply(*this, other);
			return *this;
		}

		ECS_INLINE Vec4f ECS_VECTORCALL LowSIMD() const {
			return value.get_low();
		}

		ECS_INLINE Vec4f ECS_VECTORCALL HighSIMD() const {
			return value.get_high();
		}

		ECS_INLINE float4 ECS_VECTORCALL StorageLow() const {
			return Vector8(value).AsFloat4Low();
		}

		ECS_INLINE float4 ECS_VECTORCALL StorageHigh() const {
			return Vector8(value).AsFloat4High();
		}

		ECS_INLINE float3 EulerLow() const {
			return QuaternionToEulerLow(*this);
		}

		ECS_INLINE float3 EulerHigh() const {
			return QuaternionToEulerHigh(*this);
		}

		// Returns a quaternion with the low duplicated
		ECS_INLINE Quaternion ECS_VECTORCALL SplatLow() const {
			return SplatLowLane(value);
		}

		// Returns a quaternion with the high duplicated
		ECS_INLINE Quaternion ECS_VECTORCALL SplatHigh() const {
			return SplatHighLane(value);
		}

		Vec8f value;
	};

	// ---------------------------------------------------------------------------------------------------------------

	ECS_INLINE Quaternion ECS_VECTORCALL QuaternionIdentity() {
		return LastElementOneVector();
	}

	// ---------------------------------------------------------------------------------------------------------------

	ECS_INLINE Vector8 ECS_VECTORCALL QuaternionCompareMask(Quaternion a, Quaternion b, Vector8 epsilon = VectorGlobals::EPSILON) {
		return CompareMask<true>(a.value, b.value, epsilon);
	}

	ECS_SIMD_CREATE_BOOLEAN_FUNCTIONS_FOR_MASK_FIXED_LANE(
		QuaternionCompare,
		4,
		FORWARD(Quaternion a, Quaternion b, Vector8 epsilon = VectorGlobals::EPSILON),
		FORWARD(a, b, epsilon)
	);

	// ---------------------------------------------------------------------------------------------------------------

	// The value is replicated across all register values
	ECS_INLINE Quaternion ECS_VECTORCALL QuaternionDot(Quaternion a, Quaternion b) {
		return Dot<true>(a.value, b.value);
	}

	ECS_INLINE Quaternion ECS_VECTORCALL QuaternionSquaredLength(Quaternion a) {
		return QuaternionDot(a, a);
	}

	ECS_INLINE Quaternion ECS_VECTORCALL QuaternionLength(Quaternion quaternion) {
		return sqrt(QuaternionSquaredLength(quaternion).value);
	}

	// ---------------------------------------------------------------------------------------------------------------

	template<VectorOperationPrecision precision = ECS_VECTOR_PRECISE>
	ECS_INLINE Quaternion ECS_VECTORCALL QuaternionNormalize(Quaternion quaternion) {
		return Normalize<precision, true>(Vector8(quaternion.value));
	}

	ECS_INLINE Vector8 ECS_VECTORCALL QuaternionIsNormalizedMask(Quaternion quaternion) {
		return IsNormalizedMask<true>(Vector8(quaternion));
	}

	ECS_SIMD_CREATE_BOOLEAN_FUNCTIONS_FOR_MASK_FIXED_LANE
	(
		QuaternionIsNormalized,
		4,
		Quaternion quaternion,
		quaternion
	);

	// ---------------------------------------------------------------------------------------------------------------

	// Angle must be expressed in radians
	ECS_INLINE Quaternion ECS_VECTORCALL QuaternionAngleFromAxisRad(Vector8 vector_axis, Vector8 angle_radians) {
		vector_axis = NormalizeIfNot(vector_axis);

		Vector8 half = 0.5f;
		angle_radians *= half;

		Vec8f cos;
		Vector8 sin = sincos(&cos, angle_radians.value);
		vector_axis *= sin;
		vector_axis = PerLaneBlend<0, 1, 2, 7>(vector_axis, Vector8(cos));
		return vector_axis.value;
	}

	ECS_INLINE Quaternion ECS_VECTORCALL QuaternionAngleFromAxis(Vector8 vector_axis, Vector8 angle_degrees) {
		return QuaternionAngleFromAxisRad(vector_axis, DegToRad(angle_degrees));
	}

	// ---------------------------------------------------------------------------------------------------------------

	ECS_INLINE Quaternion ECS_VECTORCALL QuaternionForAxisXRad(Vector8 angle_radians) {
		return QuaternionAngleFromAxisRad(RightVector(), angle_radians);
	}

	ECS_INLINE Quaternion ECS_VECTORCALL QuaternionForAxisYRad(Vector8 angle_radians) {
		return QuaternionAngleFromAxisRad(UpVector(), angle_radians);
	}

	ECS_INLINE Quaternion ECS_VECTORCALL QuaternionForAxisZRad(Vector8 angle_radians) {
		return QuaternionAngleFromAxisRad(ForwardVector(), angle_radians);
	}

	ECS_INLINE Quaternion ECS_VECTORCALL QuaternionForAxisX(Vector8 angle_degrees) {
		return QuaternionForAxisXRad(DegToRad(angle_degrees));
	}

	ECS_INLINE Quaternion ECS_VECTORCALL QuaternionForAxisY(Vector8 angle_degrees) {
		return QuaternionForAxisYRad(DegToRad(angle_degrees));
	}

	ECS_INLINE Quaternion ECS_VECTORCALL QuaternionForAxisZ(Vector8 angle_degrees) {
		return QuaternionForAxisZRad(DegToRad(angle_degrees));
	}

	// ---------------------------------------------------------------------------------------------------------------

	// The angles must be expressed in radians
	ECS_INLINE Quaternion ECS_VECTORCALL QuaternionFromEulerRad(float3 rotation_radians0, float3 rotation_radians1) {
		Quaternion quaternion;

		Vector8 angles(rotation_radians0, rotation_radians1);
		Vector8 half(0.5f);
		angles *= half;

		Vec8f cos;
		Vector8 sin = sincos(&cos, angles);

		// Quaternion:
		// x = sin(x) * cos(y) * cos(z) - cos(x) * sin(y) * sin(z)
		// y = cos(x) * sin(y) * cos(z) + sin(x) * cos(y) * sin(z)
		// z = cos(x) * cos(y) * sin(z) - sin(x) * sin(y) * cos(z)
		// w = cos(x) * cos(y) * cos(z) + sin(x) * sin(y) * sin(z)

		Vector8 first_permutation_left = PerLaneBlend<4, 0, 0, 0>(Vector8(cos), sin);
		Vector8 second_permutation_left = PerLaneBlend<1, 5, 1, 1>(Vector8(cos), sin);
		Vector8 third_permutation_left = PerLaneBlend<2, 2, 6, 2>(Vector8(cos), sin);

		Vector8 first_permutation_right = PerLanePermute<1, 0, 0, 0>(first_permutation_left);
		Vector8 second_permutation_right = PerLanePermute<1, 0, 1, 1>(second_permutation_left);
		Vector8 third_permutation_right = PerLanePermute<2, 2, 0, 2>(third_permutation_left);

		Vector8 left = first_permutation_left * second_permutation_left * third_permutation_left;
		Vector8 right = first_permutation_right * second_permutation_right * third_permutation_right;
		right = PerLaneChangeSign<1, 0, 1, 0>(right);

		quaternion.value = (left + right).value;
		return quaternion;
	}

	// The angles must be expressed in degrees
	ECS_INLINE Quaternion ECS_VECTORCALL QuaternionFromEuler(float3 rotation_degrees0, float3 rotation_degrees1) {
		return QuaternionFromEulerRad(DegToRad(rotation_degrees0), DegToRad(rotation_degrees1));
	}

	// The angles must be expressed in radians
	ECS_INLINE Quaternion ECS_VECTORCALL QuaternionFromEulerRad(float3 rotation) {
		return QuaternionFromEulerRad(rotation, rotation);
	}

	// The angles must be expressed in degrees
	ECS_INLINE Quaternion ECS_VECTORCALL QuaternionFromEuler(float3 rotation) {
		return QuaternionFromEulerRad(DegToRad(rotation));
	}

	// ---------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region From Vectors

	// ---------------------------------------------------------------------------------------------------------------

	// Expects the from vector and the to vector to be normalized
	ECS_INLINE Quaternion ECS_VECTORCALL QuaternionFromVectorsNormalized(Vector8 from_vector_normalized, Vector8 to_vector_normalized) {
		// If they are equal return an identity quaternion
		bool2 both_are_equal = Compare(from_vector_normalized, to_vector_normalized, VectorGlobals::QUATERNION_EPSILON);
		if (both_are_equal.x && both_are_equal.y) {
			return QuaternionIdentity();
		}
		else {
			// Now there are 2 branches left
			// If they are opposite and when they are not
			// If they are opposite return the most orthogonal axis of the from_vector vector
			// that can be used to create a pure quaternion

			// Can't be sure which branch each half must take so execute both of them
			// and then mask the result at the end

			Vector8 compare_mask = CompareMask(from_vector_normalized, -to_vector_normalized, VectorGlobals::QUATERNION_EPSILON);

#pragma region Opposite branch - it needs to be executed no matter what
			// This is the scalar code that must be executed
			// In the branches down bellow
			/*
				float3 ortho = float3(1, 0, 0);
				if (fabsf(f.y) < fabsf(f.x)) {
					orthogonal = float3(0, 1, 0);
				}
				if (fabsf(f.z) < fabs(f.y) && fabs(f.z) < fabsf(f.x)){
					orthogonal = float3(0, 0, 1);
				}
			*/

			Vector8 absolute_from_vector = abs(from_vector_normalized);

			// TODO: Make this more efficient?
			alignas(32) float4 opposite_values[2];
			absolute_from_vector.StoreAligned(&opposite_values);

			Vector8 one = VectorGlobals::ONE;
			Vector8 zero = ZeroVector();
			Vector8 opposite_orthogonal = PerLaneBlend<4, 0, 0, 0>(zero, one);

			// first assignment - no need to do a second blend to keep the original values
			if (opposite_values[0].y < opposite_values[0].x) {
				opposite_orthogonal = blend8<0, 9, 0, 0, 12, 0, 0, 0>(zero, one);
			}
			else if (opposite_values[0].z < opposite_values[0].y && opposite_values[0].z < opposite_values[0].x) {
				opposite_orthogonal = blend8<0, 0, 10, 0, 12, 0, 0, 0>(zero, one);
			}

			// second assignment - original values must be kept so a second blend must be done
			if (opposite_values[1].y < opposite_values[1].x) {
				// only the first 4 values of the zero one blend must be correct
				opposite_orthogonal = permute8<0, 1, 2, 3, 7, 4, 7, 7>(opposite_orthogonal);
			}
			else if (opposite_values[1].z < opposite_values[1].y && opposite_values[1].z < opposite_values[1].x) {
				opposite_orthogonal = permute8<0, 1, 2, 3, 7, 7, 4, 7>(opposite_orthogonal);
			}

			Vector8 opposite_quaternion_axis = Normalize(Cross(from_vector_normalized, opposite_orthogonal));
			Quaternion opposite_quaternion_result = Quaternion(PerLaneBlend<0, 1, 2, 7>(opposite_quaternion_axis, zero));

#pragma endregion

			// Test the case when they are both opposite
			// If they are, we can stop and return the result calculated here
			if (horizontal_and(compare_mask.AsMask())) {
				return opposite_quaternion_result;
			}

			// Else create the quaternion from the half vector
			Vector8 half = Normalize(from_vector_normalized + to_vector_normalized);
			Vector8 axis = Cross(from_vector_normalized, half);
			Vector8 dot = Dot(to_vector_normalized, half);
			Quaternion general_case_result = Quaternion(PerLaneBlend<0, 1, 2, 4>(axis, dot));

			// Select the branch each half needs to take
			return Select(compare_mask, opposite_quaternion_result.value, general_case_result.value);
		}
	}

	// If the vectors are already normalized you can call directly the other function
	ECS_INLINE Quaternion ECS_VECTORCALL QuaternionFromVectors(Vector8 from, Vector8 to) {
		// Normalize the vectors
		from = Normalize(from);
		to = Normalize(to);

		return QuaternionFromVectorsNormalized(from, to);
	}

	// ---------------------------------------------------------------------------------------------------------------

	// The quaternion needs to be normalized
	ECS_INLINE void ECS_VECTORCALL QuaternionAxisNormalized(Quaternion quaternion_normalized, float3* axis_low, float3* axis_high) {
		Vector8 vector_axis(quaternion_normalized.value);
		vector_axis.StoreFloat3(axis_low, axis_high);
	}

	// The quaternion needs to be normalized
	ECS_INLINE float3 ECS_VECTORCALL QuaternionAxisNormalized(Quaternion quaternion_normalized) {
		Vector8 vector_axis(quaternion_normalized.value);
		return vector_axis.AsFloat3Low();
	}

	// If the quaternion is normalized use the other variant
	ECS_INLINE void ECS_VECTORCALL QuaternionAxis(Quaternion quaternion, float3* axis_low, float3* axis_high) {
		QuaternionAxisNormalized(QuaternionNormalize(quaternion), axis_low, axis_high);
	}

	// If the quaternion is normalized use the other variant
	ECS_INLINE float3 ECS_VECTORCALL QuaternionAxis(Quaternion quaternion) {
		return QuaternionAxisNormalized(QuaternionNormalize(quaternion));
	}

	// ---------------------------------------------------------------------------------------------------------------

	// The angle is in radians
	ECS_INLINE float ECS_VECTORCALL QuaternionAngleRadLow(Quaternion quaternion) {
		return 2.0f * acosf(Vector8(quaternion).Last());
	}

	// The angle is in radians
	ECS_INLINE float2 ECS_VECTORCALL QuaternionAngleRad(Quaternion quaternion) {
		// Calculate the acos for both at the same time
		Vector8 half_angle = acos(quaternion.value);
		return float2::Splat(2.0f) * half_angle.GetLasts();
	}

	// The angle is in radians
	// The angle value is splatted across the lane
	ECS_INLINE Vector8 ECS_VECTORCALL QuaternionHalfAngleRadSIMD(Quaternion quaternion) {
		Vector8 half_angle = acos(Vector8(quaternion.value));
		return PerLaneBroadcast<3>(half_angle);
	}

	// The angle is in radians
	// The angle value is splatted across the lane
	ECS_INLINE Vector8 ECS_VECTORCALL QuaternionAngleRadSIMD(Quaternion quaternion) {	
		Vector8 two = Vector8(2.0f);
		return QuaternionHalfAngleRadSIMD(quaternion) * two;
	}

	// The angle is in degrees
	ECS_INLINE float ECS_VECTORCALL QuaternionAngleLow(Quaternion quaternion) {
		return RadToDeg(QuaternionAngleRadLow(quaternion));
	}

	// The angle is in degrees
	ECS_INLINE float2 ECS_VECTORCALL QuaternionAngle(Quaternion quaternion) {
		return RadToDeg(QuaternionAngleRad(quaternion));
	}

	ECS_INLINE Vector8 ECS_VECTORCALL QuaternionHalfAngleSIMD(Quaternion quaternion) {
		return RadToDeg(QuaternionHalfAngleRadSIMD(quaternion));
	}

	// The angle is in degrees
	// The angle value is splatted across the lane
	ECS_INLINE Vector8 ECS_VECTORCALL QuaternionAngleSIMD(Quaternion quaternion) {
		return RadToDeg(QuaternionAngleRadSIMD(quaternion));
	}

	// ---------------------------------------------------------------------------------------------------------------

	ECS_INLINE Vector8 ECS_VECTORCALL QuaternionSameOrientationMask(Quaternion a, Quaternion b) {
		return abs(a.value - b.value) < VectorGlobals::QUATERNION_EPSILON || abs(a.value + b.value) < VectorGlobals::QUATERNION_EPSILON;
	}

	ECS_SIMD_CREATE_BOOLEAN_FUNCTIONS_FOR_MASK_FIXED_LANE(
		QuaternionSameOrientation,
		4,
		FORWARD(Quaternion a, Quaternion b),
		FORWARD(a, b)
	);

	// Returns true if the quaternions are located in the same hyperspace
	// (The dot product between them is positive)
	ECS_INLINE Vector8 ECS_VECTORCALL QuaternionSameHyperspaceMask(Quaternion a, Quaternion b) {
		return QuaternionDot(a, b) > ZeroVector();
	}

	ECS_SIMD_CREATE_BOOLEAN_FUNCTIONS_FOR_MASK_FIXED_LANE(
		QuaternionSameHyperspace,
		4,
		FORWARD(Quaternion a, Quaternion b),
		FORWARD(a, b)
	);

	// ---------------------------------------------------------------------------------------------------------------

	ECS_INLINE Quaternion ECS_VECTORCALL QuaternionConjugate(Quaternion quaternion) {
		// Invert the axis
		return Quaternion(PerLaneBlend<0, 1, 2, 7>(-quaternion.value, quaternion.value));
	}

	// ---------------------------------------------------------------------------------------------------------------

	ECS_INLINE Quaternion ECS_VECTORCALL QuaternionInverseNormalized(Quaternion quaternion) {
		// The conjugate and the inverse are the same if the quaternion is normalized;
		// Avoids a division, a square root and a dot product
		return QuaternionConjugate(quaternion);
	}

	ECS_INLINE Quaternion ECS_VECTORCALL QuaternionInverse(Quaternion quaternion) {
		// Proper inverse is the conjugate divided by the squared length
		// If the squared length is 1.0f, then don't perform the division
		if (!QuaternionIsNormalizedWhole(quaternion)) {
			return Quaternion(QuaternionConjugate(quaternion).value / QuaternionSquaredLength(quaternion.value).value);
		}
		else {
			return QuaternionInverseNormalized(quaternion);
		}
	}

	// ---------------------------------------------------------------------------------------------------------------

	ECS_INLINE Quaternion ECS_VECTORCALL QuaternionMultiply(Quaternion a, Quaternion b) {
		// x = b.x * a.w + b.y * a.z - b.z * a.y + b.w * a.x
		// y = -b.x * a.z + b.y * a.w + b.z * a.x + b.w * a.y
		// z = b.x * a.y - b.y * a.x + b.z * a.w + b.w * a.z
		// w = -b.x * a.x - b.y * a.y - b.z * a.z + b.w * a.w

		Vector8 a_vector = a.value;
		Vector8 b_vector = b.value;

		Vector8 b_first = PerLaneBroadcast<0>(b_vector);
		Vector8 b_second = PerLaneBroadcast<1>(b_vector);
		Vector8 b_third = PerLaneBroadcast<2>(b_vector);
		Vector8 b_fourth = PerLaneBroadcast<3>(b_vector);

		Vector8 a_first = PerLanePermute<3, 2, 1, 0>(a_vector);
		Vector8 a_second = PerLanePermute<2, 3, 0, 1>(a_vector);
		Vector8 a_third = PerLanePermute<1, 0, 3, 2>(a_vector);
		Vector8 a_fourth = a_vector;

		// Negate the correct values and the use the dot product to get the values
		a_first = PerLaneChangeSign<0, 1, 0, 1>(a_first);
		a_second = PerLaneChangeSign<0, 0, 1, 1>(a_second);
		a_third = PerLaneChangeSign<1, 0, 0, 1>(a_third);

		Vector8 final_value = b_first * a_first;
		final_value = Fmadd(b_second, a_second, final_value);
		final_value = Fmadd(b_third, a_third, final_value);
		return Fmadd(b_fourth, a_fourth, final_value);
	}

	// ---------------------------------------------------------------------------------------------------------------

	// Can choose to preserve the 4th component from the original or not
	template<bool preserve = false>
	ECS_INLINE Vector8 ECS_VECTORCALL QuaternionVectorMultiply(Quaternion quaternion, Vector8 vector) {
		// 2.0f * Dot3(quaternion.xyz, vector.xyz) * quaternion.xyz + vector * 
		// (quaternion.w * quaternion.w - Dot(quaternion.xyz, quaternion.xyz)) + Cross(quaternion.xyz, vector.xyz) * 2.0f * quaternion.w

		Vector8 quat_vec(quaternion.value);
		// Splat the w component
		Vector8 quat_w = PerLaneBroadcast<3>(quat_vec);
		Vector8 w_squared = quat_w * quat_w;
		Vector8 dot = Dot(quat_vec, vector);
		Vector8 cross = Cross(quat_vec, vector);
		Vector8 two = Vector8(2.0f);

		Vector8 quaternion_dot = Dot(quat_vec, quat_vec);

		Vector8 first_term = two * dot * quat_vec;
		Vector8 second_term = vector * (w_squared - quaternion_dot);
		Vector8 third_term = cross * two * quat_w;

		Vector8 multiplied_vector = first_term + second_term + third_term;
		if constexpr (preserve) {
			multiplied_vector = PerLaneBlend<0, 1, 2, 7>(multiplied_vector, vector);
		}
		return multiplied_vector;
	}

	// ---------------------------------------------------------------------------------------------------------------

	// Returns the negated quaternion if they are in different hyperspaces
	ECS_INLINE Quaternion ECS_VECTORCALL QuaternionNeighbour(Quaternion quat_a, Quaternion quat_b) {
		// If the dot product between the 2 quaternions is negative, negate b and return it
		Vector8 mask = QuaternionSameHyperspaceMask(quat_a, quat_b);
		return Select(mask, quat_b.value, -quat_b.value);
	}

	// ---------------------------------------------------------------------------------------------------------------

	// If inputs are unit length, output will be unit length as well; assumes neighbour quaternions
	// Similar to a lerp but slightly different
	ECS_INLINE Quaternion ECS_VECTORCALL QuaternionMix(Quaternion from, Quaternion to, Vector8 percentages) {
		Vector8 one_minus_vector = VectorGlobals::ONE - percentages;
		return (Vector8(from.value) * one_minus_vector + Vector8(to.value) * percentages).value;
	}

	// If inputs are unit length, output will be the same; makes neighbour correction as needed
	// Similar to a lerp but slightly different
	ECS_INLINE Quaternion ECS_VECTORCALL QuaternionMixNeighbour(Quaternion from, Quaternion to, Vector8 percentages) {
		to = QuaternionNeighbour(from, to);
		return QuaternionMix(from, to, percentages);
	}

	// ---------------------------------------------------------------------------------------------------------------

	// Assumes neighbour quaternions
	ECS_INLINE Quaternion ECS_VECTORCALL QuaternionNLerp(Quaternion from, Quaternion to, Vector8 percentages) {
		return QuaternionNormalize(Lerp(from.value, to.value, percentages));
	}

	// Makes neighbour correction
	ECS_INLINE Quaternion ECS_VECTORCALL QuaternionNLerpNeighbour(Quaternion from, Quaternion to, Vector8 percentages) {
		to = QuaternionNeighbour(from, to);
		return QuaternionNLerp(from, to, percentages);
	}

	// ---------------------------------------------------------------------------------------------------------------

	ECS_INLINE Quaternion ECS_VECTORCALL QuaternionToPower(Quaternion quaternion, Vector8 power) {
		/*
			Scalar formula
			angle = acos(quaternion.w) // This is the half angle of the quaternion
			halfSin = sin(angle * power)
			halfCos = cos(angle * power)
			axis = Normalize(quaternion.xyz)

			return Quaternion (
				axis.x * halfSin,
				axis.y * halfSin,
				axis.z * halfSin,
				halfCos
			)
		*/

		Vector8 angle = QuaternionHalfAngleSIMD(quaternion);
		// The axis needs to be normalized no matter what
		Vector8 axis = Normalize(quaternion.value);

		Vec8f cos;
		Vector8 sin = sincos(&cos, power * angle);
		axis *= sin;

		return Quaternion(PerLaneBlend<0, 1, 2, 7>(axis, Vector8(cos)));
	}

	// ---------------------------------------------------------------------------------------------------------------

	// Assumes neighbour quaternions
	ECS_INLINE Quaternion ECS_VECTORCALL QuaternionSLerp(Quaternion from, Quaternion to, Vector8 percentages) {
		Quaternion dot = QuaternionDot(from, to);
		Vector8 abs_dot = abs(Vector8(dot.value));
		Vector8 one_epsilon = VectorGlobals::ONE - VectorGlobals::QUATERNION_EPSILON;
		Vector8 mask = abs_dot.value > one_epsilon.value;

		// We have 2 branches - one when the quaternions are close together where
		// slerp can produce unstable result, we fall back to nlerp
		// And the other branch is the classical slerp algorithm

		Quaternion nlerp_quaternion = QuaternionNLerp(from, to, percentages);
		if (horizontal_and(mask.AsMask())) {
			// We can already return
			return nlerp_quaternion;
		}

		Quaternion delta = QuaternionMultiply(QuaternionInverse(from), to);
		Quaternion slerp_quaternion = QuaternionNormalize(QuaternionMultiply(QuaternionToPower(delta, percentages), from));
		return Select(mask, nlerp_quaternion.value, slerp_quaternion.value);
	}

	// Makes the neighbour adjustment
	ECS_INLINE Quaternion ECS_VECTORCALL QuaternionSLerpNeighbour(Quaternion from, Quaternion to, Vector8 percentage) {
		to = QuaternionNeighbour(from, to);
		return QuaternionSLerp(from, to, percentage);
	}

	// ---------------------------------------------------------------------------------------------------------------

	ECS_INLINE Quaternion ECS_VECTORCALL QuaternionLookRotationNormalized(Vector8 direction_normalized, Vector8 up_normalized) {
		Vector8 normalized_right = Cross(up_normalized, direction_normalized); // Object Right

		Vector8 current_up = Cross(direction_normalized, normalized_right); // Object Up
		// From world forward to object forward

		Vector8 default_up = UpVector();
		Vector8 default_forward = ForwardVector();
		Vector8 world_to_object = QuaternionFromVectors(default_forward, direction_normalized);

		// This is the new up that the object wants to have
		Vector8 desired_up = QuaternionVectorMultiply(world_to_object, default_up);

		// From object up to desired up
		Quaternion object_up_to_desired_up = QuaternionFromVectors(desired_up, current_up);

		// Rotate to forward direction first then twist to correct up
		Quaternion result = QuaternionMultiply(world_to_object, object_up_to_desired_up);

		// Normalize the result
		return QuaternionNormalize(result);
	}

	ECS_INLINE Quaternion ECS_VECTORCALL QuaternionLookRotation(Vector8 direction, Vector8 up) {
		Vector8 normalized_direction = Normalize(direction); // Object Forward
		Vector8 normalized_up = Normalize(up); // Desired Up
		return QuaternionLookRotationNormalized(normalized_direction, normalized_up);
	}

	// ---------------------------------------------------------------------------------------------------------------

	template<bool preserve = false>
	ECS_INLINE Vector8 ECS_VECTORCALL RotateVectorQuaternionSIMD(Vector8 vector, Quaternion quaternion) {
		return QuaternionVectorMultiply<preserve>(quaternion, vector);
	}

	template<bool preserve = false>
	ECS_INLINE Vector8 ECS_VECTORCALL RotatePointQuaternionSIMD(Vector8 point, Quaternion quaternion) {
		// We can treat the point as a displacement vector that is rotated
		return RotateVectorQuaternionSIMD<preserve>(quaternion, point);
	}

	// This only rotates the low part
	ECS_INLINE float3 ECS_VECTORCALL RotateVectorQuaternion(Vector8 vector, Quaternion quaternion) {
		return RotateVectorQuaternionSIMD(vector, quaternion).AsFloat3Low();
	}

	ECS_INLINE float3 ECS_VECTORCALL RotatePointQuaternion(Vector8 vector, Quaternion quaternion) {
		// We can treat the point as a displacement vector that is rotated
		return RotatePointQuaternion(vector, quaternion);
	}

	// --------------------------------------------------------------------------------------------------------------

	// Given 2 directions in 3D, A and B, in the same plane, it determines if the
	// A is clockwise or anticlockwise with respect to B. Clockwise it means that
	// A needs to be rotated clockwise with the lesser amount, counterclockwise
	// means that A needs to be rotated counterclockwise with the lesser amount

	// We do this by getting the cross product of the 2 vectors. Using that cross
	// Product, we can create a quaternion that will rotate the A and B vectors
	// into the XY plane. Then we can apply the 2D version of the IsClockwise
	// that is based on the quadrants and doesn't use any trigonometric functions

	// Have specialization for the 2D case

	// The values range from 1 to 4
	ECS_INLINE int GetQuadrant(float2 vector) {
		if (vector.x > 0.0f) {
			if (vector.y > 0.0f) {
				return 1;
			}
			else {
				return 4;
			}
		}
		else {
			if (vector.y > 0.0f) {
				return 2;
			}
			else {
				return 3;
			}
		}
	}

	// If a and b are alredy normalized, you can set the boolean to false such
	// that it doesn't perform a reduntant normalization
	static bool IsClockwise(float2 a, float2 b, bool normalize_late = true) {
		// Here it is much simpler.
		// Get the quadrants of A and B
		int a_quadrant = GetQuadrant(a);
		int b_quadrant = GetQuadrant(b);

		// If in the same quadrant or opposing quadrant, we need a special case
		auto same_quadrant_case = [normalize_late](float2 a, float2 b, int quadrant) {
			if (normalize_late) {
				float4 normalized_values;
				Normalize2x2(float4(a.x, a.y, b.x, b.y)).Store(&normalized_values);
				a = { normalized_values.x, normalized_values.y };
				b = { normalized_values.z, normalized_values.w };
			}

			// TODO: Do we need both checks? If they are already normalized
			// Then we need to perform a single check
			switch (quadrant) {
			case 1:
				// It needs to have the Y component bigger and the X smaller
				return a.y > b.y;
				break;
			case 2:
				// It needs to have the Y component smaller and the X smaller
				return a.y < b.y;
				break;
			case 3:
				// It needs to have the Y component smaller and the X bigger
				return a.y < b.y;
				break;
			case 4:
				// It needs to have the Y component bigger and the X bigger
				return a.y > b.y;
				break;
			}
		};

		if (a_quadrant == b_quadrant) {
			return same_quadrant_case(a, b, a_quadrant);
		}
		else {
			int difference = abs(a_quadrant - b_quadrant);
			if (difference == 1) {
				// Neighbour quadrants
				// Return true if the a_quadrant is bigger than b
				return a_quadrant > b_quadrant;
			}
			else if (difference == 2) {
				// This is the negation of the same quadrant case
				// With the b being negated
				return !same_quadrant_case(a, -b, a_quadrant);
			}
			else if (difference == 3) {
				// Neighbour quadrants
				// One is 1 and the other one is 4
				// If a is in the first quadrant, then it needs to rotate clockwise
				return a_quadrant == 1;
			}
		}

		// Shouldn't reach here
		return false;
	}

	// The values need to be normalized before that
	ECS_INLINE bool IsCounterClockwise(float2 a_normalized, float2 b_normalized) {
		return !IsClockwise(a_normalized, b_normalized);
	}
	
	// The vectors don't need to be normalized
	ECS_INLINE bool2 ECS_VECTORCALL IsClockwise(Vector8 a, Vector8 b) {
		Vector8 cross = Cross(a, b);
		// The cross product needs to be normalized here
		cross = Normalize(cross);
		// We need to transform the cross product to the forward vector in order
		// to bring A and B into the XY plane
		Quaternion from_cross_to_YZ = QuaternionFromVectorsNormalized(cross, ForwardVector());
		Vector8 a_xy = RotateVectorQuaternionSIMD(a, from_cross_to_YZ);
		Vector8 b_xy = RotateVectorQuaternionSIMD(b, from_cross_to_YZ);

		float4 values[4];
		a_xy.Store(values);
		b_xy.Store(values + 2);

		float2 a0_2d = values[0].xy();
		float2 a1_2d = values[1].xy();
		float2 b0_2d = values[2].xy();
		float2 b1_2d = values[3].xy();
		return bool2(IsClockwise(a0_2d, b0_2d), IsClockwise(a1_2d, b1_2d));
	}

	// The vectors don't need to be normalized
	ECS_INLINE bool IsClockwiseLow(Vector8 a, Vector8 b) {
		return IsClockwise(a, b).x;
	}

	// The vectors don't need to be normalized
	ECS_INLINE bool IsClockwiseBoth(Vector8 a, Vector8 b) {
		return BasicTypeLogicAndBoolean(IsClockwise(a, b));
	}

	// The vectors don't need to be normalized
	ECS_INLINE bool2 IsCounterClockwise(Vector8 a, Vector8 b) {
		return !IsClockwise(a, b);
	}

	// The vectors don't need to be normalized
	ECS_INLINE bool IsCounterClockwiseLow(Vector8 a, Vector8 b) {
		return !IsClockwiseLow(a, b);
	}

	// The vectors don't need to be normalized
	ECS_INLINE bool IsCounterClockwiseBoth(Vector8 a, Vector8 b) {
		return !IsClockwiseBoth(a, b);
	}

	// --------------------------------------------------------------------------------------------------------------
	
	namespace SIMDHelpers {

		// There needs to be a fixup for the Y value
		// Use the function FinishEulerRotation to finalize it
		ECS_INLINE Vector8 ECS_VECTORCALL QuaternionToEulerRadHelper(Quaternion quaternion) {
			// The formula is this one
			// x_factor_first = qw * qx + qy * qz
			// x_factor_second = qx * qx + qy * qy
			// z_factor_first = qw * qz + qx * qy
			// z_factor_second = qy * qy + qz * qz
			// y_factor = qw * qy - qx * qz
			// 
			// x_rotation = atan2(2 * x_factor_first, 1 - 2 * x_factor_second)
			// y rotation = -PI/2 + 2 * atan2(sqrt(1 + 2 * y_factor), sqrt(1 - 2 * y_factor))
			// z_rotation = atan2(2 * z_factor_first, 1 - 2 * z_factor_second)

			Vector8 one = VectorGlobals::ONE;
			Vector8 two = Vector8(2.0f);

			Vector8 y_factor_first = PerLanePermute<2, 3, V_DC, V_DC>(quaternion.value);
			Vector8 y_factor_multiplied = quaternion.value * y_factor_first;
			// Now we have in the x and y of the y_factor_multiplied the values
			Vector8 y_factor_multiplied_shuffle = PerLanePermute<V_DC, 0, V_DC, V_DC>(y_factor_multiplied);
			// Just the y component has the value
			Vector8 y_factor = y_factor_multiplied - y_factor_multiplied_shuffle;
			// Just the y component has the value
			Vector8 sqrt_first = one + two * y_factor;
			Vector8 sqrt_second = one - two * y_factor;
			Vector8 sqrt_values = PerLaneBlend<1, 5, V_DC, V_DC>(sqrt_first, sqrt_second);
			// We now have the values for the atan2 in the x and y
			sqrt_values = sqrt(sqrt_values);

			// Now calculate the y_factors and z_factors
			Vector8 first_multiply_element = PerLanePermute<3, 0, 3, 1>(quaternion.value);
			Vector8 second_multiply_element = PerLanePermute<0, 0, 2, 1>(quaternion.value);
			Vector8 third_multiply_element = PerLanePermute<1, 1, 0, 2>(quaternion.value);
			Vector8 fourth_multiply_element = PerLanePermute<2, 1, 1, 2>(quaternion.value);

			Vector8 first_multiplication = first_multiply_element * second_multiply_element;
			Vector8 second_multiplication = third_multiply_element * fourth_multiply_element;
			// The x factors are in the x and y components, while the z ones are in the z and w
			Vector8 xz_factors = first_multiplication + second_multiplication;

			Vector8 double_xz_factors = two * xz_factors;
			Vector8 xz_factor_second = one - double_xz_factors;

			Vector8 xz_atan_numerators = PerLanePermute<0, V_DC, 2, V_DC>(double_xz_factors);
			Vector8 xz_atan_denominators = PerLanePermute<1, V_DC, 3, V_DC>(xz_factor_second);

			// Now we can finally bridge all values into atan numerators and denominators
			Vector8 atan_numerators = PerLaneBlend<0, 4, 2, V_DC>(xz_atan_numerators, sqrt_values);
			Vector8 atan_denominators = PerLaneBlend<0, 5, 2, V_DC>(xz_atan_denominators, sqrt_values);

			return atan2(atan_numerators, atan_denominators);
		}

		// This can be used to finish the transformation into euler rotation from quaternion
		ECS_INLINE float3 FinishEulerRotation(float3 euler_rotation) {
			euler_rotation.y = euler_rotation.y * 2.0f - PI * 0.5f;
			return euler_rotation;
		}

	}

	ECS_INLINE float3 ECS_VECTORCALL QuaternionToEulerRadLow(Quaternion quaternion) {
		Vector8 unfinished_rotation = SIMDHelpers::QuaternionToEulerRadHelper(quaternion);
		return SIMDHelpers::FinishEulerRotation(unfinished_rotation.AsFloat3Low());
	}

	ECS_INLINE void ECS_VECTORCALL QuaternionToEulerRad(Quaternion quaternion, float3* low, float3* high) {
		Vector8 unfinished_rotation = SIMDHelpers::QuaternionToEulerRadHelper(quaternion);
		unfinished_rotation.StoreFloat3(low, high);
	}

	ECS_INLINE float3 ECS_VECTORCALL QuaternionToEulerLow(Quaternion quaternion) {
		return RadToDeg(QuaternionToEulerRadLow(quaternion));
	}

	ECS_INLINE void ECS_VECTORCALL QuaternionToEuler(Quaternion quaternion, float3* low, float3* high) {
		QuaternionToEulerRad(quaternion, low, high);
		*low = RadToDeg(*low);
		*high = RadToDeg(*high);
	}

	// --------------------------------------------------------------------------------------------------------------

	// Add a rotation with respect to the local axes of the object
	ECS_INLINE Quaternion ECS_VECTORCALL AddLocalRotation(Quaternion current_rotation, Quaternion add_rotation) {
		return current_rotation * add_rotation;
	}

	// Add a rotation with respect to the global axes of the object
	ECS_INLINE Quaternion ECS_VECTORCALL AddWorldRotation(Quaternion current_rotation, Quaternion add_rotation) {
		return add_rotation * current_rotation;
	}

	// --------------------------------------------------------------------------------------------------------------

	// Converts the given direction into a quaternion
	ECS_INLINE Quaternion DirectionToQuaternion(Vector8 direction_normalized) {
		Vector8 up_vector = GetUpVectorForDirection(direction_normalized);
		return QuaternionLookRotationNormalized(direction_normalized, up_vector);
	}

	// --------------------------------------------------------------------------------------------------------------

	ECS_INLINE Quaternion ECS_VECTORCALL QuaternionAverageCumulatorInitialize() {
		return ZeroVector();
	}

	// The reference quaternion is used to changed the sign of the current quaternion if they are in different hyperplanes
	// It needs to be the same value across multiple cumulations. Usually, it is the first quaternion that is being cumulated
	// This only does the cumulative step
	ECS_INLINE void ECS_VECTORCALL QuaternionAddToAverageStep(Quaternion* cumulator, Quaternion current_quaternion) {
		// Use some default value for the reference quaternion
		Quaternion reference_quaternion = QuaternionForAxisX(0.0f);
		Quaternion neighbour_quaternion = QuaternionNeighbour(reference_quaternion, current_quaternion);
		*cumulator += neighbour_quaternion;
	}

	ECS_INLINE Quaternion ECS_VECTORCALL QuaternionAverageFromCumulator(Quaternion cumulator, unsigned int count) {
		Vector8 float_count = (float)count;
		Vector8 count_inverse = OneDividedVector<ECS_VECTOR_ACCURATE>(float_count);

		Vector8 average_values = cumulator.value * count_inverse;
		return QuaternionNormalize(average_values);
	}

	// The reference quaternion is used to changed the sign of the current quaternion if they are in different hyperplanes
	// It needs to be the same value across multiple cumulations. Usually, it is the first quaternion that is being cumulated
	ECS_INLINE Quaternion ECS_VECTORCALL QuaternionAddToAverage(Quaternion* cumulator, Quaternion current_quaternion, unsigned int count) {
		QuaternionAddToAverageStep(cumulator, current_quaternion);
		return QuaternionAverageFromCumulator(*cumulator, count);
	}

	// Call this function only for 2 quaternions - otherwise use the cumulative version
	ECS_INLINE Quaternion ECS_VECTORCALL QuaternionAverage2(Quaternion a, Quaternion b) {
		return QuaternionSLerpNeighbour(a, b, 0.5f);
	}

	// Returns the average between the low and high lane
	// It will be splatted in the low and high as well
	ECS_INLINE Quaternion ECS_VECTORCALL QuaternionAverageLowAndHigh(Quaternion average) {
		Quaternion low = average.SplatLow();
		Quaternion high = average.SplatHigh();
		return QuaternionAverage2(low, high);
	}

	// --------------------------------------------------------------------------------------------------------------

}