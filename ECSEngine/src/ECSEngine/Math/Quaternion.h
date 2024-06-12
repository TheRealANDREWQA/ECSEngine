#pragma once
#include "../Core.h"
#include "../Utilities/BasicTypes.h"
#include "VCLExtensions.h"
#include "Vector.h"

#define ECS_QUATERNION_EPSILON (0.00005f)

namespace ECSEngine {

	namespace VectorGlobals {
		ECSENGINE_API extern Vec8f QUATERNION_EPSILON;
	}

	struct QuaternionScalar;

	ECSENGINE_API QuaternionScalar QuaternionMultiply(QuaternionScalar a, QuaternionScalar b);

	struct ECSENGINE_API QuaternionScalar {
		ECS_INLINE QuaternionScalar() {}
		ECS_INLINE QuaternionScalar(const float* values) : x(values[0]), y(values[1]), z(values[2]), w(values[3]) {}
		ECS_INLINE QuaternionScalar(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}
		ECS_INLINE QuaternionScalar(float4 _value) {
			x = _value.x;
			y = _value.y;
			z = _value.z;
			w = _value.w;
		}
		ECS_INLINE QuaternionScalar(float3 axis, float angle) : QuaternionScalar(axis.x, axis.y, axis.z, angle) {}

		ECS_INLINE QuaternionScalar(const QuaternionScalar& other) = default;
		ECS_INLINE QuaternionScalar& operator = (const QuaternionScalar& other) = default;

		ECS_INLINE bool operator == (QuaternionScalar other) const {
			return x == other.x && y == other.y && z == other.z && w == other.w;
		}

		ECS_INLINE bool operator != (QuaternionScalar other) const {
			return !(*this == other);
		}

		ECS_INLINE float& operator [](size_t index) {
			return ((float*)this)[index];
		}

		ECS_INLINE const float& operator [](size_t index) const {
			return ((const float*)this)[index];
		}

		ECS_INLINE operator float4() const {
			return { x, y, z, w };
		}

		ECS_INLINE QuaternionScalar operator + (QuaternionScalar other) const {
			return { x + other.x, y + other.y, z + other.z, w + other.w };
		}

		ECS_INLINE QuaternionScalar operator - (QuaternionScalar other) const {
			return { x - other.x, y - other.y, z - other.z, w - other.w };
		}

		ECS_INLINE QuaternionScalar operator * (float scalar) const {
			return { x * scalar, y * scalar, z * scalar, w * scalar };
		}

		ECS_INLINE QuaternionScalar operator * (QuaternionScalar other) const {
			return QuaternionMultiply(*this, other);
		}

		ECS_INLINE QuaternionScalar operator -() const {
			return { -x, -y, -z, -w };
		}

		ECS_INLINE QuaternionScalar& operator += (QuaternionScalar other) {
			*this = *this + other;
			return *this;
		}

		ECS_INLINE QuaternionScalar& operator -= (QuaternionScalar other) {
			*this = *this - other;
			return *this;
		}

		ECS_INLINE QuaternionScalar& operator *= (float scalar) {
			*this = *this * scalar;
			return *this;
		}

		ECS_INLINE QuaternionScalar& operator *= (QuaternionScalar other) {
			*this = *this * other;
			return *this;
		}

		ECS_INLINE float2 xy() const {
			return { x, y };
		}

		ECS_INLINE float3 xyz() const {
			return { x, y, z };
		}

		ECS_INLINE static QuaternionScalar Splat(float value) {
			return { value, value, value, value };
		}

		float x;
		float y;
		float z;
		float w;
	};

	struct ECSENGINE_API Quaternion : public Vector4 {
		ECS_INLINE Quaternion() {}
		ECS_INLINE Quaternion(const float4* values) {
			Gather(values);
		}
		ECS_INLINE Quaternion(Vec8f x, Vec8f y, Vec8f z, Vec8f w) : Vector4(x, y, z, w) {}
		ECS_INLINE Quaternion(Vector4 _value) {
			x = _value.x;
			y = _value.y;
			z = _value.y;
			w = _value.w;
		}
		ECS_INLINE Quaternion(Vector3 axis, Vec8f angle) : Vector4(axis.x, axis.y, axis.z, angle) {}

		ECS_INLINE Quaternion(const Quaternion& other) = default;
		ECS_INLINE Quaternion& ECS_VECTORCALL operator = (const Quaternion& other) = default;

		bool ECS_VECTORCALL operator == (Quaternion other) const;

		bool ECS_VECTORCALL operator != (Quaternion other) const;

		ECS_INLINE Vec8f& operator [](size_t index) {
			return ((Vec8f*)this)[index];
		}

		ECS_INLINE const Vec8f& operator [](size_t index) const {
			return ((const Vec8f*)this)[index];
		}

		Quaternion ECS_VECTORCALL operator + (Quaternion other) const;

		Quaternion ECS_VECTORCALL operator - (Quaternion other) const;

		Quaternion ECS_VECTORCALL operator * (float scalar) const;

		Quaternion ECS_VECTORCALL operator * (Quaternion other) const;

		Quaternion ECS_VECTORCALL operator -() const;

		Quaternion& ECS_VECTORCALL operator += (Quaternion other);

		Quaternion& ECS_VECTORCALL operator -= (Quaternion other);

		Quaternion& ECS_VECTORCALL operator *= (float scalar);

		Quaternion& ECS_VECTORCALL operator *= (Quaternion other);

		QuaternionScalar ECS_VECTORCALL At(size_t index) const;

		// Changes a value at a certain index
		void Set(QuaternionScalar scalar_quat, size_t index);
	};

	// ---------------------------------------------------------------------------------------------------------------

	ECS_INLINE Quaternion ECS_VECTORCALL QuaternionIdentity() {
		return { ZeroVectorFloat(), ZeroVectorFloat(), ZeroVectorFloat(), VectorGlobals::ONE };
	}

	ECS_INLINE QuaternionScalar QuaternionIdentityScalar() {
		return QuaternionScalar(0.0f, 0.0f, 0.0f, 1.0f);
	}

	// ---------------------------------------------------------------------------------------------------------------

	ECS_INLINE bool QuaternionCompareMask(QuaternionScalar a, QuaternionScalar b, float epsilon = ECS_QUATERNION_EPSILON) {
		return CompareMask(a, b, float4::Splat(epsilon));
	}

	ECS_INLINE SIMDVectorMask ECS_VECTORCALL QuaternionCompareMask(Quaternion a, Quaternion b, Vec8f epsilon = VectorGlobals::QUATERNION_EPSILON) {
		return CompareMask(a, b, epsilon);
	}

	// ---------------------------------------------------------------------------------------------------------------

	ECS_INLINE float QuaternionDot(QuaternionScalar a, QuaternionScalar b) {
		return Dot(a, b);
	}

	ECS_INLINE Vec8f ECS_VECTORCALL QuaternionDot(Quaternion a, Quaternion b) {
		return Dot(a, b);
	}

	ECS_INLINE float QuaternionSquaredLength(QuaternionScalar quaternion) {
		return QuaternionDot(quaternion, quaternion);
	}

	ECS_INLINE Vec8f ECS_VECTORCALL QuaternionSquaredLength(Quaternion quaternion) {
		return QuaternionDot(quaternion, quaternion);
	}

	ECS_INLINE float QuaternionLength(QuaternionScalar quaternion) {
		return Length(quaternion);
	}

	ECS_INLINE Vec8f ECS_VECTORCALL QuaternionLength(Quaternion quaternion) {
		return Length(quaternion);
	}

	// ---------------------------------------------------------------------------------------------------------------

	template<ECS_VECTOR_PRECISION precision = ECS_VECTOR_PRECISE>
	ECS_INLINE QuaternionScalar QuaternionNormalize(QuaternionScalar quaternion) {
		return Normalize<precision>(quaternion);
	}

	template<ECS_VECTOR_PRECISION precision = ECS_VECTOR_PRECISE>
	ECS_INLINE Quaternion ECS_VECTORCALL QuaternionNormalize(Quaternion quaternion) {
		return Normalize<precision>(quaternion);
	}

	ECS_INLINE bool QuaternionIsNormalizedMask(QuaternionScalar quaternion) {
		return IsNormalizedSquareLengthMask(QuaternionSquaredLength(quaternion));
	}

	ECS_INLINE SIMDVectorMask ECS_VECTORCALL QuaternionIsNormalizedMask(Quaternion quaternion) {
		return IsNormalizedSquareLengthMask(QuaternionSquaredLength(quaternion));
	}

	// ---------------------------------------------------------------------------------------------------------------

	// Angle must be expressed in radians
	ECSENGINE_API QuaternionScalar QuaternionAngleFromAxisRad(float3 vector_axis, float angle_radians);

	ECS_INLINE QuaternionScalar QuaternionAngleFromAxis(float3 vector_axis, float angle_degrees) {
		return QuaternionAngleFromAxisRad(vector_axis, DegToRad(angle_degrees));
	}

	// Angle must be expressed in radians
	ECSENGINE_API Quaternion ECS_VECTORCALL QuaternionAngleFromAxisRad(Vector3 vector_axis, Vec8f angle_radians);

	ECS_INLINE Quaternion ECS_VECTORCALL QuaternionAngleFromAxis(Vector3 vector_axis, Vec8f angle_degrees) {
		return QuaternionAngleFromAxisRad(vector_axis, DegToRad(angle_degrees));
	}

	// ---------------------------------------------------------------------------------------------------------------

	ECS_INLINE QuaternionScalar QuaternionForAxisXRadScalar(float angle_radians) {
		return QuaternionAngleFromAxisRad(GetRightVector(), angle_radians);
	}

	ECS_INLINE Quaternion ECS_VECTORCALL QuaternionForAxisXRad(Vec8f angle_radians) {
		return QuaternionAngleFromAxisRad(RightVector(), angle_radians);
	}

	ECS_INLINE QuaternionScalar QuaternionForAxisYRadScalar(float angle_radians) {
		return QuaternionAngleFromAxisRad(GetUpVector(), angle_radians);
	}

	ECS_INLINE Quaternion ECS_VECTORCALL QuaternionForAxisYRad(Vec8f angle_radians) {
		return QuaternionAngleFromAxisRad(UpVector(), angle_radians);
	}

	ECS_INLINE QuaternionScalar QuaternionForAxisZRadScalar(float angle_radians) {
		return QuaternionAngleFromAxisRad(GetForwardVector(), angle_radians);
	}

	ECS_INLINE Quaternion ECS_VECTORCALL QuaternionForAxisZRad(Vec8f angle_radians) {
		return QuaternionAngleFromAxisRad(ForwardVector(), angle_radians);
	}

	ECS_INLINE QuaternionScalar QuaternionForAxisXScalar(float angle_degrees) {
		return QuaternionForAxisXRadScalar(DegToRad(angle_degrees));
	}

	ECS_INLINE Quaternion ECS_VECTORCALL QuaternionForAxisX(Vec8f angle_degrees) {
		return QuaternionForAxisXRad(DegToRad(angle_degrees));
	}

	ECS_INLINE QuaternionScalar QuaternionForAxisYScalar(float angle_degrees) {
		return QuaternionForAxisYRadScalar(DegToRad(angle_degrees));
	}

	ECS_INLINE Quaternion ECS_VECTORCALL QuaternionForAxisY(Vec8f angle_degrees) {
		return QuaternionForAxisYRad(DegToRad(angle_degrees));
	}

	ECS_INLINE QuaternionScalar QuaternionForAxisZScalar(float angle_degrees) {
		return QuaternionForAxisZRadScalar(DegToRad(angle_degrees));
	}

	ECS_INLINE Quaternion ECS_VECTORCALL QuaternionForAxisZ(Vec8f angle_degrees) {
		return QuaternionForAxisZRad(DegToRad(angle_degrees));
	}

	// ---------------------------------------------------------------------------------------------------------------

	// The angles must be expressed in radians
	ECSENGINE_API QuaternionScalar QuaternionFromEulerRad(float3 rotation_radians);

	// The angles must be expressed in radians
	ECSENGINE_API Quaternion ECS_VECTORCALL QuaternionFromEulerRad(Vector3 rotation_radians);

	// The angles must be expressed in degrees
	ECS_INLINE QuaternionScalar ECS_VECTORCALL QuaternionFromEuler(float3 rotation_degrees) {
		return QuaternionFromEulerRad(DegToRad(rotation_degrees));
	}

	// The angles must be expressed in degrees
	ECS_INLINE Quaternion ECS_VECTORCALL QuaternionFromEuler(Vector3 rotation_degrees) {
		return QuaternionFromEulerRad(DegToRad(rotation_degrees));
	}

	// ---------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region From Vectors

	// ---------------------------------------------------------------------------------------------------------------

	// Expects the from vector and the to vector to be normalized
	ECSENGINE_API QuaternionScalar QuaternionFromVectorsNormalized(float3 from_vector_normalized, float3 to_vector_normalized);

	// Expects the from vector and the to vector to be normalized. This version is identical to the other scalar one,
	// The reason for which it exists is that, if you want to write agnostic algorithms with this function there is a
	// signature mismatch with the SIMD variant
	ECS_INLINE QuaternionScalar QuaternionFromVectorsNormalized(float3 from_vector_normalized, float3 to_vector_normalized, size_t vector_count) {
		return QuaternionFromVectorsNormalized(from_vector_normalized, to_vector_normalized);
	}

	// Expects the from vector and the to vector to be normalized
	ECSENGINE_API Quaternion ECS_VECTORCALL QuaternionFromVectorsNormalized(Vector3 from_vector_normalized, Vector3 to_vector_normalized, size_t vector_count);

	// If the vectors are already normalized you can call directly the other function
	ECSENGINE_API QuaternionScalar QuaternionFromVectors(float3 from, float3 to);

	// If the vectors are already normalized you can call directly the other function. It exists to match the SIMD
	// variant for agnostic calls
	ECS_INLINE QuaternionScalar QuaternionFromVectors(float3 from, float3 to, size_t vector_count) {
		return QuaternionFromVectors(from, to);
	}

	// If the vectors are already normalized you can call directly the other function
	ECSENGINE_API Quaternion ECS_VECTORCALL QuaternionFromVectors(Vector3 from, Vector3 to, size_t vector_count);

	// ---------------------------------------------------------------------------------------------------------------
	
	// The quaternion needs to be normalized
	// The returned axis is normalized
	ECSENGINE_API float3 QuaternionAxisNormalized(QuaternionScalar quaternion_normalized);

	// The quaternion needs to be normalized
	// The returned axis is normalized
	ECSENGINE_API Vector3 ECS_VECTORCALL QuaternionAxisNormalized(Quaternion quaternion_normalized);

	// If the quaternion is normalized use the other variant
	// The returned axis is normalized
	ECSENGINE_API float3 QuaternionAxis(QuaternionScalar quaternion);

	// If the quaternion is normalized use the other variant
	// The returned axis is normalized
	ECSENGINE_API Vector3 ECS_VECTORCALL QuaternionAxis(Quaternion quaternion);

	// ---------------------------------------------------------------------------------------------------------------

	// The angle is in radians
	ECSENGINE_API float QuaternionAngleRad(QuaternionScalar quaternion);

	// The angle is in radians
	ECSENGINE_API Vec8f ECS_VECTORCALL QuaternionAngleRad(Quaternion quaternion);
	
	// The angle is in radians
	ECSENGINE_API float QuaternionHalfAngleRad(QuaternionScalar quaternion);

	// The angle is in radians
	ECSENGINE_API Vec8f ECS_VECTORCALL QuaternionHalfAngleRad(Quaternion quaternion);

	// The angle is in degrees
	ECS_INLINE float ECS_VECTORCALL QuaternionAngle(QuaternionScalar quaternion) {
		return RadToDeg(QuaternionAngleRad(quaternion));
	}

	// The angle is in degrees
	ECS_INLINE Vec8f ECS_VECTORCALL QuaternionAngle(Quaternion quaternion) {
		return RadToDeg(QuaternionAngleRad(quaternion));
	}

	// The angle is in degrees
	ECS_INLINE float QuaternionHalfAngle(QuaternionScalar quaternion) {
		return RadToDeg(QuaternionHalfAngleRad(quaternion));
	}

	// The angle is in degrees
	ECS_INLINE Vec8f ECS_VECTORCALL QuaternionHalfAngle(Quaternion quaternion) {
		return RadToDeg(QuaternionHalfAngleRad(quaternion));
	}

	// ---------------------------------------------------------------------------------------------------------------

	ECSENGINE_API bool QuaternionSameOrientationMask(QuaternionScalar a, QuaternionScalar b);

	ECSENGINE_API SIMDVectorMask ECS_VECTORCALL QuaternionSameOrientationMask(Quaternion a, Quaternion b);

	// Returns true if the quaternions are located in the same hyperspace
	// (The dot product between them is positive)
	ECS_INLINE bool QuaternionSameHyperspaceMask(QuaternionScalar a, QuaternionScalar b) {
		return QuaternionDot(a, b) > 0.0f;
	}

	// Returns true if the quaternions are located in the same hyperspace
	// (The dot product between them is positive)
	ECS_INLINE SIMDVectorMask ECS_VECTORCALL QuaternionSameHyperspaceMask(Quaternion a, Quaternion b) {
		return QuaternionDot(a, b) > ZeroVectorFloat();
	}

	// ---------------------------------------------------------------------------------------------------------------

	ECSENGINE_API QuaternionScalar QuaternionConjugate(QuaternionScalar quaternion);

	ECSENGINE_API Quaternion ECS_VECTORCALL QuaternionConjugate(Quaternion quaternion);

	// ---------------------------------------------------------------------------------------------------------------

	ECS_INLINE QuaternionScalar QuaternionInverseNormalized(QuaternionScalar quaternion) {
		// The conjugate and the inverse are the same if the quaternion is normalized;
		// Avoids a division, a square root and a dot product
		return QuaternionConjugate(quaternion);
	}

	ECS_INLINE Quaternion ECS_VECTORCALL QuaternionInverseNormalized(Quaternion quaternion) {
		// The conjugate and the inverse are the same if the quaternion is normalized;
		// Avoids a division, a square root and a dot product
		return QuaternionConjugate(quaternion);
	}

	ECSENGINE_API QuaternionScalar QuaternionInverse(QuaternionScalar quaternion);

	// It exists to match the SIMD function
	ECS_INLINE QuaternionScalar QuaternionInverse(QuaternionScalar quaternion, size_t vector_count) {
		return QuaternionInverse(quaternion);
	}

	ECSENGINE_API Quaternion ECS_VECTORCALL QuaternionInverse(Quaternion quaternion, size_t vector_count);

	// ---------------------------------------------------------------------------------------------------------------

	ECSENGINE_API QuaternionScalar QuaternionMultiply(QuaternionScalar a, QuaternionScalar b);

	ECSENGINE_API Quaternion ECS_VECTORCALL QuaternionMultiply(Quaternion a, Quaternion b);

	// ---------------------------------------------------------------------------------------------------------------

	ECSENGINE_API float3 QuaternionVectorMultiply(QuaternionScalar quaternion, float3 vector);

	ECSENGINE_API Vector3 ECS_VECTORCALL QuaternionVectorMultiply(Quaternion quaternion, Vector3 vector);

	// ---------------------------------------------------------------------------------------------------------------

	// Returns the negated quaternion if they are in different hyperspaces
	ECSENGINE_API QuaternionScalar QuaternionNeighbour(QuaternionScalar quat_a, QuaternionScalar quat_b);

	// Returns the negated quaternion if they are in different hyperspaces
	ECSENGINE_API Quaternion ECS_VECTORCALL QuaternionNeighbour(Quaternion quat_a, Quaternion quat_b);

	// ---------------------------------------------------------------------------------------------------------------

	// If inputs are unit length, output will be unit length as well; assumes neighbour quaternions
	// Similar to a lerp but slightly different
	ECSENGINE_API QuaternionScalar QuaternionMix(QuaternionScalar from, QuaternionScalar to, float percentage);

	// If inputs are unit length, output will be unit length as well; assumes neighbour quaternions
	// Similar to a lerp but slightly different
	ECSENGINE_API Quaternion ECS_VECTORCALL QuaternionMix(Quaternion from, Quaternion to, Vec8f percentage);

	// If inputs are unit length, output will be the same; makes neighbour correction as needed
	// Similar to a lerp but slightly different
	ECS_INLINE QuaternionScalar QuaternionMixNeighbour(QuaternionScalar from, QuaternionScalar to, float percentage) {
		return QuaternionMix(from, QuaternionNeighbour(from, to), percentage);
	}

	// If inputs are unit length, output will be the same; makes neighbour correction as needed
	// Similar to a lerp but slightly different
	ECS_INLINE Quaternion ECS_VECTORCALL QuaternionMixNeighbour(Quaternion from, Quaternion to, Vec8f percentage) {
		return QuaternionMix(from, QuaternionNeighbour(from, to), percentage);
	}

	// ---------------------------------------------------------------------------------------------------------------

	// Assumes neighbour quaternions
	ECSENGINE_API QuaternionScalar QuaternionNLerp(QuaternionScalar from, QuaternionScalar to, float percentage);

	// Assumes neighbour quaternions
	ECSENGINE_API Quaternion ECS_VECTORCALL QuaternionNLerp(Quaternion from, Quaternion to, Vec8f percentage);

	// Makes neighbour correction
	ECS_INLINE QuaternionScalar QuaternionNLerpNeighbour(QuaternionScalar from, QuaternionScalar to, float percentage) {
		return QuaternionNLerp(from, QuaternionNeighbour(from, to), percentage);
	}

	// Makes neighbour correction
	ECS_INLINE Quaternion ECS_VECTORCALL QuaternionNLerpNeighbour(Quaternion from, Quaternion to, Vec8f percentage) {
		return QuaternionNLerp(from, QuaternionNeighbour(from, to), percentage);
	}

	// ---------------------------------------------------------------------------------------------------------------

	ECSENGINE_API QuaternionScalar QuaternionToPower(QuaternionScalar quaternion, float power);

	ECSENGINE_API Quaternion ECS_VECTORCALL QuaternionToPower(Quaternion quaternion, Vec8f power);

	// ---------------------------------------------------------------------------------------------------------------

	// Assumes neighbour quaternions and normalized ones
	ECSENGINE_API QuaternionScalar QuaternionSlerp(QuaternionScalar from_normalized, QuaternionScalar to_normalized, float percentage);

	// Assumes neighbour quaternions and normalized ones. It exists to match the SIMD function
	ECS_INLINE QuaternionScalar QuaternionSlerp(QuaternionScalar from_normalized, QuaternionScalar to_normalized, float percentage, size_t vector_count) {
		return QuaternionSlerp(from_normalized, to_normalized, percentage);
	}

	// Assumes neighbour quaternions
	ECSENGINE_API Quaternion ECS_VECTORCALL QuaternionSlerp(Quaternion from_normalized, Quaternion to_normalized, Vec8f percentage, size_t vector_count);

	// Makes the neighbour adjustment. Assumes neighbour quaternions
	ECS_INLINE QuaternionScalar QuaternionSlerpNeighbour(QuaternionScalar from_normalized, QuaternionScalar to_normalized, float percentage) {
		return QuaternionSlerp(from_normalized, QuaternionNeighbour(from_normalized, to_normalized), percentage);
	}

	// Makes the neighbour adjustment. Assumes neighbour quaternions. It exists to match the SIMD function
	ECS_INLINE QuaternionScalar QuaternionSlerpNeighbour(QuaternionScalar from_normalized, QuaternionScalar to_normalized, float percentage, size_t vector_count) {
		return QuaternionSlerpNeighbour(from_normalized, to_normalized, percentage);
	}

	// Makes the neighbour adjustment. Assumes neighbour quaternions
	ECS_INLINE Quaternion ECS_VECTORCALL QuaternionSlerpNeighbour(Quaternion from_normalized, Quaternion to_normalized, Vec8f percentage, size_t vector_count) {
		return QuaternionSlerp(from_normalized, QuaternionNeighbour(from_normalized, to_normalized), percentage, vector_count);
	}

	// ---------------------------------------------------------------------------------------------------------------

	ECSENGINE_API QuaternionScalar QuaternionLookRotationNormalized(float3 direction_normalized, float3 up_normalized);

	// It exists to match the SIMD function
	ECS_INLINE QuaternionScalar QuaternionLookRotationNormalized(float3 direction_normalized, float3 up_normalized, size_t vector_count) {
		return QuaternionLookRotationNormalized(direction_normalized, up_normalized);
	}

	ECSENGINE_API Quaternion ECS_VECTORCALL QuaternionLookRotationNormalized(Vector3 direction_normalized, Vector3 up_normalized, size_t vector_count);

	ECS_INLINE QuaternionScalar QuaternionLookRotation(float3 direction, float3 up) {
		return QuaternionLookRotationNormalized(Normalize(direction), Normalize(up));
	}

	// It exists to match the SIMD function
	ECS_INLINE QuaternionScalar QuaternionLookRotation(float3 direction, float3 up, size_t vector_count) {
		return QuaternionLookRotation(direction, up);
	}

	ECS_INLINE Quaternion ECS_VECTORCALL QuaternionLookRotation(Vector3 direction, Vector3 up, size_t vector_count) {
		return QuaternionLookRotationNormalized(Normalize(direction), Normalize(up), vector_count);
	}

	// ---------------------------------------------------------------------------------------------------------------

	ECS_INLINE float3 RotateVector(float3 vector, QuaternionScalar quaternion) {
		return QuaternionVectorMultiply(quaternion, vector);
	}

	ECS_INLINE Vector3 ECS_VECTORCALL RotateVector(Vector3 vector, Quaternion quaternion) {
		return QuaternionVectorMultiply(quaternion, vector);
	}

	ECS_INLINE float3 RotatePoint(float3 point, QuaternionScalar quaternion) {
		// We can treat the point as a displacement vector that is rotated
		return RotateVector(point, quaternion);
	}

	ECS_INLINE Vector3 ECS_VECTORCALL RotatePoint(Vector3 point, Quaternion quaternion) {
		// We can treat the point as a displacement vector that is rotated
		return RotateVector(point, quaternion);
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

	// --------------------------------------------------------------------------------------------------------------

	ECSENGINE_API float3 QuaternionToEulerRad(QuaternionScalar quaternion);

	ECSENGINE_API Vector3 ECS_VECTORCALL QuaternionToEulerRad(Quaternion quaternion);

	// The result is in degrees
	ECS_INLINE float3 QuaternionToEuler(QuaternionScalar quaternion) {
		return RadToDeg(QuaternionToEulerRad(quaternion));
	}

	// The result is in degrees
	ECS_INLINE Vector3 ECS_VECTORCALL QuaternionToEuler(Quaternion quaternion) {
		return RadToDeg(QuaternionToEulerRad(quaternion));
	}

	// --------------------------------------------------------------------------------------------------------------

	// Applies the rotation q2 after q1. q1 can be thought of as a parent rotation
	// And q2 as a child rotation
	ECS_INLINE QuaternionScalar RotateQuaternion(QuaternionScalar q1, QuaternionScalar q2) {
		return q1 * q2;
	}

	// Applies the rotation q2 after q1. q1 can be thought of as a parent rotation
	// And q2 as a child rotation
	ECS_INLINE Quaternion RotateQuaternion(Quaternion q1, Quaternion q2) {
		return q1 * q2;
	}

	// This computes the delta between a known operand and a final quaternion
	// delta * operand = final_quaternion -> delta = final_quaternion * inverse(operand)
	ECS_INLINE QuaternionScalar QuaternionDelta(QuaternionScalar operand, QuaternionScalar final_quaternion) {
		return final_quaternion * QuaternionInverse(operand);
	}

	// This computes the delta between a known operand and a final quaternion
	// delta * operand = final_quaternion -> delta = final_quaternion * inverse(operand)
	ECS_INLINE Quaternion ECS_VECTORCALL QuaternionDelta(Quaternion operand, Quaternion final_quaternion) {
		// Assume that all lanes are utilized to not overcomplicate the API
		return final_quaternion * QuaternionInverse(operand, Vec8f::size());
	}

	// This computes the delta between a known operand and a final quaternion
	// delta * operand = final_quaternion -> delta = final_quaternion * inverse(operand)
	ECS_INLINE QuaternionScalar QuaternionDeltaNormalized(QuaternionScalar operand_normalized, QuaternionScalar final_quaternion) {
		return final_quaternion * QuaternionInverseNormalized(operand_normalized);
	}

	// This computes the delta between a known operand and a final quaternion
	// delta * operand = final_quaternion -> delta = final_quaternion * inverse(operand)
	ECS_INLINE Quaternion ECS_VECTORCALL QuaternionDeltaNormalized(Quaternion operand_normalized, Quaternion final_quaternion) {
		return final_quaternion * QuaternionInverseNormalized(operand_normalized);
	}

	// --------------------------------------------------------------------------------------------------------------

	// Converts the given direction into a quaternion
	ECSENGINE_API QuaternionScalar DirectionToQuaternion(float3 direction_normalized);

	// Converts the given direction into a quaternion. It exists to match the SIMD function
	ECS_INLINE QuaternionScalar DirectionToQuaternion(float3 direction_normalized, size_t vector_count) {
		return DirectionToQuaternion(direction_normalized);
	}

	// Converts the given direction into a quaternion
	ECSENGINE_API Quaternion ECS_VECTORCALL DirectionToQuaternion(Vector3 direction_normalized, size_t vector_count);

	// --------------------------------------------------------------------------------------------------------------

	ECS_INLINE QuaternionScalar QuaternionAverageCumulatorInitializeScalar() {
		return float4::Splat(0.0f);
	}

	ECS_INLINE Quaternion ECS_VECTORCALL QuaternionAverageCumulatorInitialize() {
		return Quaternion(ZeroVectorFloat(), ZeroVectorFloat(), ZeroVectorFloat(), ZeroVectorFloat());
	}

	// The reference quaternion is used to changed the sign of the current quaternion if they are in different hyperplanes
	// This only does the cumulative step
	ECSENGINE_API void QuaternionAddToAverageStep(QuaternionScalar* cumulator, QuaternionScalar current_quaternion);

	// The reference quaternion is used to changed the sign of the current quaternion if they are in different hyperplanes
	// This only does the cumulative step
	ECSENGINE_API void QuaternionAddToAverageStep(Quaternion* cumulator, Quaternion current_quaternion);

	ECSENGINE_API QuaternionScalar QuaternionAverageFromCumulator(QuaternionScalar cumulator, unsigned int count);

	ECSENGINE_API Quaternion ECS_VECTORCALL QuaternionAverageFromCumulator(Quaternion cumulator, unsigned int count);

	// The reference quaternion is used to changed the sign of the current quaternion if they are in different hyperplanes
	ECSENGINE_API QuaternionScalar QuaternionAddToAverage(QuaternionScalar* cumulator, float4 current_quaternion, unsigned int count);

	// The reference quaternion is used to changed the sign of the current quaternion if they are in different hyperplanes
	ECSENGINE_API Quaternion ECS_VECTORCALL QuaternionAddToAverage(Quaternion* cumulator, Quaternion current_quaternion, unsigned int count);

	ECS_INLINE QuaternionScalar QuaternionAverage2(QuaternionScalar a, QuaternionScalar b) {
		return QuaternionSlerpNeighbour(a, b, 0.5f);
	}

	// It exists to match the SIMD version
	ECS_INLINE QuaternionScalar QuaternionAverage2(QuaternionScalar a, QuaternionScalar b, size_t vector_count) {
		return QuaternionAverage2(a, b);
	}

	// Call this function only for 2 quaternions - otherwise use the cumulative version
	ECS_INLINE Quaternion ECS_VECTORCALL QuaternionAverage2(Quaternion a, Quaternion b, size_t vector_count) {
		return QuaternionSlerpNeighbour(a, b, Vec8f(0.5f), vector_count);
	}

	// --------------------------------------------------------------------------------------------------------------

}