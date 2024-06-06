#include "pch.h"
#include "Rigidbody.h"
#include "ECSEngineWorld.h"
#include "ECSEngineComponents.h"
#include "CollisionDetection/src/CollisionDetectionComponents.h"

// This code is based upon the code from https://github.com/blackedout01/simkn
// Credit to the git user blackedout01 for the implementation and explanation!

static float TetrahedronInertiaMoment(float3 point_0, float3 point_1, float3 point_2, size_t offset) {
    return point_0[offset] * point_0[offset] + point_1[offset] * point_2[offset]
        + point_1[offset] * point_1[offset] + point_0[offset] * point_2[offset]
        + point_2[offset] * point_2[offset] + point_0[offset] * point_1[offset];
}

static float TetrahedronInertiaProduct(float3 point_0, float3 point_1, float3 point_2, size_t offset_i, size_t offset_j) {
    return 2.0f * point_0[offset_i] * point_0[offset_j] + point_1[offset_i] * point_2[offset_j] + point_2[offset_i] * point_1[offset_j]
        + 2.0f * point_1[offset_i] * point_1[offset_j] + point_0[offset_i] * point_2[offset_j] + point_2[offset_i] * point_0[offset_j]
        + 2.0f * point_2[offset_i] * point_2[offset_j] + point_0[offset_i] * point_1[offset_j] + point_1[offset_i] * point_0[offset_j];
}

Matrix3x3 ComputeInertiaTensor(
    Stream<float3> points,
    Stream<uint3> triangle_indices,
    float density,
    float* output_mass,
    float3* output_center_of_mass
) {
    struct FunctorData {
        Stream<float3> points;
        Stream<uint3> triangle_indices;
    };

    FunctorData functor_data{ points, triangle_indices };
    return ComputeInertiaTensor(
        triangle_indices.size,
        &functor_data,
        [](void* _functor_data, unsigned int index) {
            FunctorData* functor_data = (FunctorData*)_functor_data;
            return functor_data->points[index];
        },
        [](void* _functor_data, unsigned int index) {
            FunctorData* functor_data = (FunctorData*)_functor_data;
            return functor_data->triangle_indices[index];
        },
            density,
            output_mass,
            output_center_of_mass
            );
}

Matrix3x3 ComputeInertiaTensor(
    const float* points_x,
    const float* points_y,
    const float* points_z,
    size_t count,
    Stream<uint3> triangle_indices,
    float density,
    float* output_mass,
    float3* output_center_of_mass
) {
    struct FunctorData {
        const float* points_x;
        const float* points_y;
        const float* points_z;
        Stream<uint3> triangle_indices;
    };

    FunctorData functor_data{ points_x, points_y, points_z, triangle_indices };
    return ComputeInertiaTensor(
        triangle_indices.size,
        &functor_data,
        [](void* _functor_data, unsigned int index) {
            FunctorData* functor_data = (FunctorData*)_functor_data;
            return float3{ functor_data->points_x[index], functor_data->points_y[index], functor_data->points_z[index] };
        },
        [](void* _functor_data, unsigned int index) {
            FunctorData* functor_data = (FunctorData*)_functor_data;
            return functor_data->triangle_indices[index];
        },
        density,
        output_mass,
        output_center_of_mass
    );
}

Matrix3x3 ComputeInertiaTensor(
    const ConvexHull* convex_hull,
    float density,
    float* output_mass,
    float3* output_center_of_mass
) {
    // We need the convex hull to be triangulated
    ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 64, ECS_MB * 16);
    // Initialize the array with a decent starting size
    ResizableStream<ushort3> triangles(&stack_allocator, convex_hull->faces.size * 2);
    convex_hull->RetrieveTriangulatedFaces(&triangles);

    struct FunctorData {
        Stream<ushort3> triangles;
        const ConvexHull* hull;
    };

    FunctorData functor_data = { triangles, convex_hull };

    return ComputeInertiaTensor(
        triangles.size,
        &functor_data,
        [](void* _data, unsigned int index) {
            FunctorData* data = (FunctorData*)_data;
            return data->hull->GetPoint(index);
        },
        [](void* _data, unsigned int index) {
            FunctorData* data = (FunctorData*)_data;
            return uint3{ data->triangles[index] };
        },
        density,
        output_mass,
        output_center_of_mass
     );
}

Matrix3x3 ComputeInertiaTensor(
    size_t triangle_count,
    void* functor_data,
    float3 (*GetPointFunction)(void*, unsigned int),
    uint3 (*GetTriangleFunction)(void*, unsigned int),
    float density,
    float* output_mass,
    float3* output_center_of_mass
) {
    Matrix3x3 inertia_tensor;

    float mass = 0.0;
    float3 center_of_mass = float3::Splat(0.0f);
    for (size_t index = 0; index < triangle_count; index++) {
        uint3 triangle_indices = GetTriangleFunction(functor_data, index);
        float3 a = GetPointFunction(functor_data, triangle_indices.x);
        float3 b = GetPointFunction(functor_data, triangle_indices.y);
        float3 c = GetPointFunction(functor_data, triangle_indices.z);

        float determinant_j = ScalarTripleProduct(a, b, c);
        float tetrahedron_volume = determinant_j / 6.0f;
        float tetrahedron_mass = density * tetrahedron_volume;

        float3 tetrahedron_center_of_mass = (a + b + c) / 4.0f;

        float v100 = TetrahedronInertiaMoment(a, b, c, 0);
        float v010 = TetrahedronInertiaMoment(a, b, c, 1);
        float v001 = TetrahedronInertiaMoment(a, b, c, 2);

        inertia_tensor.values[0][0] += determinant_j * (v010 + v001);
        inertia_tensor.values[1][1] += determinant_j * (v100 + v001);
        inertia_tensor.values[2][2] += determinant_j * (v100 + v010);

        // Only fill in one of the symmetric values, we will assign into the other
        // One at the end of the loop
        float xy_product_value = determinant_j * TetrahedronInertiaProduct(a, b, c, 0, 1);
        inertia_tensor.values[0][1] += xy_product_value;

        float xz_product_value = determinant_j * TetrahedronInertiaProduct(a, b, c, 0, 2);
        inertia_tensor.values[0][2] += xz_product_value;

        float yz_product_value = determinant_j * TetrahedronInertiaProduct(a, b, c, 1, 2);
        inertia_tensor.values[1][2] += yz_product_value;

        tetrahedron_center_of_mass *= tetrahedron_mass;
        center_of_mass += tetrahedron_center_of_mass;
        mass += tetrahedron_mass;
    }

    center_of_mass /= mass;
    const float PRIMARY_FACTOR = 60.0f;
    const float SECONDARY_FACTOR = 120.0f;

    inertia_tensor.values[0][0] = density * inertia_tensor.values[0][0] / PRIMARY_FACTOR - mass * (center_of_mass.y * center_of_mass.y + center_of_mass.z * center_of_mass.z);
    inertia_tensor.values[1][1] = density * inertia_tensor.values[1][1] / PRIMARY_FACTOR - mass * (center_of_mass.x * center_of_mass.x + center_of_mass.z * center_of_mass.z);
    inertia_tensor.values[2][2] = density * inertia_tensor.values[2][2] / PRIMARY_FACTOR - mass * (center_of_mass.x * center_of_mass.x + center_of_mass.y * center_of_mass.y);

    inertia_tensor.values[0][1] = density * inertia_tensor.values[0][1] / SECONDARY_FACTOR - mass * (center_of_mass.x * center_of_mass.y);
    inertia_tensor.values[0][2] = density * inertia_tensor.values[0][2] / SECONDARY_FACTOR - mass * (center_of_mass.x * center_of_mass.z);
    inertia_tensor.values[1][2] = density * inertia_tensor.values[1][2] / SECONDARY_FACTOR - mass * (center_of_mass.y * center_of_mass.z);
    // Invert the product elements
    inertia_tensor.values[0][1] = -inertia_tensor.values[0][1];
    inertia_tensor.values[0][2] = -inertia_tensor.values[0][2];
    inertia_tensor.values[1][2] = -inertia_tensor.values[1][2];
    inertia_tensor.values[1][0] = inertia_tensor.values[0][1];
    inertia_tensor.values[2][0] = inertia_tensor.values[0][2];
    inertia_tensor.values[2][1] = inertia_tensor.values[1][2];

    if (output_center_of_mass != nullptr) {
        *output_center_of_mass = center_of_mass;
    }
    if (output_mass != nullptr) {
        *output_mass = mass;
    }

    return inertia_tensor;
}

static ThreadTask RigidbodyBuildFunction(ModuleComponentBuildFunctionData* data) {
    if (data->entity_manager->HasComponent<ConvexCollider>(data->entity)) {
        const ConvexCollider* collider = data->entity_manager->GetComponent<ConvexCollider>(data->entity);
        ConvexHull hull = collider->hull;
        // The scale is the only transform that can affect the inertia tensor
        float3 scale = GetScale(data->entity_manager->TryGetComponent<Scale>(data->entity));
        if (scale != float3::Splat(1.0f)) {
            // We can scale directly the hull, such that to not use
            // Temporary memory and then rescale it back after the
            // Computation is finished
            hull.Scale(scale);
        }

        Rigidbody* rigidbody = (Rigidbody*)data->component;
        float mass = 0.0f;
        float3 center_of_mass = float3::Splat(0.0f);
        Matrix3x3 inertia_tensor = ComputeInertiaTensor(&hull, 1.0f, &mass, &center_of_mass);
        rigidbody->inertia_tensor_inverse = Matrix3x3Inverse(inertia_tensor);
        rigidbody->inertia_tensor_inverse = inertia_tensor;
        rigidbody->mass_inverse = rigidbody->is_static ? 0.0f : 1.0f / mass;
        rigidbody->center_of_mass = center_of_mass;
        rigidbody->velocity = float3::Splat(0.0f);
        rigidbody->angular_velocity = float3::Splat(0.0f);

        if (scale != float3::Splat(1.0f)) {
            // Scale it back to its initial size
            hull.Scale(float3::Splat(1.0f) / scale);
        }
    }
    return {};
}

void AddRigidbodyBuildEntry(ModuleRegisterComponentFunctionsData* data) {
    ModuleComponentFunctions rigidbody;

    rigidbody.build_entry.function = RigidbodyBuildFunction;
    rigidbody.build_entry.component_dependencies.Initialize(data->allocator, 1);
    rigidbody.build_entry.component_dependencies[0] = STRING(ConvexCollider);
    rigidbody.component_name = STRING(Rigidbody);

    data->functions->AddAssert(&rigidbody);
}