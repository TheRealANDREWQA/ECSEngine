#include "ecspch.h"
#include "TransformHelpers.h"
#include "../ECS/Components.h"

namespace ECSEngine {

    Component TransformToolComponentID(ECS_TRANSFORM_TOOL tool) {
        switch (tool) {
        case ECS_TRANSFORM_TRANSLATION:
            return Translation::ID();
        case ECS_TRANSFORM_ROTATION:
            return Rotation::ID();
        case ECS_TRANSFORM_SCALE:
            return Scale::ID();
        }

        ECS_ASSERT(false, "Invalid transform tool when getting component id");
        return -1;
    }

    Stream<char> TransformToolComponentName(ECS_TRANSFORM_TOOL tool) {
        switch (tool) {
        case ECS_TRANSFORM_TRANSLATION:
            return STRING(Translation);
        case ECS_TRANSFORM_ROTATION:
            return STRING(Rotation);
        case ECS_TRANSFORM_SCALE:
            return STRING(Scale);
        }

        ECS_ASSERT(false, "Invalid transform tool when getting component name");
        return {};
    }

    float3 AxisDirection(ECS_AXIS axis)
    {
        switch (axis) {
        case ECS_AXIS_X:
            return GetRightVector();
            break;
        case ECS_AXIS_Y:
            return GetUpVector();
            break;
        case ECS_AXIS_Z:
            return GetForwardVector();
            break;
        default:
            ECS_ASSERT(false, "Invalid axis");
        }

        return float3::Splat(0.0f);
    }

    float3 AxisDirectionLocal(ECS_AXIS axis, QuaternionScalar rotation)
    {
        return RotateVector(AxisDirection(axis), rotation);
    }

    float3 AxisDirection(ECS_AXIS axis, QuaternionScalar rotation, ECS_TRANSFORM_SPACE space)
    {
        if (space == ECS_TRANSFORM_WORLD_SPACE) {
            return AxisDirection(axis);
        }
        else if (space == ECS_TRANSFORM_LOCAL_SPACE) {
            return AxisDirectionLocal(axis, rotation);
        }
        else {
            ECS_ASSERT(false, "Invalid transform space");
            return float3::Splat(0.0f);
        }
    }

    PlaneScalar AxisPlane(ECS_AXIS axis, float3 offset) {
        switch (axis) {
        case ECS_AXIS_X:
            return PlaneXYScalar(offset.z);
        case ECS_AXIS_Y:
            return PlaneXYScalar(offset.z);
        case ECS_AXIS_Z:
            return PlaneXZScalar(offset.y);
        }

        // Should not reach here
        ECS_ASSERT(false);
        return PlaneScalar();
    }

    PlaneScalar AxisPlane(ECS_AXIS axis, float3 view_direction_normalized, float3 offset) {
        float view_angle_threshold = DegToRad(10.0f);
        PlaneScalar current_plane;
        PlaneScalar other_plane;
        switch (axis) {
        case ECS_AXIS_X:
        {
            current_plane = PlaneXYScalar(offset.z);
            other_plane = PlaneXZScalar(offset.y);
        }
        break;
        case ECS_AXIS_Y:
        {
            current_plane = PlaneXYScalar(offset.z);
            other_plane = PlaneYZScalar(offset.x);
        }
        break;
        case ECS_AXIS_Z:
        {
            current_plane = PlaneXZScalar(offset.y);
            other_plane = PlaneYZScalar(offset.x);
        }
        break;
        default:
            ECS_ASSERT(false, "Invalid axis");
        }

        bool select_mask = PlaneIsParallelAngleMask(current_plane, view_direction_normalized, view_angle_threshold);
        return select_mask ? other_plane : current_plane;
    }

    PlaneScalar AxisPlaneLocal(ECS_AXIS axis, QuaternionScalar rotation, float3 offset)
    {
        PlaneScalar plane = AxisPlane(axis, offset);
        plane.normal = RotateVector(plane.normal, rotation);
        return plane;
    }

    PlaneScalar AxisPlaneLocal(ECS_AXIS axis, float3 view_direction_normalized, QuaternionScalar rotation, float3 offset)
    {
        float view_angle_threshold = DegToRad(10.0f);

        // Calculate only the first plane
        // Calculate the second plane only if needed
        PlaneScalar current_plane;
        switch (axis) {
        case ECS_AXIS_X:
        {
            current_plane = PlaneXYRotated(rotation, offset);
        }
        break;
        case ECS_AXIS_Y:
        {
            current_plane = PlaneXYRotated(rotation, offset);
        }
        break;
        case ECS_AXIS_Z:
        {
            current_plane = PlaneXZRotated(rotation, offset);
        }
        break;
        default:
            ECS_ASSERT(false, "Invalid axis");
        }

        bool select_mask = PlaneIsParallelAngleMask(current_plane, view_direction_normalized, view_angle_threshold);
        if (!select_mask) {
            // We can return already since there is no conflict
            return current_plane;
        }

        PlaneScalar other_plane;
        switch (axis) {
        case ECS_AXIS_X:
        {
            other_plane = PlaneXZRotated(rotation, offset);
        }
        break;
        case ECS_AXIS_Y:
        {
            other_plane = PlaneYZRotated(rotation, offset);
        }
        break;
        case ECS_AXIS_Z:
        {
            other_plane = PlaneYZRotated(rotation, offset);
        }
        break;
        }

        return other_plane;
    }

    PlaneScalar AxisPlane(
        ECS_AXIS axis,
        QuaternionScalar rotation,
        ECS_TRANSFORM_SPACE space,
        float3 offset
    )
    {
        if (space == ECS_TRANSFORM_LOCAL_SPACE) {
            return AxisPlaneLocal(axis, rotation, offset);
        }
        else if (space == ECS_TRANSFORM_WORLD_SPACE) {
            return AxisPlane(axis, offset);
        }
        else {
            ECS_ASSERT(false, "Invalid transform space");
        }

        return PlaneScalar();
    }

    PlaneScalar AxisPlane(
        ECS_AXIS axis,
        float3 view_direction_normalized,
        QuaternionScalar rotation,
        ECS_TRANSFORM_SPACE space,
        float3 offset
    )
    {
        if (space == ECS_TRANSFORM_LOCAL_SPACE) {
            return AxisPlaneLocal(axis, view_direction_normalized, rotation, offset);
        }
        else if (space == ECS_TRANSFORM_WORLD_SPACE) {
            return AxisPlane(axis, view_direction_normalized, offset);
        }
        else {
            ECS_ASSERT(false, "Invalid transform space");
        }

        return PlaneScalar();
    }

    TransformAxisInfo AxisInfo(
        ECS_AXIS axis,
        QuaternionScalar rotation,
        ECS_TRANSFORM_SPACE space,
        float3 plane_offset
    )
    {
        TransformAxisInfo info;

        info.axis_direction = AxisDirection(axis, rotation, space);
        info.plane = AxisPlane(axis, rotation, space, plane_offset);

        return info;
    }

    TransformAxisInfo AxisInfo(
        ECS_AXIS axis,
        QuaternionScalar rotation,
        ECS_TRANSFORM_SPACE space,
        float3 plane_view_direction_normalized,
        float3 plane_offset
    )
    {
        TransformAxisInfo info;

        info.axis_direction = AxisDirection(axis, rotation, space);
        info.plane = AxisPlane(axis, plane_view_direction_normalized, rotation, space, plane_offset);

        return info;
    }

}