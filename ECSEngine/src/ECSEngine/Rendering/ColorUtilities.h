#pragma once
#include "../Math/MathHelpers.h"

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
		typedef unsigned char ComponentType;

		ECS_INLINE Color() : red(0), green(0), blue(0), alpha(255) {}
		ECS_INLINE Color(unsigned char _red) : red(_red), green(0), blue(0), alpha(255) {}

		ECS_INLINE Color(unsigned char _red, unsigned char _green) : red(_red), green(_green), blue(0), alpha(255) {}

		ECS_INLINE Color(unsigned char _red, unsigned char _green, unsigned char _blue) : red(_red), green(_green), blue(_blue), alpha(255) {}

		ECS_INLINE Color(unsigned char _red, unsigned char _green, unsigned char _blue, unsigned char _alpha) : red(_red), green(_green), blue(_blue), alpha(_alpha) {}

		// normalized values
		Color(float red, float green, float blue, float alpha);

		Color(ColorFloat color);

		ECS_INLINE Color(const unsigned char* values) : red(values[0]), green(values[1]), blue(values[2]), alpha(values[3]) {}

		// Assumes the values are in the 0.0f - 1.0f range
		Color(const float* values);

		// Assumes the values are in the 0-255 range
		Color(const double* values);

		Color(const Color& other) = default;
		Color& operator = (const Color& other) = default;

		ECS_INLINE bool operator == (const Color& other) const {
			return red == other.red && green == other.green && blue == other.blue && alpha == other.alpha;
		}
		ECS_INLINE bool operator != (const Color& other) const {
			return !(*this == other);
		}

		// these operators do not apply for alpha
		ECS_INLINE Color operator + (const Color& other) const {
			return Color(red + other.red, green + other.green, blue + other.blue);
		}
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
		typedef float ComponentType;

		ECS_INLINE ColorFloat() : red(0.0f), green(0.0f), blue(0.0f), alpha(1.0f) {}

		ECS_INLINE ColorFloat(float _red) : red(_red), green(0.0f), blue(0.0f), alpha(1.0f) {}

		ECS_INLINE ColorFloat(float _red, float _green) : red(_red), green(_green), blue(0.0f), alpha(1.0f) {}

		ECS_INLINE ColorFloat(float _red, float _green, float _blue) : red(_red), green(_green), blue(_blue), alpha(1.0f) {}

		ECS_INLINE ColorFloat(float _red, float _green, float _blue, float _alpha) : red(_red), green(_green), blue(_blue), alpha(_alpha) {}

		ColorFloat(Color color);

		ECS_INLINE ColorFloat(const float* values) : red(values[0]), green(values[1]), blue(values[2]), alpha(values[3]) {}

		// Assumes the values are in the 0.0 - 1.0 range
		ColorFloat(const double* values);

		ColorFloat(const ColorFloat& other) = default;
		ColorFloat& operator = (const ColorFloat& other) = default;

		ECS_INLINE ColorFloat operator * (const ColorFloat& other) const {
			return ColorFloat(red * other.red, green * other.green, blue * other.blue, alpha * other.alpha);
		}
		ECS_INLINE ColorFloat operator * (float percentage) const {
			return ColorFloat(red * percentage, green * percentage, blue * percentage, alpha * percentage);
		}
		ECS_INLINE ColorFloat operator + (const ColorFloat& other) const {
			return ColorFloat(red + other.red, green + other.green, blue + other.blue, alpha + other.alpha);
		}
		ECS_INLINE ColorFloat operator - (const ColorFloat& other) const {
			return ColorFloat(red - other.red, green - other.green, blue - other.blue, alpha - other.alpha);
		}

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
	
	// The percentage should be above 1.0f
	template<typename Color>
	ECS_INLINE Color LightenColor(Color color, float percentage) {
		return Color(
			(typename Color::ComponentType)(color.red * percentage), 
			(typename Color::ComponentType)(color.green * percentage), 
			(typename Color::ComponentType)(color.blue * percentage), 
			color.alpha
		);
	}

	// The percentage should be below 1.0f
	template<typename Color>
	ECS_INLINE Color DarkenColor(Color color, float percentage) {
		return Color(
			(typename Color::ComponentType)(color.red * percentage),
			(typename Color::ComponentType)(color.green * percentage),
			(typename Color::ComponentType)(color.blue * percentage),
			color.alpha
		);
	}

	// The percentage should be above 1.0f. If the color bounds are exceeded, it will clamp them to the highest possible value
	template<typename Color>
	Color LightenColorClamp(Color color, float percentage) {
		float new_red = static_cast<float>(color.red) * percentage;
		float new_green = static_cast<float>(color.green) * percentage;
		float new_blue = static_cast<float>(color.blue) * percentage;
		return Color(
			(typename Color::ComponentType)ClampMax(new_red, Color::GetRange()),
			(typename Color::ComponentType)ClampMax(new_green, Color::GetRange()),
			(typename Color::ComponentType)ClampMax(new_blue, Color::GetRange()),
			color.alpha
		);
	}

	// Basically the same as LightenColorClamp but to express intent that could lighten or darken
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