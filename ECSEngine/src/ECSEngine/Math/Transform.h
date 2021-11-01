#pragma once
#include "Quaternion.h"
#include "Conversion.h"

constexpr float ECS_TRANSFORM_SCALE_EPSILON = 0.00005f;

namespace ECSEngine {

	struct Transform {
		ECS_INLINE Transform() {}
		ECS_INLINE ECS_VECTORCALL Transform(float3 _position, Quaternion _rotation, float3 _scale) : position(_position),
			scale(_scale), rotation(_rotation) {}

		// Translation - 0.0f, 0.0f, 0.0f
		// Scale - 1.0f, 1.0f, 1.0f
		// Rotation - Quaternion Identity
		ECS_INLINE void Default() {
			position = { 0.0f, 0.0f, 0.0f };
			scale = { 1.0f, 1.0f, 1.0f };
			rotation = QuaternionIdentity();
		}

		float3 position;
		float3 scale;
		Quaternion rotation;
	};

	struct VectorTransform {
		ECS_INLINE VectorTransform() {}
		ECS_INLINE ECS_VECTORCALL VectorTransform(Vector4 _position, Vector4 _scale, Quaternion _rotation) : position(_position),
			scale(_scale), rotation(_rotation) {}

		ECS_INLINE Transform ToTransform() const {
			Transform transform;
			position.StorePartialConstant<3>(&transform.position);
			scale.StorePartialConstant<3>(&transform.scale);
			transform.rotation = rotation;

			return transform;
		}

		Vector4 position;
		Vector4 scale;
		Quaternion rotation;
	};

	struct PackedVectorTransform {
		ECS_INLINE PackedVectorTransform() {}
		ECS_INLINE ECS_VECTORCALL PackedVectorTransform(Vector8 _position, Vector8 _scale, PackedQuaternion _rotation) : position(_position),
			scale(_scale), rotation(_rotation) {}
		ECS_INLINE ECS_VECTORCALL PackedVectorTransform(VectorTransform first, VectorTransform second) : position(first.position, second.position),
			scale(first.scale, second.scale), rotation(first.rotation, second.rotation) {}

		ECS_INLINE VectorTransform Low() const {
			return { position.Low(), scale.Low(), rotation.Low() };
		}

		ECS_INLINE VectorTransform High() const {
			return { position.High(), scale.High(), rotation.High() };
		}

		Vector8 position;
		Vector8 scale;
		PackedQuaternion rotation;
	};

	ECS_INLINE VectorTransform ECS_VECTORCALL ToVectorTransform(Transform transform) {
		return { Vector4(transform.position), Vector4(transform.scale), transform.rotation };
	}

#pragma region Combine

	// ---------------------------------------------------------------------------------------------------------------------

	namespace SIMDHelpers {
		template<typename VectorTransform>
		VectorTransform ECS_VECTORCALL TransformCombine(VectorTransform parent, VectorTransform child) {
			VectorTransform result;

			// Rotation and scale can be composed by multiplication
			result.rotation = QuaternionMultiply(child.rotation, parent.rotation);
			result.scale = parent.scale * child.scale;

			// The position must be transformed by the parent's transform and then translated
			result.position = QuaternionVectorMultiply(parent.rotation, parent.scale * child.position);
			result.position += parent.position;

			return result;
		}
	}

	ECS_INLINE Transform ECS_VECTORCALL TransformCombine(Transform parent, Transform child) {
		Transform result;

		// Rotation and scale can be composed by multiplication
		result.rotation = QuaternionMultiply(child.rotation, parent.rotation);
		result.scale = parent.scale * child.scale;

		// The position must be transformed by the parent's transform and then translated
		Vector4 position = QuaternionVectorMultiply(parent.rotation, Vector4(parent.scale * child.position));
		position.StorePartialConstant<3>(&result.position);
		result.position += parent.position;

		return result;
	}

	ECS_INLINE VectorTransform ECS_VECTORCALL TransformCombine(VectorTransform parent, VectorTransform child) {
		return SIMDHelpers::TransformCombine(parent, child);
	}

	ECS_INLINE PackedVectorTransform ECS_VECTORCALL TransformCombine(PackedVectorTransform parent, PackedVectorTransform child) {
		return SIMDHelpers::TransformCombine(parent, child);
	}

	// ---------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Inverse

	// ---------------------------------------------------------------------------------------------------------------------

	ECS_INLINE Transform ECS_VECTORCALL TransformInverse(Transform transform) {
		Transform result;

		result.rotation = QuaternionInverse(transform.rotation);
		result.scale.x = fabsf(transform.scale.x) < ECS_TRANSFORM_SCALE_EPSILON ? 0.0f : 1.0f / transform.scale.x;
		result.scale.y = fabsf(transform.scale.y) < ECS_TRANSFORM_SCALE_EPSILON ? 0.0f : 1.0f / transform.scale.y;
		result.scale.z = fabsf(transform.scale.z) < ECS_TRANSFORM_SCALE_EPSILON ? 0.0f : 1.0f / transform.scale.z;

		float3 negative_position = transform.position * float3(-1.0f, -1.0f, -1.0f);
		Vector4 position = QuaternionVectorMultiply(transform.rotation, result.scale * negative_position);
		position.StorePartialConstant<3>(&result.position);

		return result;
	}

	ECS_INLINE VectorTransform ECS_VECTORCALL TransformInverse(VectorTransform transform) {
		VectorTransform result;

		result.rotation = QuaternionInverse(transform.rotation);
		result.scale = select(abs(transform.scale) < Vector4(ECS_TRANSFORM_SCALE_EPSILON), ZeroVector4(), VectorGlobals::ONE_4 / transform.scale);
		result.position = change_sign<1, 1, 1, 0>(transform.position);

		result.position = QuaternionVectorMultiply(transform.rotation, result.scale * result.position);

		return result;
	}

	ECS_INLINE PackedVectorTransform ECS_VECTORCALL TransformInverse(PackedVectorTransform transform) {
		PackedVectorTransform result;

		result.rotation = QuaternionInverse(transform.rotation);
		result.scale = select(abs(transform.scale) < Vector8(ECS_TRANSFORM_SCALE_EPSILON), ZeroVector8(), VectorGlobals::ONE_8 / transform.scale);
		result.position = change_sign<1, 1, 1, 0, 1, 1, 1, 0>(transform.position);

		result.position = QuaternionVectorMultiply(transform.rotation, result.scale * result.position);

		return result;
	}

	// ---------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Mix

	// ---------------------------------------------------------------------------------------------------------------------

	namespace SIMDHelpers {
		template<typename VectorTransform>
		VectorTransform ECS_VECTORCALL TransformMix(VectorTransform from, VectorTransform to, float percentage) {
			VectorTransform result;

			result.position = Lerp(from.position, to.position, percentage);
			result.scale = Lerp(from.scale, to.scale, percentage);
			result.rotation = QuaternionNLerpNeighbour(from.rotation, to.rotation, percentage);

			return result;
		}
	}

	// Linear blend
	ECS_INLINE Transform ECS_VECTORCALL TransformMix(Transform from, Transform to, float percentage) {
		Transform result;

		Vector4 position_lerp = Lerp(Vector4(from.position), Vector4(to.position), percentage);
		Vector4 scale_lerp = Lerp(Vector4(from.scale), Vector4(to.scale), percentage);

		position_lerp.StorePartialConstant<3>(&result.position);
		scale_lerp.StorePartialConstant<3>(&result.scale);

		Quaternion rotation_lerp = QuaternionNLerpNeighbour(from.rotation, to.rotation, percentage);
		result.rotation = rotation_lerp;

		return result;
	}

	// Linear blend
	ECS_INLINE VectorTransform ECS_VECTORCALL TransformMix(VectorTransform from, VectorTransform to, float percentage) {
		return SIMDHelpers::TransformMix(from, to, percentage);
	}

	// Linear blend
	ECS_INLINE PackedVectorTransform ECS_VECTORCALL TransformMix(PackedVectorTransform from, PackedVectorTransform to, float percentage) {
		return SIMDHelpers::TransformMix(from, to, percentage);
	}

	// Linear blend
	ECS_INLINE PackedVectorTransform ECS_VECTORCALL TransformMix(PackedVectorTransform from, PackedVectorTransform to, float percentage1, float percentage2) {
		PackedVectorTransform result;

		Vector8 percentages(percentage1, percentage1, percentage1, percentage1, percentage2, percentage2, percentage2, percentage2);
		result.position = Lerp(from.position, to.position, percentages);
		result.scale = Lerp(from.scale, to.scale, percentages);
		result.rotation = QuaternionNLerpNeighbour(from.rotation, to.rotation, percentage1, percentage2);

		return result;
	}


	// ---------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region To Matrix

	// ---------------------------------------------------------------------------------------------------------------------

	ECS_INLINE Matrix ECS_VECTORCALL TransformToMatrix(Transform transform) {
		// Extract the rotation basis of transform
		Vector4 x = QuaternionVectorMultiply(transform.rotation, VectorGlobals::RIGHT_4);
		Vector4 y = QuaternionVectorMultiply(transform.rotation, VectorGlobals::UP_4);
		Vector4 z = QuaternionVectorMultiply(transform.rotation, VectorGlobals::FORWARD_4);

		// Scale the basis vectors
		Vector4 splatted_scale_x(transform.scale.x);
		Vector4 splatted_scale_y(transform.scale.y);
		Vector4 splatted_scale_z(transform.scale.z);

		x *= splatted_scale_x;
		y *= splatted_scale_y;
		z *= splatted_scale_z;

		Vector4 zero = ZeroVector4();
		Vector4 one = VectorGlobals::ONE_4;
		x = blend4<0, 1, 2, 7>(x, zero);
		y = blend4<0, 1, 2, 7>(y, zero);
		z = blend4<0, 1, 2, 7>(z, zero);
		Vector4 position_vec;
		position_vec.Load(&transform.position);
		Vector4 position = blend4<0, 1, 2, 7>(position_vec, one);

		return Matrix(x, y, z, position);
	}

	ECS_INLINE Matrix ECS_VECTORCALL TransformToMatrix(VectorTransform transform) {
		// Extract the rotation basis of transform
		Vector4 x = QuaternionVectorMultiply(transform.rotation, VectorGlobals::RIGHT_4);
		Vector4 y = QuaternionVectorMultiply(transform.rotation, VectorGlobals::UP_4);
		Vector4 z = QuaternionVectorMultiply(transform.rotation, VectorGlobals::FORWARD_4);

		// Scale the basis vectors
		Vector4 splatted_scale_x = permute4<0, 0, 0, 0>(transform.scale);
		Vector4 splatted_scale_y = permute4<1, 1, 1, 1>(transform.scale);
		Vector4 splatted_scale_z = permute4<2, 2, 2, 2>(transform.scale);

		x *= splatted_scale_x;
		y *= splatted_scale_y;
		z *= splatted_scale_z;

		Vector4 zero = ZeroVector4();
		Vector4 one = VectorGlobals::ONE_4;
		x = blend4<0, 1, 2, 7>(x, zero);
		y = blend4<0, 1, 2, 7>(y, zero);
		z = blend4<0, 1, 2, 7>(z, zero);
		Vector4 position = blend4<0, 1, 2, 7>(transform.position, one);

		return Matrix(x, y, z, position);
	}

	ECS_INLINE void ECS_VECTORCALL TransformToMatrix(PackedVectorTransform transform, Matrix& first, Matrix& second) {
		// Extract the rotation basis of transform
		Vector8 x = QuaternionVectorMultiply(transform.rotation, VectorGlobals::RIGHT_8);
		Vector8 y = QuaternionVectorMultiply(transform.rotation, VectorGlobals::UP_8);
		Vector8 z = QuaternionVectorMultiply(transform.rotation, VectorGlobals::FORWARD_8);

		// Scale the basis vectors
		Vector8 splatted_scale_x = permute8<0, 0, 0, 0, 4, 4, 4, 4>(transform.scale);
		Vector8 splatted_scale_y = permute8<1, 1, 1, 1, 5, 5, 5, 5>(transform.scale);
		Vector8 splatted_scale_z = permute8<2, 2, 2, 2, 6, 6, 6, 6>(transform.scale);

		x *= splatted_scale_x;
		y *= splatted_scale_y;
		z *= splatted_scale_z;

		Vector8 zero = ZeroVector8();
		Vector8 one = VectorGlobals::ONE_8;
		x = blend8<0, 1, 2, 11, 4, 5, 6, 15>(x, zero);
		y = blend8<0, 1, 2, 11, 4, 5, 6, 15>(y, zero);
		z = blend8<0, 1, 2, 11, 4, 5, 6, 15>(z, zero);
		Vector8 position = blend8<0, 1, 2, 11, 4, 5, 6, 15>(transform.position, one);

		first = Matrix(x.Low(), y.Low(), z.Low(), position.Low());
		second = Matrix(x.High(), y.High(), z.High(), position.High());
	}

	ECS_INLINE void ECS_VECTORCALL TransformToMatrix(PackedVectorTransform transform, Matrix* matrices) {
		TransformToMatrix(transform, matrices[0], matrices[1]);
	}

	// ---------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Matrix To Transform

	// ---------------------------------------------------------------------------------------------------------------------

	ECS_INLINE Transform ECS_VECTORCALL MatrixToTransform(Matrix matrix) {
		Transform transform;

		Vector4 position = matrix.v[1].High();
		position.StorePartialConstant<3>(&transform.position);
		transform.rotation = MatrixToQuaternion(matrix);

		Matrix rotation_scale_matrix = matrix;

		// Zero out the last elements of the first 3 rows - might be unnecessary since the matrix should
		// already have them 0
		/*Vector8 zero = ZeroVector8();

		rotation_scale_matrix.v[0] = blend8<0, 1, 2, 11, 4, 5, 6, 15>(rotation_scale_matrix.v[0], zero);
		rotation_scale_matrix.v[1] = blend8<0, 1, 2, 11, 4, 5, 6, 7>(rotation_scale_matrix.v[1], zero);*/
		rotation_scale_matrix.v[1] = Vector8(rotation_scale_matrix.v[1].Low(), VectorGlobals::QUATERNION_IDENTITY_4);
		Matrix inverse_rotation = QuaternionToMatrix(QuaternionInverse(transform.rotation));
		Matrix scale_skew_matrix = MatrixMultiply(rotation_scale_matrix, rotation_scale_matrix);

		Vector4 x = scale_skew_matrix.v[0].Low();
		Vector4 y = scale_skew_matrix.v[0].High();
		Vector4 xy = blend4<0, 5, V_DC, V_DC>(x, y);

		Vector4 z = scale_skew_matrix.v[1].Low();
		Vector4 xyz = blend4<0, 1, 6, V_DC>(xy, z);
		xyz.StorePartialConstant<3>(&transform.scale);

		return transform;
	}

	ECS_INLINE VectorTransform ECS_VECTORCALL MatrixToVectorTransform(Matrix matrix)
	{
		VectorTransform transform;

		transform.position = matrix.v[1].High();
		transform.rotation = MatrixToQuaternion(matrix);

		Vector4 zero = ZeroVector4();
		// zero out the last element of the position
		transform.position = blend4<0, 1, 2, 7>(transform.position, zero);

		Matrix rotation_scale_matrix = matrix;
		// Zero out the last elements of the first 3 rows - might be unnecessary since the matrix should
		// already have them 0
		/*Vector8 zero = ZeroVector8();

		rotation_scale_matrix.v[0] = blend8<0, 1, 2, 11, 4, 5, 6, 15>(rotation_scale_matrix.v[0], zero);
		rotation_scale_matrix.v[1] = blend8<0, 1, 2, 11, 4, 5, 6, 7>(rotation_scale_matrix.v[1], zero);*/
		rotation_scale_matrix.v[1] = Vector8(rotation_scale_matrix.v[1].Low(), VectorGlobals::QUATERNION_IDENTITY_4);
		Matrix inverse_rotation = QuaternionToMatrix(QuaternionInverse(transform.rotation));
		Matrix scale_skew_matrix = MatrixMultiply(rotation_scale_matrix, rotation_scale_matrix);

		Vector4 x = scale_skew_matrix.v[0].Low();
		Vector4 y = scale_skew_matrix.v[0].High();
		Vector4 xy = blend4<0, 5, V_DC, V_DC>(x, y);

		Vector4 z = scale_skew_matrix.v[1].Low();
		Vector4 xyz = blend4<0, 1, 6, V_DC>(xy, z);
		xyz = blend4<0, 1, 2, 7>(xyz, zero);
		transform.scale = xyz;

		return transform;
	}

	ECS_INLINE PackedVectorTransform ECS_VECTORCALL MatrixToTransform(Matrix matrix1, Matrix matrix2)
	{
		VectorTransform first = MatrixToVectorTransform(matrix1);
		VectorTransform second = MatrixToVectorTransform(matrix2);

		return { first, second };
	}

	// ---------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Transform Point

	// ---------------------------------------------------------------------------------------------------------------------

	ECS_INLINE Vector4 ECS_VECTORCALL TransformPoint(Transform transform, Vector4 point) {
		// Scale, rotate, translate
		Vector4 scale(transform.scale);
		point = QuaternionVectorMultiply(transform.rotation, scale * point);
		return point + Vector4(transform.position);
	}

	ECS_INLINE Vector4 ECS_VECTORCALL TransformPoint(VectorTransform transform, Vector4 point) {
		// Scale, rotate, translate
		return transform.position + QuaternionVectorMultiply(transform.rotation, transform.scale * point);
	}

	ECS_INLINE Vector8 ECS_VECTORCALL TransformPoint(PackedVectorTransform transform, Vector8 points) {
		// Scale, rotate, translate
		return transform.position + QuaternionVectorMultiply(transform.rotation, transform.scale * points);
	}

	// ---------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Transform Vector

	// ---------------------------------------------------------------------------------------------------------------------

	ECS_INLINE Vector4 ECS_VECTORCALL TransformVector(Transform transform, Vector4 vector) {
		// Scale and rotate
		return QuaternionVectorMultiply(transform.rotation, Vector4(transform.scale) * vector);
	}

	ECS_INLINE Vector4 ECS_VECTORCALL TransformVector(VectorTransform transform, Vector4 vector) {
		// Scale and rotate
		return QuaternionVectorMultiply(transform.rotation, transform.scale * vector);
	}

	ECS_INLINE Vector8 ECS_VECTORCALL TransformVector(PackedVectorTransform transform, Vector8 vectors) {
		// Scale and rotate
		return QuaternionVectorMultiply(transform.rotation, transform.scale * vectors);
	}

	// ---------------------------------------------------------------------------------------------------------------------

#pragma endregion

}