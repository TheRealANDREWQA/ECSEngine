#include "ecspch.h"
#include "PBR.h"
#include "../Graphics.h"

/*
	CHECK PBR.hlsl for the exact layout of the HLSL structures that are being mirrored here
*/

namespace ECSEngine {

	namespace Shaders {

		// -------------------------------------------------------------------------------------------------------

		ConstantBuffer CreatePBRVertexConstants(Graphics* graphics, bool temporary)
		{
			return graphics->CreateConstantBuffer(sizeof(PBRVertexConstants), temporary);
		}

		// -------------------------------------------------------------------------------------------------------

		ConstantBuffer CreatePBRPixelConstants(Graphics* graphics, bool temporary)
		{
			// 4 SIMD registers are used
			return graphics->CreateConstantBuffer(sizeof(PBRPixelConstants), temporary);
		}

		// -------------------------------------------------------------------------------------------------------

		ConstantBuffer CreatePBRPixelEnvironmentConstant(Graphics* graphics, bool temporary)
		{
			return graphics->CreateConstantBuffer(sizeof(PBRPixelEnvironmentConstants), temporary);
		}

		// -------------------------------------------------------------------------------------------------------

		// A rotation matrix combined with the projection must be used
		ConstantBuffer CreatePBRSkyboxVertexConstant(Graphics* graphics, bool temporary)
		{
			return graphics->CreateConstantBuffer(sizeof(Matrix), temporary);
		}

		// -------------------------------------------------------------------------------------------------------

		void SetPBRVertexConstants(void* data, Matrix object_matrix, Matrix world_view_projection_matrix)
		{
			PBRVertexConstants* constants = (PBRVertexConstants*)data;
			constants->object_matrix = MatrixGPU(object_matrix);
			constants->world_view_projection_matrix = MatrixGPU(world_view_projection_matrix);
		}

		void SetPBRVertexConstants(
			ConstantBuffer buffer, 
			Graphics* graphics, 
			Matrix object_matrix, 
			Matrix world_view_projection_matrix
		)
		{
			SetPBRVertexConstants(buffer, graphics, { MatrixGPU(object_matrix), MatrixGPU(world_view_projection_matrix) });
		}

		void SetPBRVertexConstants(ConstantBuffer buffer, Graphics* graphics, PBRVertexConstants constants)
		{
			graphics->UpdateBuffer(buffer.buffer, &constants, sizeof(constants));
		}

		// -------------------------------------------------------------------------------------------------------

		void SetPBRPixelConstants(void* data, const PBRPixelConstants& constants)
		{
			memcpy(data, &constants, sizeof(PBRPixelConstants));
		}

		void SetPBRPixelConstants(ConstantBuffer buffer, Graphics* graphics, const PBRPixelConstants& constants)
		{
			graphics->UpdateBuffer(buffer.buffer, &constants, sizeof(PBRPixelConstants));
		}

		// -------------------------------------------------------------------------------------------------------

		void SetPBRPixelEnvironmentConstant(void* data, unsigned int mip_count, float diffuse_factor, float specular_factor)
		{
			float last_mip = (float)mip_count - 1;
			float* ptr = (float*)data;
			*ptr = last_mip;
			ptr[1] = diffuse_factor;
			ptr[2] = specular_factor;
		}

		void SetPBRPixelEnvironmentConstant(ConstantBuffer buffer, Graphics* graphics, PBRPixelEnvironmentConstants constant)
		{
			graphics->UpdateBuffer(buffer.buffer, &constant, sizeof(constant));
		}

		// -------------------------------------------------------------------------------------------------------

		void SetPBRSkyboxVertexConstant(void* data, float3 camera_rotation, Matrix projection_matrix)
		{
			Matrix combined_matrix = MatrixGPU(MatrixRotationCamera(camera_rotation) /** MatrixPerspectiveFOV(90.0f, 1.0f, 0.1f, 1000.0f)*/ * projection_matrix);
			combined_matrix.Store(data);
		}

		void SetPBRSkyboxVertexConstant(ConstantBuffer buffer, Graphics* graphics, float3 camera_rotation, Matrix projection_matrix)
		{
			void* data = graphics->MapBuffer(buffer.buffer);
			SetPBRSkyboxVertexConstant(data, camera_rotation, projection_matrix);
			graphics->UnmapBuffer(buffer.buffer);
		}

		// -------------------------------------------------------------------------------------------------------

		// -------------------------------------------------------------------------------------------------------

		// -------------------------------------------------------------------------------------------------------
	}

}