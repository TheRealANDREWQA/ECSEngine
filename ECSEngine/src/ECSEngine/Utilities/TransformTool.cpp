#include "ecspch.h"
#include "TransformTool.h"
#include "../Math/Intersection.h"
#include "../Rendering/Camera.h"

namespace ECSEngine {

    template<typename CameraType>
    float3 HandleTranslationToolDelta(
        const CameraType* camera,
        float3 plane_point,
        QuaternionScalar object_rotation,
        ECS_AXIS axis,
        ECS_TRANSFORM_SPACE space,
        uint2 window_size, 
        int2 mouse_texel_position, 
        int2 mouse_delta
    )
    {
        // Get the difference in the ray position
        if (mouse_delta.x != 0 || mouse_delta.y != 0) {
            float3 previous_ray_direction = ViewportToWorldRayDirection(camera, window_size, mouse_texel_position - mouse_delta);
            float3 current_ray_direction = ViewportToWorldRayDirection(camera, window_size, mouse_texel_position);
            return HandleTranslationToolDelta(camera, plane_point, object_rotation, axis, space, previous_ray_direction, current_ray_direction);
        }
        return float3::Splat(0.0f);
    }

    ECS_TEMPLATE_FUNCTION_2_BEFORE(
        float3, 
        HandleTranslationToolDelta, 
        const Camera*, 
        const CameraCached*, 
        float3, 
        QuaternionScalar, 
        ECS_AXIS, 
        ECS_TRANSFORM_SPACE, 
        uint2, 
        int2, 
        int2
    );

    template<typename CameraType>
    float3 HandleTranslationToolDelta(
        const CameraType* camera,
        float3 plane_point,
        QuaternionScalar object_rotation,
        ECS_AXIS axis,
        ECS_TRANSFORM_SPACE space,
        float3 previous_ray_direction,
        float3 current_ray_direction,
        bool2* success_status
    )
    {
        if (current_ray_direction != previous_ray_direction) {
            float3 camera_forward_vector = GetCameraForwardVectorScalar(camera);
            TransformAxisInfo axis_info = AxisInfo(axis, object_rotation, space, camera_forward_vector, plane_point);

            // Perform the ray intersection test
            float3 ray_start = camera->translation;

            float3 previous_intersect_point;
            float previous_intersect_point_t;
            bool previous_intersect = IntersectRayPlane(ray_start, previous_ray_direction, axis_info.plane, previous_intersect_point, previous_intersect_point_t);

            float3 current_intersect_point;
            float current_intersect_point_t;
            bool current_intersect = IntersectRayPlane(ray_start, current_ray_direction, axis_info.plane, current_intersect_point, current_intersect_point_t);

            if (success_status != nullptr) {
                *success_status = { previous_intersect, current_intersect };
            }

            if (previous_intersect && current_intersect) {
                // Project the hit point onto the axis
                float3 previous_hit_point = ProjectPointOnLineDirectionNormalized(plane_point, axis_info.axis_direction, previous_intersect_point);
                float3 current_hit_point = ProjectPointOnLineDirectionNormalized(plane_point, axis_info.axis_direction, current_intersect_point);
                float3 translation_delta = current_hit_point - previous_hit_point;
                return translation_delta;
            }
        }
        return float3::Splat(0.0f);
    }

    ECS_TEMPLATE_FUNCTION_2_BEFORE(
        float3, 
        HandleTranslationToolDelta, 
        const Camera*, 
        const CameraCached*, 
        float3,
        QuaternionScalar,
        ECS_AXIS, 
        ECS_TRANSFORM_SPACE,        
        float3, 
        float3, 
        bool2*
    );

    template<typename CameraType>
    float3 HandleTranslationToolDelta(
        const CameraType* camera, 
        float3 plane_point, 
        QuaternionScalar object_rotation, 
        TranslationToolDrag* drag_tool, 
        float3 current_ray_direction
    )
    {
        bool2 success;
        if (drag_tool->last_successful_direction.x == FLT_MAX || drag_tool->last_successful_direction.y == FLT_MAX ||
            drag_tool->last_successful_direction.z == FLT_MAX) {
            drag_tool->last_successful_direction = current_ray_direction;
        }
        float3 delta = HandleTranslationToolDelta(
            camera, 
            plane_point, 
            object_rotation, 
            drag_tool->axis, 
            drag_tool->space, 
            drag_tool->last_successful_direction, 
            current_ray_direction, 
            &success
        );
        if (success.x && success.y) {
            drag_tool->last_successful_direction = current_ray_direction;
        }
        return delta;
    }

    ECS_TEMPLATE_FUNCTION_2_BEFORE(float3, HandleTranslationToolDelta, const Camera*, const CameraCached*, float3, QuaternionScalar, TranslationToolDrag*, float3);

    template<typename CameraType>
    float3 HandleRotationToolDelta(
        const CameraType* camera, 
        float3 rotation_center, 
        QuaternionScalar rotation_value,
        RotationToolDrag* drag_tool, 
        uint2 window_size, 
        int2 mouse_texel_position,
        float rotation_factor
    )
    {
        float3 axis_direction = AxisDirection(drag_tool->axis, rotation_value, drag_tool->space);
        if (drag_tool->projected_direction.x == FLT_MAX || drag_tool->projected_direction.y == FLT_MAX) {
            // We need to calculate the projected line
            ChangeLineSpace(&rotation_center, &axis_direction, GetCameraViewProjectionMatrix(camera));
            drag_tool->projected_point = { rotation_center.x, rotation_center.y };
            // We need to negate the y direction

            // We also need to determine on which half the point was grabbed since we need to invert
            // factors if the opposite half was grabbed
            // At the moment this is not yet implemented

            drag_tool->projected_direction = { axis_direction.x, -axis_direction.y };
        }

        // Calculate the distance from the current cursor position to the projected vector
        float distance_to_line = DistanceToLineNormalized(
            float3(drag_tool->projected_point.x, drag_tool->projected_point.y, 0.0f),
            float3(drag_tool->projected_direction.x, drag_tool->projected_direction.y, 0.0f),
            float3(mouse_texel_position.x, mouse_texel_position.y, 0.0f)
        );
        // If the distance is not yet assigned, assign now
        if (drag_tool->last_distance == FLT_MAX) {
            drag_tool->last_distance = distance_to_line;
        }

        // This is the order that corresponds with the visual changes
        float delta = drag_tool->last_distance - distance_to_line;
        drag_tool->last_distance = distance_to_line;
        delta *= rotation_factor;
        return float3::Splat(delta) * axis_direction;
    }

    ECS_TEMPLATE_FUNCTION_2_BEFORE(float3, HandleRotationToolDelta, const Camera*, const CameraCached*, float3, QuaternionScalar, RotationToolDrag*, uint2, int2, float);

    template<typename CameraType>
    QuaternionScalar HandleRotationToolDeltaCircleMapping(
        const CameraType* camera,
        float3 rotation_center,
        QuaternionScalar rotation_value,
        RotationToolDrag* drag_tool,
        uint2 window_size,
        int2 mouse_texel_position,
        float rotation_factor
    ) {
        float3 rotation_center_vector = rotation_center;
        if (drag_tool->rotation_center_ndc.x == FLT_MAX) {
            // Transform the center of the rotation into ndc
            float4 clip_space_position = TransformPoint(rotation_center_vector, GetCameraViewProjectionMatrix(camera));
            float4 ndc_position = ClipSpaceToNDC(clip_space_position);
            drag_tool->rotation_center_ndc = ndc_position.xy();
        }

        // Calculate the direction from the mouse position to the center in ndc
        float2 mouse_ndc = MouseToNDC(window_size, mouse_texel_position);
        // Calculate the normalized direction to the circle center
        float2 center_direction = mouse_ndc - drag_tool->rotation_center_ndc;
        center_direction = Normalize(center_direction);

        if (drag_tool->last_circle_direction.x == FLT_MAX) {
            drag_tool->last_circle_direction = center_direction;
        }

        // Calculate the cosine between the 2 directions using dot
        float cosine = Dot(center_direction, drag_tool->last_circle_direction);
        // Now determine if the rotation is clockwise or anticlockwise in order
        // to invert the value. We shouldn't apply the negation to the cosine
        // since when we do the acosf it will return a value in between [pi/2, pi]
        // and we don't want that
        bool is_clockwise = IsClockwise(center_direction, drag_tool->last_circle_direction);

        drag_tool->last_circle_direction = center_direction;

        // Now determine the angle difference
        float angle_radians = acosf(cosine);
        float angle_degrees = RadToDeg(angle_radians);
        // If the angle values is really small (due to floating point error)
        // clamp it to zero such that it doesn't cause ghosting rotation
        if (angle_degrees >= 0.05f) {
            angle_degrees = is_clockwise ? -angle_degrees : angle_degrees;

            float3 axis_direction = AxisDirection(drag_tool->axis, rotation_value, drag_tool->space);
            // There is one more thing to take into consideration
            // If we are on different faces of the plane of rotation we need
            // to invert the rotation
            bool is_bellow_plane = !IsAbovePlaneMask(PlaneScalar(axis_direction, rotation_center_vector), camera->translation);
            angle_degrees = is_bellow_plane ? -angle_degrees : angle_degrees;

            // Construct a Quaternion from the axis direction with the given rotation
            QuaternionScalar rotation_quaternion = QuaternionAngleFromAxis(axis_direction, angle_degrees * rotation_factor);
            return rotation_quaternion;
        }
        return QuaternionIdentityScalar();
    }

    ECS_TEMPLATE_FUNCTION_2_BEFORE(QuaternionScalar, HandleRotationToolDeltaCircleMapping, const Camera*, const CameraCached*, float3, QuaternionScalar, RotationToolDrag*, uint2, int2, float);

    template<typename CameraType>
    float3 HandleScaleToolDelta(
        const CameraType* camera,
        float3 scale_center,
        QuaternionScalar rotation_value,
        ScaleToolDrag* drag_tool,
        int2 mouse_texel_delta,
        float scale_factor
    ) {
        // This is a scaling with constant factor
        if (drag_tool->axis_direction.x == FLT_MAX || drag_tool->axis_direction.y == FLT_MAX) {
            float3 axis_direction_3D = AxisDirectionLocal(drag_tool->axis, rotation_value);
            ChangeLineSpace(&scale_center, &axis_direction_3D, GetCameraViewProjectionMatrix(camera));
            // We need to negate the y axis
            drag_tool->axis_direction = float2(axis_direction_3D.x, -axis_direction_3D.y);
            // Perform the normalization after eliminating the Z coordinate
            drag_tool->axis_direction = Normalize(drag_tool->axis_direction);
        }

        float2 delta = { (float)mouse_texel_delta.x, (float)mouse_texel_delta.y };
        delta *= drag_tool->axis_direction * float2::Splat(scale_factor);
        return float3::Splat(delta.x + delta.y) * AxisDirection(drag_tool->axis);
    }

    ECS_TEMPLATE_FUNCTION_2_BEFORE(float3, HandleScaleToolDelta, const Camera*, const CameraCached*, float3, QuaternionScalar, ScaleToolDrag*, int2, float);

}