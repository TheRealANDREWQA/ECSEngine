#include "pch.h"
#include "ContactConstraint.h"
#include "ECSEngineForEach.h"
#include "Rigidbody.h"
#include "ECSEngineComponents.h"
#include "ECSEngineWorld.h"

#define SOLVER_DATA_STRING "SolverData"
#define MAX_BAUMGARTE_BIAS 1000.0f

struct ContactPair {
	ECS_INLINE bool operator == (ContactPair other) const {
		return (first == other.first && second == other.second) || (first == other.second && second == other.first);
	}
	
	ECS_INLINE unsigned int Hash() const {
		// Use CantorPair hashing such that reversed pairs will be considered the same
		return CantorPair(first.value, second.value);
	}

	Entity first;
	Entity second;
};

typedef HashTable<ContactConstraint, ContactPair, HashFunctionPowerOfTwo> ContactTable;

struct SolverData {
	unsigned int iterations;
	float baumgarte_factor;
	float linear_slop;
	MemoryManager allocator;
	ContactTable contact_table;
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
	PlaneScalar friction_plane_A;
	PlaneScalar friction_plane_B;
};

static ContactConstraintPoint ComputeConstraintPointFromManifold(const ComputeConstraintPointInfo* info) {
	ContactConstraintPoint constraint_point;

	constraint_point.normal_impulse = 0.0f;
	constraint_point.tangent_impulse_1 = 0.0f;
	constraint_point.tangent_impulse_2 = 0.0f;
	constraint_point.separation = info->separation;
	constraint_point.local_anchor_A = info->point - info->center_of_mass_A;
	constraint_point.local_anchor_B = info->point - info->center_of_mass_B;

	// Calculate the normal and tangent effective mass
	// The effective mass along the direction d 
	// has the following formula
	// The masses and the inertia tensors are the inverse
	// pA = rA x d (rA is the displacement vector - the local anchor)
	// pB = rB x d (rB is the displacement vector - the local anchor)
	// effective_mass = 1.0f / (mA + mB + Dot(IA * pA, pA) + Dot(IB * pB, pB))
	// The * for the inertia tensor is matrix multiplication
	auto compute_effective_mass = [&](float3 direction) {
		float3 perpendicular_A = Cross(constraint_point.local_anchor_A, direction);
		float3 perpendicular_B = Cross(constraint_point.local_anchor_B, direction);

		float3 rotated_A = MatrixVectorMultiply(perpendicular_A, info->rigidbody_A->world_space_inertia_tensor_inverse);
		float3 rotated_B = MatrixVectorMultiply(perpendicular_B, info->rigidbody_B->world_space_inertia_tensor_inverse);

		float value = info->rigidbody_A->mass_inverse + info->rigidbody_B->mass_inverse
			+ Dot(perpendicular_A, rotated_A)
			+ Dot(perpendicular_B, rotated_B);
		// Sanity check. This values should be above 0.0f, if it is not, set it to 0.0f
		return value > 0.0f ? 1.0f / value : 0.0f;
	};

	constraint_point.normal_mass = compute_effective_mass(info->separation_axis);
	constraint_point.tangent_mass_1 = compute_effective_mass(info->tangent_1);
	constraint_point.tangent_mass_2 = compute_effective_mass(info->tangent_2);

	// Project the local anchor on the friction plane
	constraint_point.friction_local_anchor_A = ProjectPointOnPlane(info->friction_plane_A, info->point) - info->center_of_mass_A;
	constraint_point.friction_local_anchor_B = ProjectPointOnPlane(info->friction_plane_B, info->point) - info->center_of_mass_B;

	return constraint_point;
}

// It will update the constraints with data that can be precomputed
// And reused across iterations
static void PrepareContactConstraintsData(World* world, SolverData* solver_data, Stream<unsigned int> iteration_indices) {
	for (size_t index = 0; index < iteration_indices.size; index++) {
		ContactConstraint& constraint = *solver_data->contact_table.GetValuePtrFromIndex(iteration_indices[index]);

		// Retrieve the rigidbodies for the 2 entities
		Rigidbody* rigidbody_A = world->entity_manager->GetComponent<Rigidbody>(constraint.contact.base.entity_A);
		Rigidbody* rigidbody_B = world->entity_manager->GetComponent<Rigidbody>(constraint.contact.base.entity_B);
		constraint.rigidbody_A = rigidbody_A;
		constraint.rigidbody_B = rigidbody_B;

		// We need to compute the tangent directions. What we can do, in order
		// To align the pair of tangents with the relative velocity, is to project the relative
		// Velocity in the manifold plane. That is one of the tangents and the other one is the
		// Cross product between the manifold normal and the other tangent
		//PlaneScalar manifold_plane = constraint.contact->manifold.GetPlane();
		//float3 manifold_point = constraint.contact->manifold.points[0];
		//float3 projected_point_A = ProjectPointOnPlane(manifold_plane, manifold_point + rigidbody_A->velocity);
		//float3 projected_point_B = ProjectPointOnPlane(manifold_plane, manifold_point + rigidbody_B->velocity);
		//float3 projected_speed = projected_point_B - projected_point_A;

		//// In case the projected speed is close to 0.0f, choose an arbitrary other direction
		//// Like the projected velocity of body A
		//float3 tangent_1;
		//float3 tangent_2;
		//if (CompareMask(projected_speed, float3::Splat(0.0f))) {
		//	// There is no relative speed, we can make the tangents 0
		//	tangent_1 = float3::Splat(0.0f);
		//	tangent_2 = float3::Splat(0.0f);
		//}
		//else {
		//	tangent_1 = Normalize(projected_speed);
		//	tangent_2 = Cross(constraint.contact->manifold.separation_axis, tangent_1);
		//}

		//tangent_1 = float3::Splat(0.0f);
		//tangent_2 = float3::Splat(0.0f);

		// To compute the tangents, we need the relative velocity
		// But there are cases where the objects have no relative velocity
		// From translation, but have relative velocity from angular velocity
		// As such, to perform a crude approximation of this, compute the relative
		// Velocity of the first manifold point, and then compute the tangents
		// From that velocity. Technically, in this way, the tangents are aligned
		// For one point and not for the others, but it is the best approximation
		float3 manifold_point = constraint.contact.base.manifold.points[0];
		float3 local_anchor_A = manifold_point - constraint.center_of_mass_A;
		float3 local_anchor_B = manifold_point - constraint.center_of_mass_B;
		float3 velocity_A = ComputeVelocity(constraint.rigidbody_A, local_anchor_A);
		float3 velocity_B = ComputeVelocity(constraint.rigidbody_B, local_anchor_B);
		float3 relative_speed = velocity_A - velocity_B;

		// Project the relative speed on the manifold plane
		PlaneScalar manifold_plane = constraint.contact.base.manifold.GetPlane();
		float3 projected_point = ProjectPointOnPlane(manifold_plane, manifold_point + relative_speed);

		float3 projected_speed = projected_point - manifold_point;
		float3 tangent_1;
		float3 tangent_2;
		if (CompareMask(projected_speed, float3::Splat(0.0f))) {
			tangent_1 = float3::Splat(0.0f);
			tangent_2 = float3::Splat(0.0f);
		}
		else {
			tangent_1 = Normalize(projected_speed);
			tangent_2 = Normalize(Cross(constraint.contact.base.manifold.separation_axis, tangent_1));
		}
		constraint.contact.tangent_1 = tangent_1;
		constraint.contact.tangent_2 = tangent_2;

		float body_distance_A = DistanceToPlane(manifold_plane, constraint.center_of_mass_A);
		float body_distance_B = DistanceToPlane(manifold_plane, constraint.center_of_mass_B);

		ComputeConstraintPointInfo compute_info;
		compute_info.center_of_mass_A = constraint.center_of_mass_A;
		compute_info.center_of_mass_B = constraint.center_of_mass_B;
		compute_info.rigidbody_A = rigidbody_A;
		compute_info.rigidbody_B = rigidbody_B;
		compute_info.separation = constraint.contact.base.manifold.separation_distance;
		compute_info.separation_axis = constraint.contact.base.manifold.separation_axis;
		compute_info.tangent_1 = tangent_1;
		compute_info.tangent_2 = tangent_2;
		compute_info.friction_plane_A = { manifold_plane.normal, manifold_plane.dot + body_distance_A };
		compute_info.friction_plane_B = { manifold_plane.normal, manifold_plane.dot + body_distance_B };
		for (unsigned int index = 0; index < constraint.contact.base.manifold.point_count; index++) {
			compute_info.point = constraint.contact.base.manifold.points[index];
			constraint.contact.points[index] = ComputeConstraintPointFromManifold(&compute_info);
		}
	}
}

static void SolveContactConstraintsIteration(SolverData* solver_data, float delta_time_inverse, Stream<unsigned int> iteration_indices) {
	for (size_t index = 0; index < iteration_indices.size; index++) {
		ContactConstraint& constraint = *solver_data->contact_table.GetValuePtrFromIndex(iteration_indices[index]);

		float3 normal = constraint.contact.base.manifold.separation_axis;

		float3 velocity_A = constraint.rigidbody_A->velocity;
		float3 velocity_B = constraint.rigidbody_B->velocity;
		float3 angular_A = constraint.rigidbody_A->angular_velocity;
		float3 angular_B = constraint.rigidbody_B->angular_velocity;

		auto compute_relative_velocity = [&](float3 anchor_A, float3 anchor_B) {
			// Formula: v + omega x r where v is the linear velocity, omega the angular velocity and r the local anchor
			// Choose the relative velocity to be from A to B
			float3 relative_velocity_A = ComputeVelocity(velocity_A, angular_A, anchor_A);
			float3 relative_velocity_B = ComputeVelocity(velocity_B, angular_B, anchor_B);
			return relative_velocity_B - relative_velocity_A;
		};

		auto apply_impulse = [&](float3 anchor_A, float3 anchor_B, float3 impulse) {
			// The change in linear velocity is the impulse divided by mass
			// For the B body we must add the impulse
			ApplyImpulse(velocity_A, angular_A, constraint.rigidbody_A, anchor_A, -impulse);
			ApplyImpulse(velocity_B, angular_B, constraint.rigidbody_B, anchor_B, impulse);
		};

		// Solve the normal impulse first, then apply the friction impulse
		unsigned int point_count = constraint.contact.base.manifold.point_count;
		for (unsigned int point_index = 0; point_index < point_count; point_index++) {
			ContactConstraintPoint& point = constraint.contact.points[point_index];
			
			// Compute the Baumgarte bias factor
			float bias = 0.0f;
			if (point.separation > 0.0f) {
				// This shouldn't happen
			}
			else {
				float adjusted_separation = point.separation + solver_data->linear_slop;
				adjusted_separation = ClampMax(adjusted_separation, 0.0f);
				bias = delta_time_inverse * adjusted_separation * solver_data->baumgarte_factor;
				bias = ClampMin(bias, -MAX_BAUMGARTE_BIAS);
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
		if (constraint.contact.base.friction > 0.0f) {
			for (unsigned int point_index = 0; point_index < point_count; point_index++) {
				ContactConstraintPoint& point = constraint.contact.points[point_index];

				float max_impulse = constraint.contact.base.friction * point.normal_impulse;
				if (max_impulse > 0.0f) {
					// Use the normal anchors to determine the velocity
					float3 anchor_A = point.local_anchor_A;
					float3 anchor_B = point.local_anchor_B;

					// Recalculate the relative velocity at the point
					// We can't use the value from the normal loop since it
					// Has changed because of the applied impulse
					float3 relative_velocity = compute_relative_velocity(anchor_A, anchor_B);

					// We need to calculate 2 separate impulses. One for each tangent direction
					float relative_velocity_tangent_1 = Dot(relative_velocity, constraint.contact.tangent_1);
					float relative_velocity_tangent_2 = Dot(relative_velocity, constraint.contact.tangent_2);

					// The clamping needs to be modified a bit
					// The squared sum of the impulses needs to be smaller or equal to the squared
					// Of the normal friction force. So the clamping means to scale down both components
					// By a reduction factor
					float impulse_1 = -point.tangent_mass_1 * relative_velocity_tangent_1;
					float impulse_2 = -point.tangent_mass_2 * relative_velocity_tangent_2;

					// TODO: Do we need circular clamping for better "realism"?
					//float max_impulse_squared = max_impulse * max_impulse;
					//float component_impulse_squared = impulse_1 * impulse_1 + impulse_2 * impulse_2;
					//if (component_impulse_squared > max_impulse_squared) {
					//	float component_impulse = sqrt(component_impulse_squared);
					//	// Divide the max impulse by the component impulse to determine
					//	// By how much to reduce the individual impulses
					//	float reduction_factor = max_impulse / component_impulse;
					//	impulse_1 *= reduction_factor;
					//	impulse_2 *= reduction_factor;
					//}

					float clamped_impulse_1 = Clamp(point.tangent_impulse_1 + impulse_1, -max_impulse, max_impulse);
					impulse_1 = clamped_impulse_1 - point.tangent_impulse_1;
					point.tangent_impulse_1 = clamped_impulse_1;

					float clamped_impulse_2 = Clamp(point.tangent_impulse_2 + impulse_2, -max_impulse, max_impulse);
					impulse_2 = clamped_impulse_2 - point.tangent_impulse_2;
					point.tangent_impulse_2 = clamped_impulse_2;

					// Apply the impulse now
					float3 tangent_impulse_1 = constraint.contact.tangent_1 * impulse_1;
					float3 tangent_impulse_2 = constraint.contact.tangent_2 * impulse_2;

					// Use the friction anchors to apply the impulse at
					apply_impulse(point.friction_local_anchor_A, point.friction_local_anchor_B, tangent_impulse_1);
					apply_impulse(point.friction_local_anchor_A, point.friction_local_anchor_B, tangent_impulse_2);
				}
			}
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
	
	//if (data->constraints.size > 0) {
	//	StopSimulation(world);
	//}

	if (data->contact_table.GetCount() > 0) {
		CapacityStream<unsigned int> iteration_indices;
		iteration_indices.Initialize(&data->allocator, 0, data->contact_table.GetCount());

		data->contact_table.GetElementIndices(iteration_indices);

		// Prepare the contact data
		PrepareContactConstraintsData(world, data, iteration_indices);

		// Repeat the solve step for the number of iterations
		for (unsigned int index = 0; index < data->iterations; index++) {
			SolveContactConstraintsIteration(data, world->inverse_delta_time, iteration_indices);
		}

		// Reduce the reference counts and remove those that are not needed anymore
		// Unfortunately, we cannot use the iteration indices for this, since the iteration
		// Will remove the values as it goes
		data->contact_table.ForEachIndex([&](unsigned int index) {
			ContactConstraint& constraint = *data->contact_table.GetValuePtrFromIndex(index);
			constraint.reference_count--;
			
			if (constraint.reference_count == 0) {
				data->contact_table.EraseFromIndex(index);
				return true;
			}
			return false;
		});

		data->allocator.Deallocate(iteration_indices.buffer);
		// Trim the contact table, such that it doesn't occupy too much memory
		data->contact_table.Trim(&data->allocator);
	}
}

static void SolveContactConstraintsInitialize(World* world, StaticThreadTaskInitializeInfo* info) {
	SolverData* data = info->Allocate<SolverData>();
	data->allocator = MemoryManager(ECS_MB, ECS_KB * 4, ECS_MB * 20, world->memory);
	data->contact_table.Initialize(&data->allocator, 128);
	data->iterations = 4;
	data->baumgarte_factor = 0.05f;
	data->linear_slop = 0.01f;

	// Bind this so we can access the data from outside the main function
	world->system_manager->BindData(SOLVER_DATA_STRING, data);
}

void AddSolverTasks(ModuleTaskFunctionData* data) {
	TaskSchedulerElement solve_element;
	solve_element.initialize_task_function = SolveContactConstraintsInitialize;
	solve_element.task_group = ECS_THREAD_TASK_SIMULATE_MID;
	ECS_REGISTER_TASK(solve_element, SolveContactConstraints, data);
}

// It will modify the constraint such that the new contact is accounted for,
// While preserving existing points, if they are the same
static void MatchContactConstraint(const EntityContact* contact, float3 center_of_mass_A, float3 center_of_mass_B, ContactConstraint& constraint) {
	// Firstly, check to see if the contact type has changed, from face to edge
	// Or vice versa
	if (contact->manifold.is_face_contact == constraint.contact.base.manifold.is_face_contact) {
		// Check the order of entities
		if (contact->entity_A == constraint.contact.base.entity_A) {

		}
		else {

		}
		
		//constraint.contact.base.manifold.separation_axis = contact->manifold.separation_axis;
		//constraint.contact.base.manifold.separation_distance = contact->manifold.separation_distance;
		constraint.contact.base.manifold = contact->manifold;
	}
	else {
		// Discard all the information
		constraint.contact.base.manifold = contact->manifold;
		// Zero out the entire constraint points. Technically, it 
		memset(constraint.contact.points, 0, sizeof(constraint.contact.points));
	}

	// Update the remaining data as is
	constraint.center_of_mass_A = center_of_mass_A;
	constraint.center_of_mass_B = center_of_mass_B;
	constraint.contact.base.friction = contact->friction;
	constraint.contact.base.restitution = contact->restitution;

	// Increment the reference count such that it won't get discarded
	constraint.reference_count++;
}

void AddContactConstraint(
	World* world,
	const EntityContact* contact,
	float3 center_of_mass_A,
	float3 center_of_mass_B
) {
	// At the moment, allocate the contact directly now
	SolverData* data = (SolverData*)world->system_manager->GetData(SOLVER_DATA_STRING);

	// Check to see if the pair already exists
	ContactPair contact_pair = { contact->entity_A, contact->entity_B };
	unsigned int pair_index = data->contact_table.Find(contact_pair);
	
	if (pair_index == -1) {
		ContactConstraint constraint;
		constraint.contact.base = *contact;
		constraint.reference_count = 1;
		constraint.center_of_mass_A = center_of_mass_A;
		constraint.center_of_mass_B = center_of_mass_B;

		data->contact_table.InsertDynamic(&data->allocator, constraint, contact_pair);
	}
	else {
		ContactConstraint& constraint = *data->contact_table.GetValuePtrFromIndex(pair_index);
		// Match the constraint
		MatchContactConstraint(contact, center_of_mass_A, center_of_mass_B, constraint);
	}
}