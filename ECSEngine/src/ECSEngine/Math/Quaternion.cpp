#include "ecspch.h"
#include "Quaternion.h"

ECS_OPTIMIZE_END;

namespace ECSEngine {

	namespace VectorGlobals {
		Vec8f QUATERNION_EPSILON(ECS_QUATERNION_EPSILON);
	}

	bool ECS_VECTORCALL Quaternion::operator == (Quaternion other) const {
		return Vector4::operator==(other);
	}

	bool ECS_VECTORCALL Quaternion::operator != (Quaternion other) const {
		return Vector4::operator!=(other);
	}

	Quaternion ECS_VECTORCALL Quaternion::operator + (Quaternion other) const {
		return Vector4::operator+(other);
	}

	Quaternion ECS_VECTORCALL Quaternion::operator - (Quaternion other) const {
		return Vector4::operator-(other);
	}

	Quaternion ECS_VECTORCALL Quaternion::operator * (float scalar) const {
		return Vector4::operator*(scalar);
	}

	Quaternion ECS_VECTORCALL Quaternion::operator * (Quaternion other) const {
		return QuaternionMultiply(*this, other);
	}

	Quaternion ECS_VECTORCALL Quaternion::operator -() const {
		return Vector4::operator-();
	}

	Quaternion& ECS_VECTORCALL Quaternion::operator += (Quaternion other) {
		Vector4::operator+=(other);
		return *this;
	}

	Quaternion& ECS_VECTORCALL Quaternion::operator -= (Quaternion other) {
		Vector4::operator-=(other);
		return *this;
	}

	Quaternion& ECS_VECTORCALL Quaternion::operator *= (float scalar) {
		Vector4::operator*=(scalar);
		return *this;
	}

	Quaternion& ECS_VECTORCALL Quaternion::operator *= (Quaternion other) {
		*this = QuaternionMultiply(*this, other);
		return *this;
	}

	QuaternionScalar ECS_VECTORCALL Quaternion::At(size_t index) const
	{
		return Vector4::At(index);
	}

	void Quaternion::Set(QuaternionScalar scalar_quat, size_t index)
	{
		// Broadcast the values and then blend
		Vec8f broadcast_x = scalar_quat.x;
		Vec8f broadcast_y = scalar_quat.y;
		Vec8f broadcast_z = scalar_quat.z;
		Vec8f broadcast_w = scalar_quat.w;

		x = BlendSingleSwitch(x, broadcast_x, index);
		y = BlendSingleSwitch(y, broadcast_y, index);
		z = BlendSingleSwitch(z, broadcast_z, index);
		w = BlendSingleSwitch(w, broadcast_w, index);
	}

	ECS_INLINE static float sincos(float* cos_value, float radians) {
		*cos_value = cos(radians);
		return sin(radians);
	}

	// ---------------------------------------------------------------------------------------------------------------

	template<typename Quaternion, typename Vector, typename AngleType>
	static Quaternion ECS_VECTORCALL QuaternionAngleFromAxisRadImpl(Vector vector_axis, AngleType angle_radians) {
		vector_axis = NormalizeIfNot(vector_axis, 8);

		AngleType half = 0.5f;
		angle_radians *= half;

		AngleType cos;
		AngleType sin = sincos(&cos, angle_radians);
		vector_axis *= sin;
		return { vector_axis.x, vector_axis.y, vector_axis.z, cos };
	}

	QuaternionScalar QuaternionAngleFromAxisRad(float3 vector_axis, float angle_radians) {
		return QuaternionAngleFromAxisRadImpl<QuaternionScalar>(vector_axis, angle_radians);
	}

	Quaternion ECS_VECTORCALL QuaternionAngleFromAxisRad(Vector3 vector_axis, Vec8f angle_radians) {
		return QuaternionAngleFromAxisRadImpl<Quaternion>(vector_axis, angle_radians);
	}

	// ---------------------------------------------------------------------------------------------------------------

	QuaternionScalar QuaternionFromEulerRad(float3 rotation_radians) {
		// Quaternion - x, y and z need to be the half angles:
		// x = sin(x) * cos(y) * cos(z) - cos(x) * sin(y) * sin(z)
		// y = cos(x) * sin(y) * cos(z) + sin(x) * cos(y) * sin(z)
		// z = cos(x) * cos(y) * sin(z) - sin(x) * sin(y) * cos(z)
		// w = cos(x) * cos(y) * cos(z) + sin(x) * sin(y) * sin(z)

		// Use a SIMD vector for this, but utilize only the lower 3 values
		Vec8f angles(rotation_radians.x * 0.5f, rotation_radians.y * 0.5f, rotation_radians.z * 0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);

		Vec8f cos;
		Vec8f sin = sincos(&cos, angles);

		Vec8f first_permutation_left = PerLaneBlend<4, 0, 0, 0>(cos, sin);
		Vec8f second_permutation_left = PerLaneBlend<1, 5, 1, 1>(cos, sin);
		Vec8f third_permutation_left = PerLaneBlend<2, 2, 6, 2>(cos, sin);

		Vec8f first_permutation_right = PerLanePermute<1, 0, 0, 0>(first_permutation_left);
		Vec8f second_permutation_right = PerLanePermute<1, 0, 1, 1>(second_permutation_left);
		Vec8f third_permutation_right = PerLanePermute<2, 2, 0, 2>(third_permutation_left);

		Vec8f left = first_permutation_left * second_permutation_left * third_permutation_left;
		Vec8f right = first_permutation_right * second_permutation_right * third_permutation_right;
		right = PerLaneChangeSign<1, 0, 1, 0>(right);

		Vec8f result = left + right;
		alignas(ECS_SIMD_BYTE_SIZE) QuaternionScalar results[2];
		result.store_a((float*)results);
		return results[0];
	}

	Quaternion ECS_VECTORCALL QuaternionFromEulerRad(Vector3 rotation_radians) {
		// Quaternion - x, y and z need to be the half angles:
		// x = sin(x) * cos(y) * cos(z) - cos(x) * sin(y) * sin(z)
		// y = cos(x) * sin(y) * cos(z) + sin(x) * cos(y) * sin(z)
		// z = cos(x) * cos(y) * sin(z) - sin(x) * sin(y) * cos(z)
		// w = cos(x) * cos(y) * cos(z) + sin(x) * sin(y) * sin(z)

		rotation_radians *= 0.5f;
		Vec8f cos_x, cos_y, cos_z;
		Vec8f sin_x = sincos(&cos_x, rotation_radians.x);
		Vec8f sin_y = sincos(&cos_y, rotation_radians.y);
		Vec8f sin_z = sincos(&cos_z, rotation_radians.z);

		Vec8f quat_x = sin_x * cos_y * cos_z - cos_x * sin_y * sin_z;
		Vec8f quat_y = cos_x * sin_y * cos_z + sin_x * cos_y * sin_z;
		Vec8f quat_z = cos_x * cos_y * sin_z - sin_x * sin_y * cos_z;
		Vec8f quat_w = cos_x * cos_y * cos_z + sin_x * sin_y * sin_z;
		return { quat_x, quat_y, quat_z, quat_w };
	}

	// ---------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region From Vectors

	// ---------------------------------------------------------------------------------------------------------------

	QuaternionScalar QuaternionFromVectorsNormalized(float3 from_vector_normalized, float3 to_vector_normalized) {
		// If they are equal return an identity quaternion
		bool compare_mask = CompareMask(from_vector_normalized, to_vector_normalized, float3::Splat(ECS_QUATERNION_EPSILON));
		if (compare_mask) {
			return QuaternionIdentityScalar();
		}
		else {
			// Now there are 2 branches left
			// If they are opposite and when they are not
			// If they are opposite return the most orthogonal axis of the from_vector vector
			// that can be used to create a pure quaternion

			bool opposite_compare_mask = CompareMask(from_vector_normalized, -to_vector_normalized, float3::Splat(ECS_QUATERNION_EPSILON));
			if (opposite_compare_mask) {
				// This is the scalar code that must be executed
				/*
					float3 ortho = float3(1, 0, 0);
					if (fabsf(f.y) < fabsf(f.x)) {
						orthogonal = float3(0, 1, 0);
					}
					if (fabsf(f.z) < fabs(f.y) && fabs(f.z) < fabsf(f.x)){
						orthogonal = float3(0, 0, 1);
					}
				*/

				float3 orthogonal = GetRightVector();
				float3 absolute_from = Abs(from_vector_normalized);
				bool y_mask = absolute_from.y < absolute_from.x;
				bool z_mask = absolute_from.z < absolute_from.y&& absolute_from.z < absolute_from.x;
				orthogonal = Select(y_mask, GetUpVector(), orthogonal);
				orthogonal = Select(z_mask, GetForwardVector(), orthogonal);
				float3 opposite_quaternion_axis = Normalize(Cross(from_vector_normalized, orthogonal));
				return QuaternionScalar(opposite_quaternion_axis, 0.0f);
			}

			// Else create the quaternion from the half quaternion
			/*
				The scalar code is
				float dot = dot(from, to);
				float k = sqrt(SquaredLength(from) * SquaredLength(to));

				if (dot / k == -1)
				{
					// 180 degree rotation around any orthogonal vector
					return opposite_quaternion_result;
				}

				return Normalize(Quaternion(cross(from, to), dot + k));
				Since we have normalized from and to, their squared lengths are 1, so is k
				And we can make some simplifications
			*/

			return QuaternionNormalize({
				Cross(from_vector_normalized, to_vector_normalized),
				Dot(from_vector_normalized, to_vector_normalized) + 1.0f
			});
		}
	}

	Quaternion ECS_VECTORCALL QuaternionFromVectorsNormalized(Vector3 from_vector_normalized, Vector3 to_vector_normalized, size_t vector_count) {
		// If they are equal return an identity quaternion
		SIMDVectorMask compare_mask = CompareMask(from_vector_normalized, to_vector_normalized, VectorGlobals::QUATERNION_EPSILON);
		if (VectorMask{ compare_mask }.AreAllSet(vector_count)) {
			return QuaternionIdentity();
		}
		else {
			// Now there are 2 branches left
			// If they are opposite and when they are not
			// If they are opposite return the most orthogonal axis of the from_vector vector
			// that can be used to create a pure quaternion

			// Can't be sure which branch each half must take so execute both of them
			// and then mask the result at the end

			SIMDVectorMask opposite_compare_mask = CompareMask(from_vector_normalized, -to_vector_normalized, VectorGlobals::QUATERNION_EPSILON);

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

			Vector3 orthogonal = RightVector();
			Vector3 absolute_from = Abs(from_vector_normalized);
			SIMDVectorMask y_mask = absolute_from.y < absolute_from.x;
			SIMDVectorMask z_mask = absolute_from.z < absolute_from.y && absolute_from.z < absolute_from.x;
			orthogonal = Select(y_mask, UpVector(), orthogonal);
			orthogonal = Select(z_mask, ForwardVector(), orthogonal);
			Vector3 opposite_quaternion_axis = Normalize(Cross(from_vector_normalized, orthogonal));
			Quaternion opposite_quaternion_result = Quaternion(opposite_quaternion_axis, ZeroVectorFloat());

			// Test the opposite compare mask since it can buy us some computation
			// In some cases
			if (VectorMask{ opposite_compare_mask }.AreAllSet(vector_count)) {
				return opposite_quaternion_result;
			}

			// Else create the quaternion from the half quaternion
			/*
				The scalar code is
				float dot = dot(from, to);
				float k = sqrt(SquaredLength(from) * SquaredLength(to));

				if (dot / k == -1)
				{
					// 180 degree rotation around any orthogonal vector
					return opposite_quaternion_result;
				}

				return Normalize(Quaternion(cross(from, to), dot + k));
				Since we have normalized from and to, their squared lengths are 1, so is k
				And we can make some simplifications
			*/

			Quaternion general_case_result = QuaternionNormalize({ 
				Cross(from_vector_normalized, to_vector_normalized), 
				Dot(from_vector_normalized, to_vector_normalized) + VectorGlobals::ONE
			});

			// We must still select the value since the lanes might have different branches
			// We must also take into account the initial compare mask as well
			Quaternion opposite_general_result = Select(opposite_compare_mask, opposite_quaternion_result, general_case_result);
			return Select(compare_mask, QuaternionIdentity(), opposite_quaternion_result);
		}
	}

	QuaternionScalar QuaternionFromVectors(float3 from, float3 to) {
		return QuaternionFromVectorsNormalized(Normalize(from), Normalize(to));
	}

	Quaternion ECS_VECTORCALL QuaternionFromVectors(Vector3 from, Vector3 to, size_t vector_count) {
		// Normalize the vectors
		from = Normalize(from);
		to = Normalize(to);

		return QuaternionFromVectorsNormalized(from, to, vector_count);
	}

	// ---------------------------------------------------------------------------------------------------------------

	float3 QuaternionAxisNormalized(QuaternionScalar quaternion_normalized) {
		return quaternion_normalized.xyz();
	}

	Vector3 ECS_VECTORCALL QuaternionAxisNormalized(Quaternion quaternion_normalized) {
		return { quaternion_normalized.x, quaternion_normalized.y, quaternion_normalized.z };
	}

	float3 QuaternionAxis(QuaternionScalar quaternion) {
		return QuaternionAxisNormalized(QuaternionNormalize(quaternion));
	}

	Vector3 ECS_VECTORCALL QuaternionAxis(Quaternion quaternion) {
		return QuaternionAxisNormalized(QuaternionNormalize(quaternion));
	}

	// ---------------------------------------------------------------------------------------------------------------

	float QuaternionAngleRad(QuaternionScalar quaternion) {
		return 2.0f * acosf(quaternion.w);
	}

	Vec8f ECS_VECTORCALL QuaternionAngleRad(Quaternion quaternion) {
		return 2.0f * acos(quaternion.w);
	}

	float QuaternionHalfAngleRad(QuaternionScalar quaternion) {
		return acosf(quaternion.w);
	}

	Vec8f ECS_VECTORCALL QuaternionHalfAngleRad(Quaternion quaternion) {
		return acos(quaternion.w);
	}

	// ---------------------------------------------------------------------------------------------------------------

	bool QuaternionSameOrientationMask(QuaternionScalar a, QuaternionScalar b) {
		return CompareMask(a, b, QuaternionScalar::Splat(ECS_QUATERNION_EPSILON)) || CompareMask(a + b, QuaternionScalar::Splat(0.0f), QuaternionScalar::Splat(ECS_QUATERNION_EPSILON));
	}

	SIMDVectorMask ECS_VECTORCALL QuaternionSameOrientationMask(Quaternion a, Quaternion b) {
		return CompareMask(a, b, VectorGlobals::QUATERNION_EPSILON) || CompareMask(a + b, Vector4{ ZeroVector(), ZeroVectorFloat() }, VectorGlobals::QUATERNION_EPSILON);
	}

	// ---------------------------------------------------------------------------------------------------------------

	QuaternionScalar QuaternionConjugate(QuaternionScalar quaternion) {
		// Invert the axis
		return QuaternionScalar(-quaternion.x, -quaternion.y, -quaternion.z, quaternion.w);
	}

	Quaternion ECS_VECTORCALL QuaternionConjugate(Quaternion quaternion) {
		// Invert the axis
		return Quaternion(-quaternion.x, -quaternion.y, -quaternion.z, quaternion.w);
	}

	// ---------------------------------------------------------------------------------------------------------------

	QuaternionScalar QuaternionInverse(QuaternionScalar quaternion) {
		bool mask = QuaternionIsNormalizedMask(quaternion);
		if (!mask) {
			return QuaternionConjugate(quaternion) * (1.0f / QuaternionSquaredLength(quaternion));
		}
		else {
			return QuaternionConjugate(quaternion);
		}
	}

	Quaternion ECS_VECTORCALL QuaternionInverse(Quaternion quaternion, size_t vector_count) {
		// Proper inverse is the conjugate divided by the squared length
		// If the squared length is 1.0f, then don't perform the division
		SIMDVectorMask mask = QuaternionIsNormalizedMask(quaternion);
		if (!VectorMask{ mask }.AreAllSet(vector_count)) {
			return QuaternionConjugate(quaternion) / QuaternionSquaredLength(quaternion);
		}
		else {
			return QuaternionInverseNormalized(quaternion);
		}
	}

	// ---------------------------------------------------------------------------------------------------------------

	template<typename Quaternion>
	ECS_INLINE static Quaternion ECS_VECTORCALL QuaternionMultiplyImpl(Quaternion a, Quaternion b) {
		// x = b.x * a.w + b.y * a.z - b.z * a.y + b.w * a.x
		// y = -b.x * a.z + b.y * a.w + b.z * a.x + b.w * a.y
		// z = b.x * a.y - b.y * a.x + b.z * a.w + b.w * a.z
		// w = -b.x * a.x - b.y * a.y - b.z * a.z + b.w * a.w

		auto x = b.x * a.w + b.y * a.z - b.z * a.y + b.w * a.x;
		auto y = -b.x * a.z + b.y * a.w + b.z * a.x + b.w * a.y;
		auto z = b.x * a.y - b.y * a.x + b.z * a.w + b.w * a.z;
		auto w = -b.x * a.x - b.y * a.y - b.z * a.z + b.w * a.w;
		return { x, y, z, w };
	}

	QuaternionScalar QuaternionMultiply(QuaternionScalar a, QuaternionScalar b) {
		return QuaternionMultiplyImpl(a, b);
	}

	Quaternion ECS_VECTORCALL QuaternionMultiply(Quaternion a, Quaternion b) {
		return QuaternionMultiplyImpl(a, b);
	}

	// ---------------------------------------------------------------------------------------------------------------

	template<typename Vector, typename Quaternion>
	ECS_INLINE static Vector ECS_VECTORCALL QuaternionVectorMultiplyImpl(Quaternion quaternion, Vector vector) {
		// 2.0f * Dot(quaternion.xyz, vector.xyz) * quaternion.xyz + vector * 
		// (quaternion.w * quaternion.w - Dot(quaternion.xyz, quaternion.xyz)) + Cross(quaternion.xyz, vector.xyz) * 2.0f * quaternion.w
	
		auto quat_axis = QuaternionAxisNormalized(quaternion);
		auto quat_vec_dot = Dot(quat_axis, vector);
		auto quat_quat_dot = Dot(quat_axis, quat_axis);
		auto quat_vec_cross = Cross(quat_axis, vector);
		return quat_axis * quat_vec_dot * 2.0f + vector * (quaternion.w * quaternion.w - quat_quat_dot) + quat_vec_cross * quaternion.w * 2.0f;
	}

	float3 QuaternionVectorMultiply(QuaternionScalar quaternion, float3 vector) {
		return QuaternionVectorMultiplyImpl(quaternion, vector);
	}

	Vector3 ECS_VECTORCALL QuaternionVectorMultiply(Quaternion quaternion, Vector3 vector) {
		return QuaternionVectorMultiplyImpl(quaternion, vector);
	}

	// ---------------------------------------------------------------------------------------------------------------

	template<typename Quaternion>
	ECS_INLINE static Quaternion ECS_VECTORCALL QuaternionNeighbourImpl(Quaternion quat_a, Quaternion quat_b) {
		// If the dot product between the 2 quaternions is negative, negate b and return it
		auto mask = QuaternionSameHyperspaceMask(quat_a, quat_b);
		return Select(mask, quat_b, -quat_b);
	}

	QuaternionScalar QuaternionNeighbour(QuaternionScalar quat_a, QuaternionScalar quat_b) {
		return QuaternionNeighbourImpl(quat_a, quat_b);
	}

	Quaternion ECS_VECTORCALL QuaternionNeighbour(Quaternion quat_a, Quaternion quat_b) {
		return QuaternionNeighbourImpl(quat_a, quat_b);
	}

	// ---------------------------------------------------------------------------------------------------------------

	template<typename Quaternion, typename PercentageType>
	ECS_INLINE static Quaternion ECS_VECTORCALL QuaternionMixImpl(Quaternion from, Quaternion to, PercentageType percentage) {
		PercentageType one_minus_percentage = OneVector<PercentageType>() - percentage;
		return from * Quaternion::Splat(one_minus_percentage) + to * Quaternion::Splat(percentage);
	}

	QuaternionScalar QuaternionMix(QuaternionScalar from, QuaternionScalar to, float percentage) {
		return QuaternionMixImpl(from, to, percentage);
	}

	Quaternion ECS_VECTORCALL QuaternionMix(Quaternion from, Quaternion to, Vec8f percentages) {
		return QuaternionMixImpl(from, to, percentages);
	}

	// ---------------------------------------------------------------------------------------------------------------

	QuaternionScalar QuaternionNLerp(QuaternionScalar from, QuaternionScalar to, float percentage) {
		return QuaternionNormalize(Lerp(from, to, percentage));
	}

	Quaternion ECS_VECTORCALL QuaternionNLerp(Quaternion from, Quaternion to, Vec8f percentage) {
		return QuaternionNormalize(Lerp(from, to, percentage));
	}

	// ---------------------------------------------------------------------------------------------------------------

	template<typename Vector, typename Quaternion, typename PowerType>
	ECS_INLINE static Quaternion ECS_VECTORCALL QuaternionToPowerImpl(Quaternion quaternion, PowerType power) {
		/*
			Scalar formula
			angle = acos(quaternion.w) // This is the half angle of the quaternion
			halfSin = sin(angle * power)
			halfCos = cos(angle * power)
			
			// We need this normalization since the quaternion can be normalized
			// On the whole with the w as well
			axis = Normalize(quaternion.xyz)

			return Quaternion (
				axis.x * halfSin,
				axis.y * halfSin,
				axis.z * halfSin,
				halfCos
			)
		*/

		PowerType angle = acos(quaternion.w);
		PowerType halfCos;
		PowerType halfSin = sincos(&halfCos, angle * power);
		auto axis = Normalize(Vector{ quaternion.x, quaternion.y, quaternion.z });
		return { axis.x * halfSin, axis.y * halfSin, axis.z * halfSin, halfCos };
	}

	QuaternionScalar QuaternionToPower(QuaternionScalar quaternion, float power) {
		return QuaternionToPowerImpl<float3>(quaternion, power);
	}

	Quaternion ECS_VECTORCALL QuaternionToPower(Quaternion quaternion, Vec8f power) {
		return QuaternionToPowerImpl<Vector3>(quaternion, power);
	}

	// ---------------------------------------------------------------------------------------------------------------

	QuaternionScalar QuaternionSlerp(QuaternionScalar from_normalized, QuaternionScalar to_normalized, float percentage) {
		float dot = QuaternionDot(from_normalized, to_normalized);
		if (abs(dot) > 1.0f - ECS_QUATERNION_EPSILON) {
			return QuaternionNLerp(from_normalized, to_normalized, percentage);
		}

		QuaternionScalar delta = QuaternionMultiply(QuaternionInverseNormalized(from_normalized), to_normalized);
		// Use normalize if not since raising a quaternion to a power of a unit quaternion should still be normalized,
		// but because of floating point errors it may drift
		QuaternionScalar slerp_quaternion = NormalizeIfNot(QuaternionMultiply(QuaternionToPower(delta, percentage), from_normalized));
		return slerp_quaternion;
	}

	Quaternion ECS_VECTORCALL QuaternionSlerp(Quaternion from_normalized, Quaternion to_normalized, Vec8f percentage, size_t vector_count) {
		Vec8f dot = QuaternionDot(from_normalized, to_normalized);
		SIMDVectorMask nlerp_mask = AbsSingle(dot) > (VectorGlobals::ONE - VectorGlobals::QUATERNION_EPSILON);
		Quaternion nlerp_result = QuaternionNLerp(from_normalized, to_normalized, percentage);
		if (vector_count == Vec8f::size()) {
			if (horizontal_and(nlerp_mask)) {
				return nlerp_result;
			}
		}
		else if (VectorMask{ nlerp_mask }.AreAllSet(vector_count)) {
			return nlerp_result;
		}

		// We have 2 branches - one when the quaternions are close together where
		// slerp can produce unstable result, we fall back to nlerp
		// And the other branch is the classical slerp algorithm

		Quaternion delta = QuaternionMultiply(QuaternionInverseNormalized(from_normalized), to_normalized);
		// Use normalize if not since raising a quaternion to a power of a unit quaternion should still be normalized,
		// but because of floating point errors it may drift
		Quaternion slerp_quaternion = NormalizeIfNot(QuaternionMultiply(QuaternionToPower(delta, percentage), from_normalized), vector_count);
		return Select(nlerp_mask, nlerp_result, slerp_quaternion);
	}

	// ---------------------------------------------------------------------------------------------------------------

	template<typename Quaternion, typename Vector>
	ECS_INLINE static Quaternion ECS_VECTORCALL QuaternionLookRotationNormalizedImpl(Vector direction_normalized, Vector up_normalized, size_t vector_count) {
		Vector normalized_right = Cross(up_normalized, direction_normalized); // Object Right
		Vector current_up = Cross(direction_normalized, normalized_right); // Object Up

		// From world forward to object forward
		Vector default_up = UpVector<Vector>();
		Vector default_forward = ForwardVector<Vector>();
		Quaternion world_to_object = QuaternionFromVectorsNormalized(default_forward, direction_normalized, vector_count);

		// This is the new up that the object wants to have
		Vector desired_up = QuaternionVectorMultiply(world_to_object, default_up);

		// From object up to desired up. The up needs to be normalized since the cross does not maintain this property
		// We don't need to normalize current up since cross of perpendicular normalized vectors is already normalized
		Quaternion object_up_to_desired_up = QuaternionFromVectorsNormalized(desired_up, current_up, vector_count);

		// Rotate to forward direction first then twist to correct up
		Quaternion result = QuaternionMultiply(world_to_object, object_up_to_desired_up);

		// TODO: Here we can probably eliminate the normalization, the result is a multiplication of
		// Normalized quaternions which preserves the normalization property
		return QuaternionNormalize(result);
	}

	QuaternionScalar QuaternionLookRotationNormalized(float3 direction_normalized, float3 up_normalized) {
		return QuaternionLookRotationNormalizedImpl<QuaternionScalar>(direction_normalized, up_normalized, 1);
	}

	Quaternion ECS_VECTORCALL QuaternionLookRotationNormalized(Vector3 direction_normalized, Vector3 up_normalized, size_t vector_count) {
		return QuaternionLookRotationNormalizedImpl<Quaternion>(direction_normalized, up_normalized, vector_count);
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
	int GetQuadrant(float2 vector) {
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

	bool IsClockwise(float2 a, float2 b, bool normalize_late) {
		// Here it is much simpler.
		// Get the quadrants of A and B
		int a_quadrant = GetQuadrant(a);
		int b_quadrant = GetQuadrant(b);

		// If in the same quadrant or opposing quadrant, we need a special case
		auto same_quadrant_case = [normalize_late](float2 a, float2 b, int quadrant) {
			if (normalize_late) {
				a = Normalize(a);
				b = Normalize(b);
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

			// Shouldn't be reached
			return false;
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

	// --------------------------------------------------------------------------------------------------------------
	
	template<typename Vector, typename Quaternion>
	ECS_INLINE static Vector ECS_VECTORCALL QuaternionToEulerRadImpl(Quaternion quaternion) {
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

		auto one = OneVector<Vector>();
		auto two = SingleValueVector<Vector>(2.0f);
		auto x_factor_first = quaternion.w * quaternion.x + quaternion.y * quaternion.z;
		auto x_factor_second = quaternion.x * quaternion.x + quaternion.y * quaternion.y;
		auto z_factor_first = quaternion.w * quaternion.z + quaternion.x * quaternion.y;
		auto z_factor_second = quaternion.y * quaternion.y + quaternion.z * quaternion.z;
		auto y_factor = quaternion.w * quaternion.y - quaternion.x * quaternion.z;

		auto x_rotation = atan2(two * x_factor_first, one - two * x_factor_second);
		auto basic_y_rotation = atan2(sqrt(one + two * y_factor), sqrt(one - two * y_factor));
		auto z_rotation = atan2(two * z_factor_first, one - two * z_factor_second);
		
		// There is a special case, when the basic Y rotation is NaN, in that case we need to manually set the rotation
		// To PI / 2 otherwise it won't correspond to the correct value
		auto is_y_nan = IsNanMask(basic_y_rotation);
		return { x_rotation, SelectSingle(is_y_nan, SingleValueVector<Vector>(PI / 2), SingleValueVector<Vector>(-PI / 2) + two * basic_y_rotation), z_rotation };
	}

	float3 QuaternionToEulerRad(QuaternionScalar quaternion) {
		return QuaternionToEulerRadImpl<float3>(quaternion);
	}

	Vector3 ECS_VECTORCALL QuaternionToEulerRad(Quaternion quaternion) {
		return QuaternionToEulerRadImpl<Vector3>(quaternion);
	}

	// --------------------------------------------------------------------------------------------------------------

	QuaternionScalar DirectionToQuaternion(float3 direction_normalized) {
		float3 up_vector = GetUpVectorForDirection(direction_normalized);
		return QuaternionLookRotationNormalized(direction_normalized, up_vector);
	}

	Quaternion ECS_VECTORCALL DirectionToQuaternion(Vector3 direction_normalized, size_t vector_count) {
		Vector3 up_vector = GetUpVectorForDirection(direction_normalized);
		return QuaternionLookRotationNormalized(direction_normalized, up_vector, vector_count);
	}

	// --------------------------------------------------------------------------------------------------------------

	// Annoying conversions from float to Vec8f prevent this from having a common function

	void QuaternionAddToAverageStep(QuaternionScalar* cumulator, QuaternionScalar current_quaternion) {
		// Use some default value for the reference quaternion
		QuaternionScalar reference_quaternion = QuaternionForAxisXScalar(0.0f);
		QuaternionScalar neighbour_quaternion = QuaternionNeighbour(reference_quaternion, current_quaternion);
		*cumulator += neighbour_quaternion;
	}

	void QuaternionAddToAverageStep(Quaternion* cumulator, Quaternion current_quaternion) {
		// Use some default value for the reference quaternion
		Quaternion reference_quaternion = QuaternionForAxisX(0.0f);
		Quaternion neighbour_quaternion = QuaternionNeighbour(reference_quaternion, current_quaternion);
		*cumulator += neighbour_quaternion;
	}

	QuaternionScalar QuaternionAverageFromCumulator(QuaternionScalar cumulator, unsigned int count) {
		bool is_zero = cumulator == QuaternionScalar::Splat(0.0f);
		if (is_zero) {
			return QuaternionIdentityScalar();
		}
		float count_inverse = OneDividedVector<ECS_VECTOR_ACCURATE>((float)count);
		return QuaternionNormalize(cumulator * count_inverse);
	}

	Quaternion ECS_VECTORCALL QuaternionAverageFromCumulator(Quaternion cumulator, unsigned int count) {
		Vector4 count_inverse = Vector4::Splat(OneDividedVector<ECS_VECTOR_ACCURATE>(Vec8f((float)count)));
		SIMDVectorMask is_zero = cumulator == Quaternion{ ZeroVector(), ZeroVectorFloat() };
		Quaternion average_values = Vector4(cumulator) * count_inverse;
		return Select(is_zero, QuaternionIdentity(), QuaternionNormalize(average_values));
	}

	template<typename Quaternion>
	ECS_INLINE static Quaternion ECS_VECTORCALL QuaternionAddToAverageImpl(Quaternion* cumulator, Quaternion current_quaternion, unsigned int count) {
		QuaternionAddToAverageStep(cumulator, current_quaternion);
		return QuaternionAverageFromCumulator(*cumulator, count);
	}

	QuaternionScalar QuaternionAddToAverage(QuaternionScalar* cumulator, QuaternionScalar current_quaternion, unsigned int count) {
		return QuaternionAddToAverageImpl(cumulator, current_quaternion, count);
	}

	Quaternion ECS_VECTORCALL QuaternionAddToAverage(Quaternion* cumulator, Quaternion current_quaternion, unsigned int count) {
		return QuaternionAddToAverageImpl(cumulator, current_quaternion, count);
	}

	// --------------------------------------------------------------------------------------------------------------

}