#include "pch.h"
#include "ContactConstraint.h"
#include "ECSEngineForEach.h"
#include "Rigidbody.h"
#include "ECSEngineComponents.h"
#include "ECSEngineWorld.h"

#define SOLVER_DATA_STRING "SolverData"
#define MAX_BAUMGARTE_BIAS 10000.0f

struct ContactPair {
	ECS_INLINE unsigned int Hash() const {
		return Cantor(first.value, second.value);
	}

	Entity first;
	Entity second;
};

typedef HashTable<Contact, ContactPair, HashFunctionPowerOfTwo> ContactTable;

struct SolverData {
	unsigned int iterations;
	float baumgarte_factor;
	float linear_slop;
	MemoryManager allocator;
	ResizableStream<ContactConstraint> constraints;
};

struct ComputeConstraintPointInfo {
	float3 point;
	float3 separation_axis;
	float3 tangent_1;
	float3 tangent_2;
	float separation;
	Rigidbody* rigidbody_A;
	Rigidbody* rigidbody_B;
	float3 center_of_mass_A;
	float3 center_of_mass_B;
};

static ContactConstraintPoint ComputeConstraintPointFromManifold(const ComputeConstraintPointInfo* info) {
	ContactConstraintPoint constraint_point;

	constraint_point.normal_impulse = 0.0f;
	constraint_point.tangent_impulse_1 = 0.0f;
	constraint_point.tangent_impulse_2 = 0.0f;
	constraint_point.separation = info->separation;
	constraint_point.local_anchor_A = info->point - info->center_of_mass_A;
	constraint_point.local_anchor_B = info->point - info->center_of_mass_B;

	//constraint_point.world_anchor_A = point;
	//constraint_point.world_anchor_B = point;

	// Calculate the normal and tangent effective mass
	// The effective mass along the direction d 
	// has the following formula
	// The masses and the inertia tensors are the inverse
	// pA = rA x d (rA is the displacement vector - the local anchor)
	// pB = rB x d (rB is the displacement vector - the local anchor)
	// effective_mass = 1.0f / (mA + mB + IA * pA * pA + IB * pB * pB)
	// The * for the inertia tensor is matrix multiplication
	auto compute_effective_mass = [&](float3 direction) {
		float3 perpendicular_A = Cross(constraint_point.local_anchor_A, direction);
		float3 perpendicular_B = Cross(constraint_point.local_anchor_B, direction);
		float value = info->rigidbody_A->mass_inverse + info->rigidbody_B->mass_inverse
			+ Dot(perpendicular_A, MatrixVectorMultiply(perpendicular_A, info->rigidbody_A->inertia_tensor_inverse))
			+ Dot(perpendicular_B, MatrixVectorMultiply(perpendicular_B, info->rigidbody_B->inertia_tensor_inverse));
		// Sanity check. This values should be above 0.0f, if it is not, set it to 0.0f
		return value > 0.0f ? 1.0f / value : 0.0f;
	};

	constraint_point.normal_mass = compute_effective_mass(info->separation_axis);
	constraint_point.tangent_impulse_1 = compute_effective_mass(info->tangent_1);
	constraint_point.tangent_impulse_2 = compute_effective_mass(info->tangent_2);

	return constraint_point;
}

// It will update the constraints with data that can be precomputed
// And reused across iterations
static void PrepareContactConstraintsData(World* world, SolverData* solver_data) {
	for (unsigned int index = 0; index < solver_data->constraints.size; index++) {
		ContactConstraint& constraint = solver_data->constraints[index];

		// Retrieve the rigidbodies for the 2 entities
		Rigidbody* rigidbody_A = world->entity_manager->GetComponent<Rigidbody>(constraint.contact->entity_A);
		Rigidbody* rigidbody_B = world->entity_manager->GetComponent<Rigidbody>(constraint.contact->entity_B);
		constraint.rigidbody_A = rigidbody_A;
		constraint.rigidbody_B = rigidbody_B;

		// We need to compute the tangent directions. What we can do, in order
		// To align the pair of tangents with the relative velocity, is to project the relative
		// Velocity in the manifold plane. That is one of the tangents and the other one is the
		// Cross product between the manifold normal and the other tangent
		PlaneScalar manifold_plane = constraint.contact->manifold.GetPlane();
		float3 manifold_point = constraint.contact->manifold.contact_points[0];
		float3 projected_point_A = ProjectPointOnPlane(manifold_plane, manifold_point + rigidbody_A->velocity);
		float3 projected_point_B = ProjectPointOnPlane(manifold_plane, manifold_point + rigidbody_B->velocity);
		float3 projected_speed = projected_point_B - projected_point_A;

		// In case the projected speed is close to 0.0f, choose an arbitrary other direction
		// Like the projected velocity of body A
		float3 tangent_1;
		if (CompareMask(projected_speed, float3::Splat(0.0f))) {
			tangent_1 = Normalize(projected_point_A - manifold_point);
		}
		else {
			tangent_1 = Normalize(projected_speed);
		}
		float3 tangent_2 = Cross(constraint.contact->manifold.separation_axis, tangent_1);

		ComputeConstraintPointInfo compute_info;
		compute_info.center_of_mass_A = constraint.center_of_mass_A;
		compute_info.center_of_mass_B = constraint.center_of_mass_B;
		compute_info.rigidbody_A = rigidbody_A;
		compute_info.rigidbody_B = rigidbody_B;
		compute_info.separation = constraint.contact->manifold.separation_distance;
		compute_info.separation_axis = constraint.contact->manifold.separation_axis;
		compute_info.tangent_1 = tangent_1;
		compute_info.tangent_2 = tangent_2;
		for (unsigned int index = 0; index < constraint.contact->manifold.contact_point_count; index++) {
			compute_info.point = constraint.contact->manifold.contact_points[index];
			constraint.points[index] = ComputeConstraintPointFromManifold(&compute_info);
		}
	}
}

static void SolveContactConstraintsIteration(SolverData* solver_data, float delta_time_inverse) {
	for (unsigned int index = 0; index < solver_data->constraints.size; index++) {
		ContactConstraint& constraint = solver_data->constraints[index];

		float3 normal = constraint.contact->manifold.separation_axis;

		float3 velocity_A = constraint.rigidbody_A->velocity;
		float3 velocity_B = constraint.rigidbody_B->velocity;
		float3 angular_A = constraint.rigidbody_A->angular_velocity;
		float3 angular_B = constraint.rigidbody_B->angular_velocity;

		auto compute_relative_velocity = [&](float3 anchor_A, float3 anchor_B) {
			// Formula: v + omega x r where v is the linear velocity, omega the angular velocity and r the local anchor
			// Choose the relative velocity to be from A to B
			float3 relative_velocity_A = velocity_A + Cross(angular_A, anchor_A);
			float3 relative_velocity_B = velocity_B + Cross(angular_B, anchor_B);
			return relative_velocity_B - relative_velocity_A;
		};

		auto apply_impulse = [&](float3 anchor_A, float3 anchor_B, float3 impulse) {
			// The change in linear velocity is the impulse divided by mass
			// For the B body we must add the impulse
			velocity_A -= impulse * constraint.rigidbody_A->mass_inverse;
			velocity_B += impulse * constraint.rigidbody_B->mass_inverse;

			// The change in angular velocity is the cross product of the impulse with the anchor
			// divided by the inertia tensor or conversely, multiplied with the inverse
			angular_A -= MatrixVectorMultiply(Cross(anchor_A, impulse), constraint.rigidbody_A->inertia_tensor_inverse);
			angular_B += MatrixVectorMultiply(Cross(anchor_B, impulse), constraint.rigidbody_B->inertia_tensor_inverse);
		};

		// Solve the normal impulse first, then apply the friction impulse
		unsigned int point_count = constraint.contact->manifold.contact_point_count;
		for (unsigned int point_index = 0; point_index < point_count; point_index++) {
			ContactConstraintPoint& point = constraint.points[point_index];
			
			// Compute the Baumgarte bias factor
			float bias = 0.0f;
			if (point.separation > 0.0f) {
				// This shouldn't happen
			}
			else {
				float adjusted_separation = point.separation + solver_data->linear_slop;
				adjusted_separation = ClampMax(adjusted_separation, 0.0f);
				bias = delta_time_inverse * adjusted_separation * solver_data->baumgarte_factor;
				bias = ClampMax(bias, MAX_BAUMGARTE_BIAS);
			}

			float3 anchor_A = point.local_anchor_A;
			float3 anchor_B = point.local_anchor_B;

			// Calculate the relative velocity at the point of contact
			float3 relative_velocity = compute_relative_velocity(anchor_A, anchor_B);

			float relative_velocity_normal = Dot(relative_velocity, normal);
			float impulse = -point.normal_mass * (relative_velocity_normal + bias);

			// Clamp the accumulated impulse, not this value
			float clamped_impulse = ClampMin(point.normal_impulse + impulse, 0.0f);
			impulse = clamped_impulse - point.normal_impulse;
			point.normal_impulse = clamped_impulse;

			// At last, apply the impulse value of this iteration
			float3 normal_impulse = normal * impulse;
			apply_impulse(anchor_A, anchor_B, normal_impulse);
		}

		// The friction loop
		for (unsigned int point_index = 0; point_index < point_count; point_index++) {
			ContactConstraintPoint& point = constraint.points[point_index];

			float3 anchor_A = point.local_anchor_A;
			float3 anchor_B = point.local_anchor_B;

			// Recalculate the relative velocity at the point
			// We can't use the value from the normal loop since it
			// Has changed because of the applied impulse
			float3 relative_velocity = compute_relative_velocity(anchor_A, anchor_B);

			// We need to calculate 2 separate impulses. One for each tangent direction
			float relative_velocity_tangent_1 = Dot(relative_velocity, constraint.contact->tangent_1);
			float relative_velocity_tangent_2 = Dot(relative_velocity, constraint.contact->tangent_2);

			// The clamping needs to be modified a bit
			// The squared sum of the impulses needs to be smaller or equal to the squared
			// Of the normal friction force. So the clamping means to scale down both components
			// By a reduction factor
			float impulse_1 = -point.tangent_mass_1 * relative_velocity_tangent_1;
			float impulse_2 = -point.tangent_mass_2 * relative_velocity_tangent_2;

			float max_impulse = constraint.contact->friction * point.normal_impulse;
			float max_impulse_squared = max_impulse * max_impulse;
			float component_impulse_squared = impulse_1 * impulse_1 + impulse_2 * impulse_2;
			if (component_impulse_squared > max_impulse_squared) {
				float component_impulse = sqrt(component_impulse_squared);
				float reduction_factor = component_impulse / max_impulse;
				impulse_1 *= reduction_factor;
				impulse_2 *= reduction_factor;
			}

			// Apply the impulse now
			float3 tangent_impulse_1 = constraint.contact->tangent_1 * tangent_impulse_1;
			float3 tangent_impulse_2 = constraint.contact->tangent_2 * tangent_impulse_2;
			apply_impulse(anchor_A, anchor_B, tangent_impulse_1);
			apply_impulse(anchor_A, anchor_B, tangent_impulse_2);
		}

		// Write the accumulated values now
		constraint.rigidbody_A->velocity = velocity_A;
		constraint.rigidbody_B->velocity = velocity_B;
		constraint.rigidbody_A->angular_velocity = angular_A;
		constraint.rigidbody_B->angular_velocity = angular_B;
	}
}

ECS_THREAD_TASK(SolveContactConstraints) {
	SolverData* data = (SolverData*)_data;
	
	// Prepare the contact data
	PrepareContactConstraintsData(world, data);

	// Repeat the solve step for the number of iterations
	for (unsigned int index = 0; index < data->iterations; index++) {
		SolveContactConstraintsIteration(data, world->inverse_delta_time);
	}
	
	for (unsigned int index = 0; index < data->constraints.size; index++) {
		data->allocator.Deallocate(data->constraints[index].contact);
	}
	data->constraints.Reset();
}

static void SolveContactConstraintsInitialize(World* world, StaticThreadTaskInitializeInfo* info) {
	SolverData* data = info->Allocate<SolverData>();
	data->allocator = MemoryManager(ECS_MB, ECS_KB * 4, ECS_MB * 20, world->memory);
	data->constraints.Initialize(&data->allocator, 32);
	data->iterations = 4;
	data->baumgarte_factor = 0.2f;
	data->linear_slop = 0.0f;

	// Bind this so we can access the data from outside the main function
	world->system_manager->BindData(SOLVER_DATA_STRING, data);
}

void AddSolverTasks(ModuleTaskFunctionData* data) {
	TaskSchedulerElement solve_element;
	solve_element.initialize_task_function = SolveContactConstraintsInitialize;
	solve_element.task_group = ECS_THREAD_TASK_SIMULATE_MID;
	ECS_REGISTER_TASK(solve_element, SolveContactConstraints, data);
}

//void AddContactConstraint(
//	World* world, 
//	const Contact* contact,
//	Rigidbody* rigidbody_A,
//	Rigidbody* rigidbody_B,
//	float3 center_of_mass_A, 
//	float3 center_of_mass_B
//) {
//	// At the moment, allocate the contact directly now
//	SolverData* data = (SolverData*)world->system_manager->GetData(SOLVER_DATA_STRING);
//	Contact* allocated_contact = (Contact*)data->allocator.Allocate(sizeof(Contact));
//	*allocated_contact = *contact;
//
//	// Reserve the constraint, and compute the constraint points from the manifold points
//	// But before that, we need to compute the tangent directions. What we can do, in order
//	// To align the pair of tangents with the relative velocity is to project the relative
//	// Velocity in the manifold plane. That is one of the tangents and the other one is the
//	// Cross product between the manifold normal and the other tangent
//	PlaneScalar manifold_plane = contact->manifold.GetPlane();
//	float3 manifold_point = contact->manifold.contact_points[0];
//	float3 projected_point_A = ProjectPointOnPlane(manifold_plane, manifold_point + rigidbody_A->velocity);
//	float3 projected_point_B = ProjectPointOnPlane(manifold_plane, manifold_point + rigidbody_B->velocity);
//	float3 projected_speed = projected_point_B - projected_point_A;
//
//	// In case the projected speed is close to 0.0f, choose an arbitrary other direction
//	// Like the projected velocity of body A
//	float3 tangent_1;
//	if (CompareMask(projected_speed, float3::Splat(0.0f))) {
//		tangent_1 = Normalize(projected_point_A - manifold_point);
//	}
//	else {
//		tangent_1 = Normalize(projected_speed);
//	}
//	float3 tangent_2 = Cross(contact->manifold.separation_axis, tangent_1);
//
//	ContactConstraint* constraint = data->constraints.buffer + data->constraints.ReserveRange();
//	constraint->contact = allocated_contact;
//
//	ComputeConstraintPointInfo compute_info;
//	compute_info.center_of_mass_A = center_of_mass_A;
//	compute_info.center_of_mass_B = center_of_mass_B;
//	compute_info.rigidbody_A = rigidbody_A;
//	compute_info.rigidbody_B = rigidbody_B;
//	compute_info.separation = contact->manifold.separation_distance;
//	compute_info.separation_axis = contact->manifold.separation_axis;
//	compute_info.tangent_1 = tangent_1;
//	compute_info.tangent_2 = tangent_2;
//	for (unsigned int index = 0; index < contact->manifold.contact_point_count; index++) {
//		compute_info.point = contact->manifold.contact_points[index];
//		constraint->points[index] = ComputeConstraintPointFromManifold(&compute_info);
//	}
//}

void AddContactConstraint(
	World* world,
	const EntityContact* contact,
	float3 center_of_mass_A,
	float3 center_of_mass_B
) {
	// At the moment, allocate the contact directly now
	SolverData* data = (SolverData*)world->system_manager->GetData(SOLVER_DATA_STRING);
	Contact* allocated_contact = (Contact*)data->allocator.Allocate(sizeof(Contact));
	*((EntityContact*)allocated_contact) = *contact;
	
	// TODO: Decide which tangent computation form we should use
	// Aligning the tangents with the initial projected relative velocity
	// Should provide more consistent results
	// Reserve the constraint, and compute the constraint points from the manifold points
	//// But before that, we need to compute the tangent directions. What we can do, in order
	//// To align the pair of tangents with the relative velocity is to project the relative
	//// Velocity in the manifold plane. That is one of the tangents and the other one is the
	//// Cross product between the manifold normal and the other tangent
	//PlaneScalar manifold_plane = contact->manifold.GetPlane();
	//float3 manifold_point = contact->manifold.contact_points[0];
	//float3 projected_point_A = ProjectPointOnPlane(manifold_plane, manifold_point + rigidbody_A->velocity);
	//float3 projected_point_B = ProjectPointOnPlane(manifold_plane, manifold_point + rigidbody_B->velocity);
	//float3 projected_speed = projected_point_B - projected_point_A;
	//
	//// In case the projected speed is close to 0.0f, choose an arbitrary other direction
	//// Like the projected velocity of body A
	//float3 tangent_1;
	//if (CompareMask(projected_speed, float3::Splat(0.0f))) {
	//	tangent_1 = Normalize(projected_point_A - manifold_point);
	//}
	//else {
	//	tangent_1 = Normalize(projected_speed);
	//}
	//float3 tangent_2 = Cross(contact->manifold.separation_axis, tangent_1);

	// Compute the tangents, at the moment, as the 2 arbitrary perpendicular directions
	// In the manifold plane
	if (contact->manifold.contact_point_count <= 1) {
		// We cannot compute the tangets, we require at least 2 points
		// To obtain a direction inside the plane
		// So, make the tangents 0, which is the same as disabling friction
		allocated_contact->tangent_1 = float3::Splat(0.0f);
		allocated_contact->tangent_2 = float3::Splat(0.0f);
	}
	else {
		float3 plane_direction = contact->manifold.contact_points[1] - contact->manifold.contact_points[0];
		plane_direction = Normalize(plane_direction);
		allocated_contact->tangent_1 = plane_direction;
		allocated_contact->tangent_2 = Normalize(Cross(plane_direction, contact->manifold.separation_axis));
	}

	ContactConstraint* constraint = data->constraints.buffer + data->constraints.ReserveRange();
	constraint->contact = allocated_contact;
	constraint->center_of_mass_A = center_of_mass_A;
	constraint->center_of_mass_B = center_of_mass_B;
}