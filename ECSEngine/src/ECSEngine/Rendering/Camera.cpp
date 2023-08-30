#include "ecspch.h"
#include "Camera.h"

namespace ECSEngine {

	Camera::Camera(float3 _translation, float3 _rotation) : translation(_translation), rotation(_rotation), is_orthographic(false), is_perspective_fov(false) {}

	Camera::Camera(Matrix _projection, float3 _translation, float3 _rotation) : projection(_projection),
		translation(_translation), rotation(_rotation), is_orthographic(false), is_perspective_fov(false) {}

	Camera::Camera(const CameraParameters& parameters) : is_perspective_fov(false) {
		translation = parameters.translation;
		rotation = parameters.rotation;

		if (parameters.is_orthographic) {
			is_orthographic = true;
			SetOrthographicProjection(parameters.width, parameters.height, parameters.near_z, parameters.far_z);
		}
		else {
			is_orthographic = false;
			SetPerspectiveProjection(parameters.width, parameters.height, parameters.near_z, parameters.far_z);
		}
	}

	Camera::Camera(const CameraParametersFOV& parameters) : is_perspective_fov(true) {
		translation = parameters.translation;
		rotation = parameters.rotation;

		SetPerspectiveProjectionFOV(parameters.fov, parameters.aspect_ratio, parameters.near_z, parameters.far_z);
	}

	void Camera::SetOrthographicProjection(float _width, float _height, float near_z, float far_z) {
		is_orthographic = true;
		is_perspective_fov = false;

		width = _width;
		height = _height;
		projection = MatrixOrthographic(width, height, near_z, far_z);
	}

	void Camera::SetPerspectiveProjection(float _width, float _height, float near_z, float far_z) {
		is_orthographic = false;
		is_perspective_fov = false;

		width = _width;
		height = _height;
		projection = MatrixPerspective(width, height, near_z, far_z);
	}

	void Camera::SetPerspectiveProjectionFOV(float _fov, float _aspect_ratio, float near_z, float far_z) {
		is_orthographic = false;
		is_perspective_fov = true;

		fov = _fov;
		aspect_ratio = _aspect_ratio;
		projection = MatrixPerspectiveFOV(fov, aspect_ratio, near_z, far_z);
	}

	Matrix ECS_VECTORCALL Camera::GetViewProjectionMatrix() const {
		return GetTranslation() * GetRotation() * GetProjection();
	}

	void CameraParameters::Default()
	{
		translation = { 0.0f, 0.0f, 0.0f };
		rotation = { 0.0f, 0.0f, 0.0f };

		is_orthographic = false;
		width = 0.0f;
		height = 0.0f;
		near_z = ECS_CAMERA_DEFAULT_NEAR_Z;
		far_z = ECS_CAMERA_DEFAULT_FAR_Z;
	}

	void CameraParametersFOV::Default()
	{
		translation = { 0.0f, 0.0f, 0.0f };
		rotation = { 0.0f, 0.0f, 0.0f };

		fov = 60.0f;
		aspect_ratio = 16.0f / 9.0f;
		near_z = ECS_CAMERA_DEFAULT_NEAR_Z;
		far_z = ECS_CAMERA_DEFAULT_FAR_Z;
	}

	CameraCached::CameraCached(const Camera* camera)
	{
		is_orthographic = camera->is_orthographic;
		is_perspective_fov = camera->is_perspective_fov;
		translation = camera->translation;
		rotation = camera->rotation;

		if (is_orthographic || !is_perspective_fov) {
			width = camera->width;
			height = camera->height;
		}
		else {
			fov = camera->fov;
			aspect_ratio = camera->aspect_ratio;
			horizontal_fov = HorizontalFOVFromVertical(fov, aspect_ratio);
		}

		rotation_matrix = camera->GetRotation();
		rotation_as_is_matrix = camera->GetRotationAsIs();
		projection_matrix = camera->projection;
		view_projection_matrix = camera->GetViewProjectionMatrix();
		inverse_view_projection_matrix = camera->GetInverseViewProjectionMatrix();
	}

	float2 MouseToNDC(uint2 window_size, int2 mouse_texel_position)
	{
		return MouseToNDC(float2(window_size), float2(mouse_texel_position));
	}

	float2 MouseToNDC(float2 window_size, float2 mouse_texel_position)
	{
		return float2(2.0f * mouse_texel_position.x / window_size.x - 1.0f, 1.0f - 2.0f * mouse_texel_position.y / window_size.y);
	}

	int2 NDCToViewportTexels(uint2 window_size, float2 position)
	{
		float2 percentage = { position.x * 0.5f + 0.5f, 0.5f - position.y * 0.5f };
		return int2(float2(window_size) * percentage);
	}

	// We cannot use the horizontal and vertical fov to determine the position
	// Since that will result in interpolation of angles - and we would have to
	// use spherical interpolation which would be expensive
	template<typename CameraType>
	float3 ViewportToWorldRayDirection(const CameraType* camera, uint2 window_size, int2 mouse_texel_position)
	{
		float2 normalized_values = MouseToNDC(window_size, mouse_texel_position);
		float4 near_clip_space_position(normalized_values.x, normalized_values.y, -1.0f, 1.0f);
		Matrix inverse_matrix = GetCameraInverseViewProjectionMatrix(camera);
		Vector8 eye_space_pos = MatrixVectorMultiplyLow(near_clip_space_position, inverse_matrix);
		eye_space_pos /= PerLaneBroadcast<3>(eye_space_pos);
		Vector8 ray_direction_world = eye_space_pos - camera->translation;
		return Normalize(ray_direction_world).AsFloat3Low();
	}

	ECS_TEMPLATE_FUNCTION_2_BEFORE(float3, ViewportToWorldRayDirection, const Camera*, const CameraCached*, uint2, int2);

	template<typename CameraType>
	int2 PositionToViewportTexels(const CameraType* camera, uint2 viewport_size, float3 position) {
		Matrix view_projection_matrix = GetCameraViewProjectionMatrix(camera);
		// Transform the point using the view projection matrix
		Vector8 transformed_point = TransformPoint(position, view_projection_matrix);
		// Perform the perspective divide
		transformed_point = PerspectiveDivide(transformed_point);

		// Now we have the NDC position of the point
		float4 ndc_point = transformed_point.AsFloat4Low();
		return NDCToViewportTexels(viewport_size, ndc_point.xy());
	}

	ECS_TEMPLATE_FUNCTION_2_BEFORE(int2, PositionToViewportTexels, const Camera*, const CameraCached*, uint2, float3);

	template<typename CameraType>
	float3 FocusCameraOnObject(const CameraType* camera, float3 object_translation, float distance) {
		float3 camera_forward = GetCameraForwardVector(camera).AsFloat3();
		return object_translation - camera_forward * distance
	}

	ECS_TEMPLATE_FUNCTION_2_BEFORE(float3, FocusCameraOnObject, const Camera*, const CameraCached*, float3, float);

	template<typename CameraType>
	float3 FocusCameraOnObjectViewSpace(
		const CameraType* camera,
		float3 object_translation,
		Quaternion object_rotation,
		float3 object_scale,
		float2 view_space_proportion
	) {

	}

	ECS_TEMPLATE_FUNCTION_2_BEFORE(float3, FocusCameraOnObjectViewSpace, const Camera*, const CameraCached*, float3, Quaternion, float3, float2);

}