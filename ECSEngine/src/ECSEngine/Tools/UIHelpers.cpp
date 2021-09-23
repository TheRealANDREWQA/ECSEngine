#include "ecspch.h"
#include "UIHelpers.h"
#include "../Utilities/FunctionInterfaces.h"
#include "../Utilities/Mouse.h"
#include "../Utilities/Keyboard.h"

namespace ECSEngine {

	namespace Tools {

		template<typename Element>
		void SetTransformForRectangle(
			float2 position,
			float2 scale,
			unsigned int starting_index,
			Element* buffer
		) {
			buffer[starting_index].SetTransform(position.x, -position.y);                            // top left
			buffer[starting_index + 1].SetTransform(position.x + scale.x, -position.y);              // top right
			buffer[starting_index + 2].SetTransform(position.x, -(position.y + scale.y));            // lower left
			buffer[starting_index + 3].SetTransform(position.x + scale.x, -position.y);              // top right
			buffer[starting_index + 4].SetTransform(position.x + scale.x, -(position.y + scale.y));  // lower right
			buffer[starting_index + 5].SetTransform(position.x, -(position.y + scale.y));            // lower left
		}

		ECS_TEMPLATE_FUNCTION_2_AFTER(void, SetTransformForRectangle, UIVertexColor*, UISpriteVertex*, float2, float2, unsigned int);

		void SetTransformForLine(float2 position1, float2 position2, size_t count, UIVertexColor* buffer) {
			buffer[count].SetTransform(position1);
			buffer[count + 1].SetTransform(position2);
		}

		void SetTransformForLine(float2 position1, float2 position2, size_t* counts, void** buffers, unsigned int material_offset) {
			SetTransformForLine(
				position1,
				position2,
				counts[ECS_TOOLS_UI_LINE + ECS_TOOLS_UI_MATERIALS * material_offset],
				(UIVertexColor*)buffers[ECS_TOOLS_UI_LINE + ECS_TOOLS_UI_MATERIALS * material_offset]
			);
		}

		void SetColorForLine(Color color, size_t count, UIVertexColor* buffer) {
			buffer[count].SetColor(color);
			buffer[count + 1].SetColor(color);
		}

		void SetColorForLine(Color color, size_t* counts, void** buffers, unsigned int material_offset) {
			SetColorForLine(
				color,
				counts[ECS_TOOLS_UI_LINE + ECS_TOOLS_UI_MATERIALS * material_offset],
				(UIVertexColor*)buffers[ECS_TOOLS_UI_LINE + ECS_TOOLS_UI_MATERIALS * material_offset]
			);
		}

		void SetLine(float2 position1, float2 position2, Color color, size_t& count, UIVertexColor* buffer) {
			SetTransformForLine(position1, position2, count, buffer);
			SetColorForLine(color, count, buffer);
			count += 2;
		}

		void SetLine(float2 position1, float2 position2, Color color, size_t* counts, void** buffers, unsigned int material_offset) {
			SetTransformForLine(position1, position2, counts, buffers, material_offset);
			SetColorForLine(color, counts, buffers, material_offset);
			counts[ECS_TOOLS_UI_LINE + ECS_TOOLS_UI_MATERIALS * material_offset] += 2;
		}

		template<typename Element>
		void SetVertexColorForRectangle(
			Color top_left,
			Color top_right,
			Color bottom_left,
			Color bottom_right,
			unsigned int starting_index,
			Element* buffer
		) {
			buffer[starting_index].SetColor(top_left);
			buffer[starting_index + 1].SetColor(top_right);
			buffer[starting_index + 2].SetColor(bottom_left);
			buffer[starting_index + 3].SetColor(top_right);
			buffer[starting_index + 4].SetColor(bottom_right);
			buffer[starting_index + 5].SetColor(bottom_left);
		}

		ECS_TEMPLATE_FUNCTION_2_AFTER(void, SetVertexColorForRectangle, UIVertexColor*, UISpriteVertex*, Color, Color, Color, Color, unsigned int);

		template<typename Element>
		void SetVertexColorForRectangle(
			const Color* colors,
			unsigned int starting_index,
			Element* buffer
		) {
			buffer[starting_index].SetColor(colors[0]);
			buffer[starting_index + 1].SetColor(colors[1]);
			buffer[starting_index + 2].SetColor(colors[2]);
			buffer[starting_index + 3].SetColor(colors[1]);
			buffer[starting_index + 4].SetColor(colors[3]);
			buffer[starting_index + 5].SetColor(colors[2]);
		}

		ECS_TEMPLATE_FUNCTION_2_AFTER(void, SetVertexColorForRectangle, UIVertexColor*, UISpriteVertex*, const Color*, unsigned int);

		template<typename Element>
		void SetColorForRectangle(Color color, unsigned int starting_index, Element* buffer) {
			buffer[starting_index].SetColor(color);
			buffer[starting_index + 1].SetColor(color);
			buffer[starting_index + 2].SetColor(color);
			buffer[starting_index + 3].SetColor(color);
			buffer[starting_index + 4].SetColor(color);
			buffer[starting_index + 5].SetColor(color);
		}

		ECS_TEMPLATE_FUNCTION_2_AFTER(void, SetColorForRectangle, UIVertexColor*, UISpriteVertex*, Color, unsigned int);

		template<typename Element>
		void SetColorForRectangle(
			Color color0,
			Color color1,
			Color color2,
			Color color3,
			Color color4,
			Color color5,
			unsigned int starting_index,
			Element* buffer
		) {
			buffer[starting_index].SetColor(color0);
			buffer[starting_index + 1].SetColor(color1);
			buffer[starting_index + 2].SetColor(color2);
			buffer[starting_index + 3].SetColor(color3);
			buffer[starting_index + 4].SetColor(color4);
			buffer[starting_index + 5].SetColor(color5);
		}

		ECS_TEMPLATE_FUNCTION_2_AFTER(void, SetColorForRectangle, UIVertexColor*, UISpriteVertex*, Color, Color, Color, Color, Color, Color, unsigned int);

		template<typename Element>
		void SetUVForRectangle(
			float2 uv0,
			float2 uv1,
			float2 uv2,
			float2 uv3,
			unsigned int starting_index,
			Element* buffer
		) {
			buffer[starting_index].SetUV(uv0);
			buffer[starting_index + 1].SetUV(uv1);
			buffer[starting_index + 2].SetUV(uv2);
			buffer[starting_index + 3].SetUV(uv1);
			buffer[starting_index + 4].SetUV(uv3);
			buffer[starting_index + 5].SetUV(uv2);
		}

		ECS_TEMPLATE_FUNCTION(void, SetUVForRectangle, float2, float2, float2, float2, unsigned int, UISpriteVertex*);

		template<typename Element>
		void SetUVForRectangle(
			float2 first_uv,
			float2 second_uv,
			unsigned int starting_index,
			Element* buffer
		) {
			buffer[starting_index].SetUV(first_uv);
			buffer[starting_index + 4].SetUV(second_uv);
			float2 uvs[2] = { {first_uv.x, second_uv.y}, {second_uv.x, first_uv.y} };

#pragma region Predication Explination
			/*buffer[starting_index + 1].SetUV(float2(second_uv.x, first_uv.y));
			buffer[starting_index + 2].SetUV(float2(first_uv.x, second_uv.y));
			buffer[starting_index + 3].SetUV(float2(second_uv.x, first_uv.y));
			buffer[starting_index + 5].SetUV(float2(first_uv.x, second_uv.y));

			buffer[starting_index + 1].SetUV(float2(first_uv.x, second_uv.y));
			buffer[starting_index + 2].SetUV(float2(second_uv.x, first_uv.y));
			buffer[starting_index + 3].SetUV(float2(first_uv.x, second_uv.y));
			buffer[starting_index + 5].SetUV(float2(second_uv.x, first_uv.y));*/

			//  0,0 ------------- 1,0
			//   |                 |
			//  0,1 ------------- 1,1
			//
			//  1,1 ------------- 0,1
			//   |                 |
			//  1,0 ------------- 0,0

			//  1,0 -- 1,1
			//   |      |
			//   |      |
			//   |      |
			//  0,0 -- 0,1

			//  0,1 -- 0,0
			//   |      |
			//   |      |
			//   |      |
			//  1,1 -- 1,0

			// index will test to see if the first case occurs
			// otherwise it will do the second one
#pragma endregion

			bool index = false;

			index = (first_uv.x <= second_uv.x && first_uv.y <= second_uv.y) || (first_uv.x >= second_uv.x && first_uv.y >= second_uv.y);
			buffer[starting_index + 1].SetUV(uvs[index]);
			buffer[starting_index + 2].SetUV(uvs[1 - index]);
			buffer[starting_index + 3].SetUV(uvs[index]);
			buffer[starting_index + 5].SetUV(uvs[1 - index]);
		}

		ECS_TEMPLATE_FUNCTION(void, SetUVForRectangle, float2, float2, unsigned int, UISpriteVertex*);

		void SetSolidColorRectangle(
			float2 position,
			float2 scale,
			Color color,
			UIVertexColor* buffer,
			size_t& count
		) {
			SetTransformForRectangle(position, scale, count, buffer);
			SetColorForRectangle(color, count, buffer);
			count += 6;
		}

		void SetSolidColorRectangle(
			float2 position,
			float2 scale,
			Color color,
			void** buffers,
			size_t* counts,
			unsigned int material_offset
		) {
			unsigned int material_index = ECS_TOOLS_UI_SOLID_COLOR + material_offset * ECS_TOOLS_UI_MATERIALS;
			SetTransformForRectangle(position, scale, counts[material_index], (UIVertexColor*)buffers[material_index]);
			SetColorForRectangle(color, counts[material_index], (UIVertexColor*)buffers[material_index]);
			counts[material_index] += 6;
		}

		void SetVertexColorRectangle(
			float2 position,
			float2 scale,
			Color top_left,
			Color top_right,
			Color bottom_left,
			Color bottom_right,
			UIVertexColor* buffer,
			size_t* count
		) {
			SetTransformForRectangle(position, scale, *count, buffer);
			SetVertexColorForRectangle(top_left, top_right, bottom_left, bottom_right, *count, buffer);
			*count += 6;
		}

		void SetVertexColorRectangle(
			float2 position,
			float2 scale,
			Color top_left,
			Color top_right,
			Color bottom_left,
			Color bottom_right,
			void** buffers,
			size_t* counts,
			unsigned int material_offset
		) {
			unsigned int material_index = ECS_TOOLS_UI_SOLID_COLOR + material_offset * ECS_TOOLS_UI_MATERIALS;
			SetTransformForRectangle(position, scale, counts[material_index], (UIVertexColor*)buffers[material_index]);
			SetVertexColorForRectangle(top_left, top_right, bottom_left, bottom_right, counts[material_index], (UIVertexColor*)buffers[material_index]);
			counts[material_index] += 6;
		}

		void SetVertexColorRectangle(
			float2 position,
			float2 scale,
			const Color* colors,
			UIVertexColor* buffer,
			size_t* count
		) {
			SetTransformForRectangle(position, scale, *count, buffer);
			SetVertexColorForRectangle(colors, *count, buffer);
			*count += 6;
		}

		void SetVertexColorRectangle(
			float2 position,
			float2 scale,
			const Color* colors,
			void** buffers,
			size_t* counts,
			unsigned int material_offset
		) {
			unsigned int material_index = ECS_TOOLS_UI_SOLID_COLOR + material_offset * ECS_TOOLS_UI_MATERIALS;
			SetTransformForRectangle(position, scale, counts[material_index], (UIVertexColor*)buffers[material_index]);
			SetVertexColorForRectangle(colors, counts[material_index], (UIVertexColor*)buffers[material_index]);
			counts[material_index] += 6;
		}

		void SetSpriteRectangle(
			float2 position,
			float2 scale,
			Color color,
			float2 top_left_uv,
			float2 bottom_right_uv,
			UISpriteVertex* buffer,
			size_t& count
		) {
			SetTransformForRectangle(position, scale, count, buffer);
			SetColorForRectangle(color, count, buffer);
			SetUVForRectangle(top_left_uv, bottom_right_uv, count, buffer);
			count += 6;
		}

		void SetSpriteRectangle(
			float2 position,
			float2 scale,
			Color color,
			float2 top_left_uv,
			float2 bottom_right_uv,
			void** buffers,
			size_t* counts,
			unsigned int material_index,
			unsigned int material_offset
		) {
			unsigned int compound_index = material_index + material_offset * ECS_TOOLS_UI_MATERIALS;
			SetTransformForRectangle(position, scale, counts[compound_index], (UISpriteVertex*)buffers[compound_index]);
			SetColorForRectangle(color, counts[compound_index], (UISpriteVertex*)buffers[compound_index]);
			SetUVForRectangle(top_left_uv, bottom_right_uv, counts[compound_index], (UISpriteVertex*)buffers[compound_index]);
			counts[compound_index] += 6;
		}

		void SetVertexColorSpriteRectangle(
			float2 position,
			float2 scale,
			const Color* colors,
			float2 top_left_uv,
			float2 bottom_right_uv,
			UISpriteVertex* buffer,
			size_t& count
		) {
			SetTransformForRectangle(position, scale, count, buffer);
			SetVertexColorForRectangle(colors, count, buffer);
			SetUVForRectangle(top_left_uv, bottom_right_uv, count, buffer);
			count += 6;
		}

		void SetVertexColorSpriteRectangle(
			float2 position,
			float2 scale,
			const Color* colors,
			float2 top_left_uv,
			float2 bottom_right_uv,
			void** buffers,
			size_t* counts,
			unsigned int material_index,
			unsigned int material_offset
		) {
			unsigned int compound_index = material_index + material_offset * ECS_TOOLS_UI_MATERIALS;
			SetTransformForRectangle(position, scale, counts[compound_index], (UISpriteVertex*)buffers[compound_index]);
			SetVertexColorForRectangle(colors, counts[compound_index], (UISpriteVertex*)buffers[compound_index]);
			SetUVForRectangle(top_left_uv, bottom_right_uv, counts[compound_index], (UISpriteVertex*)buffers[compound_index]);
			counts[compound_index] += 6;
		}

		void SetVertexColorSpriteRectangle(
			float2 position,
			float2 scale,
			const ColorFloat* colors,
			float2 top_left_uv,
			float2 bottom_right_uv,
			UISpriteVertex* buffer,
			size_t& count
		) {
			SetTransformForRectangle(position, scale, count, buffer);
			SetVertexColorForRectangle(Color(colors[0]), Color(colors[1]), Color(colors[2]), Color(colors[3]), count, buffer);
			SetUVForRectangle(top_left_uv, bottom_right_uv, count, buffer);
			count += 6;
		}

		void SetVertexColorSpriteRectangle(
			float2 position,
			float2 scale,
			const ColorFloat* colors,
			float2 top_left_uv,
			float2 bottom_right_uv,
			void** buffers,
			size_t* counts,
			unsigned int material_index,
			unsigned int material_offset
		) {
			unsigned int compound_index = material_index + material_offset * ECS_TOOLS_UI_MATERIALS;
			SetTransformForRectangle(position, scale, counts[compound_index], (UISpriteVertex*)buffers[compound_index]);
			SetVertexColorForRectangle(Color(colors[0]), Color(colors[1]), Color(colors[2]), Color(colors[3]), counts[compound_index], (UISpriteVertex*)buffers[compound_index]);
			SetUVForRectangle(top_left_uv, bottom_right_uv, counts[compound_index], (UISpriteVertex*)buffers[compound_index]);
			counts[compound_index] += 6;
		}

		bool IsPointInRectangle(
			float2 point_position,
			float2 rectangle_position,
			float2 rectangle_scale
		) {
			return point_position.x >= rectangle_position.x && (point_position.x <= rectangle_position.x + rectangle_scale.x)
				&& point_position.y >= rectangle_position.y && (point_position.y <= rectangle_position.y + rectangle_scale.y);
		}

		bool IsPointInRectangle(
			float2 point_position,
			const UIElementTransform& transform
		) {
			return point_position.x >= transform.position.x && (point_position.x <= transform.position.x + transform.scale.x)
				&& point_position.y >= transform.position.y && (point_position.y <= transform.position.y + transform.scale.y);

		}

		bool IsPointInRectangle(
			float point_position_x,
			float point_position_y,
			float rectangle_position_x,
			float rectangle_position_y,
			float rectangle_scale_x,
			float rectangle_scale_y
		) {
			return point_position_x >= rectangle_position_x && (point_position_x <= rectangle_position_x + rectangle_scale_x)
				&& point_position_y >= rectangle_position_y && (point_position_y <= rectangle_position_y + rectangle_scale_y);
		}

		bool IsRectangleInRectangle(
			const UIElementTransform& first,
			const UIElementTransform& second
		) {
			return first.position.x >= second.position.x && (first.position.x + first.scale.x <= second.position.x + second.scale.x)
				&& first.position.y >= second.position.y && (first.position.y + first.scale.y <= second.position.y + second.scale.y);
		}

		bool IsRectangleInRectangle(
			float2 first_position,
			float2 first_scale,
			float2 second_position,
			float2 second_scale
		) {
			return first_position.x >= second_position.x && (first_position.x + first_scale.x <= second_position.x + second_scale.x)
				&& first_position.y >= second_position.y && (first_position.y + first_scale.y <= second_position.y + second_scale.y);
		}

		bool IsRectangleInRectangle(
			unsigned short first_position_x,
			unsigned short first_position_y,
			unsigned short first_scale_x,
			unsigned short first_scale_y,
			unsigned short second_position_x,
			unsigned short second_position_y,
			unsigned short second_scale_x,
			unsigned short second_scale_y
		) {
			return first_position_x >= second_position_x && (first_position_x + first_scale_x <= second_position_x + second_scale_x)
				&& first_position_y >= second_position_y && (first_position_y + first_scale_y <= second_position_y + second_scale_y);
		}

		template<typename SimdVector>
		bool ECS_VECTORCALL AVX2RectangleOverlapp(
			SimdVector first_position_x,
			SimdVector first_position_y,
			SimdVector first_width,
			SimdVector first_height,
			SimdVector second_position_x,
			SimdVector second_position_y,
			SimdVector second_width,
			SimdVector second_height
		) {
			SimdVector first_right = first_position_x + first_width;
			SimdVector first_bottom = first_position_y + first_height;
			SimdVector second_right = second_position_x + second_width;
			SimdVector second_bottom = second_position_y + second_height;
			return horizontal_count(((first_right >= second_position_x) && (first_right <= second_right)) || ((first_position_x <= second_right) && (first_position_x >= second_position_x))
				|| ((first_bottom >= second_position_y) && (first_bottom <= second_bottom)) || (((first_position_y <= second_bottom) && (first_position_y >= second_position_y)))) > 0;
		}

		ECS_TEMPLATE_FUNCTION(bool, AVX2RectangleOverlapp, Vec8f, Vec8f, Vec8f, Vec8f, Vec8f, Vec8f, Vec8f, Vec8f);
		ECS_TEMPLATE_FUNCTION(bool, AVX2RectangleOverlapp, Vec16us, Vec16us, Vec16us, Vec16us, Vec16us, Vec16us, Vec16us, Vec16us);

		template<typename SimdVector>
		int ECS_VECTORCALL AVX2IsPointInRectangle(
			SimdVector point_position_x,
			SimdVector point_position_y,
			SimdVector rectangle_position_x,
			SimdVector rectangle_position_y,
			SimdVector rectangle_scale_x,
			SimdVector rectangle_scale_y
		) {
			return horizontal_find_first(
				point_position_x >= rectangle_position_x && point_position_x <= (rectangle_position_x + rectangle_scale_x) &&
				point_position_y >= rectangle_position_y && point_position_y <= (rectangle_position_y + rectangle_scale_y)
			);
		}

		ECS_TEMPLATE_FUNCTION(int, AVX2IsPointInRectangle, Vec8f, Vec8f, Vec8f, Vec8f, Vec8f, Vec8f);
		ECS_TEMPLATE_FUNCTION(int, AVX2IsPointInRectangle, Vec16us, Vec16us, Vec16us, Vec16us, Vec16us, Vec16us);

		template<typename Stream>
		void AlignVerticalText(Stream& vertices) {
			float left_x = vertices[0].position.x;
			float right_x = vertices[1].position.x;
			for (size_t index = 7; index < vertices.size; index += 6) {
				//left_x = function::PredicateValue(left_x > vertices[index - 1].position.x, vertices[index - 1].position.x, left_x);
				right_x = function::PredicateValue(right_x < vertices[index].position.x, vertices[index].position.x, right_x);
			}
			float x_span = right_x - left_x;
			for (size_t index = 0; index < vertices.size; index += 6) {
				float sprite_span = vertices[index + 1].position.x - vertices[index].position.x;
				if (sprite_span < x_span) {
					float translation = (x_span - sprite_span) * 0.5f;
					vertices[index].position.x += translation;
					vertices[index + 1].position.x += translation;
					vertices[index + 2].position.x += translation;
					vertices[index + 3].position.x += translation;
					vertices[index + 4].position.x += translation;
					vertices[index + 5].position.x += translation;
				}
			}
		}

		ECS_TEMPLATE_FUNCTION(void, AlignVerticalText, Stream<UISpriteVertex>&);
		ECS_TEMPLATE_FUNCTION(void, AlignVerticalText, CapacityStream<UISpriteVertex>&);

		template<typename Element>
		float GetMinX(const Element* element, size_t size) {
			float min = element[0].position.x;
			for (size_t index = 1; index < size; index++) {
				min = function::PredicateValue(min > element[index].position.x, element[index].position.x, min);
			}
			return min;
		}

		ECS_TEMPLATE_FUNCTION_2_BEFORE(float, GetMinX, const UIVertexColor*, const UISpriteVertex*, size_t);

		template<typename Element>
		float GetMinXRectangle(const Element* element, size_t size) {
			float min = element[0].position.x;
			for (size_t index = 6; index < size; index += 6) {
				min = function::PredicateValue(min > element[index].position.x, element[index].position.x, min);
			}
			return min;
		}

		ECS_TEMPLATE_FUNCTION_2_BEFORE(float, GetMinXRectangle, const UIVertexColor*, const UISpriteVertex*, size_t);

		template<typename Element>
		float GetMaxX(const Element* element, size_t size) {
			float max = element[0].position.x;
			for (size_t index = 1; index < size; index++) {
				max = function::PredicateValue(max < element[index].position.x, element[index].position.x, max);
			}
			return max;
		}

		ECS_TEMPLATE_FUNCTION_2_BEFORE(float, GetMaxX, const UIVertexColor*, const UISpriteVertex*, size_t);

		template<typename Element>
		float GetMaxXRectangle(const Element* element, size_t size) {
			float max = element[1].position.x;
			for (size_t index = 7; index < size; index += 6) {
				max = function::PredicateValue(max < element[index].position.x, element[index].position.x, max);
			}
			return max;
		}

		ECS_TEMPLATE_FUNCTION_2_BEFORE(float, GetMaxXRectangle, const UIVertexColor*, const UISpriteVertex*, size_t);

		template<typename Element>
		float GetMinY(const Element* element, size_t size) {
			float min = element[0].position.y;
			for (size_t index = 1; index < size; index++) {
				min = function::PredicateValue(min > element[index].position.y, element[index].position.y, min);
			}
			return min;
		}

		ECS_TEMPLATE_FUNCTION_2_BEFORE(float, GetMinY, const UIVertexColor*, const UISpriteVertex*, size_t);

		template<typename Element>
		float GetMinYRectangle(const Element* element, size_t size) {
			float min = element[0].position.y;
			for (size_t index = 6; index < size; index += 6) {
				min = function::PredicateValue(min > element[index].position.y, element[index].position.y, min);
			}
			return min;
		}

		ECS_TEMPLATE_FUNCTION_2_BEFORE(float, GetMinYRectangle, const UIVertexColor*, const UISpriteVertex*, size_t);

		template<typename Element>
		float GetMaxY(const Element* element, size_t size) {
			float max = element[0].position.y;
			for (size_t index = 1; index < size; index++) {
				max = function::PredicateValue(max > element[index].position.y, element[index].position.y, max);
			}
			return max;
		}

		ECS_TEMPLATE_FUNCTION_2_BEFORE(float, GetMaxY, const UIVertexColor*, const UISpriteVertex*, size_t);

		template<typename Element>
		float GetMaxYRectangle(const Element* element, size_t size) {
			float max = element[2].position.y;
			for (size_t index = 8; index < size; index += 6) {
				max = function::PredicateValue(max > element[index].position.y, element[index].position.y, max);
			}
			return max;
		}

		ECS_TEMPLATE_FUNCTION_2_BEFORE(float, GetMaxYRectangle, const UIVertexColor*, const UISpriteVertex*, size_t);

		float2 ExpandRectangle(
			float2 position,
			float2 scale,
			float2 new_scale
		) {
			float x_scale_difference_half = (new_scale.x - scale.x) * 0.5f;
			float y_scale_difference_half = (new_scale.y - scale.y) * 0.5f;
			return float2(
				position.x - x_scale_difference_half,
				position.y - y_scale_difference_half
			);
		}

		float2 ExpandRectangle(
			float2 position,
			float2 scale,
			float2 resize_factor,
			float2& new_scale
		) {
			new_scale = { scale.x * resize_factor.x, scale.y * resize_factor.y };
			return ExpandRectangle(position, scale, new_scale);
		}

		float2 ExpandRectangle(
			float2 position,
			float2 scale,
			float new_scale_factor
		) {
			return float2(
				position.x - scale.x * (new_scale_factor - 1.0f) * 0.5f,
				position.y - scale.y * (new_scale_factor - 1.0f) * 0.5f
			);
		}

		void ExpandRectangle(float2* element_transforms, float2 new_scale_factor, size_t count) {
			for (size_t index = 0; index < count; index++) {
				unsigned int scale_index = index * 2 + 1;
				float2 old_scale = element_transforms[scale_index];
				element_transforms[scale_index - 1] = ExpandRectangle(element_transforms[scale_index - 1], old_scale, new_scale_factor, element_transforms[scale_index]);
			}
		}

		void ExpandRectangle(float2* element_transforms, const float2* new_scale_factor, size_t count) {
			for (size_t index = 0; index < count; index++) {
				unsigned int scale_index = index * 2 + 1;
				float2 old_scale = element_transforms[scale_index];
				element_transforms[scale_index - 1] = ExpandRectangle(element_transforms[scale_index - 1], old_scale, new_scale_factor[index], element_transforms[scale_index]);
			}
		}

		void ExpandRectangle(Stream<float2> element_transforms, float2 new_scale_factor) {
			ExpandRectangle(element_transforms.buffer, new_scale_factor, element_transforms.size);
		}

		void ExpandRectangle(Stream<float2> element_transforms, const float2* new_scale_factor) {
			ExpandRectangle(element_transforms.buffer, new_scale_factor, element_transforms.size);
		}

		template<typename Value>
		Value AlignMiddle(Value first, Value external_scale, Value element_scale) {
			return first + (external_scale - element_scale) * 0.5f;
		}

		ECS_TEMPLATE_FUNCTION(float, AlignMiddle, float, float, float);
		ECS_TEMPLATE_FUNCTION(double, AlignMiddle, double, double, double);

		void GetEncompassingRectangle(Stream<float2> values, float2* results) {
			float min_x = 5.0f, min_y = 5.0f, max_x = -5.0f, max_y = -5.0f;
			for (size_t index = 0; index < values.size; index++) {
				min_x = function::PredicateValue(values[index].x < min_x, values[index].x, min_x);
				min_y = function::PredicateValue(values[index].y < min_y, values[index].y, min_y);
				max_x = function::PredicateValue(values[index].x > max_x, values[index].x, max_x);
				max_y = function::PredicateValue(values[index].y > max_y, values[index].y, max_y);
			}
			results[0] = float2(min_x, min_y);
			results[1] = float2(max_x - min_x, max_y - min_y);
		}

		float2 CenterRectangle(float2 scale, float2 point) {
			return float2(point.x - scale.x * 0.5f, point.y - scale.y * 0.5f);
		}

		void CreateRectangleBorder(float2 position, float2 scale, float2 border_scale, float2* results) {
			// top
			results[0] = position;
			results[1] = { scale.x, border_scale.y };

			// left
			results[2] = position;
			results[3] = { border_scale.x, scale.y };

			// bottom
			results[4] = { position.x, position.y + scale.y - border_scale.y };
			results[5] = { scale.x, border_scale.y };

			// right
			results[6] = { position.x + scale.x - border_scale.x, position.y };
			results[7] = { border_scale.x, scale.y };
		}

		void CreateRectangleOuterBorder(float2 position, float2 scale, float2 border_scale, float2* results) {
			// top 
			results[0] = { position.x - border_scale.x, position.y - border_scale.y };
			results[1] = { scale.x + border_scale.x * 2.0f, border_scale.y };

			// left
			results[2] = { position.x - border_scale.x, position.y - border_scale.y };
			results[3] = { border_scale.x, scale.y + 2.0f * border_scale.y };

			// bottom
			results[4] = { position.x - border_scale.x, position.y + scale.y };
			results[5] = { scale.x + border_scale.x * 2.0f, border_scale.y };

			// right
			results[6] = { position.x + scale.x, position.y - border_scale.y };
			results[7] = { border_scale.x, scale.y + 2.0f * border_scale.y };
		}

		template<bool is_inner = true>
		void CreateSolidColorRectangleBorder(
			float2 position,
			float2 scale,
			float2 border_scale,
			Color color,
			size_t* counts,
			void** buffers
		) {
			float2 results[8];
			UIVertexColor* solid_color = (UIVertexColor*)buffers[ECS_TOOLS_UI_SOLID_COLOR];
			if constexpr (is_inner)
				CreateRectangleBorder(position, scale, border_scale, results);
			else
				CreateRectangleOuterBorder(position, scale, border_scale, results);

			// top
			SetSolidColorRectangle(
				results[0],
				results[1],
				color,
				solid_color,
				counts[ECS_TOOLS_UI_SOLID_COLOR]
			);

			// left
			SetSolidColorRectangle(
				results[2],
				results[3],
				color,
				solid_color,
				counts[ECS_TOOLS_UI_SOLID_COLOR]
			);

			// bottom
			SetSolidColorRectangle(
				results[4],
				results[5],
				color,
				solid_color,
				counts[ECS_TOOLS_UI_SOLID_COLOR]
			);

			// right
			SetSolidColorRectangle(
				results[6],
				results[7],
				color,
				solid_color,
				counts[ECS_TOOLS_UI_SOLID_COLOR]
			);
		}

		template void ECSENGINE_API CreateSolidColorRectangleBorder<true>(float2, float2, float2, Color, size_t*, void**);
		template void ECSENGINE_API CreateSolidColorRectangleBorder<false>(float2, float2, float2, Color, size_t*, void**);

		template<bool is_inner = true>
		void CreateSolidColorRectangleBorder(
			float2 position,
			float2 scale,
			float2 border_scale,
			Color color,
			size_t* counts,
			void** buffers,
			float2* results
		) {
			UIVertexColor* solid_color = (UIVertexColor*)buffers[ECS_TOOLS_UI_SOLID_COLOR];
			if constexpr (is_inner)
				CreateRectangleBorder(position, scale, border_scale, results);
			else
				CreateRectangleOuterBorder(position, scale, border_scale, results);

			// top
			SetSolidColorRectangle(
				results[0],
				results[1],
				color,
				solid_color,
				counts[ECS_TOOLS_UI_SOLID_COLOR]
			);

			// left
			SetSolidColorRectangle(
				results[2],
				results[3],
				color,
				solid_color,
				counts[ECS_TOOLS_UI_SOLID_COLOR]
			);

			// bottom
			SetSolidColorRectangle(
				results[4],
				results[5],
				color,
				solid_color,
				counts[ECS_TOOLS_UI_SOLID_COLOR]
			);

			// right
			SetSolidColorRectangle(
				results[6],
				results[7],
				color,
				solid_color,
				counts[ECS_TOOLS_UI_SOLID_COLOR]
			);
		}

		template void ECSENGINE_API CreateSolidColorRectangleBorder<false>(float2, float2, float2, Color, size_t*, void**, float2*);
		template void ECSENGINE_API CreateSolidColorRectangleBorder<true>(float2, float2, float2, Color, size_t*, void**, float2*);

		void DrawExpandedSolidColorRectangle(
			float2 position,
			float2 scale,
			float2 new_scale_factor,
			Color color,
			size_t* counts,
			void** buffers
		) {
			float2 old_scale = scale;
			float2 new_position = ExpandRectangle(position, old_scale, new_scale_factor, scale);
			UIVertexColor* solid_color = (UIVertexColor*)buffers[ECS_TOOLS_UI_SOLID_COLOR];
			SetSolidColorRectangle(new_position, scale, color, solid_color, counts[ECS_TOOLS_UI_SOLID_COLOR]);
		}

		template<typename Stream>
		float2 GetPositionFromLastRectangle(Stream vertices) {
			return { vertices[vertices.size - 6].position.x, -vertices[vertices.size - 6].position.y };
		}

		ECS_TEMPLATE_FUNCTION_4_BEFORE(float2, GetPositionFromLastRectangle, Stream<UIVertexColor>, Stream<UISpriteVertex>, CapacityStream<UIVertexColor>, CapacityStream<UISpriteVertex>);

		template<typename Stream>
		float2 GetScaleFromLastRectangle(Stream vertices) {
			return {
				vertices[vertices.size - 5].position.x - vertices[vertices.size - 6].position.x,
				vertices[vertices.size - 3].position.y - vertices[vertices.size - 1].position.y
			};
		}

		ECS_TEMPLATE_FUNCTION_4_BEFORE(float2, GetScaleFromLastRectangle, Stream<UIVertexColor>, Stream<UISpriteVertex>, CapacityStream<UIVertexColor>, CapacityStream<UISpriteVertex>);

		template<typename Stream>
		void TranslateTextX(float new_x, unsigned int min_x_index, Stream& stream) {
			float translation = new_x - stream[min_x_index].position.x;
			for (size_t index = 0; index < stream.size; index++) {
				stream[index].position.x += translation;
			}
		}

		ECS_TEMPLATE_FUNCTION_4_AFTER(void, TranslateTextX, Stream<UIVertexColor>&, Stream<UISpriteVertex>&, CapacityStream<UIVertexColor>&, CapacityStream<UISpriteVertex>&, float, unsigned int);
	
		template<typename Stream>
		void TranslateTextY(float new_y, unsigned int min_y_index, Stream& stream) {
			float translation = -new_y - stream[min_y_index].position.y;
			for (size_t index = 0; index < stream.size; index++) {
				stream[index].position.y += translation;
			}
		}

		ECS_TEMPLATE_FUNCTION_4_AFTER(void, TranslateTextY, Stream<UIVertexColor>&, Stream<UISpriteVertex>&, CapacityStream<UIVertexColor>&, CapacityStream<UISpriteVertex>&, float, unsigned int);


		template<typename Stream>
		void TranslateText(float x_position, float y_position, Stream& stream) {
			float x_minimum = 2.0f, y_minimum = -2.0f;
			unsigned int x_min_index = 0, y_min_index = 0;
			for (size_t index = 0; index < stream.size; index += 6) {
				if (x_minimum > stream[index].position.x) {
					x_minimum = stream[index].position.x;
					x_min_index = index;
				}
				if (y_minimum < stream[index].position.y) {
					y_minimum = stream[index].position.y;
					y_min_index = index;
				}
			}
			TranslateText(x_position, y_position, stream, x_min_index, y_min_index);
		}

		ECS_TEMPLATE_FUNCTION_4_AFTER(void, TranslateText, Stream<UIVertexColor>&, Stream<UISpriteVertex>&, CapacityStream<UIVertexColor>&, CapacityStream<UISpriteVertex>&, float, float);

		template<typename Stream>
		void TranslateText(float x_position, float y_position, float x_minimum, float y_minimum, Stream& stream) {
			float x_translation, y_translation;
			x_translation = x_position - x_minimum;
			y_translation = -y_position - y_minimum;
			if (x_translation != 0.0f || y_translation != 0.0f) {
				for (size_t index = 0; index < stream.size; index++) {
					stream[index].position.x += x_translation;
					stream[index].position.y += y_translation;
				}
			}
		}

		ECS_TEMPLATE_FUNCTION_4_AFTER(void, TranslateText, Stream<UIVertexColor>&, Stream<UISpriteVertex>&, CapacityStream<UIVertexColor>&, CapacityStream<UISpriteVertex>&, float, float, float, float);

		template<typename Stream>
		void ScaleTextX(
			const Stream& input,
			UISpriteVertex* output_buffer,
			size_t* output_count,
			bool is_vertical,
			bool is_inverted,
			float inverted_current_scale,
			float new_scale,
			float character_spacing
		) {
			if (!is_vertical) {
				float x_position = function::PredicateValue(
					is_inverted,
					input[input.size - 1].position.x,
					input[0].position.x
				);

				auto kernel = [&](int64_t index) {
					float x_scale = input[index + 1].position.x - input[index].position.x;
					float new_x_scale = x_scale * new_scale * inverted_current_scale;
					SetSpriteRectangle(
						{ x_position, -input[index].position.y },
						{ new_x_scale, input[index].position.y - input[index + 2].position.y },
						input[index].color,
						input[index].uvs,
						input[index + 4].uvs,
						output_buffer,
						*output_count
					);

					x_position += new_x_scale + character_spacing;
				};
				if (!is_inverted) {
					for (int64_t index = 0; index < input.size; index += 6) {
						kernel(index);
					}
				}
				else {
					for (int64_t index = input.size - 6; index >= 0; index -= 6) {
						kernel(index);
					}
				}
			}
			else {
				auto kernel = [&](int64_t index) {
					float x_scale = input[index + 1].position.x - input[index].position.x;
					float new_x_scale = x_scale * new_scale * inverted_current_scale;
					SetSpriteRectangle(
						{ input[index].position.x, -input[index].position.y },
						{ new_x_scale, input[index].position.y - input[index + 2].position.y },
						input[index].color,
						input[index].uvs,
						input[index + 4].uvs,
						output_buffer,
						*output_count
					);
				};
				for (size_t index = 0; index < input.size; index += 6) {
					kernel(index);
				}
			}
		}

		ECS_TEMPLATE_FUNCTION_2_BEFORE(void, ScaleTextX, const Stream<UISpriteVertex>&, const CapacityStream<UISpriteVertex>&, UISpriteVertex*, size_t*, bool, bool, float, float, float);

		template<typename Stream>
		void ScaleTextY(
			const Stream& input,
			UISpriteVertex* output_buffer,
			size_t* output_count,
			bool is_vertical,
			bool is_inverted,
			float inverted_current_scale,
			float new_scale,
			float character_spacing
		) {
			float y_scale = input[0].position.y - input[2].position.y;
			float new_y_scale = y_scale * new_scale * inverted_current_scale;

			if (!is_vertical) {
				auto kernel = [&](int64_t index) {
					SetSpriteRectangle(
						{ input[index].position.x, -input[index].position.y },
						{ input[index + 1].position.x - input[index].position.x, new_y_scale },
						input[index].color,
						input[index].uvs,
						input[index + 4].uvs,
						output_buffer,
						*output_count
					);
				};

				for (int64_t index = 0; index < input.size; index += 6) {
					kernel(index);
				}
			}
			else {
				float y_position = -function::PredicateValue(
					is_inverted,
					input[input.size - 3].position.y,
					input[0].position.y
				);
				float new_character_spacing = character_spacing * 0.1f;

				auto kernel = [&](int64_t index) {
					SetSpriteRectangle(
						{ input[index].position.x, y_position },
						{ input[index + 1].position.x - input[index].position.x, new_y_scale },
						input[index].color,
						input[index].uvs,
						input[index + 4].uvs,
						output_buffer,
						*output_count
					);

					y_position += new_y_scale + new_character_spacing;
				};

				if (is_inverted) {
					for (int64_t index = input.size - 6; index >= 0; index -= 6) {
						kernel(index);
					}
				}
				else {
					for (int64_t index = 0; index < input.size; index += 6) {
						kernel(index);
					}
				}
			}
		}
	
		ECS_TEMPLATE_FUNCTION_2_BEFORE(void, ScaleTextY, const Stream<UISpriteVertex>&, const CapacityStream<UISpriteVertex>&, UISpriteVertex*, size_t*, bool, bool, float, float, float);
	
		template<typename Stream>
		void ScaleTextXY(
			const Stream& input,
			UISpriteVertex* output_buffer,
			size_t* output_count,
			bool is_vertical,
			bool is_inverted,
			float2 inverted_current_scale,
			float2 new_scale,
			float character_spacing
		) {
			float y_scale = input[0].position.y - input[2].position.y;
			float new_y_scale = y_scale * new_scale.y * inverted_current_scale.y;

			if (!is_vertical) {
				float x_position = function::PredicateValue(
					is_inverted,
					input[input.size - 1].position.x,
					input[0].position.x
				);

				auto kernel = [&](int64_t index) {
					float x_scale = input[index + 1].position.x - input[index].position.x;
					float new_x_scale = x_scale * new_scale.x * inverted_current_scale.x;
					SetSpriteRectangle(
						{ x_position, -input[index].position.y },
						{ new_x_scale, new_y_scale },
						input[index].color,
						input[index].uvs,
						input[index + 4].uvs,
						output_buffer,
						*output_count
					);

					x_position += new_x_scale + character_spacing;
				};

				if (is_inverted) {
					for (int64_t index = input.size - 6; index >= 0; index -= 6) {
						kernel(index);
					}
				}
				else {
					for (int64_t index = 0; index < input.size; index += 6) {
						kernel(index);
					}
				}
			}
			else {
				float y_position = -function::PredicateValue(
					is_inverted,
					input[input.size - 3].position.y,
					input[0].position.y
				);
				float new_character_spacing = character_spacing * 0.1f;

				auto kernel = [&](int64_t index) {
					float x_scale = input[index + 1].position.x - input[index].position.x;
					float new_x_scale = x_scale * new_scale.x * inverted_current_scale.x;
					SetSpriteRectangle(
						{ input[index].position.x, y_position },
						{ new_x_scale, new_y_scale },
						input[index].color,
						input[index].uvs,
						input[index + 4].uvs,
						output_buffer,
						*output_count
					);

					y_position += new_y_scale + new_character_spacing;
				};

				if (is_inverted) {
					for (int64_t index = input.size - 6; index >= 0; index -= 6) {
						kernel(index);
					}
				}
				else {
					for (int64_t index = 0; index < input.size; index += 6) {
						kernel(index);
					}
				}
			}
		}

		ECS_TEMPLATE_FUNCTION_2_BEFORE(void, ScaleTextXY, const Stream<UISpriteVertex>&, const CapacityStream<UISpriteVertex>&, UISpriteVertex*, size_t*, bool, bool, float2, float2, float);

		template<typename Stream>
		void ScaleText(
			Stream& input,
			float2 input_zoom,
			float2 input_inverse_zoom,
			UISpriteVertex* output_buffer,
			size_t* output_count,
			const float2* zoom_ptr,
			float character_spacing
		) {
			bool is_x_different = input_zoom.x != zoom_ptr->x;
			bool is_y_different = input_zoom.y != zoom_ptr->y;

			bool is_vertical = input[6].position.y != input[0].position.y;
			bool is_inverted = false;
			if (is_vertical && input[0].position.y < input[input.size - 1].position.y) {
				is_inverted = true;
			}
			else if (input[0].position.x > input[input.size - 1].position.x) {
				is_inverted = true;
			}

			if (is_x_different && !is_y_different) {
				ScaleTextX(input, output_buffer, output_count, is_vertical, is_inverted, input_inverse_zoom.x, zoom_ptr->x, character_spacing);
			}
			else if (is_x_different && is_y_different) {
				ScaleTextXY(input, output_buffer, output_count, is_vertical, is_inverted, input_inverse_zoom, { zoom_ptr->x, zoom_ptr->y }, character_spacing);
			}
			else if (is_y_different) {
				ScaleTextY(input, output_buffer, output_count, is_vertical, is_inverted, input_inverse_zoom.y, zoom_ptr->y, character_spacing);
			}
		}

		ECS_TEMPLATE_FUNCTION_2_BEFORE(void, ScaleText, Stream<UISpriteVertex>&, CapacityStream<UISpriteVertex>&, float2, float2, UISpriteVertex*, size_t*, const float2*, float);


		size_t ParseStringIdentifier(const char* string, size_t string_length) {
			for (int64_t index = string_length - 1; index >= ECS_TOOLS_UI_DRAWER_STRING_PATTERN_COUNT - 1; index--) {
				size_t count = 0;
				for (; count < ECS_TOOLS_UI_DRAWER_STRING_PATTERN_COUNT && string[index] == ECS_TOOLS_UI_DRAWER_STRING_PATTERN_CHAR && index >= 0; count++, index--) {}
				if (count == ECS_TOOLS_UI_DRAWER_STRING_PATTERN_COUNT) {
					return (size_t)(index + 1);
				}
			}
			return string_length;
		}

		bool IsClickableTrigger(ActionData* action_data)
		{
			return action_data->mouse_tracker->LeftButton() == MBRELEASED && IsPointInRectangle(action_data->mouse_position, action_data->position, action_data->scale);
		}

		template<typename Buffer>
		float2 GetSectionYBounds(const Buffer* buffer, size_t starting_index, size_t end_index) {
			float2 bounds = { -10.0f, 10.0f };
			for (size_t index = starting_index; index < end_index; index++) {
				bounds.x = function::PredicateValue(bounds.x < buffer[index].position.y, buffer[index].position.y, bounds.x);
				bounds.y = function::PredicateValue(bounds.y > buffer[index].position.y, buffer[index].position.y, bounds.y);
			}

			bounds.x = -bounds.x;
			bounds.y = -bounds.y;
			return bounds;
		}

		ECS_TEMPLATE_FUNCTION_2_BEFORE(float2, GetSectionYBounds, const UIVertexColor*, const UISpriteVertex*, size_t, size_t);
	
		template<typename Buffer>
		float2 GetRectangleSectionYBounds(const Buffer* buffer, size_t starting_index, size_t end_index) {
			float2 bounds = { -10.0f, 10.0f };
			for (size_t index = starting_index; index < end_index; index += 6) {
				bounds.x = function::PredicateValue(bounds.x < buffer[index].position.y, buffer[index].position.y, bounds.x);
				bounds.y = function::PredicateValue(bounds.y > buffer[index + 2].position.y, buffer[index + 2].position.y, bounds.y);
			}

			bounds.x = -bounds.x;
			bounds.y = -bounds.y;
			return bounds;
		}

		ECS_TEMPLATE_FUNCTION_2_BEFORE(float2, GetRectangleSectionYBounds, const UIVertexColor*, const UISpriteVertex*, size_t, size_t);

	}

}