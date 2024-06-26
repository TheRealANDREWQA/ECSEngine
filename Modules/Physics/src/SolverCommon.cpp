#include "pch.h"
#include "SolverCommon.h"
#include "Rigidbody.h"
#include "ECSEngineWorld.h"

static void UpdateWorldSpaceInertiaTensors_Impl(
	const ForEachEntityData* for_each_data,
	const Rotation* rotation,
	Rigidbody* rigidbody
) {
	// PERFORMANCE TODO: Check for rotation different from Identity?
	Matrix3x3 rotation_matrix = QuaternionToMatrix3x3(rotation->value);
	Matrix3x3 transpose_rotation_matrix = MatrixTranspose(rotation_matrix);
	rigidbody->world_space_inertia_tensor_inverse = MatrixMultiply(MatrixMultiply(rotation_matrix, rigidbody->inertia_tensor_inverse), transpose_rotation_matrix);
}

template<bool schedule_element>
ECS_THREAD_TASK(UpdateWorldSpaceInertiaTensors) {
	ForEachEntityCommit<schedule_element, QueryRead<Rotation>, QueryReadWrite<Rigidbody>>(thread_id, world)
		.Function(UpdateWorldSpaceInertiaTensors_Impl);
}

// For integration of the positions and velocity, we are using the Semi Implicit Euler method
// Since it provides the best speed to accuracy balance

static void IntegratePositions_Impl(
	const ForEachEntityData* for_each_data,
	Translation* translation,
	Rotation* rotation,
	const Rigidbody* rigidbody
) {
	// For the translation, we can simply apply a simple Euler integration step
	translation->value += rigidbody->velocity * for_each_data->world->delta_time;
	// For the rotation, we can apply a simple Euler integration step where we
	// Multiply the existing rotation with a quaternion of the angular rotation
	if (rigidbody->angular_velocity.x != 0.0f || rigidbody->angular_velocity.y != 0.0f || rigidbody->angular_velocity.z != 0.0f) {
		// Determine the axis
		float angular_speed = Length(rigidbody->angular_velocity);
		float3 rotation_axis = rigidbody->angular_velocity / angular_speed;
		QuaternionScalar delta_rotation = QuaternionAngleFromAxisRad(rotation_axis, angular_speed * for_each_data->world->delta_time);
		rotation->value = RotateQuaternion(rotation->value, delta_rotation);
	}
}

template<bool schedule_element>
ECS_THREAD_TASK(IntegratePositions) {
	ForEachEntityCommit<schedule_element, QueryReadWrite<Translation>, QueryReadWrite<Rotation>, QueryRead<Rigidbody>>(thread_id, world)
		.Function(IntegratePositions_Impl);
}

static void IntegrateVelocities_Impl(
	const ForEachEntityData* for_each_data,
	Rigidbody* rigidbody
) {
	// At the moment, just use a constant gravity force to update the translation velocity
	if (!rigidbody->is_static) {
		rigidbody->velocity -= GetUpVector() * 25.8f * 0.001f;
	}
}

template<bool schedule_element>
ECS_THREAD_TASK(IntegrateVelocities) {
	ForEachEntityCommit<schedule_element, QueryReadWrite<Rigidbody>>(thread_id, world).Function(IntegrateVelocities_Impl);
}

void AddSolverCommonTasks(ModuleTaskFunctionData* data) {
	ECS_REGISTER_SIMPLE_FOR_EACH_TASK(data, UpdateWorldSpaceInertiaTensors, ECS_THREAD_TASK_SIMULATE_EARLY, {});

	TaskSchedulerElement integrate_position;
	integrate_position.task_group = ECS_THREAD_TASK_SIMULATE_LATE;
	TaskDependency position_dependencies[] = {
		STRING(IntegrateVelocities)
	};
	integrate_position.task_dependencies = data->AllocateAndSetDependencies({ position_dependencies, ECS_COUNTOF(position_dependencies) });
	ECS_REGISTER_FOR_EACH_TASK(integrate_position, IntegratePositions, data);

	ECS_REGISTER_SIMPLE_FOR_EACH_TASK(data, IntegrateVelocities, ECS_THREAD_TASK_SIMULATE_LATE, {});
}