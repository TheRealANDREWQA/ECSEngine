#pragma once
#include "../Core.h"
#include "ecspch.h"
#include "../Containers/Stream.h"
#include "../Math/Matrix.h"

#define ECS_GRAPHICS_RESOURCES(function) function(VertexBuffer); \
function(IndexBuffer); \
function(ConstantBuffer); \
function(Texture1D); \
function(Texture2D); \
function(Texture3D);

#define ECS_MATERIAL_VERTEX_CONSTANT_BUFFER_COUNT 4
#define ECS_MATERIAL_PIXEL_CONSTANT_BUFFER_COUNT 4
#define ECS_MATERIAL_HULL_CONSTANT_BUFFER_COUNT 1
#define ECS_MATERIAL_DOMAIN_CONSTANT_BUFFER_COUNT 1
#define ECS_MATERIAL_GEOMETRY_CONSTANT_BUFFER_COUNT 1
#define ECS_MATERIAL_VERTEX_TEXTURES_COUNT 2
#define ECS_MATERIAL_PIXEL_TEXTURES_COUNT 8
#define ECS_MATERIAL_DOMAIN_TEXTURES_COUNT 1
#define ECS_MATERIAL_HULL_TEXTURES_COUNT 1
#define ECS_MATERIAL_GEOMETRY_TEXTURES_COUNT 1
#define ECS_MATERIAL_UAVIEW_COUNT 4

namespace ECSEngine {

	enum ECS_MESH_INDEX {
		ECS_MESH_POSITION,
		ECS_MESH_NORMAL,
		ECS_MESH_UV,
		ECS_MESH_COLOR,
		ECS_MESH_TANGENT,
		ECS_MESH_BONE_WEIGHT,
		ECS_MESH_BONE_INFLUENCE,
		ECS_MESH_BUFFER_COUNT
	};
	
	struct ECSENGINE_API ColorFloat;

	using GraphicsContext = ID3D11DeviceContext;
	using GraphicsDevice = ID3D11Device;

	struct ECSENGINE_API Color {
		Color();
		Color(unsigned char red);
		Color(unsigned char red, unsigned char green);
		Color(unsigned char red, unsigned char green, unsigned char blue);
		Color(unsigned char red, unsigned char green, unsigned char blue, unsigned char alpha);
		// normalized values
		Color(float red, float green, float blue, float alpha);
		Color(ColorFloat color);

		Color(const Color& other) = default;
		Color& operator = (const Color& other) = default;

		bool operator == (const Color& other) const;
		bool operator != (const Color& other) const;

		// these operators do not apply for alpha
		Color operator + (const Color& other) const;
		Color operator * (const Color& other) const;
		Color operator * (float percentage) const;

		static constexpr float GetRange() {
			return (float)255;
		}
		void Normalize(float* values) const;

		union {
			struct {
				unsigned char red;
				unsigned char green;
				unsigned char blue;
			};
			struct {
				unsigned char hue;
				unsigned char saturation;
				unsigned char value;
			};
		};
		unsigned char alpha;
	};

	struct ECSENGINE_API ColorRGB {
		ColorRGB();
		ColorRGB(unsigned char red);
		ColorRGB(unsigned char red, unsigned char green);
		ColorRGB(unsigned char red, unsigned char green, unsigned char blue);

		ColorRGB(const ColorRGB& other) = default;
		ColorRGB& operator = (const ColorRGB& other) = default;

		static constexpr float GetRange() {
			return (float)255;
		}
		void Normalize(float* values) const;

		union {
			struct {
				unsigned char red;
				unsigned char green;
				unsigned char blue;
			};
			struct {
				unsigned char hue;
				unsigned char saturation;
				unsigned char value;
			};
		};
	};

	struct ECSENGINE_API ColorFloat {
		ColorFloat();
		ColorFloat(float red);
		ColorFloat(float red, float green);
		ColorFloat(float red, float green, float blue);
		ColorFloat(float red, float green, float blue, float alpha);
		ColorFloat(Color color);

		ColorFloat(const ColorFloat& other) = default;
		ColorFloat& operator = (const ColorFloat& other) = default;

		ColorFloat operator * (const ColorFloat& other) const;
		ColorFloat operator * (float percentage) const;
		ColorFloat operator + (const ColorFloat& other) const;
		ColorFloat operator - (const ColorFloat& other) const;

		static constexpr float GetRange() {
			return 1.0f;
		}
		void Normalize(float* values) const;

		union {
			struct {
				float red;
				float green;
				float blue;
			};
			struct {
				float hue;
				float saturation;
				float value;
			};
		};
		float alpha;
	};

	struct ECSENGINE_API ColorFloatRGB {
		ColorFloatRGB();
		ColorFloatRGB(float red);
		ColorFloatRGB(float red, float green);
		ColorFloatRGB(float red, float green, float blue);

		ColorFloatRGB(const ColorFloatRGB& other) = default;
		ColorFloatRGB& operator = (const ColorFloatRGB& other) = default;

		static constexpr float GetRange() {
			return 1.0f;
		}
		void Normalize(float* values) const;

		union {
			struct {
				float red;
				float green;
				float blue;
			};
			struct {
				float hue;
				float saturation;
				float value;
			};
		};
	};
		
#define VERTEX_BUFFER 0
	struct ECSENGINE_API VertexBuffer {
		VertexBuffer();
		VertexBuffer(UINT _stride, UINT _size, ID3D11Buffer* _buffer);

		VertexBuffer(const VertexBuffer& other) = default;
		VertexBuffer& operator = (const VertexBuffer& other) = default;

		inline GraphicsDevice* GetDevice() {
			GraphicsDevice* device;
			buffer->GetDevice(&device);
			return device;
		}

		inline ID3D11Buffer* Resource() {
			return buffer;
		}

		//void Release();

		UINT stride;
		UINT size;
		ID3D11Buffer* buffer;
	};

#define INDEX_BUFFER 1
	struct ECSENGINE_API IndexBuffer {
		IndexBuffer();

		IndexBuffer(const IndexBuffer& other) = default;
		IndexBuffer& operator = (const IndexBuffer& other) = default;

		inline GraphicsDevice* GetDevice() {
			GraphicsDevice* device;
			buffer->GetDevice(&device);
			return device;
		}

		inline ID3D11Buffer* Resource() {
			return buffer;
		}
		//void Release();

		unsigned int count;
		unsigned int int_size;
		ID3D11Buffer* buffer;
	};

#define VERTEX_SHADER 2
	// The byte code is mostly needed to check the input layout
	// against the shader's signature; Path is stable and null terminated
	// The byte code can be released after the input layout has been created
	struct ECSENGINE_API VertexShader {
		VertexShader();

		VertexShader(const VertexShader& other) = default;
		VertexShader& operator = (const VertexShader& other) = default;

		inline GraphicsDevice* GetDevice() {
			GraphicsDevice* device;
			shader->GetDevice(&device);
			return device;
		}
		void ReleaseByteCode() {
			byte_code->Release();
		}
		//void Release();

		containers::Stream<wchar_t> path;
		ID3DBlob* byte_code;
		ID3D11VertexShader* shader;
	};

#define INPUT_LAYOUT 3
	struct ECSENGINE_API InputLayout {
		InputLayout();

		InputLayout(const InputLayout& other) = default;
		InputLayout& operator = (const InputLayout& other) = default;

		inline GraphicsDevice* GetDevice() {
			GraphicsDevice* device;
			layout->GetDevice(&device);
			return device;
		}
		//void Release();

		ID3D11InputLayout* layout;
	};

#define PIXEL_SHADER 4
	// Path is stable and null terminated
	struct ECSENGINE_API PixelShader {
		PixelShader();
		PixelShader(ID3D11PixelShader* _shader) : shader(_shader) {}

		PixelShader(const PixelShader& other) = default;
		PixelShader& operator = (const PixelShader& other) = default;

		inline GraphicsDevice* GetDevice() {
			GraphicsDevice* device;
			shader->GetDevice(&device);
			return device;
		}
		//void Release();

		containers::Stream<wchar_t> path;
		ID3D11PixelShader* shader;
	};

	struct ECSENGINE_API GeometryShader {
		GeometryShader() : path(nullptr, 0), shader(nullptr) {}
		GeometryShader(ID3D11GeometryShader* _shader) : shader(_shader) {}

		GeometryShader(const GeometryShader& other) = default;
		GeometryShader& operator = (const GeometryShader& other) = default;

		inline GraphicsDevice* GetDevice() {
			GraphicsDevice* device;
			shader->GetDevice(&device);
			return device;
		}

		containers::Stream<wchar_t> path;
		ID3D11GeometryShader* shader;
	};

	struct ECSENGINE_API DomainShader {
		DomainShader() : path(nullptr, 0), shader(nullptr) {}
		DomainShader(ID3D11DomainShader* _shader) : shader(_shader) {}

		DomainShader(const DomainShader& other) = default;
		DomainShader& operator = (const DomainShader& other) = default;

		inline GraphicsDevice* GetDevice() {
			GraphicsDevice* device;
			shader->GetDevice(&device);
			return device;
		}

		containers::Stream<wchar_t> path;
		ID3D11DomainShader* shader;
	};

	struct ECSENGINE_API HullShader {
		HullShader() : shader(nullptr) {}
		HullShader(ID3D11HullShader* _shader) : shader(_shader) {}

		HullShader(const HullShader& other) = default;
		HullShader& operator = (const HullShader& other) = default;
		
		inline GraphicsDevice* GetDevice() {
			GraphicsDevice* device;
			shader->GetDevice(&device);
			return device;
		}

		containers::Stream<wchar_t> path;
		ID3D11HullShader* shader;
	};

	// Path is stable and null terminated
	struct ECSENGINE_API ComputeShader {
		ComputeShader() : path(nullptr, 0), shader(nullptr) {}

		ComputeShader(const ComputeShader& other) = default;
		ComputeShader& operator = (const ComputeShader& other) = default;

		inline GraphicsDevice* GetDevice() {
			GraphicsDevice* device;
			shader->GetDevice(&device);
			return device;
		}

		containers::Stream<wchar_t> path;
		ID3D11ComputeShader* shader;
	};

#define PRIMITIVE_TOPOLOGY 5
	struct ECSENGINE_API Topology {
		Topology();
		Topology(D3D11_PRIMITIVE_TOPOLOGY _topology);

		Topology(const Topology& other) = default;
		Topology& operator = (const Topology& other) = default;

		D3D11_PRIMITIVE_TOPOLOGY value;
	};

#define VERTEX_CONSTANT_BUFFER 6
	struct ECSENGINE_API ConstantBuffer {
		ConstantBuffer();
		ConstantBuffer(ID3D11Buffer* buffer);

		ConstantBuffer(const ConstantBuffer& other) = default;
		ConstantBuffer& operator = (const ConstantBuffer& other) = default;

		inline GraphicsDevice* GetDevice() {
			GraphicsDevice* device;
			buffer->GetDevice(&device);
			return device;
		}

		inline ID3D11Buffer* Resource() {
			return buffer;
		}
		//void Release();

		ID3D11Buffer* buffer;
	};

#define PS_RESOURCE_VIEW 8
	struct ECSENGINE_API ResourceView {
		ResourceView();
		ResourceView(ID3D11ShaderResourceView* _view);
		//PSResourceView(Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> _view);

		ResourceView(const ResourceView& other) = default;
		ResourceView& operator = (const ResourceView& other) = default;

		inline GraphicsDevice* GetDevice() {
			GraphicsDevice* device;
			view->GetDevice(&device);
			return device;
		}
		//void Release();

		ID3D11ShaderResourceView* view;
	};

#define SAMPLER_STATE 9
	struct ECSENGINE_API SamplerState {
		SamplerState();
		SamplerState(ID3D11SamplerState* _sampler);
		//SamplerState(Microsoft::WRL::ComPtr<ID3D11SamplerState> _sampler);

		SamplerState(const SamplerState& other) = default;
		SamplerState& operator = (const SamplerState& other) = default;

		inline GraphicsDevice* GetDevice() {
			GraphicsDevice* device;
			sampler->GetDevice(&device);
			return device;
		}
		//void Release();

		ID3D11SamplerState* sampler;
	};

	struct ECSENGINE_API Texture1D {
		Texture1D();
		Texture1D(ID3D11Resource* _resource);
		Texture1D(ID3D11Texture1D* _tex);
		//Texture1D(Microsoft::WRL::ComPtr<ID3D11Texture1D> _tex);

		Texture1D(const Texture1D& other) = default;
		Texture1D& operator = (const Texture1D& other) = default;

		inline GraphicsDevice* GetDevice() {
			GraphicsDevice* device;
			tex->GetDevice(&device);
			return device;
		}

		inline ID3D11Texture1D* Resource() {
			return tex;
		}
		//void Release();

		ID3D11Texture1D* tex;
	};

	struct ECSENGINE_API Texture2D {
		Texture2D();
		Texture2D(ID3D11Resource* _resource);
		Texture2D(ID3D11Texture2D* _tex);
		//Texture2D(Microsoft::WRL::ComPtr<ID3D11Texture2D> _tex);

		Texture2D(const Texture2D& other) = default;
		Texture2D& operator = (const Texture2D& other) = default;

		inline GraphicsDevice* GetDevice() {
			GraphicsDevice* device;
			tex->GetDevice(&device);
			return device;
		}

		inline ID3D11Texture2D* Resource() {
			return tex;
		}
		//void Release();

		ID3D11Texture2D* tex;
	};

	struct ECSENGINE_API Texture3D {
		Texture3D();
		Texture3D(ID3D11Resource* _resource);
		Texture3D(ID3D11Texture3D* _tex);
		//Texture3D(Microsoft::WRL::ComPtr<ID3D11Texture3D> _tex);

		Texture3D(const Texture3D& other) = default;
		Texture3D& operator = (const Texture3D& other) = default;

		inline GraphicsDevice* GetDevice() {
			GraphicsDevice* device;
			tex->GetDevice(&device);
			return device;
		}

		inline ID3D11Texture3D* Resource() {
			return tex;
		}
		//void Release();

		ID3D11Texture3D* tex;
	};

	struct ECSENGINE_API RenderTargetView {
		RenderTargetView() : target(nullptr) {}
		RenderTargetView(ID3D11RenderTargetView* _target) : target(_target) {}

		RenderTargetView(const RenderTargetView& other) = default;
		RenderTargetView& operator = (const RenderTargetView& other) = default;

		inline GraphicsDevice* GetDevice() {
			GraphicsDevice* device;
			target->GetDevice(&device);
			return device;
		}

		ID3D11RenderTargetView* target;
	};

	struct ECSENGINE_API DepthStencilView {
		DepthStencilView() : view(nullptr) {}
		DepthStencilView(ID3D11DepthStencilView* _view) : view(_view) {}

		DepthStencilView(const DepthStencilView& other) = default;
		DepthStencilView& operator = (const DepthStencilView& other) = default;

		inline GraphicsDevice* GetDevice() {
			GraphicsDevice* device;
			view->GetDevice(&device);
			return device;
		}

		ID3D11DepthStencilView* view;
	};

	struct ECSENGINE_API StandardBuffer {
		StandardBuffer() : buffer(nullptr) {}
		StandardBuffer(ID3D11Buffer* _buffer) : buffer(_buffer) {}

		StandardBuffer(const StandardBuffer& other) = default;
		StandardBuffer& operator = (const StandardBuffer& other) = default;

		inline GraphicsDevice* GetDevice() {
			GraphicsDevice* device;
			buffer->GetDevice(&device);
			return device;
		}

		ID3D11Buffer* buffer;
	};

	struct ECSENGINE_API StructuredBuffer {
		StructuredBuffer() : buffer(nullptr) {}
		StructuredBuffer(ID3D11Buffer* _buffer) : buffer(_buffer) {}

		StructuredBuffer(const StructuredBuffer& other) = default;
		StructuredBuffer& operator = (const StructuredBuffer& other) = default;

		inline GraphicsDevice* GetDevice() {
			GraphicsDevice* device;
			buffer->GetDevice(&device);
			return device;
		}

		ID3D11Buffer* buffer;
	};

	struct ECSENGINE_API IndirectBuffer {
		IndirectBuffer() : buffer(nullptr) {}
		IndirectBuffer(ID3D11Buffer* _buffer) : buffer(_buffer) {}

		IndirectBuffer(const IndirectBuffer& other) = default;
		IndirectBuffer& operator = (const IndirectBuffer& other) = default;

		inline GraphicsDevice* GetDevice() {
			GraphicsDevice* device;
			buffer->GetDevice(&device);
			return device;
		}

		ID3D11Buffer* buffer;
	};

	struct ECSENGINE_API UABuffer {
		UABuffer() : buffer(nullptr) {}
		UABuffer(ID3D11Buffer* _buffer) : buffer(_buffer) {}

		UABuffer(const UABuffer& other) = default;
		UABuffer& operator = (const UABuffer& other) = default;

		inline GraphicsDevice* GetDevice() {
			GraphicsDevice* device;
			buffer->GetDevice(&device);
			return device;
		}

		size_t element_count;
		ID3D11Buffer* buffer;
	};

	struct ECSENGINE_API AppendStructuredBuffer {
		AppendStructuredBuffer() : buffer(nullptr) {}
		AppendStructuredBuffer(ID3D11Buffer* _buffer) : buffer(_buffer) {}

		AppendStructuredBuffer(const AppendStructuredBuffer& other) = default;
		AppendStructuredBuffer& operator = (const AppendStructuredBuffer& other) = default;

		inline GraphicsDevice* GetDevice() {
			GraphicsDevice* device;
			buffer->GetDevice(&device);
			return device;
		}

		size_t element_count;
		ID3D11Buffer* buffer;
	};

	struct ECSENGINE_API ConsumeStructuredBuffer {
		ConsumeStructuredBuffer() : buffer(nullptr) {}
		ConsumeStructuredBuffer(ID3D11Buffer* _buffer) : buffer(_buffer) {}

		ConsumeStructuredBuffer(const ConsumeStructuredBuffer& other) = default;
		ConsumeStructuredBuffer& operator = (const ConsumeStructuredBuffer& other) = default;

		inline GraphicsDevice* GetDevice() {
			GraphicsDevice* device;
			buffer->GetDevice(&device);
			return device;
		}

		size_t element_count;
		ID3D11Buffer* buffer;
	};

	struct ECSENGINE_API UAView {
		UAView() : view(nullptr) {}
		UAView(ID3D11UnorderedAccessView* _view) : view(_view) {}

		UAView(const UAView& other) = default;
		UAView& operator = (const UAView& other) = default;

		inline GraphicsDevice* GetDevice() {
			GraphicsDevice* device;
			view->GetDevice(&device);
			return device;
		}

		ID3D11UnorderedAccessView* view;
	};

#define TRANSLATION 0
	struct ECSENGINE_API Translation {
		Translation();
		Translation(float _x, float _y, float _z);

		Translation(const Translation& other) = default;
		Translation& operator = (const Translation& other) = default;

		float x;
		float y;
		float z;
	};	

	struct ECSENGINE_API Camera {
		Camera();
		Camera(float3 translation, float3 rotation);
		Camera(Matrix projection, float3 translation, float3 rotation);

		Camera(const Camera& other) = default;
		Camera& operator = (const Camera& other) = default;

		void SetPerspectiveProjection(float width, float height, float near_z, float far_z);

		void SetPerspectiveProjectionFOV(float angle_y, float aspect_ratio, float near_z, float far_z);

		void SetOrthographicProjection(float width, float height, float near_z, float far_z);

		Matrix GetProjectionViewMatrix() const;

		Matrix projection;
		float3 translation;
		float3 rotation;
	};

	struct ECSENGINE_API Mesh {
		Mesh() : mapping_count(0) {}

		Mesh(const Mesh& other) = default;
		Mesh& operator = (const Mesh& other) = default;

		IndexBuffer index_buffer;
		VertexBuffer vertex_buffers[ECS_MESH_BUFFER_COUNT];
		unsigned char mapping[ECS_MESH_BUFFER_COUNT];
		unsigned char mapping_count;
	};

	struct ECSENGINE_API Material {
		Material() : vertex_buffer_mapping_count(0), vc_buffer_count(0), pc_buffer_count(0), dc_buffer_count(0), hc_buffer_count(0), 
		gc_buffer_count(0), vertex_texture_count(0), pixel_texture_count(0), domain_texture_count(0), hull_texture_count(0),
		geometry_texture_count(0), unordered_view_count(0), domain_shader(nullptr), hull_shader(nullptr), geometry_shader(nullptr) {}

		Material(const Material& other) = default;
		Material& operator = (const Material& other) = default;

		InputLayout layout;
		VertexShader vertex_shader;
		PixelShader pixel_shader;
		DomainShader domain_shader;
		HullShader hull_shader;
		GeometryShader geometry_shader;
		ConstantBuffer vc_buffers[ECS_MATERIAL_VERTEX_CONSTANT_BUFFER_COUNT];
		ConstantBuffer pc_buffers[ECS_MATERIAL_PIXEL_CONSTANT_BUFFER_COUNT];
		ConstantBuffer dc_buffers[ECS_MATERIAL_DOMAIN_CONSTANT_BUFFER_COUNT];
		ConstantBuffer hc_buffers[ECS_MATERIAL_HULL_CONSTANT_BUFFER_COUNT];
		ConstantBuffer gc_buffers[ECS_MATERIAL_GEOMETRY_CONSTANT_BUFFER_COUNT];
		ResourceView vertex_textures[ECS_MATERIAL_VERTEX_TEXTURES_COUNT];
		ResourceView pixel_textures[ECS_MATERIAL_PIXEL_TEXTURES_COUNT];
		ResourceView domain_textures[ECS_MATERIAL_DOMAIN_TEXTURES_COUNT];
		ResourceView hull_textures[ECS_MATERIAL_HULL_TEXTURES_COUNT];
		ResourceView geometry_textures[ECS_MATERIAL_GEOMETRY_TEXTURES_COUNT];
		UAView unordered_views[ECS_MATERIAL_UAVIEW_COUNT];
		ECS_MESH_INDEX vertex_buffer_mappings[ECS_MESH_BUFFER_COUNT];
		unsigned char vertex_buffer_mapping_count;
		unsigned char vc_buffer_count;
		unsigned char pc_buffer_count;
		unsigned char dc_buffer_count;
		unsigned char hc_buffer_count;
		unsigned char gc_buffer_count;
		unsigned char vertex_texture_count;
		unsigned char pixel_texture_count;
		unsigned char domain_texture_count;
		unsigned char hull_texture_count;
		unsigned char geometry_texture_count;
		unsigned char unordered_view_count;
	};

}