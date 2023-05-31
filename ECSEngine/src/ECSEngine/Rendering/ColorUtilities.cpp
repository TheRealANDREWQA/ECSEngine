#include "ecspch.h"
#include "ColorUtilities.h"
#include "../Math/Transform.h"

namespace ECSEngine {

	Color::Color() : red(0), green(0), blue(0), alpha(255) {}

	Color::Color(unsigned char _red) : red(_red), green(0), blue(0), alpha(255) {}

	Color::Color(unsigned char _red, unsigned char _green) : red(_red), green(_green), blue(0), alpha(255) {}

	Color::Color(unsigned char _red, unsigned char _green, unsigned char _blue) : red(_red), green(_green), blue(_blue), alpha(255) {}

	Color::Color(unsigned char _red, unsigned char _green, unsigned char _blue, unsigned char _alpha)
		: red(_red), green(_green), blue(_blue), alpha(_alpha) {}

	constexpr float COLOR_RANGE = 255;

	Color::Color(float _red, float _green, float _blue, float _alpha)
	{
		red = _red * COLOR_RANGE;
		green = _green * COLOR_RANGE;
		blue = _blue * COLOR_RANGE;
		alpha = _alpha * COLOR_RANGE;
	}

	Color::Color(ColorFloat color)
	{
		red = color.red * COLOR_RANGE;
		green = color.green * COLOR_RANGE;
		blue = color.blue * COLOR_RANGE;
		alpha = color.alpha * COLOR_RANGE;
	}

	Color::Color(const unsigned char* values) : red(values[0]), green(values[1]), blue(values[2]), alpha(values[3]) {}

	Color::Color(const float* values) : red(values[0] * COLOR_RANGE), green(values[1] * COLOR_RANGE), blue(values[2] * COLOR_RANGE), alpha(values[3] * COLOR_RANGE) {}

	Color::Color(const double* values)
	{
		red = (unsigned char)values[0];
		green = (unsigned char)values[1];
		blue = (unsigned char)values[2];
		alpha = (unsigned char)values[3];
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

	// --------------------------------------------------------------------------------------------------------------------------------

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

	ColorFloat::ColorFloat(const float* values) : red(values[0]), green(values[1]), blue(values[2]), alpha(values[3]) {}

	ColorFloat::ColorFloat(const double* values)
	{
		red = (float)values[0];
		green = (float)values[1];
		blue = (float)values[2];
		alpha = (float)values[3];
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

	// --------------------------------------------------------------------------------------------------------------------------------

	template<typename Color>
	Color RGBToHSV(Color color) {
		float normalized_values[4];
		color.Normalize(normalized_values);
		float max = normalized_values[0];
		float min = normalized_values[0];
		float new_red = normalized_values[0];
		float new_green = normalized_values[1];
		float new_blue = normalized_values[2];
		max = std::max(max, new_green);
		max = std::max(max, new_blue);
		min = std::min(min, new_green);
		min = std::min(min, new_blue);
		float delta = max - min;

		Color new_color;
		float color_range = new_color.GetRange();
		// value
		new_color.value = max * color_range;

		float hue;
		// hue
		if (delta == 0.0f)
			hue = 0.0f;
		else if (max == new_red) {
			hue = 60.0f * (((new_green - new_blue) / delta));
			hue += 360.0f * (hue < 0.0f);
		}
		else if (max == new_green) {
			hue = 60.0f * (((new_blue - new_red) / delta) + 2);
		}
		else {
			hue = 60.0f * (((new_red - new_green) / delta) + 4);
		}
		hue *= color_range / 360.0f;
		new_color.hue = hue;

		// saturation
		new_color.saturation = (max == 0.0f ? 0.0f : delta) / max * color_range;
		new_color.alpha = color.alpha;
		return new_color;
	}

	ECS_TEMPLATE_FUNCTION(Color, RGBToHSV, Color);
	ECS_TEMPLATE_FUNCTION(ColorFloat, RGBToHSV, ColorFloat);

	// --------------------------------------------------------------------------------------------------------------------------------

	template<typename Color>
	Color HSVToRGB(Color color) {
		float normalized_values[4];
		color.Normalize(normalized_values);
		float hue = normalized_values[0] * 360.0f;
		float saturation = normalized_values[1];
		float value = normalized_values[2];

		float Cfactor = saturation * value;
		float x_factor_temp = hue / 60.0f;
		float mod = fmod(x_factor_temp, 2) - 1;
		mod = mod < 0 ? -mod : mod;
		float Xfactor = Cfactor * (1 - mod);
		float m_factor = value - Cfactor;

		float red, green, blue;
		if (hue < 60.0f) {
			red = Cfactor;
			green = Xfactor;
			blue = 0.0f;
		}
		else if (hue < 120.0f) {
			red = Xfactor;
			green = Cfactor;
			blue = 0.0f;
		}
		else if (hue < 180.0f) {
			red = 0.0f;
			green = Cfactor;
			blue = Xfactor;
		}
		else if (hue < 240.0f) {
			red = 0.0f;
			green = Xfactor;
			blue = Cfactor;
		}
		else if (hue < 300.0f) {
			red = Xfactor;
			green = 0.0f;
			blue = Cfactor;
		}
		else {
			red = Cfactor;
			green = 0.0f;
			blue = Xfactor;
		}
		Color new_color;
		float color_range = new_color.GetRange();
		new_color.red = (red + m_factor) * color_range;
		new_color.green = (green + m_factor) * color_range;
		new_color.blue = (blue + m_factor) * color_range;
		new_color.alpha = color.alpha;
		return new_color;
	}

	ECS_TEMPLATE_FUNCTION(Color, HSVToRGB, Color);
	ECS_TEMPLATE_FUNCTION(ColorFloat, HSVToRGB, ColorFloat);

	// --------------------------------------------------------------------------------------------------------------------------------

	Color HexToRGB(Stream<char> characters) {
		Color color;

		unsigned char current_char = 0;
		unsigned char power = 16;
		unsigned char* interpretation = (unsigned char*)&color;
		unsigned char current_value = 0;

		auto handle_value = [&](unsigned char value) {
			current_value += power * value;
			if (power == 1) {
				interpretation[current_char++] = current_value;
				current_value = 0;
				power = 16;
			}
			else {
				power = 1;
			}
		};
		for (size_t index = 0; index < characters.size; index++) {
			if (characters[index] >= '0') {
				if (characters[index] <= '9') {
					handle_value(characters[index] - '0');
				}
				else if (characters[index] <= 'F' && characters[index] >= 'A') {
					handle_value(characters[index] - 'A' + 10);
				}
				else if (characters[index] >= 'a' && characters[index] <= 'f') {
					handle_value(characters[index] - 'a' + 10);
				}

				if (current_char == 3) {
					break;
				}
			}
		}
		if (current_char < 3) {
			if (current_char == 2) {
				color.blue = current_value;
			}
			else if (current_char == 1) {
				color.green = current_value;
				color.blue = 0;
			}
			else {
				color.blue = 0;
				color.green = 0;
				color.red = current_value;
			}
		}
		color.alpha = 255;

		return color;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	void RGBToHex(CapacityStream<char>& characters, Color color) {
		char high_red = color.red / 16;
		char low_red = color.red % 16;
		char high_green = color.green / 16;
		char low_green = color.green % 16;
		char high_blue = color.blue / 16;
		char low_blue = color.blue % 16;

		auto set_letter = [&](unsigned int index, char character) {
			if (character < 10) {
				characters[index] = character + '0';
			}
			else {
				characters[index] = character - 10 + 'A';
			}
		};

		set_letter(0, high_red);
		set_letter(1, low_red);
		set_letter(2, high_green);
		set_letter(3, low_green);
		set_letter(4, high_blue);
		set_letter(5, low_blue);
		characters.size = 6;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	Color HDRColorToSDR(ColorFloat color, float* intensity) {
		Color sdr_color;

		color.alpha = function::Clamp(color.alpha, 0.0f, 1.0f);
		unsigned char alpha = color.alpha * 255.0f;
		if (color.red <= 1.0f && color.green <= 1.0f && color.blue <= 1.0f) {
			// Let the intensity be 1.0f and the values the quantized 8bit values
			if (intensity != nullptr) {
				*intensity = 1.0f;
			}

			// It will do the correct transformation
			sdr_color = color;
		}
		else {
			float largest_component = std::max(color.red, color.blue);
			largest_component = std::max(largest_component, color.green);
			float largest_component_inverse = 1.0f / largest_component;

			sdr_color.red = color.red * largest_component_inverse * sdr_color.GetRange();
			sdr_color.green = color.green * largest_component_inverse * sdr_color.GetRange();
			sdr_color.blue = color.blue * largest_component_inverse * sdr_color.GetRange();

			if (intensity != nullptr) {
				*intensity = largest_component;
			}
		}
		sdr_color.alpha = alpha;

		return sdr_color;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	ColorFloat SDRColorToHDR(Color color, float intensity) {
		ColorFloat hdr_color;

		hdr_color = color;
		hdr_color.red *= intensity;
		hdr_color.green *= intensity;
		hdr_color.blue *= intensity;

		return hdr_color;
	}

	// --------------------------------------------------------------------------------------------------------------------------------

	template<typename Color>
	Color BlendColors(Color first, Color second) {
		Color result;

		float normalized_alpha = (float)second.alpha / Color::GetRange();

		result.red = function::Lerp(first.red, second.red, normalized_alpha);
		result.green = function::Lerp(first.green, second.green, normalized_alpha);
		result.blue = function::Lerp(first.blue, second.blue, normalized_alpha);

		return result;
	}

	ECS_TEMPLATE_FUNCTION(Color, BlendColors, Color, Color);
	ECS_TEMPLATE_FUNCTION(ColorFloat, BlendColors, ColorFloat, ColorFloat);

	// --------------------------------------------------------------------------------------------------------------------------------

	template<typename Color>
	Color ChangeHue(Color rgb, float percentage)
	{
		float color_range = Color::GetRange();
		Color hsv = RGBToHSV(rgb);
		// Two step assignment because we want to preserve the range before clamping
		float new_hue = (hsv.hue + percentage) * color_range;
		hsv.hue = fmod(new_hue, color_range);
		return HSVToRGB(hsv);
	}

	ECS_TEMPLATE_FUNCTION(Color, ChangeHue, Color, float);
	ECS_TEMPLATE_FUNCTION(ColorFloat, ChangeHue, ColorFloat, float);

	// --------------------------------------------------------------------------------------------------------------------------------

	template<typename Color>
	Color ChangeSaturation(Color rgb, float percentage)
	{
		float color_range = Color::GetRange();
		Color hsv = RGBToHSV(rgb);
		float new_saturation = (hsv.saturation + percentage) * color_range;
		hsv.saturation = fmod(new_saturation, color_range);
		return HSVToRGB(hsv);
	}

	ECS_TEMPLATE_FUNCTION(Color, ChangeSaturation, Color, float);
	ECS_TEMPLATE_FUNCTION(ColorFloat, ChangeSaturation, ColorFloat, float);

	// --------------------------------------------------------------------------------------------------------------------------------

	template<typename Color>
	Color ChangeValue(Color rgb, float percentage)
	{
		return LightenColorClamp(rgb, percentage);
	}

	ECS_TEMPLATE_FUNCTION(Color, ChangeValue, Color, float);
	ECS_TEMPLATE_FUNCTION(ColorFloat, ChangeValue, ColorFloat, float);

	// --------------------------------------------------------------------------------------------------------------------------------

}