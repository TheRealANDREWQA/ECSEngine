#include "ecspch.h"
#include "Lighting.h"
#include "../Graphics.h"

/*
	CHECK LightingUtilities.hlsli for the exact layout of the HLSL structures that are being mirrored here
*/

#define MAPPING_MACRO(function_name, ...) void* data = graphics->MapBuffer(buffer.buffer); \
function_name(data, __VA_ARGS__); \
graphics->UnmapBuffer(buffer.buffer);

namespace ECSEngine {

	namespace Shaders {

		// ----------------------------------------------------------------------------------------------------

#pragma region Helpers

		// ----------------------------------------------------------------------------------------------------

		void SetFloat4FromColor(float4* location, ColorFloat color) {
			*location = { color.red, color.green, color.blue, 1.0f };
		}

		// ----------------------------------------------------------------------------------------------------

		void NormalizeFloat3(void* location, float3 value) {
			float3* float3_val = (float3*)location;
			*float3_val = Normalize(value);
		}

		void NormalizeFloat3Negate(void* location, float3 value) {
			float3* float3_val = (float3*)location;
			*float3_val = -Normalize(value);
		}

		// ----------------------------------------------------------------------------------------------------

		// ----------------------------------------------------------------------------------------------------

#pragma endregion

		// ----------------------------------------------------------------------------------------------------

		ConstantBuffer CreateHemisphericConstantBuffer(Graphics* graphics, bool temporary) {
			return graphics->CreateConstantBuffer(sizeof(float4) * 2, temporary);
		}

		// ----------------------------------------------------------------------------------------------------

		ConstantBuffer CreateDirectionalLightBuffer(Graphics* graphics, bool temporary) {
			return graphics->CreateConstantBuffer(sizeof(float4) * 2, temporary);
		}

		// ----------------------------------------------------------------------------------------------------

		ConstantBuffer CreatePointLightBuffer(Graphics* graphics, bool temporary) {
			return graphics->CreateConstantBuffer(sizeof(float4) * 2, temporary);
		}

		// ----------------------------------------------------------------------------------------------------

		ConstantBuffer CreateSpotLightBuffer(Graphics* graphics, bool temporary) {
			return graphics->CreateConstantBuffer(sizeof(float4) * 4, temporary);
		}

		// ----------------------------------------------------------------------------------------------------

		ConstantBuffer CreateCapsuleLightBuffer(Graphics* graphics, bool temporary) {
			return graphics->CreateConstantBuffer(sizeof(float4) * 3, temporary);
		}

		// ----------------------------------------------------------------------------------------------------

		ConstantBuffer CreateCameraPositionBuffer(Graphics* graphics, bool temporary) {
			return graphics->CreateConstantBuffer(sizeof(float4) * 1, temporary);
		}

		// ----------------------------------------------------------------------------------------------------

		void SetHemisphericConstants(void* data, ColorFloat color_down, ColorFloat color_up) {
			float4* floats = (float4*)data;
			ColorFloat difference = color_up - color_down;

			SetFloat4FromColor(floats, color_down);
			SetFloat4FromColor(floats + 1, difference);
		}

		void SetHemisphericConstants(ConstantBuffer buffer, Graphics* graphics, ColorFloat color_down, ColorFloat color_up) {
			MAPPING_MACRO(SetHemisphericConstants, color_down, color_up);
		}

		void SetHemisphericConstants(void* data, const HemisphericConstants& values)
		{
			SetHemisphericConstants(data, values.color_down, values.color_up);
		}

		// ----------------------------------------------------------------------------------------------------

		void SetDirectionalLight(void* data, float3 direction, ColorFloat color) {
			float4* floats = (float4*)data;
			NormalizeFloat3Negate(floats, direction);
			SetFloat4FromColor(floats + 1, color);
		}

		void SetDirectionalLight(ConstantBuffer buffer, Graphics* graphics, float3 direction, ColorFloat color) {
			MAPPING_MACRO(SetDirectionalLight, direction, color);
		}

		void SetDirectionalLight(void* data, const DirectionalLight& light)
		{
			SetDirectionalLight(data, light.direction, light.color);
		}

		// ----------------------------------------------------------------------------------------------------

		void SetPointLight(void* data, float3 position, float range, float attenuation, ColorFloat color) {
			float4* floats = (float4*)data;

			// Range must be reciprocated
			floats[0] = { position.x, position.y, position.z, 1.0f / range };

			SetFloat4FromColor(floats + 1, color);
			floats[1].w = attenuation;
		}

		void SetPointLight(ConstantBuffer buffer, Graphics* graphics, float3 direction, float range, float attenuation, ColorFloat color) {
			MAPPING_MACRO(SetPointLight, direction, range, attenuation, color);
		}

		void SetPointLight(void* data, const PointLight& light)
		{
			SetPointLight(data, light.position, light.range, light.attenuation, light.color);
		}

		// ----------------------------------------------------------------------------------------------------

		// Angles expressed as degrees
		void SetSpotLight(
			void* data,
			float3 position,
			float3 direction,
			float inner_angle_degrees,
			float outer_angle_degrees,
			float range,
			float range_attenuation,
			float cone_attenuation,
			ColorFloat color
		) {
			float4* floats = (float4*)data;

			float inner_angle_radians = DegToRad(inner_angle_degrees);
			float outer_angle_radians = DegToRad(outer_angle_degrees);

			float outer_cosine = cos(outer_angle_radians);
			float inner_cosine = cos(inner_angle_radians);

			// The range must be reciprocated
			floats[0] = { position.x, position.y, position.z, 1.0f / range };

			NormalizeFloat3Negate(floats + 1, direction);
			floats[1].w = outer_cosine;

			// The inner to outer difference must be reciprocated
			SetFloat4FromColor(floats + 2, color);
			floats[2].w = 1.0f / (inner_cosine - outer_cosine);

			floats[3].x = cone_attenuation;
			floats[3].y = range_attenuation;
		}

		// Angles expressed as degrees
		void SetSpotLight(
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
		) {
			MAPPING_MACRO(SetSpotLight, position, direction, inner_angle_degrees, outer_angle_degrees, range, range_attenuation, cone_attenuation, color);
		}

		void SetSpotLight(void* data, const SpotLight& light)
		{
			SetSpotLight(data, light.position, light.direction, light.inner_angle_degrees, light.outer_angle_degrees, light.range, light.range_attenuation, light.cone_attenuation, light.color);
		}

		// ----------------------------------------------------------------------------------------------------

		void SetCapsuleLight(
			void* data,
			float3 first_position,
			float3 second_position,
			float range,
			float range_attenuation,
			ColorFloat color
		) {
			// Direction must be negated, so substract from first second
			float3 direction = first_position - second_position;
			SetCapsuleLight(data, first_position, direction, Length(direction), range, range_attenuation, color);
		}

		void SetCapsuleLight(
			ConstantBuffer buffer,
			Graphics* graphics,
			float3 first_position,
			float3 second_position,
			float range,
			float range_attenuation,
			ColorFloat color
		) {
			MAPPING_MACRO(SetCapsuleLight, first_position, second_position, range, range_attenuation, color);
		}

		// ----------------------------------------------------------------------------------------------------

		void SetCapsuleLight(
			void* data,
			float3 first_point,
			float3 direction,
			float length,
			float range,
			float range_attenuation,
			ColorFloat color
		) {
			float4* floats = (float4*)data;

			// The range must be reciprocated
			floats[0] = { first_point.x, first_point.y, first_point.z, 1.0f / range };

			NormalizeFloat3(floats + 1, direction);
			floats[1].w = length;

			SetFloat4FromColor(floats + 2, color);
			floats[2].w = range_attenuation;
		}

		void SetCapsuleLight(
			ConstantBuffer buffer,
			Graphics* graphics,
			float3 first_point,
			float3 direction,
			float length,
			float range,
			float range_attenuation,
			ColorFloat color
		) {
			MAPPING_MACRO(SetCapsuleLight, first_point, direction, length, range, range_attenuation, color);
		}

		void SetCapsuleLight(void* data, const CapsuleLight& light)
		{
			SetCapsuleLight(data, light.first_point, light.direction, light.length, light.range, light.range_attenuation, light.color);
		}

		// ----------------------------------------------------------------------------------------------------

		void SetCameraPosition(void* data, float3 position) {
			float3* floats = (float3*)data;
			*floats = position;
		}

		void SetCameraPosition(ConstantBuffer buffer, Graphics* graphics, float3 position) {
			MAPPING_MACRO(SetCameraPosition, position);
		}

		// ----------------------------------------------------------------------------------------------------

	}

}