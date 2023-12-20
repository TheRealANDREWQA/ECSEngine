#pragma once
#include "../Core.h"
#include "../Utilities/BasicTypes.h"
#include "VCLExtensions.h"
#include "Vector.h"

#define ECS_QUATERNION_EPSILON (0.00005f)

namespace ECSEngine {

	namespace VectorGlobals {
		ECSENGINE_API extern Vector8 QUATERNION_EPSILON;
	}

	struct Quaternion;

	ECSENGINE_API Quaternion ECS_VECTORCALL QuaternionMultiply(Quaternion a, Quaternion b);
	ECSENGINE_API Quaternion ECS_VECTORCALL QuaternionFromEuler(float3 rotation);
	ECSENGINE_API Quaternion ECS_VECTORCALL QuaternionFromEuler(float3 rotation0, float3 rotation1);
	float3 ECS_VECTORCALL QuaternionToEulerLow(Quaternion quaternion);
	float3 ECS_VECTORCALL QuaternionToEulerHigh(Quaternion quaternion);

	// When wanting to store a quaternion value into structures but you don't want
	// to store the SIMD quaternion is this value
	typedef float4 QuaternionStorage;

	struct ECSENGINE_API Quaternion {
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

		bool2 ECS_VECTORCALL operator == (Quaternion other) const;

		bool2 ECS_VECTORCALL operator != (Quaternion other) const;

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

		ECS_INLINE void Store(void* destination) const {
			value.store((float*)destination);
		}

		Vec8f value;
	};

	// ---------------------------------------------------------------------------------------------------------------

	ECS_INLINE Quaternion ECS_VECTORCALL QuaternionIdentity() {
		return LastElementOneVector();
	}

	// ---------------------------------------------------------------------------------------------------------------

	ECS_INLINE Vector8 ECS_VECTORCALL QuaternionCompareMask(Quaternion a, Quaternion b, Vector8 epsilon = VectorGlobals::QUATERNION_EPSILON) {
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
	ECSENGINE_API Quaternion ECS_VECTORCALL QuaternionAngleFromAxisRad(Vector8 vector_axis, Vector8 angle_radians);

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
	ECSENGINE_API Quaternion ECS_VECTORCALL QuaternionFromEulerRad(float3 rotation_radians0, float3 rotation_radians1);

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
	ECSENGINE_API Quaternion ECS_VECTORCALL QuaternionFromVectorsNormalized(Vector8 from_vector_normalized, Vector8 to_vector_normalized);

	// If the vectors are already normalized you can call directly the other function
	ECSENGINE_API Quaternion ECS_VECTORCALL QuaternionFromVectors(Vector8 from, Vector8 to);

	// ---------------------------------------------------------------------------------------------------------------

	// The quaternion needs to be normalized
	ECSENGINE_API void ECS_VECTORCALL QuaternionAxisNormalized(Quaternion quaternion_normalized, float3* axis_low, float3* axis_high);

	// The quaternion needs to be normalized
	ECS_INLINE float3 ECS_VECTORCALL QuaternionAxisNormalized(Quaternion quaternion_normalized) {
		Vector8 vector_axis(quaternion_normalized.value);
		return vector_axis.AsFloat3Low();
	}

	// If the quaternion is normalized use the other variant
	ECSENGINE_API void ECS_VECTORCALL QuaternionAxis(Quaternion quaternion, float3* axis_low, float3* axis_high);

	// If the quaternion is normalized use the other variant
	ECSENGINE_API float3 ECS_VECTORCALL QuaternionAxis(Quaternion quaternion);

	// ---------------------------------------------------------------------------------------------------------------

	// The angle is in radians
	ECSENGINE_API float ECS_VECTORCALL QuaternionAngleRadLow(Quaternion quaternion);

	// The angle is in radians
	ECSENGINE_API float2 ECS_VECTORCALL QuaternionAngleRad(Quaternion quaternion);

	// The angle is in radians
	// The angle value is splatted across the lane
	ECSENGINE_API Vector8 ECS_VECTORCALL QuaternionHalfAngleRadSIMD(Quaternion quaternion);

	// The angle is in radians
	// The angle value is splatted across the lane
	ECSENGINE_API Vector8 ECS_VECTORCALL QuaternionAngleRadSIMD(Quaternion quaternion);

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

	ECSENGINE_API Vector8 ECS_VECTORCALL QuaternionSameOrientationMask(Quaternion a, Quaternion b);

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

	ECSENGINE_API Quaternion ECS_VECTORCALL QuaternionConjugate(Quaternion quaternion);

	// ---------------------------------------------------------------------------------------------------------------

	ECS_INLINE Quaternion ECS_VECTORCALL QuaternionInverseNormalized(Quaternion quaternion) {
		// The conjugate and the inverse are the same if the quaternion is normalized;
		// Avoids a division, a square root and a dot product
		return QuaternionConjugate(quaternion);
	}

	ECSENGINE_API Quaternion ECS_VECTORCALL QuaternionInverse(Quaternion quaternion);

	// ---------------------------------------------------------------------------------------------------------------

	ECSENGINE_API Quaternion ECS_VECTORCALL QuaternionMultiply(Quaternion a, Quaternion b);

	// ---------------------------------------------------------------------------------------------------------------

	// Can choose to preserve the 4th component from the original or not
	template<bool preserve = false>
	ECSENGINE_API Vector8 ECS_VECTORCALL QuaternionVectorMultiply(Quaternion quaternion, Vector8 vector);

	// ---------------------------------------------------------------------------------------------------------------

	// Returns the negated quaternion if they are in different hyperspaces
	ECSENGINE_API Quaternion ECS_VECTORCALL QuaternionNeighbour(Quaternion quat_a, Quaternion quat_b);

	// ---------------------------------------------------------------------------------------------------------------

	// If inputs are unit length, output will be unit length as well; assumes neighbour quaternions
	// Similar to a lerp but slightly different
	ECSENGINE_API Quaternion ECS_VECTORCALL QuaternionMix(Quaternion from, Quaternion to, Vector8 percentages);

	// If inputs are unit length, output will be the same; makes neighbour correction as needed
	// Similar to a lerp but slightly different
	ECSENGINE_API Quaternion ECS_VECTORCALL QuaternionMixNeighbour(Quaternion from, Quaternion to, Vector8 percentages);

	// ---------------------------------------------------------------------------------------------------------------

	// Assumes neighbour quaternions
	ECSENGINE_API Quaternion ECS_VECTORCALL QuaternionNLerp(Quaternion from, Quaternion to, Vector8 percentages);

	// Makes neighbour correction
	ECSENGINE_API Quaternion ECS_VECTORCALL QuaternionNLerpNeighbour(Quaternion from, Quaternion to, Vector8 percentages);

	// ---------------------------------------------------------------------------------------------------------------

	ECSENGINE_API Quaternion ECS_VECTORCALL QuaternionToPower(Quaternion quaternion, Vector8 power);

	// ---------------------------------------------------------------------------------------------------------------

	// Assumes neighbour quaternions
	ECSENGINE_API Quaternion ECS_VECTORCALL QuaternionSLerp(Quaternion from, Quaternion to, Vector8 percentages);

	// Makes the neighbour adjustment
	ECSENGINE_API Quaternion ECS_VECTORCALL QuaternionSLerpNeighbour(Quaternion from, Quaternion to, Vector8 percentage);

	// ---------------------------------------------------------------------------------------------------------------

	ECSENGINE_API Quaternion ECS_VECTORCALL QuaternionLookRotationNormalized(Vector8 direction_normalized, Vector8 up_normalized);

	ECSENGINE_API Quaternion ECS_VECTORCALL QuaternionLookRotation(Vector8 direction, Vector8 up);

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

	// The values range from 1 to 4
	ECSENGINE_API int GetQuadrant(float2 vector);

	// If a and b are already normalized, you can set the boolean to false such
	// that it doesn't perform a reduntant normalization
	ECSENGINE_API bool IsClockwise(float2 a, float2 b, bool normalize_late = true);

	// If a and b are already normalized, you can set the boolean to false such
	// that it doesn't perform a reduntant normalization
	ECS_INLINE bool IsCounterClockwise(float2 a, float2 b, bool normalize_late = true) {
		return !IsClockwise(a, b, normalize_late);
	}

	// The vectors don't need to be normalized
	ECSENGINE_API bool2 ECS_VECTORCALL IsClockwise(Vector8 a, Vector8 b);

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

	ECSENGINE_API float3 ECS_VECTORCALL QuaternionToEulerRadLow(Quaternion quaternion);

	ECSENGINE_API void ECS_VECTORCALL QuaternionToEulerRad(Quaternion quaternion, float3* low, float3* high);

	ECS_INLINE float3 ECS_VECTORCALL QuaternionToEulerLow(Quaternion quaternion) {
		return RadToDeg(QuaternionToEulerRadLow(quaternion));
	}

	ECS_INLINE float3 ECS_VECTORCALL QuaternionToEulerHigh(Quaternion quaternion) {
		float3 low; float3 high;
		QuaternionToEulerRad(quaternion, &low, &high);
		return RadToDeg(high);
	}

	ECSENGINE_API void ECS_VECTORCALL QuaternionToEuler(Quaternion quaternion, float3* low, float3* high);

	// --------------------------------------------------------------------------------------------------------------

	// Add a rotation with respect to the local axes of the object
	ECS_INLINE Quaternion ECS_VECTORCALL AddLocalRotation(Quaternion current_rotation, Quaternion add_rotation) {
		return current_rotation * add_rotation;
	}

	// Add a rotation with respect to the global axes of the object
	ECS_INLINE Quaternion ECS_VECTORCALL AddWorldRotation(Quaternion current_rotation, Quaternion add_rotation) {
		return add_rotation * current_rotation;
	}

	// This computes the delta between a known operand and a final quaternion
	// delta * operand = final_quaternion -> delta = final_quaternion * inverse(operand)
	ECS_INLINE Quaternion ECS_VECTORCALL QuaternionDelta(Quaternion operand, Quaternion final_quaternion) {
		return final_quaternion * QuaternionInverse(operand);
	}

	// This computes the delta between a known operand and a final quaternion
	// delta * operand = final_quaternion -> delta = final_quaternion * inverse(operand)
	ECS_INLINE Quaternion ECS_VECTORCALL QuaternionDeltaNormalized(Quaternion operand_normalized, Quaternion final_quaternion) {
		return final_quaternion * QuaternionInverseNormalized(operand_normalized);
	}

	// --------------------------------------------------------------------------------------------------------------

	// Converts the given direction into a quaternion
	ECSENGINE_API Quaternion ECS_VECTORCALL DirectionToQuaternion(Vector8 direction_normalized);

	// --------------------------------------------------------------------------------------------------------------

	ECS_INLINE Quaternion ECS_VECTORCALL QuaternionAverageCumulatorInitialize() {
		return ZeroVector();
	}

	// The reference quaternion is used to changed the sign of the current quaternion if they are in different hyperplanes
	// It needs to be the same value across multiple cumulations. Usually, it is the first quaternion that is being cumulated
	// This only does the cumulative step
	ECSENGINE_API void ECS_VECTORCALL QuaternionAddToAverageStep(Quaternion* cumulator, Quaternion current_quaternion);

	ECSENGINE_API Quaternion ECS_VECTORCALL QuaternionAverageFromCumulator(Quaternion cumulator, unsigned int count);

	// The reference quaternion is used to changed the sign of the current quaternion if they are in different hyperplanes
	// It needs to be the same value across multiple cumulations. Usually, it is the first quaternion that is being cumulated
	ECSENGINE_API Quaternion ECS_VECTORCALL QuaternionAddToAverage(Quaternion* cumulator, Quaternion current_quaternion, unsigned int count);

	// Call this function only for 2 quaternions - otherwise use the cumulative version
	ECS_INLINE Quaternion ECS_VECTORCALL QuaternionAverage2(Quaternion a, Quaternion b) {
		return QuaternionSLerpNeighbour(a, b, 0.5f);
	}

	// Returns the average between the low and high lane
	// It will be splatted in the low and high as well
	ECSENGINE_API Quaternion ECS_VECTORCALL QuaternionAverageLowAndHigh(Quaternion average);

	// --------------------------------------------------------------------------------------------------------------

}