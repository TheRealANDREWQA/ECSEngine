#include "ecspch.h"
#include "WASDController.h"

namespace ECSEngine {

    float3 WASDController(bool w, bool a, bool s, bool d, float movement_factor, float3 forward_direction_normalized, float3 right_direction_normalized)
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
            delta += float3::Splat(forward_factor * movement_factor) * forward_direction_normalized;
        }
        if (right_factor != 0.0f) {
            delta += float3::Splat(right_factor * movement_factor) * right_direction_normalized;
        }
        return delta;
    }

    float3 WASDController(
        const InputMapping* mapping, 
        unsigned int w, 
        unsigned int a, 
        unsigned int s, 
        unsigned int d, 
        float movement_factor, 
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
            forward_direction_normalized,
            right_direction_normalized
        );
    }

}
