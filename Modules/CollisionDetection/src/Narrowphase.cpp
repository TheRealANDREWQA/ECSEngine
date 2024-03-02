#include "pch.h"
#include "Narrowphase.h"
#include "Graphics/src/Components.h"
#include "GJK.h"
#include "SAT.h"
#include "Components.h"

ECS_THREAD_TASK(NarrowphaseGridHandler) {
	FixedGridHandlerData* data = (FixedGridHandlerData*)_data;
	EntityManager* entity_manager = world->entity_manager;
	// Retrieve the meshes and check the collisions
	//const RenderMesh* first_mesh = entity_manager->TryGetComponent<RenderMesh>(data->first_identifier);
	const ConvexCollider* first_collider = entity_manager->TryGetComponent<ConvexCollider>(data->first_identifier);
	if (first_collider != nullptr && first_collider->hull.vertex_size > 0) {
		//const RenderMesh* second_mesh = entity_manager->TryGetComponent<RenderMesh>(data->second_identifier);
		const ConvexCollider* second_collider = entity_manager->TryGetComponent<ConvexCollider>(data->second_identifier);
		if (second_collider != nullptr && second_collider->hull.vertex_size > 0) {
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
			ConvexHull first_collider_transformed = first_collider->hull.TransformToTemporary(first_matrix, &stack_allocator);

			Matrix second_matrix = GetEntityTransformMatrix(second_translation, second_rotation, second_scale);
			ConvexHull second_collider_transformed = second_collider->hull.TransformToTemporary(second_matrix, &stack_allocator);

			float distance = GJK(&first_collider_transformed, &second_collider_transformed);
			if (distance < 0.0f) {
				// Intersection
				//LogInfo("Collision");
				// Call the SAT to determine the contacting features
				//Timer timer;
				SATQuery query = SAT(&first_collider_transformed, &second_collider_transformed);
				//float duration = timer.GetDurationFloat(ECS_TIMER_DURATION_MS);
				//ECS_FORMAT_TEMP_STRING(message, "{#}\n", duration);
				//OutputDebugStringA(message.buffer);
				if (query.type == SAT_QUERY_NONE) {
					world->debug_drawer->AddStringThread(thread_id, first_translation != nullptr ? first_translation->value : float3::Splat(0.0f), 
						float3::Splat(1.0f), 1.0f, "None", ECS_COLOR_ORANGE);
					Line3D first_line = first_collider_transformed.GetEdgePoints(17);
					Line3D second_line = second_collider_transformed.GetEdgePoints(1);
					world->debug_drawer->AddLineThread(thread_id, first_line.A, first_line.B, ECS_COLOR_ORANGE);
					world->debug_drawer->AddLineThread(thread_id, second_line.A, second_line.B, ECS_COLOR_ORANGE);
				}
				else if (query.type == SAT_QUERY_EDGE) {
					world->debug_drawer->AddStringThread(thread_id, first_translation != nullptr ? first_translation->value : float3::Splat(0.0f),
						float3::Splat(1.0f), 1.0f, "Edge", ECS_COLOR_ORANGE);
					Line3D first_line = first_collider_transformed.GetEdgePoints(query.edge.edge_1_index);
					Line3D second_line = second_collider_transformed.GetEdgePoints(query.edge.edge_2_index);
					world->debug_drawer->AddLineThread(thread_id, first_line.A, first_line.B, ECS_COLOR_ORANGE);
					world->debug_drawer->AddLineThread(thread_id, second_line.A, second_line.B, ECS_COLOR_ORANGE);

					const ConvexHullEdge& first_edge = first_collider_transformed.edges[query.edge.edge_1_index];
					const ConvexHullEdge& second_edge = second_collider_transformed.edges[query.edge.edge_2_index];
					float3 first_normal_1 = first_collider_transformed.faces[first_edge.face_1_index].plane.normal;
					float3 first_normal_2 = first_collider_transformed.faces[first_edge.face_2_index].plane.normal;
					float3 second_normal_1 = second_collider_transformed.faces[second_edge.face_1_index].plane.normal;
					float3 second_normal_2 = second_collider_transformed.faces[second_edge.face_2_index].plane.normal;
					world->debug_drawer->AddLineThread(thread_id, float3::Splat(0.0f), first_normal_1, ECS_COLOR_LIME);
					world->debug_drawer->AddLineThread(thread_id, float3::Splat(0.0f), first_normal_2, ECS_COLOR_LIME);
					world->debug_drawer->AddLineThread(thread_id, float3::Splat(0.0f), second_normal_1, ECS_COLOR_RED);
					world->debug_drawer->AddLineThread(thread_id, float3::Splat(0.0f), second_normal_2, ECS_COLOR_RED);
					world->debug_drawer->AddLineThread(thread_id, first_normal_1, first_normal_2, ECS_COLOR_AQUA);
					world->debug_drawer->AddLineThread(thread_id, second_normal_1, second_normal_2, ECS_COLOR_WHITE);
				}
				else if (query.type == SAT_QUERY_FACE) {
					world->debug_drawer->AddStringThread(thread_id, first_translation != nullptr ? first_translation->value : float3::Splat(0.0f),
						float3::Splat(1.0f), 1.0f, "Face", ECS_COLOR_ORANGE);
					const ConvexHull* convex_hull = query.face.first_collider ? &first_collider_transformed : &second_collider_transformed;
					const ConvexHullFace& face_1 = convex_hull->faces[query.face.face_index];
					for (unsigned int index = 0; index < face_1.points.size; index++) {
						unsigned int next_index = index == face_1.points.size - 1 ? 0 : index + 1;
						float3 A = convex_hull->GetPoint(face_1.points[index]);
						float3 B = convex_hull->GetPoint(face_1.points[next_index]);
						world->debug_drawer->AddLineThread(thread_id, A, B, ECS_COLOR_ORANGE);
					}
				}
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