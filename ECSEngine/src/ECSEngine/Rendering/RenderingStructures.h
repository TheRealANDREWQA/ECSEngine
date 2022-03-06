#pragma once
#include "../Core.h"
#include "ecspch.h"
#include "../Containers/Stream.h"
#include "../Math/Matrix.h"
#include "../Allocators/AllocatorTypes.h"

#define ECS_GRAPHICS_BUFFERS(function) /* Useful for macro expansion */ function(VertexBuffer); \
function(IndexBuffer); \
function(ConstantBuffer); \
function(StandardBuffer); \
function(StructuredBuffer); \
function(UABuffer); 

#define ECS_GRAPHICS_TEXTURES(function) /* Useful for macro expansion */ function(Texture1D); \
function(Texture2D); \
function(Texture3D); 

#define ECS_GRAPHICS_RESOURCES(function) /* Useful for macro expansion */ ECS_GRAPHICS_TEXTURES(function); \
ECS_GRAPHICS_BUFFERS(function); 

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

		Color(const unsigned char* values);

		Color(const float* values);

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

	struct ECSENGINE_API ColorFloat {
		ColorFloat();

		ColorFloat(float red);

		ColorFloat(float red, float green);

		ColorFloat(float red, float green, float blue);

		ColorFloat(float red, float green, float blue, float alpha);

		ColorFloat(Color color);

		ColorFloat(const float* values);

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

	struct ECSENGINE_API VertexBuffer {
		VertexBuffer();
		VertexBuffer(UINT _stride, UINT _size, ID3D11Buffer* _buffer);

		VertexBuffer(const VertexBuffer& other) = default;
		VertexBuffer& operator = (const VertexBuffer& other) = default;

		inline GraphicsDevice* GetDevice() {
			GraphicsDevice* device;
			buffer->GetDevice(&device);
			device->Release();
			return device;
		}

		inline ID3D11Buffer* Interface() {
			return buffer;
		}

		inline unsigned int Release() {
			return buffer->Release();
		}

		static VertexBuffer RawCreate(GraphicsDevice* device, const D3D11_BUFFER_DESC* descriptor, const D3D11_SUBRESOURCE_DATA* initial_data = nullptr);

		unsigned int stride;
		unsigned int size;
		ID3D11Buffer* buffer;
	};

	struct ECSENGINE_API IndexBuffer {
		IndexBuffer();

		IndexBuffer(const IndexBuffer& other) = default;
		IndexBuffer& operator = (const IndexBuffer& other) = default;

		inline GraphicsDevice* GetDevice() {
			GraphicsDevice* device;
			buffer->GetDevice(&device);
			device->Release();
			return device;
		}

		inline ID3D11Buffer* Interface() {
			return buffer;
		}

		inline unsigned int Release() {
			return buffer->Release();
		}

		static IndexBuffer RawCreate(GraphicsDevice* device, const D3D11_BUFFER_DESC* descriptor, const D3D11_SUBRESOURCE_DATA* initial_data = nullptr);

		unsigned int count;
		unsigned int int_size;
		ID3D11Buffer* buffer;
	};

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
			device->Release();
			return device;
		}

		inline ID3D11VertexShader* Interface() {
			return shader;
		}

		inline unsigned int Release() {
			return shader->Release();
		}

		static VertexShader RawCreate(GraphicsDevice* device, Stream<void> byte_code);

		Stream<void> byte_code;
		ID3D11VertexShader* shader;
	};

	struct ECSENGINE_API InputLayout {
		InputLayout();

		InputLayout(const InputLayout& other) = default;
		InputLayout& operator = (const InputLayout& other) = default;

		inline GraphicsDevice* GetDevice() {
			GraphicsDevice* device;
			layout->GetDevice(&device);
			device->Release();
			return device;
		}

		inline ID3D11InputLayout* Interface() {
			return layout;
		}

		inline unsigned int Release() {
			return layout->Release();
		}

		static InputLayout RawCreate(GraphicsDevice* device, Stream<D3D11_INPUT_ELEMENT_DESC> descriptor, Stream<void> byte_code);

		ID3D11InputLayout* layout;
	};

	// Path is stable and null terminated
	struct ECSENGINE_API PixelShader {
		PixelShader();
		PixelShader(ID3D11PixelShader* _shader) : shader(_shader) {}

		PixelShader(const PixelShader& other) = default;
		PixelShader& operator = (const PixelShader& other) = default;

		inline GraphicsDevice* GetDevice() {
			GraphicsDevice* device;
			shader->GetDevice(&device);
			device->Release();
			return device;
		}

		inline ID3D11PixelShader* Interface() {
			return shader;
		}

		inline unsigned int Release() {
			return shader->Release();
		}

		static PixelShader RawCreate(GraphicsDevice* device, Stream<void> byte_code);

		ID3D11PixelShader* shader;
	};

	struct ECSENGINE_API GeometryShader {
		GeometryShader() : shader(nullptr) {}
		GeometryShader(ID3D11GeometryShader* _shader) : shader(_shader) {}

		GeometryShader(const GeometryShader& other) = default;
		GeometryShader& operator = (const GeometryShader& other) = default;

		inline GraphicsDevice* GetDevice() {
			GraphicsDevice* device;
			shader->GetDevice(&device);
			device->Release();
			return device;
		}

		inline ID3D11GeometryShader* Interface() {
			return shader;
		}

		inline unsigned int Release() {
			return shader->Release();
		}

		static GeometryShader RawCreate(GraphicsDevice* device, Stream<void> byte_code);

		ID3D11GeometryShader* shader;
	};

	struct ECSENGINE_API DomainShader {
		DomainShader() : shader(nullptr) {}
		DomainShader(ID3D11DomainShader* _shader) : shader(_shader) {}

		DomainShader(const DomainShader& other) = default;
		DomainShader& operator = (const DomainShader& other) = default;

		inline GraphicsDevice* GetDevice() {
			GraphicsDevice* device;
			shader->GetDevice(&device);
			device->Release();
			return device;
		}

		inline ID3D11DomainShader* Interface() {
			return shader;
		}

		inline unsigned int Release() {
			return shader->Release();
		}

		static DomainShader RawCreate(GraphicsDevice* device, Stream<void> byte_code);

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
			device->Release();
			return device;
		}
		
		inline ID3D11HullShader* Interface() {
			return shader;
		}

		inline unsigned int Release() {
			return shader->Release();
		}

		static HullShader RawCreate(GraphicsDevice* device, Stream<void> byte_code);

		ID3D11HullShader* shader;
	};

	// Path is stable and null terminated
	struct ECSENGINE_API ComputeShader {
		ComputeShader() : shader(nullptr) {}

		ComputeShader(const ComputeShader& other) = default;
		ComputeShader& operator = (const ComputeShader& other) = default;

		inline GraphicsDevice* GetDevice() {
			GraphicsDevice* device;
			shader->GetDevice(&device);
			device->Release();
			return device;
		}

		inline ID3D11ComputeShader* Interface() {
			return shader;
		}

		inline unsigned int Release() {
			return shader->Release();
		}

		static ComputeShader RawCreate(GraphicsDevice* device, Stream<void> byte_code);

		ID3D11ComputeShader* shader;
	};

	struct ECSENGINE_API Topology {
		Topology();
		Topology(D3D11_PRIMITIVE_TOPOLOGY _topology);

		Topology(const Topology& other) = default;
		Topology& operator = (const Topology& other) = default;

		D3D11_PRIMITIVE_TOPOLOGY value;
	};

	struct ECSENGINE_API ConstantBuffer {
		ConstantBuffer();
		ConstantBuffer(ID3D11Buffer* buffer);

		ConstantBuffer(const ConstantBuffer& other) = default;
		ConstantBuffer& operator = (const ConstantBuffer& other) = default;

		inline GraphicsDevice* GetDevice() {
			GraphicsDevice* device;
			buffer->GetDevice(&device);
			device->Release();
			return device;
		}

		inline ID3D11Buffer* Interface() {
			return buffer;
		}

		inline unsigned int Release() {
			return buffer->Release();
		}

		static ConstantBuffer RawCreate(GraphicsDevice* device, const D3D11_BUFFER_DESC* descriptor, const D3D11_SUBRESOURCE_DATA* initial_data = nullptr);

		ID3D11Buffer* buffer;
	};

	struct ECSENGINE_API ResourceView {
		ResourceView();
		ResourceView(ID3D11ShaderResourceView* _view);

		ResourceView(const ResourceView& other) = default;
		ResourceView& operator = (const ResourceView& other) = default;

		inline GraphicsDevice* GetDevice() {
			GraphicsDevice* device;
			view->GetDevice(&device);
			device->Release();
			return device;
		}

		inline ID3D11ShaderResourceView* Interface() {
			return view;
		}

		inline unsigned int Release() {
			return view->Release();
		}

		static ResourceView RawCreate(GraphicsDevice* device, ID3D11Resource* resource, const D3D11_SHADER_RESOURCE_VIEW_DESC* descriptor);

		ID3D11ShaderResourceView* view;
	};

	struct ECSENGINE_API SamplerState {
		SamplerState();
		SamplerState(ID3D11SamplerState* _sampler);

		SamplerState(const SamplerState& other) = default;
		SamplerState& operator = (const SamplerState& other) = default;

		inline GraphicsDevice* GetDevice() {
			GraphicsDevice* device;
			sampler->GetDevice(&device);
			device->Release();
			return device;
		}

		inline ID3D11SamplerState* Interface() {
			return sampler;
		}

		inline unsigned int Release() {
			return sampler->Release();
		}

		static SamplerState RawCreate(GraphicsDevice* device, const D3D11_SAMPLER_DESC* descriptor);

		ID3D11SamplerState* sampler;
	};

	struct ECSENGINE_API Texture1D {
		using RawDescriptor = D3D11_TEXTURE1D_DESC;
		using RawInterface = ID3D11Texture1D;

		Texture1D();
		Texture1D(ID3D11Resource* _resource);
		Texture1D(ID3D11Texture1D* _tex);

		Texture1D(const Texture1D& other) = default;
		Texture1D& operator = (const Texture1D& other) = default;

		inline GraphicsDevice* GetDevice() {
			GraphicsDevice* device;
			tex->GetDevice(&device);
			device->Release();
			return device;
		}

		inline ID3D11Texture1D* Interface() {
			return tex;
		}

		inline unsigned int Release() {
			return tex->Release();
		}

		static Texture1D RawCreate(GraphicsDevice* device, const RawDescriptor* descriptor, const D3D11_SUBRESOURCE_DATA* initial_data = nullptr);

		ID3D11Texture1D* tex;
	};

	struct ECSENGINE_API Texture2D {
		using RawDescriptor = D3D11_TEXTURE2D_DESC;
		using RawInterface = ID3D11Texture2D;

		Texture2D();
		Texture2D(ID3D11Resource* _resource);
		Texture2D(ID3D11Texture2D* _tex);

		Texture2D(const Texture2D& other) = default;
		Texture2D& operator = (const Texture2D& other) = default;

		inline GraphicsDevice* GetDevice() {
			GraphicsDevice* device;
			tex->GetDevice(&device);
			device->Release();
			return device;
		}

		inline ID3D11Texture2D* Interface() {
			return tex;
		}

		inline unsigned int Release() {
			return tex->Release();
		}

		static Texture2D RawCreate(GraphicsDevice* device, const RawDescriptor* descriptor, const D3D11_SUBRESOURCE_DATA* initial_data = nullptr);

		ID3D11Texture2D* tex;
	};

	struct ECSENGINE_API Texture3D {
		using RawDescriptor = D3D11_TEXTURE3D_DESC;
		using RawInterface = ID3D11Texture3D;

		Texture3D();
		Texture3D(ID3D11Resource* _resource);
		Texture3D(ID3D11Texture3D* _tex);
		//Texture3D(Microsoft::WRL::ComPtr<ID3D11Texture3D> _tex);

		Texture3D(const Texture3D& other) = default;
		Texture3D& operator = (const Texture3D& other) = default;

		inline GraphicsDevice* GetDevice() {
			GraphicsDevice* device;
			tex->GetDevice(&device);
			device->Release();
			return device;
		}

		inline ID3D11Texture3D* Interface() {
			return tex;
		}

		inline unsigned int Release() {
			return tex->Release();
		}

		static Texture3D RawCreate(GraphicsDevice* graphics, const RawDescriptor* descriptor, const D3D11_SUBRESOURCE_DATA* initial_data = nullptr);

		ID3D11Texture3D* tex;
	};

	struct ECSENGINE_API TextureCube {
		using RawDescriptor = D3D11_TEXTURE2D_DESC;
		using RawInterface = ID3D11Texture2D;

		TextureCube();
		TextureCube(ID3D11Resource* _resource);
		TextureCube(ID3D11Texture2D* _text);

		TextureCube(const TextureCube& other) = default;
		TextureCube& operator = (const TextureCube& other) = default;

		inline GraphicsDevice* GetDevice() {
			GraphicsDevice* device;
			tex->GetDevice(&device);
			device->Release();
			return device;
		}

		inline ID3D11Texture2D* Interface() {
			return tex;
		}

		inline unsigned int Release() {
			return tex->Release();
		}

		static TextureCube RawCreate(GraphicsDevice* device, const RawDescriptor* descriptor, const D3D11_SUBRESOURCE_DATA* initial_data = nullptr);

		ID3D11Texture2D* tex;
	};

	enum TextureCubeFace {
		ECS_TEXTURE_CUBE_X_POS,
		ECS_TEXTURE_CUBE_X_NEG,
		ECS_TEXTURE_CUBE_Y_POS,
		ECS_TEXTURE_CUBE_Y_NEG,
		ECS_TEXTURE_CUBE_Z_POS,
		ECS_TEXTURE_CUBE_Z_NEG
	};

	struct ECSENGINE_API RenderTargetView {
		RenderTargetView() : target(nullptr) {}
		RenderTargetView(ID3D11RenderTargetView* _target) : target(_target) {}

		RenderTargetView(const RenderTargetView& other) = default;
		RenderTargetView& operator = (const RenderTargetView& other) = default;

		inline GraphicsDevice* GetDevice() {
			GraphicsDevice* device;
			target->GetDevice(&device);
			device->Release();
			return device;
		}

		inline ID3D11RenderTargetView* Interface() {
			return target;
		}

		inline unsigned int Release() {
			return target->Release();
		}

		static RenderTargetView RawCreate(GraphicsDevice* device, ID3D11Resource* resource, const D3D11_RENDER_TARGET_VIEW_DESC* descriptor);

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
			device->Release();
			return device;
		}

		inline ID3D11DepthStencilView* Interface() {
			return view;
		}

		inline unsigned int Release() {
			return view->Release();
		}

		static DepthStencilView RawCreate(GraphicsDevice* device, ID3D11Resource* resource, const D3D11_DEPTH_STENCIL_VIEW_DESC* descriptor);

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
			device->Release();
			return device;
		}

		inline ID3D11Buffer* Interface() {
			return buffer;
		}

		inline unsigned int Release() {
			return buffer->Release();
		}

		static StandardBuffer RawCreate(GraphicsDevice* device, const D3D11_BUFFER_DESC* descriptor, const D3D11_SUBRESOURCE_DATA* initial_data = nullptr);

		ID3D11Buffer* buffer;
		size_t count;
	};

	struct ECSENGINE_API StructuredBuffer {
		StructuredBuffer() : buffer(nullptr) {}
		StructuredBuffer(ID3D11Buffer* _buffer) : buffer(_buffer) {}

		StructuredBuffer(const StructuredBuffer& other) = default;
		StructuredBuffer& operator = (const StructuredBuffer& other) = default;

		inline GraphicsDevice* GetDevice() {
			GraphicsDevice* device;
			buffer->GetDevice(&device);
			device->Release();
			return device;
		}

		inline ID3D11Buffer* Interface() {
			return buffer;
		}

		inline unsigned int Release() {
			return buffer->Release();
		}

		static StructuredBuffer RawCreate(GraphicsDevice* device, const D3D11_BUFFER_DESC* descriptor, const D3D11_SUBRESOURCE_DATA* initial_data = nullptr);

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
			device->Release();
			return device;
		}

		inline ID3D11Buffer* Interface() {
			return buffer;
		}

		inline unsigned int Release() {
			return buffer->Release();
		}

		static IndirectBuffer RawCreate(GraphicsDevice* device, const D3D11_BUFFER_DESC* descriptor, const D3D11_SUBRESOURCE_DATA* initial_data = nullptr);

		ID3D11Buffer* buffer;
	};

	struct ECSENGINE_API UABuffer {
		UABuffer() : buffer(nullptr), element_count(0) {}
		UABuffer(ID3D11Buffer* _buffer) : buffer(_buffer), element_count(0) {}

		UABuffer(const UABuffer& other) = default;
		UABuffer& operator = (const UABuffer& other) = default;

		inline GraphicsDevice* GetDevice() {
			GraphicsDevice* device;
			buffer->GetDevice(&device);
			device->Release();
			return device;
		}

		inline ID3D11Buffer* Interface() {
			return buffer;
		}

		inline unsigned int Release() {
			return buffer->Release();
		}

		static UABuffer RawCreate(GraphicsDevice* device, const D3D11_BUFFER_DESC* descriptor, const D3D11_SUBRESOURCE_DATA* initial_data = nullptr);

		size_t element_count;
		ID3D11Buffer* buffer;
	};

	struct ECSENGINE_API AppendStructuredBuffer {
		AppendStructuredBuffer() : buffer(nullptr), element_count(0) {}
		AppendStructuredBuffer(ID3D11Buffer* _buffer) : buffer(_buffer), element_count(0) {}

		AppendStructuredBuffer(const AppendStructuredBuffer& other) = default;
		AppendStructuredBuffer& operator = (const AppendStructuredBuffer& other) = default;

		inline GraphicsDevice* GetDevice() {
			GraphicsDevice* device;
			buffer->GetDevice(&device);
			device->Release();
			return device;
		}

		inline ID3D11Buffer* Interface() {
			return buffer;
		}

		inline unsigned int Release() {
			return buffer->Release();
		}

		static AppendStructuredBuffer RawCreate(GraphicsDevice* device, const D3D11_BUFFER_DESC* descriptor, const D3D11_SUBRESOURCE_DATA* initial_data = nullptr);

		size_t element_count;
		ID3D11Buffer* buffer;
	};

	struct ECSENGINE_API ConsumeStructuredBuffer {
		ConsumeStructuredBuffer() : buffer(nullptr), element_count(0) {}
		ConsumeStructuredBuffer(ID3D11Buffer* _buffer) : buffer(_buffer), element_count(0) {}

		ConsumeStructuredBuffer(const ConsumeStructuredBuffer& other) = default;
		ConsumeStructuredBuffer& operator = (const ConsumeStructuredBuffer& other) = default;

		inline GraphicsDevice* GetDevice() {
			GraphicsDevice* device;
			buffer->GetDevice(&device);
			device->Release();
			return device;
		}

		inline ID3D11Buffer* Interface() {
			return buffer;
		}

		inline unsigned int Release() {
			return buffer->Release();
		}

		static ConsumeStructuredBuffer RawCreate(GraphicsDevice* device, const D3D11_BUFFER_DESC* descriptor, const D3D11_SUBRESOURCE_DATA* initial_data = nullptr);

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
			device->Release();
			return device;
		}

		inline ID3D11UnorderedAccessView* Interface() {
			return view;
		}

		inline unsigned int Release() {
			return view->Release();
		}

		static UAView RawCreate(GraphicsDevice* device, ID3D11Resource* resource, const D3D11_UNORDERED_ACCESS_VIEW_DESC* descriptor);

		ID3D11UnorderedAccessView* view;
	};

	struct ECSENGINE_API BlendState {
		BlendState() : state(nullptr) {}
		BlendState(ID3D11BlendState* _state) : state(_state) {}

		BlendState(const BlendState& other) = default;
		BlendState& operator = (const BlendState& other) = default;

		inline GraphicsDevice* GetDevice() {
			GraphicsDevice* device;
			state->GetDevice(&device);
			device->Release();
			return device;
		}

		inline ID3D11BlendState* Interface() {
			return state;
		}

		inline unsigned int Release() {
			return state->Release();
		}

		ID3D11BlendState* state;
	};

	struct ECSENGINE_API RasterizerState {
		RasterizerState() : state(nullptr) {}
		RasterizerState(ID3D11RasterizerState* _state) : state(_state) {}

		RasterizerState(const RasterizerState& other) = default;
		RasterizerState& operator = (const RasterizerState& other) = default;

		inline GraphicsDevice* GetDevice() {
			GraphicsDevice* device;
			state->GetDevice(&device);
			device->Release();
			return device;
		}

		inline ID3D11RasterizerState* Interface() {
			return state;
		}

		inline unsigned int Release() {
			return state->Release();
		}

		ID3D11RasterizerState* state;
	};

	struct ECSENGINE_API DepthStencilState {
		DepthStencilState() : state(nullptr) {}
		DepthStencilState(ID3D11DepthStencilState* _state) : state(_state) {}

		DepthStencilState(const DepthStencilState& other) = default;
		DepthStencilState& operator = (const DepthStencilState& other) = default;

		inline GraphicsDevice* GetDevice() {
			GraphicsDevice* device;
			state->GetDevice(&device);
			device->Release();
			return device;
		}

		inline ID3D11DepthStencilState* Interface() {
			return state;
		}

		inline unsigned int Release() {
			return state->Release();
		}

		ID3D11DepthStencilState* state;
	};

	struct ECSENGINE_API CommandList {
		CommandList() : list(nullptr) {}
		CommandList(ID3D11CommandList* _list) : list(_list) {}

		CommandList(const CommandList& other) = default;
		CommandList& operator = (const CommandList& other) = default;

		inline GraphicsDevice* GetDevice() {
			GraphicsDevice* device;
			list->GetDevice(&device);
			device->Release();
			return device;
		}

		inline ID3D11CommandList* Interface() {
			return list;
		}

		inline unsigned int Release() {
			return list->Release();
		}

		ID3D11CommandList* list;
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
		Mesh() : name(nullptr), mapping_count(0) {}

		Mesh(const Mesh& other) = default;
		Mesh& operator = (const Mesh& other) = default;

		const char* name;
		IndexBuffer index_buffer;
		VertexBuffer vertex_buffers[ECS_MESH_BUFFER_COUNT];
		ECS_MESH_INDEX mapping[ECS_MESH_BUFFER_COUNT];
		unsigned char mapping_count;
	};

	struct ECSENGINE_API Submesh {
		Submesh() : name(nullptr), index_buffer_offset(0), vertex_buffer_offset(0), index_count(0), vertex_count(0) {}
		Submesh(unsigned int _index_buffer_offset, unsigned int _vertex_buffer_offset, unsigned int _index_count, unsigned int _vertex_count) 
			: name(nullptr), index_buffer_offset(_index_buffer_offset), vertex_buffer_offset(_vertex_buffer_offset), index_count(_index_count), 
		vertex_count(_vertex_count) {}

		Submesh(const Submesh& other) = default;
		Submesh& operator = (const Submesh& other) = default;

		const char* name;
		unsigned int index_buffer_offset;
		unsigned int vertex_buffer_offset;
		unsigned int index_count;
		unsigned int vertex_count;
	};

	// Contains the actual pipeline objects that can be bound to the graphics context
	struct ECSENGINE_API Material {
		Material();

		Material(const Material& other) = default;
		Material& operator = (const Material& other) = default;

		const char* name;
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

	struct PBRMaterial {
		const char* name;
		float metallic_factor;
		float roughness_factor;
		Color tint;
		float3 emissive_factor;
		Stream<wchar_t> color_texture;
		Stream<wchar_t> normal_texture;
		Stream<wchar_t> metallic_texture;
		Stream<wchar_t> roughness_texture;
		Stream<wchar_t> occlusion_texture;
		Stream<wchar_t> emissive_texture;
	};

	enum PBRMaterialTextureIndex : unsigned char {
		ECS_PBR_MATERIAL_COLOR,
		ECS_PBR_MATERIAL_NORMAL,
		ECS_PBR_MATERIAL_METALLIC,
		ECS_PBR_MATERIAL_ROUGHNESS,
		ECS_PBR_MATERIAL_OCCLUSION,
		ECS_PBR_MATERIAL_EMISSIVE,
		ECS_PBR_MATERIAL_MAPPING_COUNT
	};

	struct PBRMaterialMapping {
		Stream<wchar_t> texture;
		PBRMaterialTextureIndex index;
	};

	struct CoallescedMesh {
		Mesh mesh;
		Stream<Submesh> submeshes;
	};

	// Each submesh has associated a material
	struct PBRMesh {
		CoallescedMesh mesh;
		PBRMaterial* materials;
	};

	ECSENGINE_API void SetPBRMaterialTexture(PBRMaterial* material, uintptr_t& memory, Stream<wchar_t> texture, PBRMaterialTextureIndex texture_index);

	ECSENGINE_API void AllocatePBRMaterial(
		PBRMaterial& material, 
		Stream<char> name, 
		Stream<PBRMaterialMapping> mappings,
		AllocatorPolymorphic allocator
	);

	// Releases the memory used for the texture names and the material name - it's coallesced
	// in the name buffer
	ECSENGINE_API void FreePBRMaterial(const PBRMaterial& material, AllocatorPolymorphic allocator);

	// It will search every directory in order to find each texture - they can be situated
	// in different folders; the texture mask can be used to tell the function which textures
	// to look for, by default it will search for all
	// Might change the texture mask 
	ECSENGINE_API PBRMaterial CreatePBRMaterialFromName(
		Stream<char> material_name,
		Stream<char> texture_base_name, 
		Stream<wchar_t> search_directory, 
		AllocatorPolymorphic allocator,
		Stream<PBRMaterialTextureIndex>* texture_mask = nullptr
	);

	// It will search every directory in order to find each texture - they can be situated
	// in different folders; the texture mask can be used to tell the function which textures
	// to look for, by default it will search for all
	// Might change the texture mask 
	ECSENGINE_API PBRMaterial CreatePBRMaterialFromName(
		Stream<char> material_name,
		Stream<wchar_t> texture_base_name,
		Stream<wchar_t> search_directory,
		AllocatorPolymorphic allocator,
		Stream<PBRMaterialTextureIndex>* texture_mask = nullptr
	);

	ECSENGINE_API VertexBuffer GetMeshVertexBuffer(const Mesh& mesh, ECS_MESH_INDEX buffer_type);

	// It does not release the vertex buffer that is being replaced
	ECSENGINE_API void SetMeshVertexBuffer(Mesh& mesh, ECS_MESH_INDEX buffer_type, VertexBuffer buffer);

}