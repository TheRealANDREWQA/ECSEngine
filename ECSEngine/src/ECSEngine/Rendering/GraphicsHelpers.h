#pragma once
#include "../Core.h"
#include "ecspch.h"
#include "RenderingStructures.h"
#include "../Internal/Resources/ResourceTypes.h"
#include "../Allocators/AllocatorTypes.h"

namespace ECSEngine {

	struct Graphics;

	struct DecodedTexture {
		Stream<void> data;
		size_t width;
		size_t height;
		DXGI_FORMAT format;
	};

	inline Matrix ProjectionMatrixTextureCube() {
		return MatrixPerspectiveFOV(90.0f, 1.0f, 0.01f, 10.0f);
	}

	inline Matrix ViewMatrixTextureCubeXPositive() {
		return MatrixLookAt(ZeroVector4(), VectorGlobals::RIGHT_4, VectorGlobals::UP_4);
	}

	inline Matrix ViewMatrixTextureCubeXNegative() {
		return MatrixLookAt(ZeroVector4(), -VectorGlobals::RIGHT_4, VectorGlobals::UP_4);
	}

	// For some reason, the matrices for up must be inverted - positive direction takes negative up
	// and negative direction takes positive up
	// These matrices are used in constructing the matrix for texture cube conversion and 
	// diffuse environment convolution
	inline Matrix ViewMatrixTextureCubeYPositive() {
		return MatrixLookAt(ZeroVector4(), -VectorGlobals::UP_4, VectorGlobals::FORWARD_4);
	}

	inline Matrix ViewMatrixTextureCubeYNegative() {
		return MatrixLookAt(ZeroVector4(), VectorGlobals::UP_4, -VectorGlobals::FORWARD_4);
	}

	inline Matrix ViewMatrixTextureCubeZPositive() {
		return MatrixLookAt(ZeroVector4(), VectorGlobals::FORWARD_4, VectorGlobals::UP_4);
	}

	inline Matrix ViewMatrixTextureCubeZNegative() {
		return MatrixLookAt(ZeroVector4(), -VectorGlobals::FORWARD_4, VectorGlobals::UP_4);
	}

	inline Matrix ViewMatrixTextureCube(TextureCubeFace face) {
		using view_function = Matrix(*)();
		view_function functions[6] = {
			ViewMatrixTextureCubeXPositive,
			ViewMatrixTextureCubeXNegative,
			ViewMatrixTextureCubeYPositive,
			ViewMatrixTextureCubeYNegative,
			ViewMatrixTextureCubeZPositive,
			ViewMatrixTextureCubeZNegative
		};

		return functions[face]();
	}

	ECSENGINE_API ID3D11Resource* GetResource(Texture1D texture);

	ECSENGINE_API ID3D11Resource* GetResource(Texture2D texture);

	ECSENGINE_API ID3D11Resource* GetResource(Texture3D texture);

	ECSENGINE_API ID3D11Resource* GetResource(TextureCube texture);
	
	ECSENGINE_API ID3D11Resource* GetResource(ResourceView ps_view);

	ECSENGINE_API ID3D11Resource* GetResource(RenderTargetView target_view);

	ECSENGINE_API ID3D11Resource* GetResource(DepthStencilView depth_view);

	ECSENGINE_API ID3D11Resource* GetResource(UAView view);

	ECSENGINE_API ID3D11Resource* GetResource(VertexBuffer vertex_buffer);

	ECSENGINE_API ID3D11Resource* GetResource(IndexBuffer index_buffer);

	ECSENGINE_API ID3D11Resource* GetResource(ConstantBuffer vc_buffer);

	ECSENGINE_API ID3D11Resource* GetResource(StructuredBuffer buffer);

	ECSENGINE_API ID3D11Resource* GetResource(StandardBuffer buffer);

	ECSENGINE_API ID3D11Resource* GetResource(IndirectBuffer buffer);

	ECSENGINE_API ID3D11Resource* GetResource(UABuffer buffer);

	ECSENGINE_API void CreateCubeVertexBuffer(Graphics* graphics, float positive_span, VertexBuffer& vertex_buffer, IndexBuffer& index_buffer, bool temporary = false);

	ECSENGINE_API VertexBuffer CreateRectangleVertexBuffer(Graphics* graphics, float3 top_left, float3 bottom_right, bool temporary = false);

	ECSENGINE_API uint2 GetTextureDimensions(Texture2D texture);

	// It will make a staging texture that has copied the contents of the supplied texture
	// It relies on the immediate context to copy the contents - SINGLE THREADED
	// If it fails, the returned interface will be nullptr
	template<typename Texture>
	ECSENGINE_API Texture TextureToStaging(Graphics* graphics, Texture texture, bool temporary = true);

	// It will make a buffer that has copied the contents of the supplied buffer
	// It relies on the immediate context to copy the contents - SINGLE THREADED
	// If it fails, the returned interface will be nullptr
	// Available template arguments: StandardBuffer, StructuredBuffer, UABuffer, IndexBuffer
	// VertexBuffer 
	template<typename Buffer>
	ECSENGINE_API Buffer BufferToStaging(Graphics* graphics, Buffer buffer, bool temporary = true);

	// It relies on the immediate context to copy the contents - SINGLE THREADED
	// It will create a staging texture, than copy the contents back on the cpu
	// then create an immutable texture from it - a lot of round trip
	// If it fails, the returned interface will be nullptr
	template<typename Texture>
	ECSENGINE_API Texture TextureToImmutableWithStaging(Graphics* graphics, Texture texture, bool temporary = false);

	// It relies on the immediate context to copy the contents - SINGLE THREADED
	// If it fails, the returned interface will be nullptr
	// The Buffer will not be tracked by graphics
	// Available template arguments: StandardBuffer, StructuredBuffer, UABuffer,
	// ConstantBuffer
	template<typename Buffer>
	ECSENGINE_API Buffer BufferToImmutableWithStaging(Graphics* graphics, Buffer buffer, bool temporary = false);

	// It will make a staging texture that has copied the contents
	// It relies on the immediate context to copy the contents - SINGLE THREADED
	// It will map the current texture using D3D11_MAP_READ
	// If it fails, the returned interface will be nullptr
	// The texture will not be tracked by graphics
	template<typename Texture>
	ECSENGINE_API Texture TextureToImmutableWithMap(Graphics* graphics, Texture texture, bool temporary = false);

	// It relies on the immediate context to copy the contents - SINGLE THREADED
	// It will map the current buffer using D3D11_MAP_READ
	// If it fails, the returned interface will be nullptr
	// The Buffer will not be tracked by graphics
	// Available template arguments: StandardBuffer, StructuredBuffer, UABuffer,
	// ConstantBuffer
	template<typename Buffer>
	ECSENGINE_API Buffer BufferToImmutableWithMap(Graphics* graphics, Buffer buffer, bool temporary = false);

	enum ResizeTextureFlag {
		ECS_RESIZE_TEXTURE_FILTER_POINT = 1 << 0, // Nearest neighbour, weakest and fastest filter method
		ECS_RESIZE_TEXTURE_FILTER_LINEAR = 1 << 1, // Bilinear/Trilinear Interpolation
		ECS_RESIZE_TEXTURE_FILTER_CUBIC = 1 << 2, // Bicubic/Tricubic Interpolation
		ECS_RESIZE_TEXTURE_FILTER_BOX = 1 << 3, // Box filtering
		ECS_RESIZE_TEXTURE_MIP_MAPS = 1 << 4 // Generate mip maps
	};

	// Resizing is done on the CPU; a staging texture is created and then mapped
	// It relies on the immediate context to copy the contents
	// The top is resized and a texture is created either with the first mip or with the whole
	// mip map chain
	// If it fails, it will return nullptr
	ECSENGINE_API Texture2D ResizeTextureWithStaging(
		Graphics* graphics,
		Texture2D texture,
		size_t new_width,
		size_t new_height, 
		size_t resize_flags = ECS_RESIZE_TEXTURE_FILTER_LINEAR,
		bool temporary = false
	);

	// Resizing is done on the CPU; the texture is mapped directly with D3D11_MAP_READ
	// It relies on the immediate context to copy the contents
	// The top is resized and a texture is created either with the first mip or with the whole
	// mip map chain
	// If it fails, it will return nullptr
	ECSENGINE_API Texture2D ResizeTextureWithMap(
		Graphics* graphics,
		Texture2D texture,
		size_t new_width,
		size_t new_height,
		size_t resize_flags = ECS_RESIZE_TEXTURE_FILTER_LINEAR,
		bool temporary = false
	);

	// Resizing is done on the CPU; it will not modify the given data and take parameters from the texture definition
	// The top is resized and a texture is created either with the first mip or with the whole
	// mip map chain
	// If it fails, it will return nullptr
	ECSENGINE_API Texture2D ResizeTexture(
		Graphics* graphics,
		void* texture_data,
		Texture2D texture,
		size_t new_width,
		size_t new_height,
		AllocatorPolymorphic allocator = {nullptr},
		size_t resize_flags = ECS_RESIZE_TEXTURE_FILTER_LINEAR,
		bool temporary = false
	);

	// Resizing is done on the CPU; an allocation will be made for the newly resized data
	// If it fails, it will return { nullptr, 0 }
	ECSENGINE_API Stream<void> ResizeTexture(
		void* texture_data,
		size_t current_width,
		size_t current_height,
		DXGI_FORMAT format,
		size_t new_width,
		size_t new_height,
		AllocatorPolymorphic allocator = { nullptr },
		size_t resize_flags = ECS_RESIZE_TEXTURE_FILTER_LINEAR
	);

	// Returns the section of the texture without the header - it will do internally another allocation for the 
	// decompressed data; Momentarly there are no flags
	// If it fails it returns a { nullptr, 0 } 
	ECSENGINE_API DecodedTexture DecodeTexture(Stream<void> data, TextureExtension extension, AllocatorPolymorphic allocator, size_t flags = 0);

	// Returns the section of the texture without the header - it will do internally another allocation for the 
	// decompressed data; Momentarly there are no flags. The filename is needed in for the texture extension
	// If it fails it returns a { nullptr, 0 } 
	ECSENGINE_API DecodedTexture DecodeTexture(Stream<void> data, const wchar_t* filename, AllocatorPolymorphic allocator, size_t flags = 0);

	// It will invert the mesh on the Z axis
	ECSENGINE_API void InvertMeshZAxis(Graphics* graphics, Mesh& mesh);

	// Convert a texture from DXGI_FORMAT_R8_UNROM to DXGI_FORMAT_R8G8B8A8_UNORM texture
	// Allocator nullptr means use malloc
	ECSENGINE_API Stream<Stream<void>> ConvertSingleChannelTextureToGrayscale(
		Stream<Stream<void>> mip_data,
		size_t width,
		size_t height,
		AllocatorPolymorphic allocator = {nullptr}
	);

	// It will use the immediate context if none specified. If a deffered context is specified, the copy calls
	// will be placed on the command list.
	// It will create a texture cube and then CopySubresourceRegion into it
	// It will have default usage and all mips will be copied
	// All textures must have the same size
	ECSENGINE_API TextureCube ConvertTexturesToCube(
		Graphics* graphics,
		Texture2D x_positive,
		Texture2D x_negative,
		Texture2D y_positive,
		Texture2D y_negative,
		Texture2D z_positive,
		Texture2D z_negative,
		bool temporary = false
	);

	// Expects all the textures to be in the correct order
	// All the constraints that apply to the 6 texture parameter version apply to this one aswell
	ECSENGINE_API TextureCube ConvertTexturesToCube(Graphics* graphics, const Texture2D* textures, bool temporary = false);

	// Equirectangular to cube map
	// Involves setting up multiple renders - can be used only in single thread circumstances
	// It will overwrite most of the pipeline; rebind resources if needed again
	ECSENGINE_API TextureCube ConvertTextureToCube(
		Graphics* graphics,
		ResourceView texture_view,
		DXGI_FORMAT cube_format,
		uint2 face_size,
		bool temporary = false
	);

	// Returns the dimensions of a texture from a file
	// Returns {0, 0} if it fails
	ECSENGINE_API uint2 GetTextureDimensions(const wchar_t* filename);

	// Returns the dimensions of a texture from memory
	// Returns {0, 0} if it fails
	ECSENGINE_API uint2 GetTextureDimensions(Stream<void> data, TextureExtension texture_extension);

	// Returns the dimensions of a texture from memory
	// Returns {0, 0} if it fails
	ECSENGINE_API uint2 GetTextureDimensionsPNG(Stream<void> data);

	// Returns the dimensions of a texture from memory
	// Returns {0, 0} if it fails
	ECSENGINE_API uint2 GetTextureDimensionsJPG(Stream<void> data);

	// Returns the dimensions of a texture from memory
	// Returns {0, 0} if it fails
	ECSENGINE_API uint2 GetTextureDimensionsBMP(Stream<void> data);

	// Returns the dimensions of a texture from memory
	// Returns {0, 0} if it fails
	ECSENGINE_API uint2 GetTextureDimensionsTIFF(Stream<void> data);

	// Returns the dimensions of a texture from memory
	// Returns {0, 0} if it fails
	ECSENGINE_API uint2 GetTextureDimensionsTGA(Stream<void> data);

	// Returns the dimensions of a texture from memory
	// Returns {0, 0} if it fails
	ECSENGINE_API uint2 GetTextureDimensionsHDR(Stream<void> data);



}