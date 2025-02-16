#include "pch.h"
#include "ContactConstraint.h"
#include "ECSEngineForEach.h"
#include "Rigidbody.h"
#include "ECSEngineComponents.h"
#include "ECSEngineWorld.h"
#include "CollisionDetection/src/CollisionDetectionComponents.h"
#include "CollisionDetection/src/GJK.h"
#include "SolverData.h"
#include "Settings.h"

#define MAX_BAUMGARTE_BIAS 1000.0f

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

static void ComputeConstraintPointFromManifold(const ComputeConstraintPointInfo* info, ContactConstraintPoint& constraint_point) {
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
}

static void WarmStartConstraint(ContactConstraint& constraint) {
	float3 velocity_A = constraint.rigidbody_A->velocity;
	float3 velocity_B = constraint.rigidbody_B->velocity;
	float3 angular_A = constraint.rigidbody_A->angular_velocity;
	float3 angular_B = constraint.rigidbody_B->angular_velocity;

	size_t point_count = constraint.PointCount();
	for (size_t index = 0; index < point_count; index++) {
		const ContactConstraintPoint& point = constraint.contact.points[index];

		float3 normal_impulse = constraint.contact.base.manifold.separation_axis * point.normal_impulse;
		ApplyImpulse(constraint.rigidbody_A, point.local_anchor_A, -normal_impulse);
		ApplyImpulse(constraint.rigidbody_B, point.local_anchor_B, normal_impulse);
	
		float3 tangent_impulse_1 = constraint.contact.tangent_1 * point.tangent_impulse_1;
		float3 tangent_impulse_2 = constraint.contact.tangent_2 * point.tangent_impulse_2;
		ApplyImpulse(constraint.rigidbody_A, point.friction_local_anchor_A, -tangent_impulse_1);
		ApplyImpulse(constraint.rigidbody_B, point.friction_local_anchor_B, tangent_impulse_1);
		ApplyImpulse(constraint.rigidbody_A, point.friction_local_anchor_A, -tangent_impulse_2);
		ApplyImpulse(constraint.rigidbody_B, point.friction_local_anchor_B, tangent_impulse_2);
	}

	constraint.rigidbody_A->velocity = velocity_A;
	constraint.rigidbody_A->angular_velocity = angular_A;
	constraint.rigidbody_B->velocity = velocity_B;
	constraint.rigidbody_B->angular_velocity = angular_B;
}

// It will update the constraints with data that can be precomputed
// And reused across iterations
static void PrepareContactConstraintsData(World* world, SolverData* solver_data, Stream<unsigned int> iteration_indices) {
	for (size_t index = 0; index < iteration_indices.size; index++) {
		ContactConstraint& constraint = *solver_data->contact_table.GetValueFromIndex(iteration_indices[index]);

		// Retrieve the rigidbodies for the 2 entities
		Rigidbody* rigidbody_A = world->entity_manager->GetComponent<Rigidbody>(constraint.contact.base.entity_A);
		Rigidbody* rigidbody_B = world->entity_manager->GetComponent<Rigidbody>(constraint.contact.base.entity_B);
		constraint.rigidbody_A = rigidbody_A;
		constraint.rigidbody_B = rigidbody_B;

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
		PlaneScalar manifold_plane = ContactManifoldGetPlane(constraint.contact.base.manifold);
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

		auto initialize_points = [&](auto use_warm_starting) {
			size_t point_count = constraint.PointCount();

			for (size_t point_index = 0; point_index < point_count; point_index++) {
				compute_info.point = constraint.contact.base.manifold.points[point_index];
				ContactConstraintPoint& point = constraint.contact.points[point_index];
				ComputeConstraintPointFromManifold(&compute_info, point);

				// Warm start the points here as well
				// This will help to remove another loop iteration
				// And have better temporal coherence, although
				// The accessed data set is small and probably won't
				// Make too much of a difference
				if constexpr (use_warm_starting) {
					float3 normal = constraint.contact.base.manifold.separation_axis;
					float3 normal_impulse = normal * point.normal_impulse;

					float3 tangent_1_impulse = tangent_1 * point.tangent_impulse_1;
					float3 tangent_2_impulse = tangent_2 * point.tangent_impulse_2;
					float3 tangent_impulse = tangent_1_impulse + tangent_2_impulse;

					// Apply the impulses at the 2 separate local anchors: the normal
					// Anchor and the friction anchor
					ApplyImpulse(rigidbody_A, point.local_anchor_A, -normal_impulse);
					ApplyImpulse(rigidbody_B, point.local_anchor_B, normal_impulse);
					
					ApplyImpulse(rigidbody_A, point.friction_local_anchor_A, -tangent_impulse);
					ApplyImpulse(rigidbody_B, point.friction_local_anchor_B, tangent_impulse);
				}
			}
		};

		if (solver_data->use_warm_starting) {
			initialize_points(std::true_type{});
		}
		else {
			initialize_points(std::false_type{});
		}
	}
}

static void SolveContactConstraintsIteration(SolverData* solver_data, float delta_time_inverse, Stream<unsigned int> iteration_indices) {
	for (size_t index = 0; index < iteration_indices.size; index++) {
		ContactConstraint& constraint = *solver_data->contact_table.GetValueFromIndex(iteration_indices[index]);

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
		size_t point_count = constraint.PointCount();
		for (size_t point_index = 0; point_index < point_count; point_index++) {
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
			for (size_t point_index = 0; point_index < point_count; point_index++) {
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

					// We can compose the 2 impulses into a single value and apply it just once
					float3 tangent_impulse = tangent_impulse_1 + tangent_impulse_2;
					
					// Use the friction anchors to apply the impulse at
					apply_impulse(point.friction_local_anchor_A, point.friction_local_anchor_B, tangent_impulse);
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

static void DiscardContactConstraintPoint(ContactConstraintPoint& point) {
	// The information to be discarded are the accumulated impulses
	point.normal_impulse = 0.0f;
	point.tangent_impulse_1 = 0.0f;
	point.tangent_impulse_2 = 0.0f;
}

static void DiscardContactConstraintWarmStarting(ContactConstraint& constraint) {
	// Each discard can happen individually for each point
	size_t point_count = constraint.PointCount();
	for (size_t index = 0; index < point_count; index++) {
		DiscardContactConstraintPoint(constraint.contact.points[index]);
	}
}

// It will modify the constraint such that the new contact is accounted for,
// While preserving existing points, if they are the same
static void MatchContactConstraint(const EntityContact* contact, float3 center_of_mass_A, float3 center_of_mass_B, ContactConstraint& constraint) {
	// Firstly, check to see if the contact type has changed, from face to edge
	// Or vice versa
	if (contact->manifold.is_face_contact == constraint.contact.base.manifold.is_face_contact) {
		// Check the order of entities
		bool same_features_and_order = false;
		bool same_features_inverted_order = false;
		if (contact->entity_A == constraint.contact.base.entity_A) {
			same_features_and_order = contact->manifold.feature_index_A == constraint.contact.base.manifold.feature_index_A
				&& contact->manifold.feature_index_B == constraint.contact.base.manifold.feature_index_B;
			same_features_inverted_order = contact->manifold.feature_index_A == constraint.contact.base.manifold.feature_index_B
				&& contact->manifold.feature_index_B == constraint.contact.base.manifold.feature_index_A;
		}
		else {
			same_features_and_order = contact->manifold.feature_index_A == constraint.contact.base.manifold.feature_index_B
				&& contact->manifold.feature_index_B == constraint.contact.base.manifold.feature_index_A;
			same_features_inverted_order = contact->manifold.feature_index_A == constraint.contact.base.manifold.feature_index_A
				&& contact->manifold.feature_index_B == constraint.contact.base.manifold.feature_index_B;
		}
		/*same_features_and_order = contact->entity_A == constraint.contact.base.entity_A &&
			contact->entity_B == constraint.contact.base.entity_B;*/

			// The matching function receives as value the index of the constraint point
		auto match_points = [&](auto matching_function) {
			ECS_STACK_CAPACITY_STREAM(unsigned int, contact_add_indices, ECS_COUNTOF(ContactManifold::points));
			contact_add_indices.size = contact->manifold.point_count;
			MakeSequence(contact_add_indices);

			ECS_STACK_CAPACITY_STREAM(unsigned int, matched_indices, 64);

			// Must reference the point directly since we are removing swap back
			// Which modifies its value
			for (unsigned int index = 0; index < constraint.contact.base.manifold.point_count; index++) {
				unsigned int contact_index = matching_function(contact->manifold, index, contact_add_indices);
				if (contact_index != -1) {
					// If we find the contact point in the new manifold, discard its addition
					unsigned int indices_index = contact_add_indices.Find(contact_index);
					matched_indices.Add(contact_index);
					contact_add_indices.RemoveSwapBack(indices_index);

					// Update the location of the point
					constraint.contact.base.manifold.points[index] = contact->manifold.points[contact_index];
				}
				else {
					// The constraint point no longer appears in the new contact manifold
					// This constraint point needs to be discarded with a remove swap back
					constraint.contact.base.manifold.RemoveSwapBack(index);

					// We also need to remove swap back the point extra information
					constraint.contact.points[index] = constraint.contact.points[constraint.contact.base.manifold.point_count];
					index--;
				}
			}

			ECS_CRASH_CONDITION(constraint.contact.base.manifold.point_count + contact_add_indices.size <= ECS_COUNTOF(ContactManifold::points),
				"Contact Manifold matching for warm starting produced too many values!");

			// Add any remaining points
			for (unsigned int index = 0; index < contact_add_indices.size; index++) {
				unsigned int add_index = contact_add_indices[index];
				DiscardContactConstraintPoint(constraint.contact.points[constraint.contact.base.manifold.point_count]);
				ContactManifoldFeaturesAddPoint(
					constraint.contact.base.manifold,
					contact->manifold.points[add_index],
					contact->manifold.point_indices[add_index],
					contact->manifold.point_edge_indices[add_index]
				);
			}

			// Update the separation axis and distance
			constraint.contact.base.manifold.separation_axis = contact->manifold.separation_axis;
			constraint.contact.base.manifold.separation_distance = contact->manifold.separation_distance;
		};

		// If they have the same features and in the same order, try to match the points by their IDs
		if (same_features_and_order) {
			match_points([&](const ContactManifoldFeatures& manifold, unsigned int index, Stream<unsigned int> manifold_indices) {
				return ContactManifoldFeaturesFind(
					manifold,
					constraint.contact.base.manifold.point_indices[index],
					constraint.contact.base.manifold.point_edge_indices[index],
					manifold_indices
				);
				});
		}
		else {
			// If the features are the same but inverted, try to match them based upon the distance
			// TODO: Determine what is the best epsilon value for matching points based upon distance
			// And if using a small Lerp factor in case it is outside the epsilon but close to it is a
			// Good idea
			if (same_features_inverted_order) {
				match_points([&](const ContactManifoldFeatures& manifold, unsigned int index, Stream<unsigned int> manifold_indices) {
					const float DISTANCE_EPSILON = 0.001f;
					return ContactManifoldFindByPosition(
						manifold,
						constraint.contact.base.manifold.points[index],
						float3::Splat(DISTANCE_EPSILON),
						manifold_indices
					);
					});
			}
			else {
				// Can discard all the data, since the contact features have changed
				constraint.contact.base.manifold = contact->manifold;
				DiscardContactConstraintWarmStarting(constraint);
			}
		}
	}
	else {
		// Discard all the information
		constraint.contact.base.manifold = contact->manifold;
		DiscardContactConstraintWarmStarting(constraint);
	}

	// Update the remaining data as is
	constraint.center_of_mass_A = center_of_mass_A;
	constraint.center_of_mass_B = center_of_mass_B;
	constraint.contact.base.friction = contact->friction;
	constraint.contact.base.restitution = contact->restitution;
	// We also need to update the entities
	if (constraint.contact.base.entity_A != contact->entity_A) {
		// The order has changed. We need to change the rigidbody pointers,
		// Since if an immediate removal of this contact is to happen, then
		// The rigidbody will be reversed, causing issues.
		constraint.contact.base.entity_A = contact->entity_A;
		constraint.contact.base.entity_B = contact->entity_B;
		swap(constraint.rigidbody_A, constraint.rigidbody_B);
	}

	// Increment the reference count such that it won't get discarded
	constraint.reference_count++;
}

// Returns the hash table index of the contact constraint
static unsigned int GetContactConstraintIndex(SolverData* solver_data, Entity entity_A, Entity entity_B) {
	EntityPair contact_pair = { entity_A, entity_B };
	return solver_data->contact_table.Find(contact_pair);
}

static void InsertContactConstraint(SolverData* solver_data, const EntityContact* contact, float3 center_of_mass_A, float3 center_of_mass_B, const Rigidbody* first_rigidbody, const Rigidbody* second_rigidbody) {
	ContactConstraint* constraint = (ContactConstraint*)solver_data->allocator.Allocate(sizeof(ContactConstraint));
	constraint->contact.base = *contact;
	// Insert the constraint with the reference count of 2, such that
	// It survives the first decrement and will get evicted the 2nd time,
	// Unless it is added again
	constraint->reference_count = 2;
	constraint->center_of_mass_A = center_of_mass_A;
	constraint->center_of_mass_B = center_of_mass_B;

	// Set the rigidbodies now, even tho they are given as const and the structure takes them as mutable,
	// They might be needed to be accessed in the functions in the call chain, since they might need to read
	// Data from them
	constraint->rigidbody_A = (Rigidbody*)first_rigidbody;
	constraint->rigidbody_B = (Rigidbody*)second_rigidbody;

	// This will discard the impulses, such that they don't have junk values
	DiscardContactConstraintWarmStarting(*constraint);
	solver_data->contact_table.InsertDynamic(&solver_data->allocator, constraint, { contact->entity_A, contact->entity_B });

	// Update the island manager
	solver_data->island_manager.AddContact(constraint);
}

static void DeallocateContactConstraint(SolverData* solver_data, ContactConstraint* constraint) {
	// Also, remove it from the island manager
	solver_data->island_manager.RemoveContact(constraint);
	solver_data->allocator.Deallocate(constraint);
}

ECS_THREAD_TASK(SolveContactConstraints) {
	//SolverData* data = (SolverData*)_data;
	SolverData* data = world->entity_manager->GetGlobalComponent<SolverData>();
	
	//if (data->constraints.size > 0) {
	//	StopSimulation(world);
	//}

	if (data->contact_table.GetCount() > 0) {
		if (world->entity_manager->ExistsGlobalComponent(PhysicsSettings::ID())) {
			PhysicsSettings* settings = world->entity_manager->GetGlobalComponent<PhysicsSettings>();
			data->iterations = settings->iterations;
			data->baumgarte_factor = settings->baumgarte_factor;
			data->use_warm_starting = settings->use_warm_starting;
		}

		CapacityStream<unsigned int> iteration_indices;
		iteration_indices.Initialize(&data->allocator, 0, data->contact_table.GetCount());

		// Iterate the table and retrieve the indices where items are alive, while
		// Reducing the reference count and removing the elements that have a reference
		// Count of 0
		data->contact_table.ForEachIndex([&iteration_indices, data](unsigned int index) {
			ContactConstraint* constraint = data->contact_table.GetValueFromIndex(index);
			constraint->reference_count--;

			if (constraint->reference_count == 0) {
				// Deallocate the constraint itself
				DeallocateContactConstraint(data, constraint);
				data->contact_table.EraseFromIndex(index);
				return true;
			}
			iteration_indices.Add(index);
			return false;
		});

		// Determine if a simulation time step should be performed
		float elapsed_time = data->previous_time_step_remainder + world->delta_time;
		while (elapsed_time > data->time_step_tick) {
			// Prepare the contact data
			PrepareContactConstraintsData(world, data, iteration_indices);

			// Repeat the solve step for the number of iterations
			for (unsigned int index = 0; index < data->iterations; index++) {
				SolveContactConstraintsIteration(data, data->inverse_time_step_tick, iteration_indices);
			}

			// Decrement the elapsed time
			elapsed_time -= data->time_step_tick;
		}

		data->allocator.Deallocate(iteration_indices.buffer);

		// Trim the contact table, such that it doesn't occupy too much memory
		data->contact_table.Trim(&data->allocator);

		// Update the simulation remainder
		data->previous_time_step_remainder = elapsed_time;
	}
}

static void SolveContactConstraintsInitialize(World* world, StaticThreadTaskInitializeInfo* info) {
	//SolverData* data = info->Allocate<SolverData>();
	//data->Initialize(world->memory);
	SolverData data;
	data.Initialize(world->memory);
	world->entity_manager->RegisterGlobalComponentCommit(&data);
}

void AddSolverTasks(ModuleTaskFunctionData* data) {
	TaskSchedulerElement solve_element;
	solve_element.initialize_task_function = SolveContactConstraintsInitialize;
	solve_element.task_group = ECS_THREAD_TASK_SIMULATE_MID;
	ECS_REGISTER_TASK(solve_element, SolveContactConstraints, data);
}

void AddContactConstraint(
	World* world,
	const EntityContact* contact,
	float3 center_of_mass_A,
	float3 center_of_mass_B,
	const Rigidbody* first_rigidbody,
	const Rigidbody* second_rigidbody
) {
	// At the moment, allocate the contact directly now
	SolverData* data = world->entity_manager->GetGlobalComponent<SolverData>();

	// Try to retrieve the existing pair
	unsigned int pair_index = GetContactConstraintIndex(data, contact->entity_A, contact->entity_B);
	if (pair_index == -1) {
		InsertContactConstraint(data, contact, center_of_mass_A, center_of_mass_B, first_rigidbody, second_rigidbody);
	}
	else {
		// Match the constraint
		ContactConstraint& constraint = *data->contact_table.GetValueFromIndex(pair_index);
		if (data->use_warm_starting) {
			MatchContactConstraint(contact, center_of_mass_A, center_of_mass_B, constraint);
		}
		else {
			constraint.contact.base = *contact;
			constraint.center_of_mass_A = center_of_mass_A;
			constraint.center_of_mass_B = center_of_mass_B;
			constraint.reference_count++;
			DiscardContactConstraintWarmStarting(constraint);
		}
	}
}

void AddContactPair(
	World* world,
	Entity entity_A,
	Entity entity_B
) {
	EntityManager* entity_manager = world->entity_manager;

	// PERFORMANCE TODO: Multiple try get components result in multiple repeated safety checks
	// Eliminate them by hoisting the component
	const ConvexCollider* first_collider = entity_manager->TryGetComponent<ConvexCollider>(entity_A);
	if (first_collider != nullptr && first_collider->hull.vertex_size > 0) {
		const Rigidbody* first_rigidbody = entity_manager->TryGetComponent<Rigidbody>(entity_A);
		if (first_rigidbody != nullptr) {
			const ConvexCollider* second_collider = entity_manager->TryGetComponent<ConvexCollider>(entity_B);
			if (second_collider != nullptr && second_collider->hull.vertex_size > 0) {
				const Rigidbody* second_rigidbody = entity_manager->TryGetComponent<Rigidbody>(entity_B);
				if (second_rigidbody != nullptr) {
					TransformScalar first_transform = GetEntityTransform(entity_manager, entity_A);
					TransformScalar second_transform = GetEntityTransform(entity_manager, entity_B);

					ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 64, ECS_MB);
					Matrix first_matrix = TransformToMatrix(&first_transform);
					ConvexHull first_collider_transformed = first_collider->hull.TransformToTemporary(first_matrix, &stack_allocator);

					Matrix second_matrix = TransformToMatrix(&second_transform);
					ConvexHull second_collider_transformed = second_collider->hull.TransformToTemporary(second_matrix, &stack_allocator);

					float distance = GJK(&first_collider_transformed, &second_collider_transformed);
					if (distance < 0.0f) {
						// Intersection
						//LogInfo("Collision");
						// Call the SAT to determine the contacting features
						//Timer timer;

						// Try to retrieve the contact constraint. If we successfully find it,
						// Then we can see what the order of the entities is, in order to match
						// The separation axis
						SATQuery query = SAT(&first_collider_transformed, &second_collider_transformed);
						//float duration = timer.GetDurationFloat(ECS_TIMER_DURATION_MS);
						//ECS_FORMAT_TEMP_STRING(message, "{#}\n", duration);
						//OutputDebugStringA(message.buffer);
						//bool redirect_value = world->debug_drawer->ActivateRedirectThread(thread_id);
						//if (query.type == SAT_QUERY_NONE) {
						//	world->debug_drawer->AddStringThread(thread_id, first_transform.position,
						//		float3::Splat(1.0f), 1.0f, "None", ECS_COLOR_ORANGE);
						//	/*Line3D first_line = first_collider_transformed.GetEdgePoints(17);
						//	Line3D second_line = second_collider_transformed.GetEdgePoints(1);
						//	world->debug_drawer->AddLineThread(thread_id, first_line.A, first_line.B, ECS_COLOR_ORANGE);
						//	world->debug_drawer->AddLineThread(thread_id, second_line.A, second_line.B, ECS_COLOR_ORANGE);*/
						//}
						//else if (query.type == SAT_QUERY_EDGE) {
						//	world->debug_drawer->AddStringThread(thread_id, first_transform.position,
						//		float3::Splat(1.0f), 1.0f, "Edge", ECS_COLOR_ORANGE);
						//	Line3D first_line = first_collider_transformed.GetEdgePoints(query.edge.edge_1_index);
						//	Line3D second_line = second_collider_transformed.GetEdgePoints(query.edge.edge_2_index);
						//	world->debug_drawer->AddLineThread(thread_id, first_line.A, first_line.B, ECS_COLOR_ORANGE);
						//	world->debug_drawer->AddLineThread(thread_id, second_line.A, second_line.B, ECS_COLOR_ORANGE);

						//	ContactManifold manifold = ComputeContactManifold(&first_collider_transformed, &second_collider_transformed, query);
						//	world->debug_drawer->AddPointThread(thread_id, manifold.points[0], 2.0f, ECS_COLOR_AQUA);

						//	//query.edge.edge_1_index = 97;
						//	//query.edge.edge_2_index = 1;
						//	const ConvexHullEdge& first_edge = first_collider_transformed.edges[query.edge.edge_1_index];
						//	const ConvexHullEdge& second_edge = second_collider_transformed.edges[query.edge.edge_2_index];
						//	float3 first_normal_1 = first_collider_transformed.faces[first_edge.face_1_index].plane.normal;
						//	float3 first_normal_2 = first_collider_transformed.faces[first_edge.face_2_index].plane.normal;
						//	float3 second_normal_1 = second_collider_transformed.faces[second_edge.face_1_index].plane.normal;
						//	float3 second_normal_2 = second_collider_transformed.faces[second_edge.face_2_index].plane.normal;
						//	world->debug_drawer->AddLineThread(thread_id, float3::Splat(0.0f), first_normal_1, ECS_COLOR_LIME);
						//	world->debug_drawer->AddLineThread(thread_id, float3::Splat(0.0f), first_normal_2, ECS_COLOR_LIME);
						//	world->debug_drawer->AddLineThread(thread_id, float3::Splat(0.0f), second_normal_1, ECS_COLOR_RED);
						//	world->debug_drawer->AddLineThread(thread_id, float3::Splat(0.0f), second_normal_2, ECS_COLOR_RED);
						//	world->debug_drawer->AddLineThread(thread_id, float3::Splat(0.0f), -second_normal_2, ECS_COLOR_BROWN);
						//	world->debug_drawer->AddLineThread(thread_id, first_normal_1, first_normal_2, ECS_COLOR_AQUA);
						//	world->debug_drawer->AddLineThread(thread_id, second_normal_1, second_normal_2, ECS_COLOR_WHITE);
						//}
						//else if (query.type == SAT_QUERY_FACE) {
						//	world->debug_drawer->AddStringThread(thread_id, first_transform.position,
						//		float3::Splat(1.0f), 1.0f, "Face", ECS_COLOR_ORANGE);
						//	const ConvexHull* convex_hull = query.face.first_collider ? &first_collider_transformed : &second_collider_transformed;
						//	const ConvexHull* second_hull = query.face.first_collider ? &second_collider_transformed : &first_collider_transformed;
						//	const ConvexHullFace& face_1 = convex_hull->faces[query.face.face_index];
						//	for (unsigned int index = 0; index < face_1.points.size; index++) {
						//		unsigned int next_index = index == face_1.points.size - 1 ? 0 : index + 1;
						//		float3 A = convex_hull->GetPoint(face_1.points[index]);
						//		float3 B = convex_hull->GetPoint(face_1.points[next_index]);
						//		world->debug_drawer->AddLineThread(thread_id, A, B, ECS_COLOR_ORANGE);
						//	}

						//	/*query.face.second_face_index = 9;
						//	const ConvexHullFace& face_2 = second_hull->faces[query.face.second_face_index];
						//	for (unsigned int index = 0; index < face_2.points.size; index++) {
						//		unsigned int next_index = index == face_2.points.size - 1 ? 0 : index + 1;
						//		float3 A = second_hull->GetPoint(face_2.points[index]);
						//		float3 B = second_hull->GetPoint(face_2.points[next_index]);
						//		world->debug_drawer->AddLineThread(thread_id, A, B, ECS_COLOR_ORANGE);
						//	}*/

						//	ContactManifold manifold = ComputeContactManifold(&first_collider_transformed, &second_collider_transformed, query);
						//	for (size_t index = 0; index < manifold.point_count; index++) {
						//		world->debug_drawer->AddPointThread(thread_id, manifold.points[index], 2.0f, ECS_COLOR_AQUA);
						//	}
						//}

						//if (query.type == SAT_QUERY_EDGE) {
						//	StopSimulation(world);
						//	Line3D first_line = first_collider_transformed.GetEdgePoints(query.edge.edge_1_index);
						//	Line3D second_line = second_collider_transformed.GetEdgePoints(query.edge.edge_2_index);
						//	world->debug_drawer->AddLine(first_line.A, first_line.B, ECS_COLOR_ORANGE);
						//	world->debug_drawer->AddLine(second_line.A, second_line.B, ECS_COLOR_ORANGE);
						//	world->debug_drawer->AddLine(first_collider_transformed.center, second_collider_transformed.center, ECS_COLOR_ORANGE);
						//	return;
						//}

						if (query.type != SAT_QUERY_NONE) {
							EntityContact contact;
							contact.entity_A = entity_A;
							contact.entity_B = entity_B;
							contact.friction = first_rigidbody->is_static ? second_rigidbody->friction : first_rigidbody->friction;
							contact.restitution = 0.0f;
							contact.manifold = ComputeContactManifold(
								world,
								&first_collider_transformed, 
								&second_collider_transformed, 
								query
							);

							float3 first_center_of_mass = first_rigidbody->center_of_mass + first_transform.position;
							float3 second_center_of_mass = second_rigidbody->center_of_mass + second_transform.position;
							// Perform the swap after the manifold computation because it can
							// Modify the query order
							if (query.type == SAT_QUERY_FACE) {
								if (!query.face.first_collider) {
									swap(contact.entity_A, contact.entity_B);
									swap(first_center_of_mass, second_center_of_mass);
									swap(first_rigidbody, second_rigidbody);
								}
							}
							
							AddContactConstraint(
								world,
								&contact,
								first_center_of_mass,
								second_center_of_mass,
								first_rigidbody,
								second_rigidbody
							);
						}

						//world->debug_drawer->DeactivateRedirectThread(thread_id, redirect_value);
					}
				}
			}
		}
	}
}