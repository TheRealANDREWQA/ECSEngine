#pragma once

#define ECS_UI_OMIT_FIELD_REFLECT

// Can tell the UI the default value for that element
#define ECS_UI_DEFAULT_REFLECT(default_value)
		// Can tell the UI the bounds for that element (ints, floats and doubles)
#define ECS_UI_RANGE_REFLECT(lower_bound, upper_bound)
		// Can tell the UI the default value + the bounds for that element(ints, floats and doubles)
#define ECS_UI_PARAMETERS_REFLECT(default_value, lower_bound, upper_bound)