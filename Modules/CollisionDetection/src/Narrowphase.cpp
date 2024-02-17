#include "pch.h"
#include "Narrowphase.h"
#include "Graphics/src/Components.h"
#include "GJK.h"
#include "Components.h"

static ConvexHull CreateCubeHull(float* storage) {
	ConvexHull hull;

	hull.vertices_x = storage;
	hull.vertices_y = storage + 8;
	hull.vertices_z = storage + 16;
	hull.size = 8;

	hull.SetPoint({ -1.0f, -1.0f, -1.0f }, 0);
	hull.SetPoint({ 1.0f, -1.0f, -1.0f }, 1);
	hull.SetPoint({ -1.0f, 1.0f, -1.0f }, 2);
	hull.SetPoint({ -1.0f, -1.0f, 1.0f }, 3);
	hull.SetPoint({ 1.0f, 1.0f, -1.0f }, 4);
	hull.SetPoint({ 1.0f, -1.0f, 1.0f }, 5);
	hull.SetPoint({ -1.0f, 1.0f, 1.0f }, 6);
	hull.SetPoint({ 1.0f, 1.0f, 1.0f }, 7);

	return hull;
}

ECS_THREAD_TASK(NarrowphaseGridHandler) {
	FixedGridHandlerData* data = (FixedGridHandlerData*)_data;
	EntityManager* entity_manager = world->entity_manager;
	// Retrieve the meshes and check the collisions
	//const RenderMesh* first_mesh = entity_manager->TryGetComponent<RenderMesh>(data->first_identifier);
	const ConvexCollider* first_collider = entity_manager->TryGetComponent<ConvexCollider>(data->first_identifier);
	if (first_collider != nullptr && first_collider->mesh.position_size > 0) {
		//const RenderMesh* second_mesh = entity_manager->TryGetComponent<RenderMesh>(data->second_identifier);
		const ConvexCollider* second_collider = entity_manager->TryGetComponent<ConvexCollider>(data->second_identifier);
		if (second_collider != nullptr && second_collider->mesh.position_size > 0) {
			Translation* first_translation;
			Rotation* first_rotation;
			Scale* first_scale;
			GetEntityTransform(entity_manager, data->first_identifier, &first_translation, &first_rotation, &first_scale);

			Translation* second_translation;
			Rotation* second_rotation;
			Scale* second_scale;
			GetEntityTransform(entity_manager, data->second_identifier, &second_translation, &second_rotation, &second_scale);

			ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 64, ECS_MB);
			Matrix first_matrix = GetEntityTransformMatrix(first_translation, first_rotation, first_scale);
			TriangleMesh first_collider_transformed = first_collider->mesh.Transform(first_matrix, &stack_allocator);

			Matrix second_matrix = GetEntityTransformMatrix(second_translation, second_rotation, second_scale);
			TriangleMesh second_collider_transformed = second_collider->mesh.Transform(second_matrix, &stack_allocator);

			float distance = GJK(&first_collider_transformed, &second_collider_transformed);
			if (distance < 0.0f) {
				// Intersection
				LogInfo("Collision");
			}
			else {
				/*CameraCached camera = world->entity_manager->GetGlobalComponent<CameraComponent>()->value;
				float3 camera_pos = camera.translation;
				ECS_FORMAT_TEMP_STRING(duration_string, "{#}", distance);*/
				//world->debug_drawer->AddStringThread(thread_id, camera_pos + camera.GetForwardVector() * 2.0f, camera.GetRightVector(), 0.5f, duration_string, ECS_COLOR_AQUA);

				//float3 offset = second_translation != nullptr ? second_translation->value : float3::Splat(0.0f);
				//world->debug_drawer->AddStringThread(thread_id, offset, float3{ 1.0f, 0.0f, 0.0f }, 0.5f, duration_string, ECS_COLOR_AQUA);
				//world->debug_drawer->AddPointThread(thread_id, offset, 2.0f, ECS_COLOR_MAGENTA);

				//size_t reduction = first_collider->hull_size + second_collider->hull_size;
				///*size_t draw_count = reduction < GJK_MK_COUNT ? GJK_MK_COUNT - reduction : 0;
				//for (size_t index = 0; index < draw_count; index++) {
				//	world->debug_drawer->AddPointThread(thread_id, offset + GJK_MK[index], 3.0f, ECS_COLOR_PINK);
				//}*/
				//GJKSimplex simplex = GJK_SIMPLICES[reduction < 50 ? reduction : 49];
				//GJK_SIMPLEX[0] = simplex.points[0];
				//GJK_SIMPLEX[1] = simplex.points[1];
				//GJK_SIMPLEX[2] = simplex.points[2];
				//GJK_SIMPLEX[3] = simplex.points[3];
				//GJK_SIMPLEX_COUNT = simplex.point_count;

				//world->debug_drawer->AddPointThread(thread_id, GJK_A, 3.0f, ECS_COLOR_PINK);
				//world->debug_drawer->AddPointThread(thread_id, GJK_B, 3.0f, ECS_COLOR_PINK);
				//world->debug_drawer->AddLineThread(thread_id, GJK_A, GJK_B, ECS_COLOR_PURPLE);
			}
		}
	}
}

void SetNarrowphaseTasks(ModuleTaskFunctionData* data) {
	// At the moment, there is nothing to be done
}