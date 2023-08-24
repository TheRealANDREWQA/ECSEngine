#pragma once
#include "../Core.h"
#include "../Rendering/RenderingStructures.h"
#include "../Multithreading/ThreadTask.h"

namespace ECSEngine {

	struct Graphics;

	struct GLTFThumbnailInfo {
		float3 object_rotation;
		float3 object_translation;
		float initial_camera_radius;
		float camera_radius;
	};

	struct GLTFThumbnail {
		GLTFThumbnailInfo info;
		ResourceView texture;
	};

	struct GLTFGenerateThumbnailTaskData {
		Graphics* graphics;
		const Mesh* mesh;
		uint2 texture_size;
		GLTFThumbnail* thumbnail_to_update;
	};

	struct GLTFUpdateThumbnailTaskData {
		Graphics* graphics;
		const Mesh* mesh;
		GLTFThumbnail* thumbnail;
		float radius_delta;
		float3 translation_delta;
		float3 rotation_delta;
	};

	// This can be used as an asyncronous call to generate the thumbnail. It will use the GPU immediate context 
	// As such, cannot be used from multiple threads. But can be placed into a queue for a thread to consume GPU requests
	ECS_THREAD_TASK(GLTFGenerateThumbnailTask);

	// This can be used as an asyncronous call to generate the thumbnail. It will use the GPU immediate context 
	// As such, cannot be used from multiple threads. But can be placed into a queue for a thread to consume GPU requests
	ECS_THREAD_TASK(GLTFUpdateThumbnailTask);

	// It will read the mesh' position buffer onto the CPU in order to determine the bounds of the mesh and then will create a
	// camera according to the bounds. The texture will be tracked by graphics
	ECSENGINE_API GLTFThumbnail GLTFGenerateThumbnail(
		Graphics* graphics,
		uint2 texture_size, 
		const Mesh* mesh
	);

	// It will update the thumbnail with the given deltas
	ECSENGINE_API void GLTFUpdateThumbnail(
		Graphics* graphics, 
		const Mesh* mesh,
		GLTFThumbnail& thumbnail, 
		float radius_delta, 
		float3 translation_delta,
		float3 rotation_delta
	);

}