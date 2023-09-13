#include "ecspch.h"
#include "Camera.h"
#include "../Utilities/FunctionInterfaces.h"

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

	void Camera::SetOrthographicProjection(float _width, float _height, float _near_z, float _far_z) {
		is_orthographic = true;
		is_perspective_fov = false;

		width = _width;
		height = _height;
		near_z = _near_z;
		far_z = _far_z;
		projection = MatrixOrthographic(width, height, near_z, far_z);
	}

	void Camera::SetPerspectiveProjection(float _width, float _height, float _near_z, float _far_z) {
		is_orthographic = false;
		is_perspective_fov = false;

		width = _width;
		height = _height;
		near_z = _near_z;
		far_z = _far_z;
		projection = MatrixPerspective(width, height, near_z, far_z);
	}

	void Camera::SetPerspectiveProjectionFOV(float _fov, float _aspect_ratio, float _near_z, float _far_z) {
		is_orthographic = false;
		is_perspective_fov = true;

		fov = _fov;
		aspect_ratio = _aspect_ratio;
		near_z = _near_z;
		far_z = _far_z;
		projection = MatrixPerspectiveFOV(fov, aspect_ratio, near_z, far_z);
	}

	Matrix ECS_VECTORCALL Camera::GetViewProjectionMatrix() const {
		return GetTranslation() * GetRotation() * GetProjection();
	}

	CameraParameters Camera::AsParameters() const
	{
		CameraParameters parameters;

		parameters.near_z = near_z;
		parameters.far_z = far_z;
		parameters.width = width;
		parameters.height = height;
		parameters.is_orthographic = is_orthographic;
		parameters.rotation = rotation;
		parameters.translation = translation;

		return parameters;
	}

	CameraParametersFOV Camera::AsParametersFOV() const
	{
		CameraParametersFOV parameters;

		parameters.near_z = near_z;
		parameters.far_z = far_z;
		parameters.aspect_ratio = aspect_ratio;
		parameters.fov = fov;
		parameters.rotation = rotation;
		parameters.translation = translation;

		return parameters;
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
		near_z = camera->near_z;
		far_z = camera->far_z;

		if (is_orthographic || !is_perspective_fov) {
			width = camera->width;
			height = camera->height;

			fov = 0.0f;
			aspect_ratio = 0.0f;
			horizontal_fov = 0.0f;
		}
		else {
			fov = camera->fov;
			aspect_ratio = camera->aspect_ratio;
			horizontal_fov = HorizontalFOVFromVertical(fov, aspect_ratio);
		
			width = 0.0f;
			height = 0.0f;
		}

		rotation_matrix = camera->GetRotation();
		rotation_as_is_matrix = camera->GetRotationAsIs();
		projection_matrix = camera->projection;
		view_projection_matrix = camera->GetViewProjectionMatrix();
		inverse_view_projection_matrix = camera->GetInverseViewProjectionMatrix();
	}

	void CameraCached::Recalculate()
	{
		Camera camera;
		if (is_orthographic) {
			camera.SetOrthographicProjection(width, height, near_z, far_z);
		}
		else if (is_perspective_fov) {
			camera.SetPerspectiveProjectionFOV(fov, aspect_ratio, near_z, far_z);
		}
		else {
			camera.SetPerspectiveProjection(width, height, near_z, far_z);
		}

		camera.translation = translation;
		camera.rotation = rotation;
		*this = CameraCached(&camera);
	}

	CameraParameters CameraCached::AsParameters() const
	{
		CameraParameters parameters;

		parameters.near_z = near_z;
		parameters.far_z = far_z;
		parameters.width = width;
		parameters.height = height;
		parameters.is_orthographic = is_orthographic;
		parameters.rotation = rotation;
		parameters.translation = translation;

		return parameters;
	}

	CameraParametersFOV CameraCached::AsParametersFOV() const
	{
		CameraParametersFOV parameters;

		parameters.near_z = near_z;
		parameters.far_z = far_z;
		parameters.aspect_ratio = aspect_ratio;
		parameters.fov = fov;
		parameters.rotation = rotation;
		parameters.translation = translation;

		return parameters;
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
		float3 camera_forward = GetCameraForwardVector(camera).AsFloat3Low();
		return object_translation - camera_forward * distance;
	}

	ECS_TEMPLATE_FUNCTION_2_BEFORE(float3, FocusCameraOnObject, const Camera*, const CameraCached*, float3, float);

	template<typename CameraType>
	float3 FocusCameraOnObjectViewSpace(
		const CameraType* camera,
		float3 object_translation,
		Quaternion object_rotation,
		float3 object_scale,
		AABBStorage object_bounds,
		float2 view_space_proportion
	) {
		ECS_ASSERT(view_space_proportion.x == 0.0f || view_space_proportion.y == 0.0f);

		// Use a binary search of the right distance
		const float min_distance = 0.0f;
		const float max_distance = 10000.0f;

		const float BINARY_SEARCH_EPSILON = 0.05f;
		const float LINEAR_SEARCH_EPSILON = 0.01f;
		const float LINEAR_SEARCH_STEP_SIZE = 0.001f;

		// Transform the AABB into the world space position
		AABB transformed_bounds = TransformAABB(object_bounds, object_translation, QuaternionToMatrixLow(object_rotation), object_scale);
		Matrix view_projection_matrix = GetCameraViewProjectionMatrix(camera);

		// Bring the center of the world space AABB right at the camera
		// Inside the binary search and linear search we will offset in alongside the 
		// negative forward vector of the camera
		Vector8 transformed_bounds_center = AABBCenter(transformed_bounds);
		Vector8 camera_forward = GetCameraForwardVector(camera);
		Vector8 camera_translation = { camera->translation, camera->translation };

		//// Get the closest point to the camera and push the camera away from the center
		//// (by moving the aabb further from where the camera is such that we don't modify the camera)
		//// By the projection of that point to the camera direction
		//Vector8 aabb_corners[4];
		//GetAABBCorners(transformed_bounds, aabb_corners);
		//Vector8 closest_aabb_point = ClosestPoint({ aabb_corners, 4 }, camera_translation);
		//Vector8 projection_point = ProjectPointOnLineDirectionNormalized(camera_translation, camera_forward, closest_aabb_point);
		//Vector8 displacement = camera_translation - projection_point;

		Vector8 displacement = camera_translation - transformed_bounds_center;
		transformed_bounds = TranslateAABB(transformed_bounds, displacement);

		auto aabb_compare = [&](float current_distance, float epsilon) {
			Vector8 aabb_translation = camera_forward * Vector8(current_distance);
			AABB current_aabb = TranslateAABB(transformed_bounds, aabb_translation);
			// Project the min and max point on the screen and determine the percentage of coverage
			Vector8 projected_points = ApplyMatrixOnAABB(current_aabb, view_projection_matrix);
			Vector8 ndc_values = ClipSpaceToNDC(projected_points);

			float4 scalar_ndc_values[2];
			ndc_values.Store(scalar_ndc_values);
			float2 min_pos = scalar_ndc_values[0].xy();
			float2 max_pos = scalar_ndc_values[1].xy();
			float2 coverage = AbsoluteDifference(min_pos, max_pos);
			float current_difference = 0.0f;
			bool is_smaller = false;

			bool behind = !CullClipSpaceWWhole(projected_points);
			if (view_space_proportion.x == 0.0f) {
				// The Y proportion is specified
				current_difference = AbsoluteDifference(view_space_proportion.y, coverage.y);
				is_smaller = coverage.y < view_space_proportion.y;
			}
			else {
				// The X proportion is specified
				current_difference = AbsoluteDifference(view_space_proportion.x, coverage.x);
				is_smaller = coverage.x < view_space_proportion.x;
			}

			if (!isnan(current_difference) && current_difference < epsilon) {
				return 0;
			}
			is_smaller = behind ? !is_smaller : is_smaller;
			return is_smaller ? 1 : -1;
		};

		float resulting_distance = 0.0f;
		if (function::BinaryIntoLinearSearch(min_distance, max_distance, 0.001f, 2.0f, LINEAR_SEARCH_STEP_SIZE, &resulting_distance,
			[&](float current_distance) {
				return aabb_compare(current_distance, BINARY_SEARCH_EPSILON);
			},
			[&](float current_distance) {
				return aabb_compare(current_distance, LINEAR_SEARCH_EPSILON);
			}
		)) {
			return transformed_bounds_center.AsFloat3Low() - camera_forward.AsFloat3Low() * float3::Splat(resulting_distance);
		}
		return float3::Splat(FLT_MAX);
	}

	ECS_TEMPLATE_FUNCTION_2_BEFORE(float3, FocusCameraOnObjectViewSpace, const Camera*, const CameraCached*, float3, Quaternion, float3, AABBStorage, float2);

}