#pragma once
#include "../Core.h"
#include "RenderingStructures.h"
#include "../Allocators/AllocatorTypes.h"
#include "../Math/Matrix.h"
#include "../Math/AABB.h"

namespace ECSEngine {

	struct Graphics;
	struct SpinLock;

	ECS_INLINE Matrix ProjectionMatrixTextureCube() {
		return MatrixPerspectiveFOV(90.0f, 1.0f, 0.01f, 10.0f);
	}

	ECS_INLINE Matrix ViewMatrixTextureCubeXPositive() {
		return MatrixLookAt(float3::Splat(0.0f), GetRightVector(), GetUpVector());
	}

	ECS_INLINE Matrix ViewMatrixTextureCubeXNegative() {
		return MatrixLookAt(float3::Splat(0.0f), -GetRightVector(), GetUpVector());
	}

	// For some reason, the matrices for up must be inverted - positive direction takes negative up
	// and negative direction takes positive up
	// These matrices are used in constructing the matrix for texture cube conversion and 
	// diffuse environment convolution
	ECS_INLINE Matrix ViewMatrixTextureCubeYPositive() {
		return MatrixLookAt(float3::Splat(0.0f), -GetUpVector(), GetForwardVector());
	}

	ECS_INLINE Matrix ViewMatrixTextureCubeYNegative() {
		return MatrixLookAt(float3::Splat(0.0f), GetUpVector(), -GetForwardVector());
	}

	ECS_INLINE Matrix ViewMatrixTextureCubeZPositive() {
		return MatrixLookAt(float3::Splat(0.0f), GetForwardVector(), GetUpVector());
	}

	ECS_INLINE Matrix ViewMatrixTextureCubeZNegative() {
		return MatrixLookAt(float3::Splat(0.0f), -GetForwardVector(), GetUpVector());
	}

	inline Matrix ViewMatrixTextureCube(TextureCubeFace face) {
		typedef Matrix(*view_function)();
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

	// Returns a viewport that spans the given texture (from (0, 0) to (width, height))
	ECSENGINE_API GraphicsViewport GetGraphicsViewportForTexture(Texture2D texture, float min_depth = 0.0f, float max_depth = 1.0f);

	ECSENGINE_API void CreateCubeVertexBuffer(Graphics* graphics, float positive_span, VertexBuffer& vertex_buffer, IndexBuffer& index_buffer, bool temporary = false);

	ECSENGINE_API VertexBuffer CreateRectangleVertexBuffer(Graphics* graphics, float3 top_left, float3 bottom_right, bool temporary = false);

	ECSENGINE_API AABBScalar GetMeshBoundingBox(Stream<float3> positions);

	// If the vertex buffer is on the GPU, it will copy it to the CPU and then perform the bounding box deduction
	// It needs the immediate context to perform the copy and the mapping
	ECSENGINE_API AABBScalar GetMeshBoundingBox(Graphics* graphics, VertexBuffer positions_buffer);

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
	// It doesn't support TextureCube at the moment
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
	// It doesn't support TextureCube at the moment
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

	// It will invert the mesh on the Z axis
	ECSENGINE_API void InvertMeshZAxis(Graphics* graphics, Mesh& mesh);

	// Returns a size such that the object remains relatively the same
	// when the camera is moving (it is only a rough approximate)
	ECSENGINE_API float GetConstantObjectSizeInPerspective(float camera_fov, float distance_to_camera, float object_size);

	// The pipeline must be set previously
	ECSENGINE_API void DrawWholeViewportQuad(Graphics* graphics, GraphicsContext* context);

	// Creates a constant buffer that can be used as a colorization array
	// Where values can be indexed into it in order to have better visualization
	// The theoretical maximum count can be ECS_KB * 4 (because the values are stored as ColorFloat)
	// But for now 256 is the limit - for aesthetics reasons mostly otherwise colors would start to appear again
	ECSENGINE_API ConstantBuffer CreateColorizeConstantBuffer(Graphics* graphics, unsigned int count, bool temporary = true);
	
	// Returns the data size that would be needed to copy this buffer from the GPU to the CPU
	template<typename Buffer>
	ECSENGINE_API size_t GetBufferCPUDataSize(Buffer buffer);

	struct TextureCPUData {
		Stream<void> bytes;
		size_t width;
		size_t height;
		size_t depth;
		size_t row_pitch;
		size_t depth_pitch;
	};

	// Returns the data size that would be needed to copy this texture from the GPU to the CPU
	// The bytes buffer is nullptr
	template<typename Texture>
	ECSENGINE_API TextureCPUData GetTextureCPUDataSize(Texture texture, unsigned int mip_level);

	// Retrieves the data from the GPU to the CPU. This requires access to the immediate context (it will copy from a staging)
	// You can optionally provide a spin lock to be acquired in order to get exclusivity to the immediate context
	// Use As<>() to convert to the actual stored type
	template<typename Buffer>
	ECSENGINE_API Stream<void> GetGPUBufferDataToCPU(Graphics* graphics, Buffer buffer, AllocatorPolymorphic allocator, SpinLock* lock = nullptr);

	// Retrieves the data from the GPU to the CPU. This requires access to the immediate context (it will copy from a staging)
	// You can optionally provide a spin lock to be acquired in order to get exclusivity to the immediate context
	// Use As<>() to convert to the actual stored type.
	// At the moment, it doesn't support TextureCube
	template<typename Texture>
	ECSENGINE_API TextureCPUData GetTextureDataToCPU(Graphics* graphics, Texture texture, unsigned int mip_level, AllocatorPolymorphic allocator, SpinLock* lock = nullptr);

	// It will transfer the GPU positions to the CPU. Requires access to the immediate context
	// Can provide a spin lock to acquire exclusivity to the immediate context
	ECS_INLINE Stream<float3> GetMeshPositionsCPU(Graphics* graphics, const Mesh& mesh, AllocatorPolymorphic allocator, SpinLock* lock = nullptr) {
		return GetGPUBufferDataToCPU(graphics, mesh.GetBuffer(ECS_MESH_POSITION), allocator, lock).As<float3>();
	}

	// It will transfer the GPU positions to the CPU. Requires access to the immediate context
	// Can provide a spin lock to acquire exclusivity to the immediate context
	ECS_INLINE Stream<float3> GetMeshNormalsCPU(Graphics* graphics, const Mesh& mesh, AllocatorPolymorphic allocator, SpinLock* lock = nullptr) {
		return GetGPUBufferDataToCPU(graphics, mesh.GetBuffer(ECS_MESH_NORMAL), allocator, lock).As<float3>();
	}

}