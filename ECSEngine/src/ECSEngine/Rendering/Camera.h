// ECS_REFLECT
#pragma once
#include "../Core.h"
#include "../Utilities/Reflection/ReflectionMacros.h"
#include "../Math/Conversion.h"
#include "../Math/AABB.h"

namespace ECSEngine {

	struct ECSENGINE_API ECS_REFLECT CameraParameters {
		void Default();

		float width;
		float height;
		float near_z;
		float far_z;
		float3 translation;
		float3 rotation;

		bool is_orthographic = false;
	};

	struct ECSENGINE_API ECS_REFLECT CameraParametersFOV {
		void Default();

		// The angle needs to be specified in degrees
		float fov;
		float aspect_ratio;
		float near_z;
		float far_z;
		float3 translation;
		float3 rotation;
	};

#define ECS_CAMERA_DEFAULT_NEAR_Z 0.025f
#define ECS_CAMERA_DEFAULT_FAR_Z 1000.0f

	struct ECSENGINE_API Camera {
		ECS_INLINE Camera() : translation(0.0f, 0.0f, 0.0f), rotation(0.0f, 0.0f, 0.0f), is_orthographic(false), is_perspective_fov(false) {}
		Camera(float3 translation, float3 rotation);
		Camera(Matrix projection, float3 translation, float3 rotation);
		Camera(const CameraParameters& camera_parameters);
		Camera(const CameraParametersFOV& camera_parameters);

		ECS_CLASS_DEFAULT_CONSTRUCTOR_AND_ASSIGNMENT(Camera);

		void SetPerspectiveProjection(float width, float height, float near_z = ECS_CAMERA_DEFAULT_NEAR_Z, float far_z = ECS_CAMERA_DEFAULT_FAR_Z);

		// The FOV angle needs to be expressed in angles
		void SetPerspectiveProjectionFOV(float fov, float aspect_ratio, float near_z = ECS_CAMERA_DEFAULT_NEAR_Z, float far_z = ECS_CAMERA_DEFAULT_FAR_Z);

		void SetOrthographicProjection(float width, float height, float near_z = ECS_CAMERA_DEFAULT_NEAR_Z, float far_z = ECS_CAMERA_DEFAULT_FAR_Z);

		Matrix ECS_VECTORCALL GetViewProjectionMatrix() const;

		ECS_INLINE Matrix ECS_VECTORCALL GetInverseViewProjectionMatrix() const {
			return MatrixInverse(GetViewProjectionMatrix());
		}

		ECS_INLINE Matrix ECS_VECTORCALL GetViewMatrix() const {
			return GetTranslation() * GetRotation();
		}

		ECS_INLINE Matrix ECS_VECTORCALL GetRotation() const {
			// The rotation should be in reversed order, Z, Y and then X
			//return QuaternionRotationMatrix(-rotation);
			return MatrixRotationCamera(rotation);
		}

		ECS_INLINE Matrix ECS_VECTORCALL GetRotationAsIs() const {
			//return QuaternionRotationMatrix(rotation);
			return MatrixRotation(rotation);
		}

		ECS_INLINE QuaternionScalar ECS_VECTORCALL GetRotationQuaternion() const {
			return QuaternionFromEuler(-rotation);
		}

		ECS_INLINE QuaternionScalar ECS_VECTORCALL GetRotationQuaternionAsIs() const {
			return QuaternionFromEuler(rotation);
		}

		ECS_INLINE Matrix ECS_VECTORCALL GetTranslation() const {
			return MatrixTranslation(-translation);
		}

		ECS_INLINE Matrix ECS_VECTORCALL GetProjection() const {
			return projection;
		}

		// The vector is already normalized
		ECS_INLINE Vector3 ECS_VECTORCALL RightVector() const {
			return RotateVector(ECSEngine::RightVector(), GetRotationAsIs());
		}

		// The vector is already normalized
		ECS_INLINE float3 ECS_VECTORCALL GetRightVector() const {
			return RotateVector(ECSEngine::GetRightVector(), GetRotationAsIs());
		}

		// The vector is already normalized
		ECS_INLINE Vector3 ECS_VECTORCALL UpVector() const {
			return RotateVector(ECSEngine::UpVector(), GetRotationAsIs());
		}

		// The vector is already normalized
		ECS_INLINE float3 ECS_VECTORCALL GetUpVector() const {
			return RotateVector(ECSEngine::GetUpVector(), GetRotationAsIs());
		}

		// The vector is already normalized
		ECS_INLINE Vector3 ECS_VECTORCALL ForwardVector() const {
			return RotateVector(ECSEngine::ForwardVector(), GetRotationAsIs());
		}

		// The vector is already normalized
		ECS_INLINE float3 ECS_VECTORCALL GetForwardVector() const {
			return RotateVector(ECSEngine::GetForwardVector(), GetRotationAsIs());
		}

		ECS_INLINE float VerticalFOV() const {
			return fov;
		}

		ECS_INLINE float HorizontalFOV() const {
			return HorizontalFOVFromVertical(fov, aspect_ratio);
		}

		ECS_INLINE float NearZ() const {
			return near_z;
		}

		ECS_INLINE float FarZ() const {
			return far_z;
		}

		ECS_INLINE float AspectRatio() const {
			return aspect_ratio;
		}

		CameraParameters AsParameters() const;

		CameraParametersFOV AsParametersFOV() const;

		bool is_orthographic;
		bool is_perspective_fov;
		union {
			struct {
				float width;
				float height;
			};
			struct {
				float fov;
				float aspect_ratio;
			};
		};
		float near_z;
		float far_z;
		
		float3 translation;
		float3 rotation;
		Matrix projection;
	};

	struct ECSENGINE_API ECS_REFLECT CameraCached {
		ECS_INLINE CameraCached() {}
		CameraCached(const Camera* camera);

		ECS_CLASS_DEFAULT_CONSTRUCTOR_AND_ASSIGNMENT(CameraCached);

		ECS_INLINE Matrix ECS_VECTORCALL GetViewProjectionMatrix() const {
			return view_projection_matrix;
		}

		ECS_INLINE Matrix ECS_VECTORCALL GetInverseViewProjectionMatrix() const {
			return inverse_view_projection_matrix;
		}

		ECS_INLINE Matrix ECS_VECTORCALL GetViewMatrix() const {
			return GetTranslation() * GetRotation();
		}

		ECS_INLINE Matrix ECS_VECTORCALL GetRotation() const {
			// The rotation should be in reversed order, Z, Y and then X
			//return QuaternionRotationMatrix(-rotation);
			return rotation_matrix;
		}

		ECS_INLINE Matrix ECS_VECTORCALL GetRotationAsIs() const {
			//return QuaternionRotationMatrix(rotation);
			return rotation_as_is_matrix;
		}

		ECS_INLINE QuaternionScalar ECS_VECTORCALL GetRotationQuaternion() const {
			return QuaternionFromEuler(-rotation);
		}

		ECS_INLINE QuaternionScalar ECS_VECTORCALL GetRotationQuaternionAsIs() const {
			return QuaternionFromEuler(rotation);
		}

		ECS_INLINE Matrix ECS_VECTORCALL GetTranslation() const {
			return MatrixTranslation(-translation);
		}

		ECS_INLINE Matrix ECS_VECTORCALL GetProjection() const {
			return projection_matrix;
		}

		// The vector is already normalized
		ECS_INLINE Vector3 ECS_VECTORCALL RightVector() const {
			return RotateVector(ECSEngine::RightVector(), rotation_as_is_matrix);
		}

		// The vector is already normalized
		ECS_INLINE float3 ECS_VECTORCALL GetRightVector() const {
			return RotateVector(ECSEngine::GetRightVector(), rotation_as_is_matrix);
		}

		// The vector is already normalized
		ECS_INLINE Vector3 ECS_VECTORCALL UpVector() const {
			return RotateVector(ECSEngine::UpVector(), rotation_as_is_matrix);
		}

		// The vector is already normalized
		ECS_INLINE float3 ECS_VECTORCALL GetUpVector() const {
			return RotateVector(ECSEngine::GetUpVector(), rotation_as_is_matrix);
		}

		// The vector is already normalized
		ECS_INLINE Vector3 ECS_VECTORCALL ForwardVector() const {
			return RotateVector(ECSEngine::ForwardVector(), rotation_as_is_matrix);
		}

		// The vector is already normalized
		ECS_INLINE float3 ECS_VECTORCALL GetForwardVector() const {
			return RotateVector(ECSEngine::GetForwardVector(), rotation_as_is_matrix);
		}

		ECS_INLINE float VerticalFOV() const {
			return fov;
		}

		ECS_INLINE float HorizontalFOV() const {
			return horizontal_fov;
		}

		ECS_INLINE float NearZ() const {
			return near_z;
		}

		ECS_INLINE float FarZ() const {
			return far_z;
		}

		ECS_INLINE float AspectRatio() const {
			return aspect_ratio;
		}

		// Recalculates all the stored matrices
		void Recalculate();

		CameraParameters AsParameters() const;

		CameraParametersFOV AsParametersFOV() const;

		bool is_orthographic;
		bool is_perspective_fov;
		// Store the values without a union in order for this to be reflected by the reflection system
		float width;
		float height;
		float fov;
		float horizontal_fov;
		float aspect_ratio;
		float near_z;
		float far_z;

		float3 translation;
		float3 rotation;

		// This is the rotation that should be applied on objects
		Matrix rotation_matrix; ECS_SKIP_REFLECTION()
		// This is the rotation that is used to get the Right, Up and Forward directions
		Matrix rotation_as_is_matrix; ECS_SKIP_REFLECTION()
		Matrix projection_matrix; ECS_SKIP_REFLECTION()
		Matrix view_projection_matrix; ECS_SKIP_REFLECTION()
		Matrix inverse_view_projection_matrix; ECS_SKIP_REFLECTION()
	};

	ECS_INLINE Matrix ECS_VECTORCALL CombineCameraRotationAndProjection(Matrix rotation_matrix, Matrix projection_matrix) {
		return rotation_matrix * projection_matrix;
	}

	ECS_INLINE Matrix ECS_VECTORCALL CombineCameraTranslationAndRotation(Matrix translation_matrix, Matrix rotation_matrix) {
		return translation_matrix * rotation_matrix;
	}

	ECS_INLINE Matrix ECS_VECTORCALL CombineCameraTranslationAndRotationProjection(Matrix translation_matrix, Matrix rotation_projection_matrix) {
		return translation_matrix * rotation_projection_matrix;
	}

	// This make it easier to write templated code to make sure that the
	// correct method is called and not have to go through recompilation
	// processes because of misspelled names

	template<typename CameraType>
	ECS_INLINE Matrix ECS_VECTORCALL GetCameraViewProjectionMatrix(const CameraType* camera) {
		return camera->GetViewProjectionMatrix();
	}

	template<typename CameraType>
	ECS_INLINE Matrix ECS_VECTORCALL GetCameraInverseViewProjectionMatrix(const CameraType* camera) {
		return camera->GetInverseViewProjectionMatrix();
	}

	template<typename CameraType>
	ECS_INLINE Matrix ECS_VECTORCALL GetCameraViewMatrix(const CameraType* camera) {
		return camera->GetViewMatrix();
	}

	template<typename CameraType>
	ECS_INLINE Matrix ECS_VECTORCALL GetCameraRotation(const CameraType* camera) {
		return camera->GetRotation();
	}

	template<typename CameraType>
	ECS_INLINE Matrix ECS_VECTORCALL GetCameraRotationAsIs(const CameraType* camera) {
		return camera->GetRotationAsIs();
	}

	template<typename CameraType>
	ECS_INLINE Quaternion ECS_VECTORCALL GetCameraRotationQuaternion(const CameraType* camera) {
		return camera->GetRotationQuaternion();
	}

	template<typename CameraType>
	ECS_INLINE Quaternion ECS_VECTORCALL GetCameraRotationQuaternionAsIs(const CameraType* camera) {
		return camera->GetRotationQuaternionAsIs();
	}

	template<typename CameraType>
	ECS_INLINE Matrix ECS_VECTORCALL GetCameraTranslation(const CameraType* camera) {
		return camera->GetTranslation();
	}

	template<typename CameraType>
	ECS_INLINE Matrix ECS_VECTORCALL GetCameraProjection(const CameraType* camera) {
		return camera->GetProjection();
	}

	// The vector is already normalized
	template<typename CameraType>
	ECS_INLINE Vector3 ECS_VECTORCALL GetCameraRightVector(const CameraType* camera) {
		return camera->RightVector();
	}

	// The vector is already normalized
	template<typename CameraType>
	ECS_INLINE float3 ECS_VECTORCALL GetCameraRightVectorScalar(const CameraType* camera) {
		return camera->GetRightVector();
	}

	// The vector is already normalized
	template<typename CameraType>
	ECS_INLINE Vector3 ECS_VECTORCALL GetCameraUpVector(const CameraType* camera) {
		return camera->UpVector();
	}

	// The vector is already normalized
	template<typename CameraType>
	ECS_INLINE float3 ECS_VECTORCALL GetCameraUpVectorScalar(const CameraType* camera) {
		return camera->GetUpVector();
	}

	// The vector is already normalized
	template<typename CameraType>
	ECS_INLINE Vector3 ECS_VECTORCALL GetCameraForwardVector(const CameraType* camera) {
		return camera->ForwardVector();
	}

	// The vector is already normalized
	template<typename CameraType>
	ECS_INLINE float3 ECS_VECTORCALL GetCameraForwardVectorScalar(const CameraType* camera) {
		return camera->GetForwardVector();
	}

	template<typename CameraType>
	ECS_INLINE float GetCameraHorizontalFOV(const CameraType* camera) {
		return camera->HorizontalFOV();
	}

	template<typename CameraType>
	ECS_INLINE float GetCameraVerticalFOV(const CameraType* camera) {
		return camera->VerticalFOV();
	}

	template<typename CameraType>
	ECS_INLINE float GetCameraNearZ(const CameraType* camera) {
		return camera->NearZ();
	}

	template<typename CameraType>
	ECS_INLINE float GetCameraFarZ(const CameraType* camera) {
		return camera->FarZ();
	}

	template<typename CameraType>
	ECS_INLINE float GetCameraAspectRatio(const CameraType* camera) {
		return camera->AspectRatio();
	}

	// Converts the mouse position into [-1, 1] range if inside, else accordingly
	ECSENGINE_API float2 MouseToNDC(uint2 window_size, int2 mouse_texel_position);

	// Converts the mouse position into [-1, 1] range if inside, else accordingly
	ECSENGINE_API float2 MouseToNDC(float2 window_size, float2 mouse_texel_position);

	// Position needs to be in range [-1, 1]
	ECSENGINE_API int2 NDCToViewportTexels(uint2 window_size, float2 position);

	// Returns the direction of the ray that passes through the current cursor position
	// Mouse texel positions must be relative to the top left corner
	// At the moment this works only for perspective FOV cameras
	template<typename CameraType>
	ECSENGINE_API float3 ViewportToWorldRayDirection(const CameraType* camera, uint2 window_size, int2 mouse_texel_position);

	// Projects the point into texel coordinates relative to the top left corner of the viewport
	template<typename CameraType>
	ECSENGINE_API int2 PositionToViewportTexels(const CameraType* camera, uint2 viewport_size, float3 position);

	// Returns the translation such that the camera can see the object from a given fixed distance
	// Right on the given point
	template<typename CameraType>
	ECSENGINE_API float3 FocusCameraOnObject(const CameraType* camera, float3 object_translation, float distance);

	// Returns the translation such that the camera can see the object with the proportion on the screen specified
	// Only one of the X or Y proportion can be active at a time. This is helpful if you wanna focus on objects
	// Regardless of their orientation and size. If the proportion cannot be reached, then it returns Splat(FLT_MAX)
	template<typename CameraType>
	ECSENGINE_API float3 FocusCameraOnObjectViewSpace(
		const CameraType* camera, 
		float3 object_translation, 
		QuaternionScalar object_rotation, 
		float3 object_scale,
		AABBScalar object_bounds,
		float2 view_space_proportion
	);

	ECSENGINE_API float2 GetCameraFrustumPlaneDimensions(float vertical_fov, float aspect_ratio, float z_depth);

	// Calculates the dimensions at multiple depths. The dimensions pointer must have z_depth.size entries
	ECSENGINE_API void GetCameraFrustumPlaneDimensions(float vertical_fov, float aspect_ratio, Stream<float> z_depth, float2* dimensions);

	template<typename CameraType>
	ECSENGINE_API float2 GetCameraFrustumNearPlaneDimensions(const CameraType* camera);

	template<typename CameraType>
	ECSENGINE_API float2 GetCameraFrustumFarPlaneDimensions(const CameraType* camera);

	// The first 2 components are for the near plane, the other 2 for the far plane
	template<typename CameraType>
	ECSENGINE_API float4 GetCameraFrustumNearAndFarPlaneDimensions(const CameraType* camera);

	struct FrustumPoints {
		Rectangle3D near_plane;
		Rectangle3D far_plane;
	};

	// Calculates the camera frustum points
	template<typename CameraType>
	ECSENGINE_API FrustumPoints GetCameraFrustumPoints(const CameraType* camera);

}