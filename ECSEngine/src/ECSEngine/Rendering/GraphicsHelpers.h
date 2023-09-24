#pragma once
#include "../Core.h"
#include "ecspch.h"
#include "RenderingStructures.h"
#include "../Allocators/AllocatorTypes.h"
#include "../Math/Matrix.h"
#include "../Math/AABB.h"

namespace ECSEngine {

	struct Graphics;

	ECS_INLINE Matrix ProjectionMatrixTextureCube() {
		return MatrixPerspectiveFOV(90.0f, 1.0f, 0.01f, 10.0f);
	}

	ECS_INLINE Matrix ViewMatrixTextureCubeXPositive() {
		return MatrixLookAtLow(ZeroVector(), RightVector(), UpVector());
	}

	ECS_INLINE Matrix ViewMatrixTextureCubeXNegative() {
		return MatrixLookAtLow(ZeroVector(), -RightVector(), UpVector());
	}

	// For some reason, the matrices for up must be inverted - positive direction takes negative up
	// and negative direction takes positive up
	// These matrices are used in constructing the matrix for texture cube conversion and 
	// diffuse environment convolution
	ECS_INLINE Matrix ViewMatrixTextureCubeYPositive() {
		return MatrixLookAtLow(ZeroVector(), -UpVector(), ForwardVector());
	}

	ECS_INLINE Matrix ViewMatrixTextureCubeYNegative() {
		return MatrixLookAtLow(ZeroVector(), UpVector(), -ForwardVector());
	}

	ECS_INLINE Matrix ViewMatrixTextureCubeZPositive() {
		return MatrixLookAtLow(ZeroVector(), ForwardVector(), UpVector());
	}

	ECS_INLINE Matrix ViewMatrixTextureCubeZNegative() {
		return MatrixLookAtLow(ZeroVector(), -ForwardVector(), UpVector());
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

	ECSENGINE_API AABBStorage GetMeshBoundingBox(Stream<float3> positions);

	// If the vertex buffer is on the GPU, it will copy it to the CPU and then perform the bounding box deduction
	// It needs the immediate context to perform the copy and the mapping
	ECSENGINE_API AABBStorage GetMeshBoundingBox(Graphics* graphics, VertexBuffer positions_buffer);

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

}