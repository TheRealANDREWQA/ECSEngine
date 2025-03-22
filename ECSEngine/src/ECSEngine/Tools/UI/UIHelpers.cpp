#include "ecspch.h"
#include "UIHelpers.h"
#include "../../Input/Mouse.h"
#include "../../Input/Keyboard.h"

namespace ECSEngine {

	namespace Tools {

		// -------------------------------------------------------------------------------------------------------

		template<typename Element>
		void SetTransformForRectangle(
			float2 position,
			float2 scale,
			unsigned int starting_index,
			Element* buffer
		) {
			buffer[starting_index].SetTransform({ position.x, position.y });                            // top left
			buffer[starting_index + 1].SetTransform({ position.x + scale.x, position.y });              // top right
			buffer[starting_index + 2].SetTransform({ position.x, position.y + scale.y });            // lower left
			buffer[starting_index + 3].SetTransform({ position.x + scale.x, position.y });              // top right
			buffer[starting_index + 4].SetTransform({ position.x + scale.x, position.y + scale.y });  // lower right
			buffer[starting_index + 5].SetTransform({ position.x, position.y + scale.y });            // lower left
		}

		ECS_TEMPLATE_FUNCTION_2_AFTER(void, SetTransformForRectangle, UIVertexColor*, UISpriteVertex*, float2, float2, unsigned int);

		// -------------------------------------------------------------------------------------------------------

		template<typename VertexType>
		static Rectangle2D GetRectangleFromVerticesImpl(const VertexType* vertices) {
			Rectangle2D rectangle;
			rectangle.top_left = vertices[0].position;
			rectangle.bottom_right = vertices[4].position;
			// Invert the y positions
			rectangle.top_left.y = -rectangle.top_left.y;
			rectangle.bottom_right.y = -rectangle.bottom_right.y;
			return rectangle;
		}

		Rectangle2D GetRectangleFromVertices(const UIVertexColor* vertices) {
			return GetRectangleFromVerticesImpl(vertices);
		}

		Rectangle2D GetRectangleFromVertices(const UISpriteVertex* vertices) {
			return GetRectangleFromVerticesImpl(vertices);
		}

		// -------------------------------------------------------------------------------------------------------

		void SetTransformForLine(float2 position1, float2 position2, CapacityStream<void> buffer) {
			CapacityStream<UIVertexColor> vertices = buffer.AsIs<UIVertexColor>();
			vertices[vertices.size - 2].SetTransform(position1);
			vertices[vertices.size - 1].SetTransform(position2);
		}

		// -------------------------------------------------------------------------------------------------------

		void SetTransformForLine(float2 position1, float2 position2, Stream<CapacityStream<void>> buffers, unsigned int material_offset) {
			SetTransformForLine(
				position1,
				position2,
				buffers[ECS_TOOLS_UI_LINE + ECS_TOOLS_UI_MATERIALS * material_offset]
			);
		}

		// -------------------------------------------------------------------------------------------------------

		void SetColorForLine(Color color, CapacityStream<void> buffer) {
			CapacityStream<UIVertexColor> vertices = buffer.AsIs<UIVertexColor>();
			vertices[vertices.size - 2].SetColor(color);
			vertices[vertices.size - 1].SetColor(color);
		}

		// -------------------------------------------------------------------------------------------------------

		void SetColorForLine(Color color, Stream<CapacityStream<void>> buffers, unsigned int material_offset) {
			SetColorForLine(
				color,
				buffers[ECS_TOOLS_UI_LINE + ECS_TOOLS_UI_MATERIALS * material_offset]
			);
		}

		// -------------------------------------------------------------------------------------------------------

		void SetLine(float2 position1, float2 position2, Color color, CapacityStream<void>& buffer) {
			buffer.AssertCapacity(2);
			buffer.size += 2;
			SetTransformForLine(position1, position2, buffer);
			SetColorForLine(color, buffer);
		}

		// -------------------------------------------------------------------------------------------------------

		void SetLine(float2 position1, float2 position2, Color color, Stream<CapacityStream<void>> buffers, unsigned int material_offset) {
			SetLine(position1, position2, color, buffers[ECS_TOOLS_UI_LINE + ECS_TOOLS_UI_MATERIALS * material_offset]);
		}

		// -------------------------------------------------------------------------------------------------------

		float GetDockspaceMaskFromType(DockspaceType type)
		{
			const float masks[4] = { 1.0f, 0.0f, 1.0f, 0.0f };
			return masks[(unsigned int)type];
		}

		// -------------------------------------------------------------------------------------------------------

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

		// -------------------------------------------------------------------------------------------------------

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
		
		// -------------------------------------------------------------------------------------------------------

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

		// -------------------------------------------------------------------------------------------------------

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

		// -------------------------------------------------------------------------------------------------------

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

		// -------------------------------------------------------------------------------------------------------

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

#pragma region Predication Explanation
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

		// -------------------------------------------------------------------------------------------------------

		Rectangle2D GetUVFromVertices(const UISpriteVertex* vertices)
		{
			return { vertices[0].uvs, vertices[4].uvs };
		}

		void SetSolidColorRectangle(
			float2 position,
			float2 scale,
			Color color,
			CapacityStream<void>& buffer
		) {
			buffer.AssertCapacity(6);
			SetTransformForRectangle(position, scale, buffer.size, (UIVertexColor*)buffer.buffer);
			SetColorForRectangle(color, buffer.size, (UIVertexColor*)buffer.buffer);
			buffer.size += 6;
		}

		// -------------------------------------------------------------------------------------------------------

		void SetSolidColorRectangle(
			float2 position,
			float2 scale,
			Color color,
			Stream<CapacityStream<void>> buffers,
			unsigned int material_offset
		) {
			unsigned int material_index = ECS_TOOLS_UI_SOLID_COLOR + material_offset * ECS_TOOLS_UI_MATERIALS;
			SetSolidColorRectangle(position, scale, color, buffers[material_index]);
		}

		// -------------------------------------------------------------------------------------------------------

		void SetVertexColorRectangle(
			float2 position,
			float2 scale,
			Color top_left,
			Color top_right,
			Color bottom_left,
			Color bottom_right,
			CapacityStream<void>& buffer
		) {
			buffer.AssertCapacity(6);
			SetTransformForRectangle(position, scale, buffer.size, (UIVertexColor*)buffer.buffer);
			SetVertexColorForRectangle(top_left, top_right, bottom_left, bottom_right, buffer.size, (UIVertexColor*)buffer.buffer);
			buffer.size += 6;
		}

		// -------------------------------------------------------------------------------------------------------

		void SetVertexColorRectangle(
			float2 position,
			float2 scale,
			Color top_left,
			Color top_right,
			Color bottom_left,
			Color bottom_right,
			Stream<CapacityStream<void>> buffers,
			unsigned int material_offset
		) {
			unsigned int material_index = ECS_TOOLS_UI_SOLID_COLOR + material_offset * ECS_TOOLS_UI_MATERIALS;
			SetVertexColorRectangle(position, scale, top_left, top_right, bottom_left, bottom_right, buffers[material_index]);
		}

		// -------------------------------------------------------------------------------------------------------

		void SetVertexColorRectangle(
			float2 position,
			float2 scale,
			const Color* colors,
			CapacityStream<void>& buffer
		) {
			buffer.AssertCapacity(6);
			SetTransformForRectangle(position, scale, buffer.size, (UIVertexColor*)buffer.buffer);
			SetVertexColorForRectangle(colors, buffer.size, (UIVertexColor*)buffer.buffer);
			buffer.size += 6;
		}

		// -------------------------------------------------------------------------------------------------------

		void SetVertexColorRectangle(
			float2 position,
			float2 scale,
			const Color* colors,
			Stream<CapacityStream<void>> buffers,
			unsigned int material_offset
		) {
			unsigned int material_index = ECS_TOOLS_UI_SOLID_COLOR + material_offset * ECS_TOOLS_UI_MATERIALS;
			SetVertexColorRectangle(position, scale, colors, buffers[material_index]);
		}

		// -------------------------------------------------------------------------------------------------------

		void SetSpriteRectangle(
			float2 position,
			float2 scale,
			Color color,
			float2 top_left_uv,
			float2 bottom_right_uv,
			CapacityStream<void>& buffer
		) {
			buffer.AssertCapacity(6);
			SetTransformForRectangle(position, scale, buffer.size, (UISpriteVertex*)buffer.buffer);
			SetColorForRectangle(color, buffer.size, (UISpriteVertex*)buffer.buffer);
			SetUVForRectangle(top_left_uv, bottom_right_uv, buffer.size, (UISpriteVertex*)buffer.buffer);
			buffer.size += 6;
		}

		// -------------------------------------------------------------------------------------------------------

		void SetSpriteRectangle(
			float2 position,
			float2 scale,
			Color color,
			float2 top_left_uv,
			float2 bottom_right_uv,
			Stream<CapacityStream<void>> buffers,
			unsigned int material_index,
			unsigned int material_offset
		) {
			unsigned int compound_index = material_index + material_offset * ECS_TOOLS_UI_MATERIALS;
			SetSpriteRectangle(position, scale, color, top_left_uv, bottom_right_uv, buffers[compound_index]);
		}

		// -------------------------------------------------------------------------------------------------------

		void SetVertexColorSpriteRectangle(
			float2 position,
			float2 scale,
			const Color* colors,
			float2 top_left_uv,
			float2 bottom_right_uv,
			CapacityStream<void>& buffer
		) {
			buffer.AssertCapacity(6);
			SetTransformForRectangle(position, scale, buffer.size, (UISpriteVertex*)buffer.buffer);
			SetVertexColorForRectangle(colors, buffer.size, (UISpriteVertex*)buffer.buffer);
			SetUVForRectangle(top_left_uv, bottom_right_uv, buffer.size, (UISpriteVertex*)buffer.buffer);
			buffer.size += 6;
		}

		// -------------------------------------------------------------------------------------------------------

		void SetVertexColorSpriteRectangle(
			float2 position,
			float2 scale,
			const Color* colors,
			float2 top_left_uv,
			float2 bottom_right_uv,
			Stream<CapacityStream<void>> buffers,
			unsigned int material_index,
			unsigned int material_offset
		) {
			unsigned int compound_index = material_index + material_offset * ECS_TOOLS_UI_MATERIALS;
			SetVertexColorSpriteRectangle(position, scale, colors, top_left_uv, bottom_right_uv, buffers[compound_index]);
		}

		// -------------------------------------------------------------------------------------------------------

		void SetVertexColorSpriteRectangle(
			float2 position,
			float2 scale,
			const ColorFloat* colors,
			float2 top_left_uv,
			float2 bottom_right_uv,
			CapacityStream<void>& buffer
		) {
			buffer.AssertCapacity(6);
			SetTransformForRectangle(position, scale, buffer.size, (UISpriteVertex*)buffer.buffer);
			SetVertexColorForRectangle(Color(colors[0]), Color(colors[1]), Color(colors[2]), Color(colors[3]), buffer.size, (UISpriteVertex*)buffer.buffer);
			SetUVForRectangle(top_left_uv, bottom_right_uv, buffer.size, (UISpriteVertex*)buffer.buffer);
			buffer.size += 6;
		}

		// -------------------------------------------------------------------------------------------------------

		void SetVertexColorSpriteRectangle(
			float2 position,
			float2 scale,
			const ColorFloat* colors,
			float2 top_left_uv,
			float2 bottom_right_uv,
			Stream<CapacityStream<void>> buffers,
			unsigned int material_index,
			unsigned int material_offset
		) {
			unsigned int compound_index = material_index + material_offset * ECS_TOOLS_UI_MATERIALS;
			SetVertexColorSpriteRectangle(position, scale, colors, top_left_uv, bottom_right_uv, buffers[compound_index]);
		}

		// -------------------------------------------------------------------------------------------------------

		bool IsPointInRectangle(
			float2 point_position,
			float2 rectangle_position,
			float2 rectangle_scale
		) {
			return point_position.x >= rectangle_position.x && (point_position.x <= rectangle_position.x + rectangle_scale.x)
				&& point_position.y >= rectangle_position.y && (point_position.y <= rectangle_position.y + rectangle_scale.y);
		}

		// -------------------------------------------------------------------------------------------------------

		bool IsPointInRectangle(
			float2 point_position,
			const UIElementTransform& transform
		) {
			return point_position.x >= transform.position.x && (point_position.x <= transform.position.x + transform.scale.x)
				&& point_position.y >= transform.position.y && (point_position.y <= transform.position.y + transform.scale.y);

		}

		// -------------------------------------------------------------------------------------------------------

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

		// -------------------------------------------------------------------------------------------------------

		bool IsRectangleInRectangle(
			const UIElementTransform& first,
			const UIElementTransform& second
		) {
			return first.position.x >= second.position.x && (first.position.x + first.scale.x <= second.position.x + second.scale.x)
				&& first.position.y >= second.position.y && (first.position.y + first.scale.y <= second.position.y + second.scale.y);
		}

		// -------------------------------------------------------------------------------------------------------

		bool IsRectangleInRectangle(
			float2 first_position,
			float2 first_scale,
			float2 second_position,
			float2 second_scale
		) {
			return first_position.x >= second_position.x && (first_position.x + first_scale.x <= second_position.x + second_scale.x)
				&& first_position.y >= second_position.y && (first_position.y + first_scale.y <= second_position.y + second_scale.y);
		}

		// -------------------------------------------------------------------------------------------------------

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

		// -------------------------------------------------------------------------------------------------------

		bool RectangleOverlap(const UIElementTransform& first, const UIElementTransform& second) {
			bool x_overlap = IsInRange(first.position.x, second.position.x, second.position.x + second.scale.x) || IsInRange(second.position.x, first.position.x, first.position.x + first.scale.x);
			bool y_overlap = IsInRange(first.position.y, second.position.y, second.position.y + second.scale.y) || IsInRange(second.position.y, first.position.y, first.position.y + first.scale.y);
			return x_overlap && y_overlap;
		}

		// -------------------------------------------------------------------------------------------------------

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

		// -------------------------------------------------------------------------------------------------------

		template<typename Stream>
		void AlignVerticalText(Stream& vertices) {
			float left_x = vertices[0].position.x;
			float right_x = vertices[1].position.x;
			for (size_t index = 7; index < vertices.size; index += 6) {
				//left_x = min(left_x, vertices[index - 1].position.x);
				right_x = max(right_x, vertices[index].position.x);
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

		// -------------------------------------------------------------------------------------------------------

		template<typename Element>
		float GetMinXRectangle(const Element* element, size_t size) {
			float min_value = element[0].position.x;
			for (size_t index = 6; index < size; index += 6) {
				min_value = min(min_value, element[index].position.x);
			}
			return min_value;
		}

		ECS_TEMPLATE_FUNCTION_2_BEFORE(float, GetMinXRectangle, const UIVertexColor*, const UISpriteVertex*, size_t);

		// -------------------------------------------------------------------------------------------------------

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

		// -------------------------------------------------------------------------------------------------------

		float2 ExpandRectangle(
			float2 position,
			float2 scale,
			float2 resize_factor,
			float2& new_scale
		) {
			new_scale = { scale.x * resize_factor.x, scale.y * resize_factor.y };
			return ExpandRectangle(position, scale, new_scale);
		}

		// -------------------------------------------------------------------------------------------------------

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

		// -------------------------------------------------------------------------------------------------------

		void ExpandRectangle(float2* element_transforms, float2 new_scale_factor, size_t count) {
			for (size_t index = 0; index < count; index++) {
				unsigned int scale_index = index * 2 + 1;
				float2 old_scale = element_transforms[scale_index];
				element_transforms[scale_index - 1] = ExpandRectangle(element_transforms[scale_index - 1], old_scale, new_scale_factor, element_transforms[scale_index]);
			}
		}

		// -------------------------------------------------------------------------------------------------------

		void ExpandRectangle(float2* element_transforms, const float2* new_scale_factor, size_t count) {
			for (size_t index = 0; index < count; index++) {
				unsigned int scale_index = index * 2 + 1;
				float2 old_scale = element_transforms[scale_index];
				element_transforms[scale_index - 1] = ExpandRectangle(element_transforms[scale_index - 1], old_scale, new_scale_factor[index], element_transforms[scale_index]);
			}
		}

		// -------------------------------------------------------------------------------------------------------

		void ExpandRectangle(Stream<float2> element_transforms, float2 new_scale_factor) {
			ExpandRectangle(element_transforms.buffer, new_scale_factor, element_transforms.size);
		}

		// -------------------------------------------------------------------------------------------------------

		void ExpandRectangle(Stream<float2> element_transforms, const float2* new_scale_factor) {
			ExpandRectangle(element_transforms.buffer, new_scale_factor, element_transforms.size);
		}

		// -------------------------------------------------------------------------------------------------------

		template<typename Value>
		Value AlignMiddle(Value first, Value external_scale, Value element_scale) {
			return first + (external_scale - element_scale) * 0.5f;
		}

		ECS_TEMPLATE_FUNCTION(float, AlignMiddle, float, float, float);
		ECS_TEMPLATE_FUNCTION(double, AlignMiddle, double, double, double);
		ECS_TEMPLATE_FUNCTION(float2, AlignMiddle, float2, float2, float2);

		// -------------------------------------------------------------------------------------------------------

		void GetEncompassingRectangle(Stream<float2> values, float2* results) {
			float min_x = 5.0f, min_y = 5.0f, max_x = -5.0f, max_y = -5.0f;
			for (size_t index = 0; index < values.size; index++) {
				min_x = min(min_x, values[index].x);
				min_y = min(min_y, values[index].y);
				max_x = max(max_x, values[index].x);
				max_y = max(max_y, values[index].y);
			}
			results[0] = float2(min_x, min_y);
			results[1] = float2(max_x - min_x, max_y - min_y);
		}

		// -------------------------------------------------------------------------------------------------------

		float2 CenterRectangle(float2 scale, float2 point) {
			return float2(point.x - scale.x * 0.5f, point.y - scale.y * 0.5f);
		}

		// -------------------------------------------------------------------------------------------------------

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

		// -------------------------------------------------------------------------------------------------------

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

		// -------------------------------------------------------------------------------------------------------

		template<bool is_inner>
		void CreateSolidColorRectangleBorder(
			float2 position,
			float2 scale,
			float2 border_scale,
			Color color,
			Stream<CapacityStream<void>> buffers
		) {
			float2 results[8];
			CreateSolidColorRectangleBorder<is_inner>(position, scale, border_scale, color, buffers, results);
		}

		template void ECSENGINE_API CreateSolidColorRectangleBorder<true>(float2, float2, float2, Color, Stream<CapacityStream<void>>);
		template void ECSENGINE_API CreateSolidColorRectangleBorder<false>(float2, float2, float2, Color, Stream<CapacityStream<void>>);

		// -------------------------------------------------------------------------------------------------------

		template<bool is_inner>
		void CreateSolidColorRectangleBorder(
			float2 position,
			float2 scale,
			float2 border_scale,
			Color color,
			Stream<CapacityStream<void>> buffers,
			float2* results
		) {
			if constexpr (is_inner)
				CreateRectangleBorder(position, scale, border_scale, results);
			else
				CreateRectangleOuterBorder(position, scale, border_scale, results);

			// top
			SetSolidColorRectangle(
				results[0],
				results[1],
				color,
				buffers
			);

			// left
			SetSolidColorRectangle(
				results[2],
				results[3],
				color,
				buffers
			);

			// bottom
			SetSolidColorRectangle(
				results[4],
				results[5],
				color,
				buffers
			);

			// right
			SetSolidColorRectangle(
				results[6],
				results[7],
				color,
				buffers
			);
		}

		template void ECSENGINE_API CreateSolidColorRectangleBorder<false>(float2, float2, float2, Color, Stream<CapacityStream<void>>, float2*);
		template void ECSENGINE_API CreateSolidColorRectangleBorder<true>(float2, float2, float2, Color, Stream<CapacityStream<void>>, float2*);

		// -------------------------------------------------------------------------------------------------------

		void DrawExpandedSolidColorRectangle(
			float2 position,
			float2 scale,
			float2 new_scale_factor,
			Color color,
			Stream<CapacityStream<void>> buffers,
			ECS_UI_DRAW_PHASE draw_phase
		) {
			float2 old_scale = scale;
			float2 new_position = ExpandRectangle(position, old_scale, new_scale_factor, scale);
			SetSolidColorRectangle(new_position, scale, color, buffers, GetDrawPhaseMaterialOffset(draw_phase));
		}

		// -------------------------------------------------------------------------------------------------------

		template<typename Stream>
		float2 GetPositionFromLastRectangle(Stream vertices) {
			return { vertices[vertices.size - 6].position.x, -vertices[vertices.size - 6].position.y };
		}

		ECS_TEMPLATE_FUNCTION_4_BEFORE(float2, GetPositionFromLastRectangle, Stream<UIVertexColor>, Stream<UISpriteVertex>, CapacityStream<UIVertexColor>, CapacityStream<UISpriteVertex>);

		// -------------------------------------------------------------------------------------------------------

		template<typename Stream>
		float2 GetScaleFromLastRectangle(Stream vertices) {
			return {
				vertices[vertices.size - 5].position.x - vertices[vertices.size - 6].position.x,
				vertices[vertices.size - 3].position.y - vertices[vertices.size - 1].position.y
			};
		}

		ECS_TEMPLATE_FUNCTION_4_BEFORE(float2, GetScaleFromLastRectangle, Stream<UIVertexColor>, Stream<UISpriteVertex>, CapacityStream<UIVertexColor>, CapacityStream<UISpriteVertex>);

		// -------------------------------------------------------------------------------------------------------

		template<typename Stream>
		void TranslateTextX(float new_x, unsigned int min_x_index, Stream& stream) {
			float translation = new_x - stream[min_x_index].position.x;
			for (size_t index = 0; index < stream.size; index++) {
				stream[index].position.x += translation;
			}
		}

		ECS_TEMPLATE_FUNCTION_4_AFTER(void, TranslateTextX, Stream<UIVertexColor>&, Stream<UISpriteVertex>&, CapacityStream<UIVertexColor>&, CapacityStream<UISpriteVertex>&, float, unsigned int);
	
		// -------------------------------------------------------------------------------------------------------

		template<typename Stream>
		void TranslateTextY(float new_y, unsigned int min_y_index, Stream& stream) {
			float translation = -new_y - stream[min_y_index].position.y;
			for (size_t index = 0; index < stream.size; index++) {
				stream[index].position.y += translation;
			}
		}

		ECS_TEMPLATE_FUNCTION_4_AFTER(void, TranslateTextY, Stream<UIVertexColor>&, Stream<UISpriteVertex>&, CapacityStream<UIVertexColor>&, CapacityStream<UISpriteVertex>&, float, unsigned int);

		// -------------------------------------------------------------------------------------------------------

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

		// -------------------------------------------------------------------------------------------------------

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

		// -------------------------------------------------------------------------------------------------------

		template<typename Stream>
		void ScaleTextX(
			const Stream& input,
			CapacityStream<void>& buffer,
			bool is_vertical,
			bool is_inverted,
			float inverted_current_scale,
			float new_scale,
			float character_spacing
		) {
			if (!is_vertical) {
				float x_position = is_inverted ? input[input.size - 1].position.x : input[0].position.x;

				auto kernel = [&](int64_t index) {
					float x_scale = input[index + 1].position.x - input[index].position.x;
					float new_x_scale = x_scale * new_scale * inverted_current_scale;
					SetSpriteRectangle(
						{ x_position, -input[index].position.y },
						{ new_x_scale, input[index].position.y - input[index + 2].position.y },
						input[index].color,
						input[index].uvs,
						input[index + 4].uvs,
						buffer
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
						buffer
					);
				};
				for (size_t index = 0; index < input.size; index += 6) {
					kernel(index);
				}
			}
		}

		ECS_TEMPLATE_FUNCTION_2_BEFORE(void, ScaleTextX, const Stream<UISpriteVertex>&, const CapacityStream<UISpriteVertex>&, CapacityStream<void>&, bool, bool, float, float, float);

		// -------------------------------------------------------------------------------------------------------

		template<typename Stream>
		void ScaleTextY(
			const Stream& input,
			CapacityStream<void>& buffer,
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
						buffer
					);
				};

				for (int64_t index = 0; index < input.size; index += 6) {
					kernel(index);
				}
			}
			else {
				float y_position = -(is_inverted ? input[input.size - 3].position.y : input[0].position.y);
				float new_character_spacing = character_spacing * 0.1f;

				auto kernel = [&](int64_t index) {
					SetSpriteRectangle(
						{ input[index].position.x, y_position },
						{ input[index + 1].position.x - input[index].position.x, new_y_scale },
						input[index].color,
						input[index].uvs,
						input[index + 4].uvs,
						buffer
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
	
		ECS_TEMPLATE_FUNCTION_2_BEFORE(void, ScaleTextY, const Stream<UISpriteVertex>&, const CapacityStream<UISpriteVertex>&, CapacityStream<void>&, bool, bool, float, float, float);
	
		// -------------------------------------------------------------------------------------------------------

		template<typename Stream>
		void ScaleTextXY(
			const Stream& input,
			CapacityStream<void>& buffer,
			bool is_vertical,
			bool is_inverted,
			float2 inverted_current_scale,
			float2 new_scale,
			float character_spacing
		) {
			float y_scale = input[0].position.y - input[2].position.y;
			float new_y_scale = y_scale * new_scale.y * inverted_current_scale.y;

			if (!is_vertical) {
				float x_position = is_inverted ? input[input.size - 1].position.x : input[0].position.x;

				auto kernel = [&](int64_t index) {
					float x_scale = input[index + 1].position.x - input[index].position.x;
					float new_x_scale = x_scale * new_scale.x * inverted_current_scale.x;
					SetSpriteRectangle(
						{ x_position, -input[index].position.y },
						{ new_x_scale, new_y_scale },
						input[index].color,
						input[index].uvs,
						input[index + 4].uvs,
						buffer
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
				float y_position = -(is_inverted ? input[input.size - 3].position.y : input[0].position.y);
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
						buffer
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

		ECS_TEMPLATE_FUNCTION_2_BEFORE(void, ScaleTextXY, const Stream<UISpriteVertex>&, const CapacityStream<UISpriteVertex>&, CapacityStream<void>&, bool, bool, float2, float2, float);

		// -------------------------------------------------------------------------------------------------------

		template<typename Stream>
		void ScaleText(
			const Stream& input,
			float2 input_zoom,
			float2 input_inverse_zoom,
			CapacityStream<void>& buffer,
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
				ScaleTextX(input, buffer, is_vertical, is_inverted, input_inverse_zoom.x, zoom_ptr->x, character_spacing);
			}
			else if (is_x_different && is_y_different) {
				ScaleTextXY(input, buffer, is_vertical, is_inverted, input_inverse_zoom, { zoom_ptr->x, zoom_ptr->y }, character_spacing);
			}
			else if (is_y_different) {
				ScaleTextY(input, buffer, is_vertical, is_inverted, input_inverse_zoom.y, zoom_ptr->y, character_spacing);
			}
		}

		ECS_TEMPLATE_FUNCTION_2_BEFORE(void, ScaleText, const Stream<UISpriteVertex>&, const CapacityStream<UISpriteVertex>&, float2, float2, CapacityStream<void>&, const float2*, float);

		// -------------------------------------------------------------------------------------------------------

		size_t ParseStringIdentifier(Stream<char> string) {
			Stream<char> separator = FindFirstToken(string, ECS_TOOLS_UI_DRAWER_STRING_PATTERN_CHAR_COUNT);
			if (separator.size > 0) {
				return string.StartDifference(separator).size;
			}
			return string.size;
		}

		// -------------------------------------------------------------------------------------------------------

		bool IsClickableTrigger(ActionData* action_data, ECS_MOUSE_BUTTON button_type)
		{
			return action_data->mouse->IsReleased(button_type) && IsPointInRectangle(action_data->mouse_position, action_data->position, action_data->scale);
		}

		// -------------------------------------------------------------------------------------------------------

		template<typename Buffer>
		float2 GetSectionYBounds(const Buffer* buffer, size_t starting_index, size_t end_index) {
			float2 bounds = { -10.0f, 10.0f };
			for (size_t index = starting_index; index < end_index; index++) {
				bounds.x = bounds.x < buffer[index].position.y ? buffer[index].position.y : bounds.x;
				bounds.y = bounds.y > buffer[index].position.y ? buffer[index].position.y : bounds.y;
			}

			bounds.x = -bounds.x;
			bounds.y = -bounds.y;
			return bounds;
		}

		ECS_TEMPLATE_FUNCTION_2_BEFORE(float2, GetSectionYBounds, const UIVertexColor*, const UISpriteVertex*, size_t, size_t);
	
		// -------------------------------------------------------------------------------------------------------

		template<typename Buffer>
		float2 GetRectangleSectionYBounds(const Buffer* buffer, size_t starting_index, size_t end_index) {
			float2 bounds = { -10.0f, 10.0f };
			for (size_t index = starting_index; index < end_index; index += 6) {
				bounds.x = bounds.x < buffer[index].position.y ? buffer[index].position.y : bounds.x;
				bounds.y = bounds.y > buffer[index + 2].position.y ? buffer[index + 2].position.y : bounds.y;
			}

			bounds.x = -bounds.x;
			bounds.y = -bounds.y;
			return bounds;
		}

		ECS_TEMPLATE_FUNCTION_2_BEFORE(float2, GetRectangleSectionYBounds, const UIVertexColor*, const UISpriteVertex*, size_t, size_t);

		// -------------------------------------------------------------------------------------------------------

		// -------------------------------------------------------------------------------------------------------

		// -------------------------------------------------------------------------------------------------------

	}

}