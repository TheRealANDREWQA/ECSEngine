// ECS_REFLECT
#pragma once
#include "..\..\Core.h"
#include "..\RenderingStructures.h"
#include "..\..\Utilities\Reflection\ReflectionMacros.h"

namespace ECSEngine {

	struct Graphics;

	namespace Shaders {

		struct ECS_REFLECT HemisphericConstants {
			ColorFloat color_down;
			ColorFloat color_up;
		};

		struct ECS_REFLECT DirectionalLight {
			float3 direction;
			ColorFloat color;
		};

		struct ECS_REFLECT PointLight {
			float3 position;
			float range; 
			float attenuation;
			ColorFloat color;
		};

		struct ECS_REFLECT SpotLight {
			float3 position;
			float3 direction;
			float inner_angle_degrees;
			float outer_angle_degrees;
			float range;
			float range_attenuation;
			float cone_attenuation;
			ColorFloat color;
		};

		struct ECS_REFLECT CapsuleLight {
			float3 first_point;
			float3 direction;
			float length;
			float range;
			float range_attenuation;
			ColorFloat color;
		};

		// ----------------------------------------------------------------------------------------------------

		ECSENGINE_API ConstantBuffer CreateHemisphericConstantBuffer(Graphics* graphics);

		// ----------------------------------------------------------------------------------------------------
		
		ECSENGINE_API ConstantBuffer CreateDirectionalLightBuffer(Graphics* graphics);

		// ----------------------------------------------------------------------------------------------------

		ECSENGINE_API ConstantBuffer CreatePointLightBuffer(Graphics* graphics);

		// ----------------------------------------------------------------------------------------------------

		ECSENGINE_API ConstantBuffer CreateSpotLightBuffer(Graphics* graphics);

		// ----------------------------------------------------------------------------------------------------

		ECSENGINE_API ConstantBuffer CreateCapsuleLightBuffer(Graphics* graphics);

		// ----------------------------------------------------------------------------------------------------

		ECSENGINE_API ConstantBuffer CreateCameraPositionBuffer(Graphics* graphics);

		// ----------------------------------------------------------------------------------------------------

		ECSENGINE_API void SetHemisphericConstants(void* data, ColorFloat color_down, ColorFloat color_up);

		ECSENGINE_API void SetHemisphericConstants(ConstantBuffer buffer, Graphics* graphics, ColorFloat color_down, ColorFloat color_up);

		ECSENGINE_API void SetHemisphericConstants(void* data, const HemisphericConstants& values);

		// ----------------------------------------------------------------------------------------------------

		ECSENGINE_API void SetDirectionalLight(void* data, float3 direction, ColorFloat color);

		ECSENGINE_API void SetDirectionalLight(ConstantBuffer buffer, Graphics* graphics, float3 direction, ColorFloat color);

		ECSENGINE_API void SetDirectionalLight(void* data, const DirectionalLight& light);

		// ----------------------------------------------------------------------------------------------------

		ECSENGINE_API void SetPointLight(void* data, float3 position, float range, float attenuation, ColorFloat color);

		ECSENGINE_API void SetPointLight(ConstantBuffer buffer, Graphics* graphics, float3 position, float range, float attenuation, ColorFloat color);

		ECSENGINE_API void SetPointLight(void* data, const PointLight& light);

		// ----------------------------------------------------------------------------------------------------
		
		// Angles expressed as degrees
		ECSENGINE_API void SetSpotLight(
			void* data,
			float3 position,
			float3 direction,
			float inner_angle_degrees,
			float outer_angle_degrees,
			float range,
			float range_attenuation,
			float cone_attenuation,
			ColorFloat color
		);

		// Angles expressed as degrees
		ECSENGINE_API void SetSpotLight(
			ConstantBuffer buffer,
			Graphics* graphics,
			float3 position,
			float3 direction,
			float inner_angle_degrees,
			float outer_angle_degrees,
			float range,
			float range_attenuation,
			float cone_attenuation,
			ColorFloat color
		);

		// Angles expressed as degrees
		ECSENGINE_API void SetSpotLight(void* data, const SpotLight& light);

		// ----------------------------------------------------------------------------------------------------

		ECSENGINE_API void SetCapsuleLight(
			void* data,
			float3 first_position,
			float3 second_position,
			float range,
			float range_attenuation,
			ColorFloat color
		);

		ECSENGINE_API void SetCapsuleLight(
			ConstantBuffer constant_buffer,
			Graphics* graphics,
			float3 first_position,
			float3 second_position,
			float range,
			float range_attenuation,
			ColorFloat color
		);

		// ----------------------------------------------------------------------------------------------------

		ECSENGINE_API void SetCapsuleLight(
			void* data,
			float3 first_point,
			float3 direction,
			float length,
			float range,
			float range_attenuation,
			ColorFloat color
		);

		ECSENGINE_API void SetCapsuleLight(
			ConstantBuffer buffer,
			Graphics* graphics,
			float3 first_point,
			float3 direction,
			float length,
			float range,
			float range_attenuation,
			ColorFloat color
		);

		ECSENGINE_API void SetCapsuleLight(void* data, const CapsuleLight& light);

		// ----------------------------------------------------------------------------------------------------

		ECSENGINE_API void SetCameraPosition(void* data, float3 position);

		ECSENGINE_API void SetCameraPosition(ConstantBuffer buffer, Graphics* graphics, float3 position);

		// ----------------------------------------------------------------------------------------------------

	}
}
