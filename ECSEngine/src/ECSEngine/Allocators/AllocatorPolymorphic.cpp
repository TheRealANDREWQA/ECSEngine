#include "ecspch.h"
#include "AllocatorPolymorphic.h"
#include "../../Dependencies/DirectXTex/DirectXTex/DirectXTex.h"

namespace ECSEngine {

    void ECSEngine::SetInternalImageAllocator(DirectX::ScratchImage* image, AllocatorPolymorphic allocator)
    {
        if (allocator.allocator != nullptr) {
            image->SetAllocator(allocator.allocator, GetAllocateFunction(allocator), GetDeallocateMutableFunction(allocator));
        }
    }

}
