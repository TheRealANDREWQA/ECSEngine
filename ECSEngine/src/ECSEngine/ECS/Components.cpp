#include "ecspch.h"
#include "Components.h"
#include "../Tools/Modules/ModuleDefinition.h"
#include "../Tools/Modules/ModuleExtraInformation.h"
#include "../Tools/Debug Draw/DebugDraw.h"

namespace ECSEngine {

	void ConvertLinkToRotation(ModuleLinkComponentFunctionData* data) {
		const RotationLink* rotation_link = (const RotationLink*)data->link_component;
		Rotation* rotation = (Rotation*)data->component;

		//if (data->previous_link_component != nullptr && data->previous_component != nullptr) {
		//	// If we have a previous link component and previous target data,
		//	// We can just apply a delta quaternion
		//	const RotationLink* previous_rotation_link = (const RotationLink*)data->previous_link_component;
		//	const Rotation* previous_rotation = (const Rotation*)data->previous_component;

		//	Quaternion delta_quaternion = QuaternionFromEuler(rotation_link->value - previous_rotation_link->value);
		//	rotation->value = AddLocalRotation(previous_rotation->value, delta_quaternion).StorageLow();
		//}
		//else {
		//	rotation->value = QuaternionFromEuler(rotation_link->value).StorageLow();
		//}
		rotation->value = QuaternionFromEuler(rotation_link->value).StorageLow();
	}

	void ConvertRotationToLink(ModuleLinkComponentReverseFunctionData* data) {
		const Rotation* rotation = (const Rotation*)data->component;
		RotationLink* rotation_link = (RotationLink*)data->link_component;

		rotation_link->value = QuaternionToEulerLow(rotation->value);
	}

	//void ApplyLinkToRotation(ModuleLinkComponentApplyModifierFieldsFunctionData* data) {
	//	const RotationLink* rotation_link = (const RotationLink*)data->link_component;
	//	Rotation* rotation = (Rotation*)data->component;

	//	if (data->previous_component != nullptr && data->previous_link_component != nullptr) {
	//		// Apply a delta quaternion
	//		const RotationLink* previous_rotation_link = (const RotationLink*)data->previous_link_component;
	//		const Rotation* previous_rotation = (const Rotation*)data->previous_component;

	//		Quaternion delta_quaternion = QuaternionFromEuler(rotation_link->add_rotation - previous_rotation_link->add_rotation);
	//		rotation->value = AddLocalRotation(previous_rotation->value, delta_quaternion).StorageLow();
	//	}
	//}

	void ConvertLinkToCameraComponent(ModuleLinkComponentFunctionData* data) {
		const CameraComponentLink* link = (const CameraComponentLink*)data->link_component;
		CameraComponent* component = (CameraComponent*)data->component;

		Camera temp_camera = link->value;
		component->value = &temp_camera;
	}

	void ConvertCameraComponentToLink(ModuleLinkComponentReverseFunctionData* data) {
		const CameraComponent* component = (const CameraComponent*)data->component;
		CameraComponentLink* link = (CameraComponentLink*)data->link_component;

		link->value = component->value.AsParametersFOV();
	}

	void RegisterECSLinkComponents(ModuleRegisterLinkComponentFunctionData* register_data)
	{
		ModuleLinkComponentTarget target;
		target.build_function = ConvertLinkToRotation;
		target.reverse_function = ConvertRotationToLink;
		target.component_name = STRING(RotationLink);
		//target.apply_modifier = ApplyLinkToRotation;
		register_data->functions->AddAssert(target);

		target.build_function = ConvertLinkToCameraComponent;
		target.reverse_function = ConvertCameraComponentToLink;
		target.component_name = STRING(CameraComponentLink);
		register_data->functions->AddAssert(target);
	}

	void RegisterECSModuleExtraInformation(ModuleRegisterExtraInformationFunctionData* register_data)
	{
		GlobalComponentTransformGizmos camera_gizmo;
		camera_gizmo.component = STRING(CameraComponent);
		camera_gizmo.translation_field = STRING(value.translation);
		camera_gizmo.rotation_field = STRING(value.rotation);

		SetGlobalComponentTransformGizmos(register_data, { { &camera_gizmo, 1} });
	}

	void CameraComponentDebugDraw(ModuleDebugDrawComponentFunctionData* draw_data) {
		const CameraComponent* camera = (const CameraComponent*)draw_data->component;
		FrustumPoints camera_frustum = GetCameraFrustumPoints(&camera->value);
		AddDebugFrustumThread(camera_frustum, draw_data->debug_drawer, draw_data->thread_id, ECS_COLOR_LIME);
	}

	void RegisterECSDebugDrawElements(ModuleRegisterDebugDrawFunctionData* register_data) {
		ModuleDebugDrawElement element;
		element.component = CameraComponent::ID();
		element.component_type = ECS_COMPONENT_GLOBAL;
		element.draw_function = CameraComponentDebugDraw;

		register_data->elements->AddAssert(element);
	}

}
