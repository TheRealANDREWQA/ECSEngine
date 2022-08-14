#pragma once
#include "../Utilities/Function.h"

#define ECS_COLOR_RED ECSEngine::Color(255, 0, 0)
#define ECS_COLOR_GREEN ECSEngine::Color(0, 128, 0)
#define ECS_COLOR_BLUE ECSEngine::Color(0, 0, 255)
#define ECS_COLOR_YELLOW ECSEngine::Color(255, 255, 0)
#define ECS_COLOR_ORANGE ECSEngine::Color(255, 102, 0)
#define ECS_COLOR_MAGENTA ECSEngine::Color(255, 0, 255)
#define ECS_COLOR_PURPLE ECSEngine::Color(128, 0, 128)
#define ECS_COLOR_BROWN ECSEngine::Color(135, 42, 12)
#define ECS_COLOR_WHITE ECSEngine::Color(255, 255, 255)
#define ECS_COLOR_BLACK ECSEngine::Color(0, 0, 0)
#define ECS_COLOR_GRAY ECSEngine::Color(128, 128, 128)
#define ECS_COLOR_LIGHT_GRAY ECSEngine::Color(209, 209, 209)
#define ECS_COLOR_DARK_GRAY Color(94, 94, 94)
#define ECS_COLOR_CYAN ECSEngine::Color(0, 255, 255)
#define ECS_COLOR_PINK ECSEngine::Color(255, 153, 168)
#define ECS_COLOR_LIME ECSEngine::Color(0, 255, 0)
#define ECS_COLOR_LIGHT_BLUE ECSEngine::Color(128, 204, 255)
#define ECS_COLOR_WARM_BLUE ECSEngine::Color(26, 163, 255)
#define ECS_COLOR_DARK_BLUE ECSEngine::Color(0, 60, 179)
#define ECS_COLOR_AQUA ECSEngine::Color(0, 153, 255)
#define ECS_COLOR_MAROON ECSEngine::Color(128, 0, 0)
#define ECS_COLOR_WOOD ECSEngine::Color(102, 51, 0)
#define ECS_COLOR_BACKGROUND_BLACK ECSEngine::Color(26, 26, 26)
#define ECS_COLOR_PUNK ECSEngine::Color(255, 51, 204)
#define ECS_COLOR_CLOUD ECSEngine::Color(230, 230, 230)
#define ECS_COLOR_MUSTARD ECSEngine::Color(255, 179, 26)

namespace ECSEngine {

	template<typename Color>
	Color LightenColor(Color color, float percentage) {
		return Color(color.red * percentage, color.green * percentage, color.blue * percentage);
	}

	template<typename Color>
	Color DarkenColor(Color color, float percentage) {
		return Color(color.red * percentage, color.green * percentage, color.blue * percentage);
	}

	template<typename Color>
	Color LightenColorClamp(Color color, float percentage) {
		float new_red = static_cast<float>(color.red) * percentage;
		float new_green = static_cast<float>(color.green) * percentage;
		float new_blue = static_cast<float>(color.blue) * percentage;
		return Color(
			function::ClampMax(new_red, Color::GetRange()),
			function::ClampMax(new_green, Color::GetRange()),
			function::ClampMax(new_blue, Color::GetRange())
		);
	}

	// basically the same as LightenColorClamp but to express intent that could lighten or darken
	template<typename Color>
	Color ToneColor(Color color, float percentage) {
		return LightenColorClamp(color, percentage);
	}

	// succesive conversions from one to the other can affect accuracy and skew results
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

	// succesive conversions from one to the other can affect accuracy and skew results
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

	// it will omit characters that are not valid 
	template<typename Stream>
	Color HexToRGB(const Stream& characters) {
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

	static Color HexToRGB(char* characters) {
		size_t character_count = strlen(characters);

		return HexToRGB(Stream<char>(characters, character_count));
	}

	template<typename Stream>
	void RGBToHex(Stream& characters, Color color) {
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

	// Normalizes the 3 component rgb and returns the base color along side
	// the intensity. The alpha channel is clamped to [0, 1]
	static Color HDRColorToSDR(ColorFloat color, float* intensity = nullptr) {
		Color sdr_color;

		color.alpha = function::Clamp(color.alpha, 0.0f, 1.0f);
		Vector4 vector_color;
		vector_color.Load(&color);

		unsigned char alpha = color.alpha * 255.0f;
		Vector4 intensity_vector = Length3(vector_color);
		if (intensity != nullptr) {
			*intensity = intensity_vector.First();
		}
		Vector4 normalized_vector_color = vector_color / intensity_vector;
		ColorFloat normalized_color;
		normalized_vector_color.Store(&normalized_color);
		sdr_color = normalized_color;
		sdr_color.alpha = alpha;

		return sdr_color;
	}

	static ColorFloat SDRColorToHDR(Color color, float intensity) {
		ColorFloat hdr_color;

		hdr_color = color;
		hdr_color.red *= intensity;
		hdr_color.green *= intensity;
		hdr_color.blue *= intensity;

		return hdr_color;
	}

}