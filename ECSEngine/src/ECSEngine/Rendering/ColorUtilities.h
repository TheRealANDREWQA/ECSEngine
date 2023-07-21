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

	struct ECSENGINE_API ColorFloat;

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

		// Assumes the values are in the 0.0f - 1.0f range
		Color(const float* values);

		// Assumes the values are in the 0-255 range
		Color(const double* values);

		Color(const Color& other) = default;
		Color& operator = (const Color& other) = default;

		bool operator == (const Color& other) const;
		bool operator != (const Color& other) const;

		// these operators do not apply for alpha
		Color operator + (const Color& other) const;
		Color operator * (const Color& other) const;
		Color operator * (float percentage) const;

		ECS_INLINE static constexpr float GetRange() {
			return 255.0f;
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

		// Assumes the values are in the 0.0 - 1.0 range
		ColorFloat(const double* values);

		ColorFloat(const ColorFloat& other) = default;
		ColorFloat& operator = (const ColorFloat& other) = default;

		ColorFloat operator * (const ColorFloat& other) const;
		ColorFloat operator * (float percentage) const;
		ColorFloat operator + (const ColorFloat& other) const;
		ColorFloat operator - (const ColorFloat& other) const;

		ECS_INLINE static constexpr float GetRange() {
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

	template<typename Color>
	ECS_INLINE Color LightenColor(Color color, float percentage) {
		return Color(color.red * percentage, color.green * percentage, color.blue * percentage);
	}

	template<typename Color>
	ECS_INLINE Color DarkenColor(Color color, float percentage) {
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
	ECS_INLINE Color ToneColor(Color color, float percentage) {
		return LightenColorClamp(color, percentage);
	}

	// succesive conversions from one to the other can affect accuracy and skew results
	template<typename Color>
	ECSENGINE_API Color RGBToHSV(Color color);

	// succesive conversions from one to the other can affect accuracy and skew results
	template<typename Color>
	ECSENGINE_API Color HSVToRGB(Color color);

	// it will omit characters that are not valid 
	ECSENGINE_API Color HexToRGB(Stream<char> characters);

	ECS_INLINE Color HexToRGB(char* characters) {
		size_t character_count = strlen(characters);
		return HexToRGB(Stream<char>(characters, character_count));
	}

	ECSENGINE_API void RGBToHex(CapacityStream<char>& characters, Color color);

	// Normalizes the 3 component rgb and returns the base color along side
	// the intensity. The alpha channel is clamped to [0, 1]
	ECSENGINE_API Color HDRColorToSDR(ColorFloat color, float* intensity = nullptr);

	ECSENGINE_API ColorFloat SDRColorToHDR(Color color, float intensity);

	// It uses the second.alpha as the percentage for lerp
	template<typename Color>
	ECSENGINE_API Color BlendColors(Color first, Color second);

	// Changes the hue of an rgb color and returns it as an rgb
	template<typename Color>
	ECSENGINE_API Color ChangeHue(Color rgb, float percentage);

	// Changes the saturation of an rgb color and returns it as an rgb
	template<typename Color>
	ECSENGINE_API Color ChangeSaturation(Color rgb, float percentage);

	// Changes the value of an rgb color and returns it as an rgb
	template<typename Color>
	ECSENGINE_API Color ChangeValue(Color rgb, float percentage);

	// Converts a color from linear space to gamma space
	template<typename Color>
	ECSENGINE_API Color LinearToSRGB(Color color);

	// Converts a color from gamma space to linear space
	template<typename Color>
	ECSENGINE_API Color SRGBToLinear(Color color);

	// The conversion is applied only to the xyz channels
	ECSENGINE_API double4 LinearToSRGB(double4 value);

	// The conversion is applied only to the xyz channels
	ECSENGINE_API double4 SRGBToLinear(double4 value);

}