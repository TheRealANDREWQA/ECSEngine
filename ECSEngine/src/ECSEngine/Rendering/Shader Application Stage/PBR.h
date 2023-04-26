#pragma once
#include "../../Core.h"
#include "../RenderingStructures.h"

namespace ECSEngine {

	struct Graphics;

	namespace Shaders {

		struct PBRVertexConstants {
			Matrix object_matrix;
			Matrix world_view_projection_matrix;
			float2 uv_tiling = { 1.0f, 1.0f };
			float2 uv_offsets = { 0.0f, 0.0f };
		};

		struct PBRPixelConstants {
			ColorFloat tint = ColorFloat(1.0f, 1.0f, 1.0f, 1.0f);
			float metallic_factor = 1.0f;
			float roughness_factor = 1.0f;
			float normal_strength = 1.0f;
			float ambient_occlusion_factor = 1.0f;
			float3 emissive_factor = { 1.0f, 1.0f, 1.0f };
		};

		struct PBRPixelEnvironmentConstants {
			float specular_map_last_mip;
			float diffuse_factor;
			float specular_factor;
		};

		// ---------------------------------------------------------------------------------------------

		ECSENGINE_API ConstantBuffer CreatePBRVertexConstants(Graphics* graphics, bool temporary = false);

		// ---------------------------------------------------------------------------------------------

		ECSENGINE_API ConstantBuffer CreatePBRPixelConstants(Graphics* graphics, bool temporary = false);

		// ---------------------------------------------------------------------------------------------

		ECSENGINE_API ConstantBuffer CreatePBRPixelEnvironmentConstant(Graphics* graphics, bool temporary = false);

		// ---------------------------------------------------------------------------------------------

		ECSENGINE_API ConstantBuffer CreatePBRSkyboxVertexConstant(Graphics* graphics, bool temporary = false);

		// ---------------------------------------------------------------------------------------------

		// The matrices will be transposed inside here (they need to be row major when giving them as parameters)
		ECSENGINE_API void SetPBRVertexConstants(
			void* data,
			Matrix object_matrix, 
			Matrix world_view_projection_matrix, 
			float2 uv_tiling = {1.0f, 1.0f},
			float2 uv_offsets = {0.0f, 0.0f}
		);

		// The matrices will be transposed inside here (they need to be row major when giving them as parameters)
		ECSENGINE_API void SetPBRVertexConstants(
			ConstantBuffer buffer, 
			Graphics* graphics, 
			Matrix object_matrix, 
			Matrix world_view_projection_matrix, 
			float2 uv_tiling = {1.0f, 1.0f},
			float2 uv_offsets = {0.0f, 0.0f}
		);

		ECSENGINE_API void SetPBRVertexConstants(ConstantBuffer buffer, Graphics* graphics, PBRVertexConstants constants);

		// ---------------------------------------------------------------------------------------------

		ECSENGINE_API void SetPBRPixelConstants(void* data, const PBRPixelConstants& constants);

		ECSENGINE_API void SetPBRPixelConstants(ConstantBuffer buffer, Graphics* graphics, const PBRPixelConstants& constants);

		// ---------------------------------------------------------------------------------------------

		ECSENGINE_API void SetPBRPixelEnvironmentConstant(void* data, unsigned int mip_count, float diffuse_factor = 1.0f, float specular_factor = 1.0f);

		ECSENGINE_API void SetPBRPixelEnvironmentConstant(ConstantBuffer buffer, Graphics* graphics, PBRPixelEnvironmentConstants constant);

		// ---------------------------------------------------------------------------------------------

		ECSENGINE_API void SetPBRSkyboxVertexConstant(void* data, float3 camera_rotation, Matrix camera_projection);

		ECSENGINE_API void SetPBRSkyboxVertexConstant(ConstantBuffer buffer, Graphics* graphics, float3 camera_rotation, Matrix camera_projection);

		// ---------------------------------------------------------------------------------------------

	}

}