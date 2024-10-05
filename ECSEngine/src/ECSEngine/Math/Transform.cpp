#include "ecspch.h"
#include "Transform.h"
#include "Conversion.h"

namespace ECSEngine {

	// ---------------------------------------------------------------------------------------------------------------------

	template<typename Transform>
	ECS_INLINE static Transform TransformCombineImpl(const Transform* parent, const Transform* child) {
		Transform result;

		// Rotation and scale can be composed by multiplication
		result.rotation = QuaternionMultiply(child->rotation, parent->rotation);
		result.scale = parent->scale * child->scale;

		// The position must be transformed by the parent's rotation and then translated
		result.position = QuaternionVectorMultiply(parent->rotation, parent->scale * child->position);
		result.position += parent->position;

		return result;
	}

	TransformScalar TransformCombine(const TransformScalar* parent, const TransformScalar* child) {
		return TransformCombineImpl(parent, child);
	}

	Transform TransformCombine(const Transform* parent, const Transform* child) {
		return TransformCombineImpl(parent, child);
	}

	// ---------------------------------------------------------------------------------------------------------------------

	template<typename SingleValue, typename Transform>
	ECS_INLINE static Transform TransformInverseImpl(const Transform* transform) {
		Transform result;

		result.rotation = QuaternionInverse(transform->rotation, Vec8f::size());

		auto zero_vector = SingleZeroVector<SingleValue>();
		auto epsilon = SingleValueVector<SingleValue>(ECS_TRANSFORM_SCALE_EPSILON);
		auto smaller_mask_x = CompareMaskSingle(transform->scale.x, zero_vector, epsilon);
		auto smaller_mask_y = CompareMaskSingle(transform->scale.y, zero_vector, epsilon);
		auto smaller_mask_z = CompareMaskSingle(transform->scale.z, zero_vector, epsilon);
		auto one_divided_vector = OneDividedVector(transform->scale);
		result.scale.x = SelectSingle<SingleValue>(smaller_mask_x, zero_vector, one_divided_vector.x);
		result.scale.y = SelectSingle<SingleValue>(smaller_mask_y, zero_vector, one_divided_vector.y);
		result.scale.z = SelectSingle<SingleValue>(smaller_mask_z, zero_vector, one_divided_vector.z);

		// The position needs to be negated
		result.position = -transform->position;

		// And afterwards we need to rotate this current position according to the scale and the quaternion
		result.position = QuaternionVectorMultiply(transform->rotation, transform->scale * result.position);

		return result;
	}

	TransformScalar TransformInverse(const TransformScalar* transform) {
		return TransformInverseImpl<float>(transform);
	}

	Transform TransformInverse(const Transform* transform) {
		return TransformInverseImpl<Vec8f>(transform);
	}

	// ---------------------------------------------------------------------------------------------------------------------

	template<typename Transform, typename PercentageType>
	ECS_INLINE static Transform TransformMixImpl(const Transform* from, const Transform* to, PercentageType percentage) {
		Transform result;

		result.position = Lerp(from->position, to->position, percentage);
		result.scale = Lerp(from->scale, to->scale, percentage);
		result.rotation = QuaternionNLerpNeighbour(from->rotation, to->rotation, percentage);

		return result;
	}

	TransformScalar TransformMix(const TransformScalar* from, const TransformScalar* to, float percentage) {
		return TransformMixImpl(from, to, percentage);
	}

	Transform TransformMix(const Transform* from, const Transform* to, Vec8f percentage) {
		return TransformMixImpl(from, to, percentage);
	}

	// ---------------------------------------------------------------------------------------------------------------------

	Matrix ECS_VECTORCALL TransformToMatrix(const TransformScalar* transform) {
		// Extract the rotation basis of transform
		float3 x = RotateVector(GetRightVector(), transform->rotation);
		float3 y = RotateVector(GetUpVector(), transform->rotation);
		float3 z = RotateVector(GetForwardVector(), transform->rotation);

		// Scale the basis vectors
		x *= transform->scale.x;
		y *= transform->scale.y;
		z *= transform->scale.z;

		// Extract the position and create the matrix as the basis vectors
		// As rows and the position with 1.0f as the final row
		// So the final matrix looks like (p is the position)
		// { 
		//		x.x, x.y, x.z, 0.0f
		//		y.x, y.y, y.z, 0.0f
		//		z.x, z.y, z.z, 0.0f
		//		p.x, p.y, p.z, 1.0f
		// }

		float3 position = transform->position;

		alignas(ECS_SIMD_BYTE_SIZE) float4 matrix_elements[sizeof(Matrix) / sizeof(float4)];
		matrix_elements[0] = { x, 0.0f };
		matrix_elements[1] = { y, 0.0f };
		matrix_elements[2] = { z, 0.0f };
		matrix_elements[3] = { position, 1.0f };
		return Matrix().LoadAligned(matrix_elements);
	}

	void TransformToMatrix(const Transform* transform, Matrix* matrices, size_t write_count) {
		// Extract the rotation basis of transform
		Vector3 x = RotateVector(RightVector(), transform->rotation);
		Vector3 y = RotateVector(UpVector(), transform->rotation);
		Vector3 z = RotateVector(ForwardVector(), transform->rotation);

		// Scale the basis vectors
		x *= transform->scale.x;
		y *= transform->scale.y;
		z *= transform->scale.z;
		
		// Extract the position and create the matrix as the basis vectors
		// As rows and the position with 1.0f as the final row
		// So the final matrix looks like (p is the position)
		// { 
		//		x.x, x.y, x.z, 0.0f
		//		y.x, y.y, y.z, 0.0f
		//		z.x, z.y, z.z, 0.0f
		//		p.x, p.y, p.z, 1.0f
		// }
		Vector3 position = transform->position;

		const size_t MATRIX_ELEMENT_COUNT = sizeof(Matrix) / sizeof(float);
		int store_indices[Vec8f::size()] = { -1 };

		auto store_vector = [matrices, write_count, &store_indices](Vec8f vector) {
			for (size_t index = 0; index < write_count; index++) {
				store_indices[index]++;
			}
			Scatter(vector, matrices, store_indices, write_count);
		};

		Vec8f zero = ZeroVectorFloat();
		// The first row
		for (size_t index = 0; index < write_count; index++) {
			store_indices[index] = index * MATRIX_ELEMENT_COUNT;
		}
		Scatter(x.x, matrices, store_indices, write_count);
		store_vector(x.y);
		store_vector(x.z);
		store_vector(zero);

		// The second row
		store_vector(y.x);
		store_vector(y.y);
		store_vector(y.z);
		store_vector(zero);

		// The third row
		store_vector(z.x);
		store_vector(z.y);
		store_vector(z.z);
		store_vector(zero);

		// The fourth row
		store_vector(position.x);
		store_vector(position.y);
		store_vector(position.z);
		store_vector(VectorGlobals::ONE);
	}

	// ---------------------------------------------------------------------------------------------------------------------

	TransformScalar ECS_VECTORCALL MatrixToTransform(Matrix matrix)
	{
		// The position is simply the last row
		// In order to extract the rotation and scale, we need to
		// get the rotation quaternion from the matrix, then create
		// a matrix from its inverse and then multiply the existing matrix
		// (but with the position zeroed out) with that matrix
		// The values on the diagonal are the scale, and the rotation
		// Is simply the initial quaternion

		alignas(ECS_SIMD_BYTE_SIZE) float4 matrix_elements[sizeof(Matrix) / sizeof(float4)];
		matrix.StoreAligned(matrix_elements);

		float3 position = matrix_elements[3].xyz();
		QuaternionScalar rotation = MatrixToQuaternion(matrix);
		Matrix inverse_rotation_matrix = QuaternionToMatrix(QuaternionInverseNormalized(rotation));
		matrix_elements[3] = { 0.0f, 0.0f, 0.0f, 1.0f };
		Matrix no_position_matrix = Matrix().LoadAligned(matrix_elements);
		// The order is the current rotation multiplied with the inverse matrix
		Matrix scale_skew_matrix = no_position_matrix * inverse_rotation_matrix;
		scale_skew_matrix.StoreAligned(matrix_elements);
		
		float3 scale = { matrix_elements[0][0], matrix_elements[1][1], matrix_elements[2][2] };
		return { position, rotation, scale };
	}

	void ECS_VECTORCALL MatrixToTransform(Matrix matrix, Transform* transform, size_t index) {
		TransformScalar scalar = MatrixToTransform(matrix);
		transform->Set(&scalar, index);
	}

	// ---------------------------------------------------------------------------------------------------------------------

	float3 ECS_VECTORCALL TransformPoint(const TransformScalar* transform, float3 point) {
		// Scalar, rotate, translate
		return transform->position + QuaternionVectorMultiply(transform->rotation, transform->scale * point);
	}

	Vector3 ECS_VECTORCALL TransformPoint(const Transform* transform, Vector3 point) {
		// Scale, rotate, translate
		return transform->position + QuaternionVectorMultiply(transform->rotation, transform->scale * point);
	}

	// ---------------------------------------------------------------------------------------------------------------------

	float3 ECS_VECTORCALL TransformVector(const TransformScalar* transform, float3 vector) {
		// Scale and rotate
		return QuaternionVectorMultiply(transform->rotation, transform->scale * vector);
	}

	Vector3 ECS_VECTORCALL TransformVector(const Transform* transform, Vector3 vector) {
		// Scale and rotate
		return QuaternionVectorMultiply(transform->rotation, transform->scale * vector);
	}

	// ---------------------------------------------------------------------------------------------------------------------

	float3 ECS_VECTORCALL OrbitPoint(float radius, float2 rotation) {
		Matrix matrix = MatrixRotation({ rotation.x, rotation.y, 0.0f });
		float4 new_position = TransformPoint(float3(0.0f, 0.0f, -radius), matrix);
		return new_position.xyz();
	}

	// ---------------------------------------------------------------------------------------------------------------------

}