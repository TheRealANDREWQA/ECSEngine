#include "pch.h"
#include "Narrowphase.h"
#include "Graphics/src/Components.h"
#include "GJK.h"

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
	const RenderMesh* first_mesh = entity_manager->TryGetComponent<RenderMesh>(data->first_identifier);
	if (first_mesh != nullptr && first_mesh->Validate()) {
		const RenderMesh* second_mesh = entity_manager->TryGetComponent<RenderMesh>(data->second_identifier);
		if (second_mesh != nullptr && second_mesh->Validate()) {
			Translation* first_translation;
			Rotation* first_rotation;
			Scale* first_scale;
			GetEntityTransform(entity_manager, data->first_identifier, &first_translation, &first_rotation, &first_scale);

			Translation* second_translation;
			Rotation* second_rotation;
			Scale* second_scale;
			GetEntityTransform(entity_manager, data->second_identifier, &second_translation, &second_rotation, &second_scale);

			float cube_hull_storage[24];
			ConvexHull cube_hull = CreateCubeHull(cube_hull_storage);

			float first_storage[24];
			Matrix first_matrix = GetEntityTransformMatrix(first_translation, first_rotation, first_scale);
			ConvexHull first_hull = cube_hull.Transform(first_matrix, first_storage);

			float second_storage[24];
			Matrix second_matrix = GetEntityTransformMatrix(second_translation, second_rotation, second_scale);
			ConvexHull second_hull = cube_hull.Transform(second_matrix, second_storage);

			float distance = GJK(&first_hull, &second_hull);
			if (distance < 0.0f) {
				// Intersection
				LogInfo("Collision");
			}
		}
	}
}

void SetNarrowphaseTasks(ModuleTaskFunctionData* data) {
	// At the moment, there is nothing to be done
}