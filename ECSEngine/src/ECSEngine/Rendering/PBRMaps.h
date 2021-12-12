#pragma once
#include "GraphicsHelpers.h"

namespace ECSEngine {

	// Creates a new cube texture that will contain the diffuse lambertian part of the BRDF
	// for IBL use; dimensions specifies the size of this new generated map
	// Environment must be a resource view of a texture cube
	ECSENGINE_API TextureCube ConvertEnvironmentMapToDiffuseIBL(ResourceView environment, Graphics* graphics, uint2 dimensions, unsigned int sample_count);

	// Creates a new cube texture that will contain the specular irradiance part of the BRDF
	// for IBL use; dimensions specifies the size of the first mip - a whole mip chain is generated for different roughness levels
	// Environment must be a resource view of a textue cube
	ECSENGINE_API TextureCube ConvertEnvironmentMapToSpecularIBL(ResourceView environment, Graphics* graphics, uint2 dimensions, unsigned int sample_count);

	// Create a new texture that will contain the BRDF integration LUT (look up table) for IBL use
	ECSENGINE_API Texture2D CreateBRDFIntegrationLUT(Graphics* graphics, uint2 dimensions, unsigned int sample_count);

}