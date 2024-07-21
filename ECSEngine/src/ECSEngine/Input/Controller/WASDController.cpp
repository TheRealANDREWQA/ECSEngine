#include "ecspch.h"
#include "WASDController.h"
#include "../Keyboard.h"

namespace ECSEngine {

    float3 WASDController(bool w, bool a, bool s, bool d, float movement_factor, float delta_time, float3 forward_direction_normalized, float3 right_direction_normalized)
    {
        float forward_factor = 0.0f;
        float right_factor = 0.0f;

        if (w) {
            forward_factor += 1.0f;
        }
        if (a) {
            right_factor -= 1.0f;
        }
        if (s) {
            forward_factor -= 1.0f;
        }
        if (d) {
            right_factor += 1.0f;
        }
        
        float3 delta = float3::Splat(0.0f);
        if (forward_factor != 0.0f) {
            delta += float3::Splat(forward_factor * (movement_factor * delta_time)) * forward_direction_normalized;
        }
        if (right_factor != 0.0f) {
            delta += float3::Splat(right_factor * (movement_factor * delta_time)) * right_direction_normalized;
        }
        return delta;
    }

    float3 WASDController(const Keyboard* keyboard, float movement_factor, float delta_time, float3 forward_direction_normalized, float3 right_direction_normalized)
    {
        return WASDController(
            keyboard->IsDown(ECS_KEY_W),
            keyboard->IsDown(ECS_KEY_A),
            keyboard->IsDown(ECS_KEY_S),
            keyboard->IsDown(ECS_KEY_D),
            movement_factor,
            delta_time,
            forward_direction_normalized,
            right_direction_normalized
        );
    }

    float3 WASDController(
        const InputMapping* mapping, 
        unsigned int w, 
        unsigned int a, 
        unsigned int s, 
        unsigned int d, 
        float movement_factor,
        float delta_time,
        float3 forward_direction_normalized, 
        float3 right_direction_normalized
    )
    {
        return WASDController(
            mapping->IsTriggered(w),
            mapping->IsTriggered(a),
            mapping->IsTriggered(s),
            mapping->IsTriggered(d),
            movement_factor,
            delta_time,
            forward_direction_normalized,
            right_direction_normalized
        );
    }

}
