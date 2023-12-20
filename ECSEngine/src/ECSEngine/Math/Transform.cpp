#include "ecspch.h"
#include "Transform.h"
#include "Conversion.h"

namespace ECSEngine {

	// ---------------------------------------------------------------------------------------------------------------------

	VectorTransform ECS_VECTORCALL TransformCombine(VectorTransform parent, VectorTransform child) {
		VectorTransform result;

		// Rotation and scale can be composed by multiplication
		result.rotation = QuaternionMultiply(child.rotation, parent.rotation);
		result.scale = parent.scale * child.scale;

		// The position must be transformed by the parent's rotation and then translated
		result.position = QuaternionVectorMultiply(parent.rotation, parent.scale * child.position);
		result.position += parent.position;

		return result;
	}

	// ---------------------------------------------------------------------------------------------------------------------

	VectorTransform ECS_VECTORCALL TransformInverse(VectorTransform transform) {
		VectorTransform result;

		result.rotation = QuaternionInverse(transform.rotation);
		result.scale = Select(abs(transform.scale) < Vector8(ECS_TRANSFORM_SCALE_EPSILON), ZeroVector(), VectorGlobals::ONE / transform.scale);
		// The position needs to be negated
		result.position = PerLaneChangeSign<1, 1, 1, 0>(transform.position);

		// And afterwards we need to rotate this current position according to the scale and the quaternion
		result.position = QuaternionVectorMultiply(transform.rotation, result.scale * result.position);

		return result;
	}

	// ---------------------------------------------------------------------------------------------------------------------

	VectorTransform ECS_VECTORCALL TransformMix(VectorTransform from, VectorTransform to, Vector8 percentage) {
		VectorTransform result;

		result.position = Lerp(from.position, to.position, percentage);
		result.scale = Lerp(from.scale, to.scale, percentage);
		result.rotation = QuaternionNLerpNeighbour(from.rotation, to.rotation, percentage);

		return result;
	}

	// ---------------------------------------------------------------------------------------------------------------------

	void ECS_VECTORCALL TransformToMatrix(VectorTransform transform, Matrix* low, Matrix* high) {
		// Extract the rotation basis of transform
		Vector8 x = RotateVectorQuaternionSIMD(RightVector(), transform.rotation);
		Vector8 y = RotateVectorQuaternionSIMD(UpVector(), transform.rotation);
		Vector8 z = RotateVectorQuaternionSIMD(ForwardVector(), transform.rotation);

		// Scale the basis vectors
		Vector8 splatted_scale_x = PerLaneBroadcast<0>(transform.scale);
		Vector8 splatted_scale_y = PerLaneBroadcast<1>(transform.scale);
		Vector8 splatted_scale_z = PerLaneBroadcast<2>(transform.scale);

		x *= splatted_scale_x;
		y *= splatted_scale_y;
		z *= splatted_scale_z;

		Vector8 zero = ZeroVector();
		Vector8 one = VectorGlobals::ONE;
		x = PerLaneBlend<0, 1, 2, 7>(x, zero);
		y = PerLaneBlend<0, 1, 2, 7>(y, zero);
		z = PerLaneBlend<0, 1, 2, 7>(z, zero);
		Vector8 position = PerLaneBlend<0, 1, 2, 7>(transform.position, one);

		*low = Matrix(
			Permute2f128Helper<0, 2>(x, y),
			Permute2f128Helper<0, 2>(z, position)
		);

		*high = Matrix(
			Permute2f128Helper<1, 3>(x, y),
			Permute2f128Helper<1, 3>(z, position)
		);
	}

	Matrix ECS_VECTORCALL TransformToMatrixLow(VectorTransform transform) {
		Quaternion splatted_rotation = Permute2f128Helper<0, 0>(transform.rotation, transform.rotation);

		Vector8 right_up = BlendLowAndHigh(RightVector(), UpVector());

		// Extract the rotation basis of transform
		Vector8 xy = RotateVectorQuaternionSIMD(right_up, splatted_rotation);
		Vector8 z = RotateVectorQuaternionSIMD(ForwardVector(), transform.rotation);

		// Scale the basis vectors
		Vector8 splatted_scale_x = PerLaneBroadcast<0>(transform.scale);
		Vector8 splatted_scale_y = PerLaneBroadcast<1>(transform.scale);
		Vector8 splatted_scale_z = PerLaneBroadcast<2>(transform.scale);

		Vector8 splatted_scale_xy = Permute2f128Helper<0, 2>(splatted_scale_x, splatted_scale_y);
		xy *= splatted_scale_xy;
		z *= splatted_scale_z;

		Vector8 zero = ZeroVector();
		Vector8 one = VectorGlobals::ONE;
		xy = PerLaneBlend<0, 1, 2, 7>(xy, zero);
		z = PerLaneBlend<0, 1, 2, 7>(z, zero);
		Vector8 position = PerLaneBlend<0, 1, 2, 7>(transform.position, one);

		return Matrix(
			xy,
			Permute2f128Helper<0, 2>(z, position)
		);
	}

	// ---------------------------------------------------------------------------------------------------------------------

	VectorTransform ECS_VECTORCALL MatrixToVectorTransform(Matrix matrix)
	{
		VectorTransform transform;

		transform.position = Permute2f128Helper<1, 1>(matrix.v[1], matrix.v[1]);
		transform.rotation = MatrixToQuaternion(matrix);

		Vector8 zero = ZeroVector();
		// Zero out the last element of the position
		transform.position = PerLaneBlend<0, 1, 2, 7>(transform.position, zero);

		Matrix rotation_scale_matrix = matrix;
		// Zero out the last elements of the first 3 rows - might be unnecessary since the matrix should
		// already have them 0
		/*Vector8 zero = ZeroVector8();

		rotation_scale_matrix.v[0] = blend8<0, 1, 2, 11, 4, 5, 6, 15>(rotation_scale_matrix.v[0], zero);
		rotation_scale_matrix.v[1] = blend8<0, 1, 2, 11, 4, 5, 6, 7>(rotation_scale_matrix.v[1], zero);*/

		Vector8 quat_identity = LastElementOneVector();
		rotation_scale_matrix.v[1] = BlendLowAndHigh(rotation_scale_matrix.v[1], quat_identity);
		Matrix inverse_rotation = QuaternionToMatrixLow(QuaternionInverse(transform.rotation));
		Matrix scale_skew_matrix = MatrixMultiply(rotation_scale_matrix, inverse_rotation);

		// Extract the scale from the principal diagonal
		Vector8 yz_shuffle = Permute2f128Helper<1, 2>(scale_skew_matrix.v[0], scale_skew_matrix.v[1]);
		Vector8 xyz = blend8<0, 9, 10, V_DC, V_DC, V_DC, V_DC, V_DC>(scale_skew_matrix.v[0], yz_shuffle);
		xyz = PerLaneBlend<0, 1, 2, 7>(xyz, zero);
		transform.scale = xyz;

		return transform;
	}

	VectorTransform ECS_VECTORCALL MatrixToTransform(Matrix matrix1, Matrix matrix2)
	{
		VectorTransform transform;

		transform.position = Permute2f128Helper<1, 3>(matrix1.v[1], matrix2.v[1]);
		transform.rotation = MatrixToQuaternion(matrix1, matrix2);

		Vector8 zero = ZeroVector();
		// Zero out the last element of the position
		transform.position = PerLaneBlend<0, 1, 2, 7>(transform.position, zero);

		Matrix rotation_scale_matrix1 = matrix1;
		Matrix rotation_scale_matrix2 = matrix2;
		// Zero out the last elements of the first 3 rows - might be unnecessary since the matrix should
		// already have them 0
		/*Vector8 zero = ZeroVector8();

		rotation_scale_matrix.v[0] = blend8<0, 1, 2, 11, 4, 5, 6, 15>(rotation_scale_matrix.v[0], zero);
		rotation_scale_matrix.v[1] = blend8<0, 1, 2, 11, 4, 5, 6, 7>(rotation_scale_matrix.v[1], zero);*/

		Vector8 quat_identity = LastElementOneVector();
		rotation_scale_matrix1.v[1] = BlendLowAndHigh(rotation_scale_matrix1.v[1], quat_identity);
		rotation_scale_matrix2.v[1] = BlendLowAndHigh(rotation_scale_matrix2.v[1], quat_identity);

		Matrix inverse_rotation1, inverse_rotation2;
		QuaternionToMatrix(QuaternionInverse(transform.rotation), &inverse_rotation1, &inverse_rotation2);
		Matrix scale_skew_matrix1 = MatrixMultiply(rotation_scale_matrix1, inverse_rotation1);
		Matrix scale_skew_matrix2 = MatrixMultiply(rotation_scale_matrix2, inverse_rotation2);

		// Extract the scale from the principal diagonal
		Vector8 yz_shuffle1 = Permute2f128Helper<1, 2>(scale_skew_matrix1.v[0], scale_skew_matrix1.v[1]);
		Vector8 yz_shuffle2 = Permute2f128Helper<1, 2>(scale_skew_matrix2.v[0], scale_skew_matrix2.v[1]);
		Vector8 xyz1 = blend8<0, 9, 10, V_DC, V_DC, V_DC, V_DC, V_DC>(scale_skew_matrix1.v[0], yz_shuffle1);
		Vector8 xyz2 = blend8<0, 9, 10, V_DC, V_DC, V_DC, V_DC, V_DC>(scale_skew_matrix2.v[0], yz_shuffle2);
		xyz1 = PerLaneBlend<0, 1, 2, 7>(xyz2, zero);
		xyz2 = PerLaneBlend<0, 1, 2, 7>(xyz2, zero);
		transform.scale = Permute2f128Helper<0, 2>(xyz1, xyz2);

		return transform;
	}

	// ---------------------------------------------------------------------------------------------------------------------

	Vector8 ECS_VECTORCALL TransformPoint(VectorTransform transform, Vector8 point) {
		// Scale, rotate, translate
		return transform.position + QuaternionVectorMultiply(transform.rotation, transform.scale * point);
	}

	// ---------------------------------------------------------------------------------------------------------------------

	Vector8 ECS_VECTORCALL TransformVector(VectorTransform transform, Vector8 vector) {
		// Scale and rotate
		return QuaternionVectorMultiply(transform.rotation, transform.scale * vector);
	}

	// ---------------------------------------------------------------------------------------------------------------------

	float3 OrbitPoint(float radius, float2 rotation) {
		Matrix matrix = MatrixRotation({ rotation.x, rotation.y, 0.0f });
		Vector8 new_position = TransformPoint(float3(0.0f, 0.0f, -radius), matrix);
		return new_position.AsFloat3Low();
	}

	// ---------------------------------------------------------------------------------------------------------------------

}