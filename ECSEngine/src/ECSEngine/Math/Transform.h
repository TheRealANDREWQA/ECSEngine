#pragma once
#include "Quaternion.h"
#include "Matrix.h"

#define ECS_TRANSFORM_SCALE_EPSILON 0.00005f

namespace ECSEngine {

	// The rotation is a quaternion lane
	struct TransformScalar {
		ECS_INLINE TransformScalar() {}
		ECS_INLINE TransformScalar(float3 _position, QuaternionScalar _rotation, float3 _scale) : position(_position),
			rotation(_rotation), scale(_scale) {}

		// Translation - 0.0f, 0.0f, 0.0f
		// Scale - 1.0f, 1.0f, 1.0f
		// Rotation - Quaternion Identity
		ECS_INLINE void Default() {
			position = { 0.0f, 0.0f, 0.0f };
			rotation = QuaternionIdentityScalar();
			scale = { 1.0f, 1.0f, 1.0f };
		}

		float3 position;
		QuaternionScalar rotation;
		float3 scale;
	};

	struct Transform {
		ECS_INLINE Transform() {}
		ECS_INLINE Transform(Vector3 _position, Quaternion _rotation, Vector3 _scale) : position(_position),
			rotation(_rotation), scale(_scale) {}

		// Translation - 0.0f, 0.0f, 0.0f
		// Scale - 1.0f, 1.0f, 1.0f
		// Rotation - Quaternion Identity
		ECS_INLINE void Default() {
			position = ZeroVector();
			rotation = QuaternionIdentity();
			scale = Vector3::Splat(VectorGlobals::ONE);
		}

		ECS_INLINE TransformScalar At(size_t index) const {
			return { position.At(index), rotation.At(index), scale.At(index) };
		}

		ECS_INLINE void Set(const TransformScalar* scalar, size_t index) {
			position.Set(scalar->position, index);
			scale.Set(scalar->scale, index);
			rotation.Set(scalar->rotation, index);
		}

		Vector3 position;
		Quaternion rotation;
		Vector3 scale;
	};

	// ---------------------------------------------------------------------------------------------------------------------

	ECSENGINE_API TransformScalar TransformCombine(const TransformScalar* parent, const TransformScalar* child);

	ECSENGINE_API Transform TransformCombine(const Transform* parent, const Transform* child);

	// ---------------------------------------------------------------------------------------------------------------------

	ECSENGINE_API TransformScalar TransformInverse(const TransformScalar* transform);

	ECSENGINE_API Transform TransformInverse(const Transform* transform);

	// ---------------------------------------------------------------------------------------------------------------------

	// Linear blend
	ECSENGINE_API TransformScalar TransformMix(const TransformScalar* from, const TransformScalar* to, float percentage);

	// Linear blend
	ECSENGINE_API Transform TransformMix(const Transform* from, const Transform* to, Vec8f percentage);

	// ---------------------------------------------------------------------------------------------------------------------
	
	ECSENGINE_API Matrix ECS_VECTORCALL TransformToMatrix(const TransformScalar* transform);

	ECSENGINE_API void TransformToMatrix(const Transform* transform, Matrix* matrices, size_t write_count);

	// ---------------------------------------------------------------------------------------------------------------------

	ECSENGINE_API TransformScalar ECS_VECTORCALL MatrixToTransform(Matrix matrix);

	ECSENGINE_API void ECS_VECTORCALL MatrixToTransform(Matrix matrix, Transform* transform, size_t index);

	// ---------------------------------------------------------------------------------------------------------------------
	
	// Use these only if you have a one off transformation. For repeated transformations,
	// convert into a matrix and the transform the points
	ECSENGINE_API float3 ECS_VECTORCALL TransformPoint(const TransformScalar* transform, float3 point);

	// Use these only if you have a one off transformation. For repeated transformations,
	// convert into a matrix and the transform the points
	ECSENGINE_API Vector3 ECS_VECTORCALL TransformPoint(const Transform* transform, Vector3 point);

	// ---------------------------------------------------------------------------------------------------------------------

	// Use these only if you have a one off transformation. For repeated transformations,
	// convert into a matrix and the transform the vectors
	ECSENGINE_API float3 ECS_VECTORCALL TransformVector(const TransformScalar* transform, float3 vector);

	// Use these only if you have a one off transformation. For repeated transformations,
	// convert into a matrix and the transform the vectors
	ECSENGINE_API Vector3 ECS_VECTORCALL TransformVector(const Transform* transform, Vector3 vector);

	// ---------------------------------------------------------------------------------------------------------------------

	// It will first select a slice inside the XZ plane using the rotation given by the x component
	// Then it will rotate in the plane determined by the diameter of that rotation and the Y axis
	// The point is centered around the origin, if a translation is desired, it should be added afterwards
	// Rotation is in degrees
	ECSENGINE_API float3 ECS_VECTORCALL OrbitPoint(float radius, float2 rotation);

	// ---------------------------------------------------------------------------------------------------------------------

}