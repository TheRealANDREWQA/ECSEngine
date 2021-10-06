#include "ecspch.h"
#include "RenderingStructures.h"
#include "../Utilities/Function.h"

namespace ECSEngine {

	VertexBuffer::VertexBuffer() : stride(0), buffer(nullptr) {}

	VertexBuffer::VertexBuffer(UINT _stride, UINT _size, ID3D11Buffer* _buffer) 
		: stride(_stride), size(_size), buffer(_buffer) {}

	IndexBuffer::IndexBuffer() : count(0), buffer(nullptr) {}

	InputLayout::InputLayout() : layout(nullptr) {}

	VertexShader::VertexShader() : byte_code(nullptr), shader(nullptr) {}

	PixelShader::PixelShader() : shader(nullptr) {}

	Topology::Topology() : value(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST) {}

	Topology::Topology(D3D11_PRIMITIVE_TOPOLOGY topology) : value(topology) {}

	ResourceView::ResourceView() : view(nullptr) {}

	ResourceView::ResourceView(ID3D11ShaderResourceView* _view) : view(_view) {}

	ConstantBuffer::ConstantBuffer() : buffer(nullptr) {}

	ConstantBuffer::ConstantBuffer(ID3D11Buffer* _buffer) : buffer(_buffer) {}

	SamplerState::SamplerState() : sampler(nullptr) {}

	SamplerState::SamplerState(ID3D11SamplerState* _sampler) : sampler(_sampler) {}

	Texture1D::Texture1D() : tex(nullptr) {}

	Texture1D::Texture1D(ID3D11Resource* _resource)
	{
		Microsoft::WRL::ComPtr<ID3D11Resource> ptr;
		ptr.Attach(_resource);
		Microsoft::WRL::ComPtr<ID3D11Texture1D> com_tex;
		HRESULT result = ptr.As(&com_tex);

		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Converting resource to Texture1D failed!", true);
		tex = com_tex.Detach();
	}

	Texture1D::Texture1D(ID3D11Texture1D* _tex) : tex(_tex) {}

	Texture2D::Texture2D() : tex(nullptr) {}

	Texture2D::Texture2D(ID3D11Texture2D* _tex) : tex(_tex) {}

	Texture2D::Texture2D(ID3D11Resource* _resource)
	{
		Microsoft::WRL::ComPtr<ID3D11Resource> ptr;
		ptr.Attach(_resource);
		Microsoft::WRL::ComPtr<ID3D11Texture2D> com_tex;
		HRESULT result = ptr.As(&com_tex);

		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Converting resource to Texture1D failed!", true);
		tex = com_tex.Detach();
	}

	Texture3D::Texture3D() : tex(nullptr) {}

	Texture3D::Texture3D(ID3D11Texture3D* _tex) : tex(_tex) {}

	Texture3D::Texture3D(ID3D11Resource* _resource)
	{
		Microsoft::WRL::ComPtr<ID3D11Resource> ptr;
		ptr.Attach(_resource);
		Microsoft::WRL::ComPtr<ID3D11Texture3D> com_tex;
		HRESULT result = ptr.As(&com_tex);

		ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(result, L"Converting resource to Texture1D failed!", true);
		tex = com_tex.Detach();
	}

	Translation::Translation() : x(0.0f), y(0.0f), z(0.0f) {}

	Translation::Translation(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}

	Color::Color() : red(0), green(0), blue(0), alpha(255) {}

	Color::Color(unsigned char _red) : red(_red), green(0), blue(0), alpha(255) {}

	Color::Color(unsigned char _red, unsigned char _green) : red(_red), green(_green), blue(0), alpha(255) {}

	Color::Color(unsigned char _red, unsigned char _green, unsigned char _blue) : red(_red), green(_green), blue(_blue), alpha(255) {}

	Color::Color(unsigned char _red, unsigned char _green, unsigned char _blue, unsigned char _alpha)
	 : red(_red), green(_green), blue(_blue), alpha(_alpha) {}

	Color::Color(float _red, float _green, float _blue, float _alpha)
	{
		constexpr float range = 255;
		red = _red * range;
		green = _green * range;
		blue = _blue * range;
		alpha = _alpha * range;
	}

	Color::Color(ColorFloat color)
	{
		constexpr float range = 255;
		red = color.red * range;
		green = color.green * range;
		blue = color.blue * range;
		alpha = color.alpha * range;
	}

	bool Color::operator==(const Color& other) const
	{
		return red == other.red && green == other.green && blue == other.blue && alpha == other.alpha;
	}

	bool Color::operator != (const Color& other) const {
		return red != other.red || green != other.green || blue != other.blue || alpha != other.alpha;
	}

	Color Color::operator+(const Color& other) const
	{
		return Color(red + other.red, green + other.green, blue + other.blue);
	}

	Color Color::operator*(const Color& other) const
	{
		constexpr float inverse_range = 1.0f / 65535;
		Color new_color;

		float current_red = static_cast<float>(red);
		new_color.red = current_red * other.red * inverse_range;

		float current_green = static_cast<float>(green);
		new_color.green = current_green * other.green * inverse_range;

		float current_blue = static_cast<float>(blue);
		new_color.blue = current_blue * other.blue * inverse_range;

		float current_alpha = static_cast<float>(alpha);
		new_color.alpha = current_alpha * other.alpha * inverse_range;

		return new_color;
	}

	Color Color::operator*(float percentage) const
	{
		Color new_color;
		new_color.red = static_cast<float>(red) * percentage;
		new_color.green = static_cast<float>(green) * percentage;
		new_color.blue = static_cast<float>(blue) * percentage;
		new_color.alpha = static_cast<float>(alpha) * percentage;
		return new_color;
	}

	void Color::Normalize(float* values) const
	{
		constexpr float inverse = 1.0f / 255.0f;
		values[0] = (float)red * inverse;
		values[1] = (float)green * inverse;
		values[2] = (float)blue * inverse;
		values[3] = (float)alpha * inverse;
	}

	ColorRGB::ColorRGB() : red(0), green(0), blue(0) {}

	ColorRGB::ColorRGB(unsigned char _red) : red(_red), green(0), blue(0) {}

	ColorRGB::ColorRGB(unsigned char _red, unsigned char _green) : red(_red), green(_green), blue(0) {}

	ColorRGB::ColorRGB(unsigned char _red, unsigned char _green, unsigned char _blue)
		: red(_red), green(_green), blue(_blue) {}

	void ColorRGB::Normalize(float* values) const
	{
		float inverse = 1.0f / 255.0f;
		values[0] = (float)red * inverse;
		values[1] = (float)green * inverse;
		values[2] = (float)blue * inverse;
	}

	ColorFloat::ColorFloat() : red(0.0f), green(0.0f), blue(0.0f), alpha(1.0f) {}

	ColorFloat::ColorFloat(float _red) : red(_red), green(0.0f), blue(0.0f), alpha(1.0f) {}

	ColorFloat::ColorFloat(float _red, float _green) : red(_red), green(_green), blue(0.0f), alpha(1.0f) {}

	ColorFloat::ColorFloat(float _red, float _green, float _blue)
		: red(_red), green(_green), blue(_blue), alpha(1.0f) {}

	ColorFloat::ColorFloat(float _red, float _green, float _blue, float _alpha)
		: red(_red), green(_green), blue(_blue), alpha(_alpha) {}

	ColorFloat::ColorFloat(Color color)
	{
		constexpr float inverse_range = 1.0f / Color::GetRange();
		red = static_cast<float>(color.red) * inverse_range;
		green = static_cast<float>(color.green) * inverse_range;
		blue = static_cast<float>(color.blue) * inverse_range;
		alpha = static_cast<float>(color.alpha) * inverse_range;
	}

	ColorFloat ColorFloat::operator*(const ColorFloat& other) const
	{
		return ColorFloat(red * other.red, green * other.green, blue * other.blue, alpha * other.alpha);
	}

	ColorFloat ColorFloat::operator*(float percentage) const
	{
		return ColorFloat(red * percentage, green * percentage, blue * percentage, alpha * percentage);
	}

	ColorFloat ColorFloat::operator+(const ColorFloat& other) const
	{
		return ColorFloat(red + other.red, green + other.green, blue + other.blue, alpha + other.alpha);
	}

	ColorFloat ColorFloat::operator-(const ColorFloat& other) const
	{
		return ColorFloat(red - other.red, green - other.green, blue - other.blue, alpha - other.alpha);
	}

	void ColorFloat::Normalize(float* values) const
	{
		values[0] = red;
		values[1] = green;
		values[2] = blue;
		values[3] = alpha;
	}

	ColorFloatRGB::ColorFloatRGB() : red(0.0f), green(0.0f), blue(0.0f) {}

	ColorFloatRGB::ColorFloatRGB(float _red) : red(_red), green(0.0f), blue(0.0f) {}

	ColorFloatRGB::ColorFloatRGB(float _red, float _green) : red(_red), green(_green), blue(0.0f) {}

	ColorFloatRGB::ColorFloatRGB(float _red, float _green, float _blue)
		: red(_red), green(_green), blue(_blue) {}

	void ColorFloatRGB::Normalize(float* values) const
	{
		values[0] = red;
		values[1] = green;
		values[2] = blue;
	}

	Camera::Camera() : translation(0.0f, 0.0f, 0.0f), rotation(0.0f, 0.0f, 0.0f) {}

	Camera::Camera(float3 _translation, float3 _rotation) : translation(_translation), rotation(_rotation) {}

	Camera::Camera(Matrix _projection, float3 _translation, float3 _rotation) : projection(_projection), translation(_translation), rotation(_rotation) {}

	void Camera::SetOrthographicProjection(float width, float height, float near_z, float far_z) {
		projection = MatrixOrthographic(width, height, near_z, far_z);
	}

	void Camera::SetPerspectiveProjection(float width, float height, float near_z, float far_z) {
		projection = MatrixPerspective(width, height, near_z, far_z);
	}

	void Camera::SetPerspectiveProjectionFOV(float angle_y, float aspect_ratio, float near_z, float far_z) {
		projection = MatrixPerspectiveFOV(angle_y, aspect_ratio, near_z, far_z);
	}

	Matrix Camera::GetProjectionViewMatrix() const {
		return MatrixTranslation(-translation) * MatrixRotationZ(-rotation.z) * MatrixRotationY(-rotation.y) * MatrixRotationX(-rotation.x) * projection;
	}

}