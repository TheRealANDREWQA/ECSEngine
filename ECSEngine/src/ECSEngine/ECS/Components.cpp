#include "ecspch.h"
#include "Components.h"
#include "../Tools/Modules/ModuleDefinition.h"

namespace ECSEngine {

	void ConvertLinkToRotation(ModuleLinkComponentFunctionData* data) {
		const RotationLink* rotation_link = (const RotationLink*)data->link_component;
		Rotation* rotation = (Rotation*)data->component;

		rotation->value = QuaternionFromEuler(rotation_link->value).StorageLow();
	}

	void ConvertRotationToLink(ModuleLinkComponentReverseFunctionData* data) {
		const Rotation* rotation = (const Rotation*)data->component;
		RotationLink* rotation_link = (RotationLink*)data->link_component;

		rotation_link->value = QuaternionToEulerLow(rotation->value);
	}

	void RegisterECSLinkComponents(ModuleRegisterLinkComponentFunctionData* register_data)
	{
		ModuleLinkComponentTarget target;
		target.build_function = ConvertLinkToRotation;
		target.reverse_function = ConvertRotationToLink;
		target.component_name = STRING(RotationLink);
		register_data->functions->Add(target);
	}

}
