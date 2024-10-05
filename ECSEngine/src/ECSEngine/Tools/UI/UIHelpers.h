#pragma once
#include "../../Core.h"
#include "../../Rendering/RenderingStructures.h"
#include "UIStructures.h"

namespace ECSEngine {

	namespace Tools {

		template<typename Element>
		void SetTransformForRectangle(
			float2 position,
			float2 scale,
			unsigned int starting_index,
			Element* buffer
		);

		// Rectangle must have 6 vertices that represent a rectangle filled in with SetTransformForRectangle
		ECSENGINE_API Rectangle2D GetRectangleFromVertices(const UIVertexColor* vertices);

		// Rectangle must have 6 vertices that represent a rectangle filled in with SetTransformForRectangle
		ECSENGINE_API Rectangle2D GetRectangleFromVertices(const UISpriteVertex* vertices);

		ECSENGINE_API void SetTransformForLine(float2 position1, float2 position2, size_t count, UIVertexColor* buffer);

		ECSENGINE_API void SetTransformForLine(float2 position1, float2 position2, size_t* counts, void** buffers, unsigned int material_offset = 0);

		ECSENGINE_API void SetColorForLine(Color color, size_t count, UIVertexColor* buffer);

		ECSENGINE_API void SetColorForLine(Color color, size_t* counts, void** buffers, unsigned int material_offset = 0);

		ECSENGINE_API void SetLine(float2 position1, float2 position2, Color color, size_t& count, UIVertexColor* buffer);

		ECSENGINE_API void SetLine(float2 position1, float2 position2, Color color, size_t* counts, void** buffers, unsigned int material_offset = 0);

		ECSENGINE_API float GetDockspaceMaskFromType(DockspaceType type);

		template<typename Element>
		void SetVertexColorForRectangle(
			Color top_left,
			Color top_right,
			Color bottom_left,
			Color bottom_right,
			unsigned int starting_index,
			Element* buffer
		);

		template<typename Element>
		void SetVertexColorForRectangle(
			const Color* colors,
			unsigned int starting_index,
			Element* buffer
		);

		template<typename Element>
		void SetColorForRectangle(Color color, unsigned int starting_index, Element* buffer);

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
		);

		template<typename Element>
		void SetUVForRectangle(
			float2 uv0,
			float2 uv1,
			float2 uv2,
			float2 uv3,
			unsigned int starting_index,
			Element* buffer
		);

		// Returns the uvs for the top left and bottom right only, it is assumed that the other 2 uvs
		// Can be deduced from the other 2.
		ECSENGINE_API Rectangle2D GetUVFromVertices(const UISpriteVertex* vertices);

		// manages all the cases or rectangle rotation through predication
		template<typename Element>
		void SetUVForRectangle(
			float2 first_uv,
			float2 second_uv,
			unsigned int starting_index,
			Element* buffer
		);

		ECSENGINE_API void SetSolidColorRectangle(
			float2 position,
			float2 scale,
			Color color,
			UIVertexColor* buffer,
			size_t& count
		);

		ECSENGINE_API void SetSolidColorRectangle(
			float2 position,
			float2 scale,
			Color color,
			void** buffers,
			size_t* counts,
			unsigned int material_offset = 0
		);

		ECSENGINE_API void SetVertexColorRectangle(
			float2 position,
			float2 scale,
			Color top_left,
			Color top_right,
			Color bottom_left,
			Color bottom_right,
			UIVertexColor* buffer,
			size_t* count
		);

		ECSENGINE_API void SetVertexColorRectangle(
			float2 position,
			float2 scale,
			Color top_left,
			Color top_right,
			Color bottom_left,
			Color bottom_right,
			void** buffers,
			size_t* counts,
			unsigned int material_offset = 0
		);

		ECSENGINE_API void SetVertexColorRectangle(
			float2 position,
			float2 scale,
			const Color* colors,
			UIVertexColor* buffer,
			size_t* count
		);

		ECSENGINE_API void SetVertexColorRectangle(
			float2 position,
			float2 scale,
			const Color* colors,
			void** buffers,
			size_t* counts,
			unsigned int material_offset = 0
		);

		ECSENGINE_API void SetSpriteRectangle(
			float2 position,
			float2 scale,
			Color color,
			float2 top_left_uv,
			float2 bottom_right_uv,
			UISpriteVertex* buffer,
			size_t& count
		);

		ECSENGINE_API void SetSpriteRectangle(
			float2 position,
			float2 scale,
			Color color,
			float2 top_left_uv,
			float2 bottom_right_uv,
			void** buffers,
			size_t* counts,
			unsigned int material_index,
			unsigned int material_offset = 0
		);

		ECSENGINE_API void SetVertexColorSpriteRectangle(
			float2 position,
			float2 scale,
			const Color* colors,
			float2 top_left_uv,
			float2 bottom_right_uv,
			UISpriteVertex* buffer,
			size_t& count
		);

		ECSENGINE_API void SetVertexColorSpriteRectangle(
			float2 position,
			float2 scale,
			const Color* colors,
			float2 top_left_uv,
			float2 bottom_right_uv,
			void** buffers,
			size_t* counts,
			unsigned int material_index,
			unsigned int material_offset
		);

		ECSENGINE_API void SetVertexColorSpriteRectangle(
			float2 position,
			float2 scale,
			const ColorFloat* colors,
			float2 top_left_uv,
			float2 bottom_right_uv,
			UISpriteVertex* buffer,
			size_t& count
		);

		ECSENGINE_API void SetVertexColorSpriteRectangle(
			float2 position,
			float2 scale,
			const ColorFloat* colors,
			float2 top_left_uv,
			float2 bottom_right_uv,
			void** buffers,
			size_t* counts,
			unsigned int material_index,
			unsigned int material_offset
		);

		ECSENGINE_API bool IsPointInRectangle(
			float2 point_position,
			float2 rectangle_position,
			float2 rectangle_scale
		);

		ECSENGINE_API bool IsPointInRectangle(
			float2 point_position,
			const UIElementTransform& transform
		);

		ECSENGINE_API bool IsPointInRectangle(
			float point_position_x,
			float point_position_y,
			float rectangle_position_x,
			float rectangle_position_y,
			float rectangle_scale_x,
			float rectangle_scale_y
		);

		ECSENGINE_API bool IsRectangleInRectangle(
			const UIElementTransform& first,
			const UIElementTransform& second
		);

		ECSENGINE_API bool IsRectangleInRectangle(
			float2 first_position,
			float2 first_scale,
			float2 second_position,
			float2 second_scale
		);

		ECSENGINE_API bool IsRectangleInRectangle(
			unsigned short first_position_x,
			unsigned short first_position_y,
			unsigned short first_scale_x,
			unsigned short first_scale_y,
			unsigned short second_position_x,
			unsigned short second_position_y,
			unsigned short second_scale_x,
			unsigned short second_scale_y
		);

		// ElementType must be float, Vec8f or Vec16us
		template<typename ElementType>
		bool ECS_VECTORCALL RectangleOverlap(
			ElementType first_position_x,
			ElementType first_position_y,
			ElementType first_width,
			ElementType first_height,
			ElementType second_position_x,
			ElementType second_position_y,
			ElementType second_width,
			ElementType second_height
		);

		ECSENGINE_API bool RectangleOverlap(const UIElementTransform& first, const UIElementTransform& second);

		template<typename SimdVector>
		int ECS_VECTORCALL AVX2IsPointInRectangle(
			SimdVector point_position_x,
			SimdVector point_position_y,
			SimdVector rectangle_position_x,
			SimdVector rectangle_position_y,
			SimdVector rectangle_scale_x,
			SimdVector rectangle_scale_y
		);

		template<typename Stream>
		void AlignVerticalText(Stream& vertices);

		// mode - 0; cull horizontal if greater
		// mode - 1; cull horizontal if smaller
		// mode - 2; cull vertical if greater
		// mode - 3; cull vertical if smaller
		// returns the count of the valid vertices (multiple of 6)
		template<size_t mode = 0, typename Stream>
		size_t CullTextSprites(Stream vertices, float bound) {
			if constexpr (mode == 0) {
				for (int64_t index = 1; index < vertices.size; index += 6) {
					if (vertices[index].position.x > bound) {
						return index - 1;
					}
				}
				return vertices.size;
			}
			else if constexpr (mode == 1) {
				for (int64_t index = 0; index < vertices.size; index += 6) {
					if (vertices[index].position.x < bound) {
						return index;
					}
				}
				return vertices.size;
			}
			else if constexpr (mode == 2) {
				for (int64_t index = 2; index < vertices.size; index += 6) {
					if (vertices[index].position.y > bound) {
						return index - 2;
					}
				}
				return vertices.size;
			}
			else if constexpr (mode == 3) {
				for (int64_t index = vertices.size - 1; index >= 0; index -= 6) {
					if (vertices[index].position.y < bound) {
						return vertices.size - index - 1;
					}
				}
				return vertices.size;
			}
		}

		// mode - 0; cull horizontal if greater
		// mode - 1; cull horizontal if smaller
		// mode - 2; cull vertical if greater
		// mode - 3; cull vertical if smaller
		// copy_mode - 0; copy sprite by sprite
		// copy_mode - 1; copy after determining the number of valid sprites
		template<size_t mode = 0, size_t copy_mode = 0, typename Stream, typename OutputStream>
		void CullTextSprites(Stream vertices, OutputStream& output, float bound) {
			if constexpr (copy_mode == 0) {
				size_t sprite_copy_size = sizeof(UISpriteVertex) * 6;
				if constexpr (mode == 0) {
					for (int64_t index = 1; index < vertices.size; index += 6) {
						if (vertices[index].position.x > bound) {
							output.size = index - 1;
							return;
						}
						memcpy(output.buffer + index - 1, vertices.buffer + index - 1, sprite_copy_size);
					}
					output.size = vertices.size;
				}
				else if constexpr (mode == 1) {
					for (int64_t index = vertices.size - 1; index >= 0; index -= 6) {
						if (vertices[index].position.x < bound) {
							output.size = vertices.size - index - 1;
							return;
						}
						memcpy(output.buffer + index, vertices.buffer + index, sprite_copy_size);
					}
					output.size = vertices.size;
				}
				else if constexpr (mode == 2) {
					for (int64_t index = 2; index < vertices.size; index += 6) {
						if (vertices[index].position.y > bound) {
							output.size = index - 2;
							return;
						}
						memcpy(output.buffer + index - 2, vertices.buffer + index - 2, sprite_copy_size);
					}
					output.size = vertices.size;
				}
				else if constexpr (mode == 3) {
					for (int64_t index = 0; index < vertices.size; index += 6) {
						if (vertices[index].position.y < bound) {
							output.size = index;
							return;
						}
						memcpy(output.buffer + index, vertices.buffer + index, sprite_copy_size);
					}
					output.size = vertices.size;
				}
			}
			else {
				size_t valid_sprite_vertices = CullTextSprites<mode>(vertices, bound);
				memcpy(output.buffer, vertices.buffer, valid_sprite_vertices * sizeof(UISpriteVertex));
				output.size = valid_sprite_vertices;
			}
		}

		template<typename Element>
		float GetMinXRectangle(const Element* element, size_t size);

		ECSENGINE_API float2 ExpandRectangle(
			float2 position,
			float2 scale,
			float2 new_scale
		);

		ECSENGINE_API float2 ExpandRectangle(
			float2 position,
			float2 scale,
			float2 resize_factor,
			float2& new_scale
		);

		ECSENGINE_API float2 ExpandRectangle(
			float2 position,
			float2 scale,
			float new_scale_factor
		);

		ECSENGINE_API void ExpandRectangle(float2* element_transforms, float2 new_scale_factor, size_t count);

		ECSENGINE_API void ExpandRectangle(float2* element_transforms, const float2* new_scale_factor, size_t count);

		ECSENGINE_API void ExpandRectangle(Stream<float2> element_transforms, float2 new_scale_factor);

		ECSENGINE_API void ExpandRectangle(Stream<float2> element_transforms, const float2* new_scale_factor);

		template<typename Value>
		Value AlignMiddle(Value first, Value external_scale, Value element_scale);

		// first is the position, second is scale
		ECSENGINE_API void GetEncompassingRectangle(Stream<float2> values, float2* results);

		template<typename Stream>
		float2 GetPositionFromLastRectangle(Stream vertices);

		template<typename Stream>
		float2 GetScaleFromLastRectangle(Stream vertices);

		ECSENGINE_API float2 CenterRectangle(float2 scale, float2 point);

		ECSENGINE_API void CreateRectangleBorder(float2 position, float2 scale, float2 border_scale, float2* results);

		ECSENGINE_API void CreateRectangleOuterBorder(float2 position, float2 scale, float2 border_scale, float2* results);

		template<bool is_inner = true>
		void CreateSolidColorRectangleBorder(
			float2 position,
			float2 scale,
			float2 border_scale,
			Color color,
			size_t* counts,
			void** buffers
		);

		template<bool is_inner = true>
		void CreateSolidColorRectangleBorder(
			float2 position,
			float2 scale,
			float2 border_scale,
			Color color,
			size_t* counts,
			void** buffers,
			float2* results
		);

		ECSENGINE_API void DrawExpandedSolidColorRectangle(
			float2 position,
			float2 scale,
			float2 new_scale_factor,
			Color color,
			size_t* counts,
			void** buffers
		);

		template<bool horizontal, typename Buffer>
		static void CreateDottedLine(
			Buffer* buffer, 
			size_t offset, 
			size_t line_count,
			float2 starting_point, 
			float end_point, 
			float spacing,
			float width, 
			Color color
		) {
			float total_spacing_count = spacing * (line_count - 1);
			if constexpr (horizontal) {
				float line_total_length = end_point - starting_point.x - total_spacing_count;
				line_total_length = ClampMin(line_total_length, 0.0f);
				float2 scale = { line_total_length / line_count, width };
				float position_offset = 0.0f;
				for (size_t index = 0; index < line_count; index++) {
					size_t buffer_index = offset + index * 6;
					SetTransformForRectangle(
						float2(starting_point.x + position_offset, starting_point.y),
						scale,
						buffer_index,
						buffer
					);
					SetColorForRectangle(
						color,
						buffer_index,
						buffer
					);
					position_offset += scale.x + spacing;
				}
			}
			else {
				float line_total_length = end_point - starting_point.y - total_spacing_count;
				line_total_length = ClampMin(line_total_length, 0.0f);
				float2 scale = { width, line_total_length / line_count };
				float position_offset = 0.0f;
				for (size_t index = 0; index < line_count; index++) {
					size_t buffer_index = offset + index * 6;
					SetTransformForRectangle(
						float2(starting_point.x, starting_point.y + position_offset),
						scale,
						buffer_index,
						buffer
					);
					SetColorForRectangle(
						color,
						buffer_index,
						buffer
					);
					position_offset += scale.y + spacing;
				}
			}
		}

		template<typename Stream>
		void TranslateTextX(float new_x, unsigned int min_x_index, Stream& stream);
		
		template<typename Stream>
		void TranslateTextY(float new_y, unsigned int min_y_index, Stream& stream);

		template<typename Stream>
		void TranslateText(float x_position, float y_position, Stream& stream);

		template<typename Stream>
		void TranslateText(float x_position, float y_position, float x_minimum, float y_minimum, Stream& stream);

		template<typename Stream>
		void TranslateText(float x_position, float y_position, Stream& stream, unsigned int x_minimum_index, unsigned int y_minimum_index) {
			TranslateText(x_position, y_position, stream[x_minimum_index].position.x, stream[y_minimum_index].position.y, stream);
		}

		template<typename Stream>
		float2 GetTextSpan(Stream texts, bool horizontal = true, bool invert_order = false) {
			if (texts.size > 0) {
				if (horizontal) {
					if (!invert_order) {
						return {
							texts[texts.size - 2].position.x - texts[0].position.x,
							texts[0].position.y - texts[texts.size - 2].position.y
						};
					}
					else {
						return {
							texts[1].position.x - texts[texts.size - 1].position.x,
							texts[0].position.y - texts[texts.size - 2].position.y
						};
					}
				}
				else {
					float smallest = 2.0f;
					float biggest = -2.0f;
					for (size_t index = 0; index < texts.size; index += 6) {
						smallest = min(smallest, texts[index].position.x);
						biggest = max(biggest, texts[index + 1].position.x);
					}
					if (!invert_order) {
						return {
							biggest - smallest,
							texts[0].position.y - texts[texts.size - 2].position.y
						};
					}
					else {
						return {
							biggest - smallest,
							texts[texts.size - 3].position.y - texts[2].position.y
						};
					}
				}
			}
			else {
				return { 0.0f, 0.0f };
			}
		}

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
		);

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
		);

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
		);

		// it only scales the sprites, it does not translate them
		template<typename TextElement>
		void ScaleText(
			TextElement* element,
			UISpriteVertex* output_buffer, 
			size_t* output_count, 
			const float2* zoom_ptr,
			float character_spacing
		) {
			bool is_x_different = element->GetZoomX() != zoom_ptr->x;
			bool is_y_different = element->GetZoomY() != zoom_ptr->y;

			const CapacityStream<UISpriteVertex>* vertices = element->TextStream();
			float2 element_inverse_zoom = element->GetInverseZoom();
			bool is_vertical = vertices->buffer[6].position.y != vertices->buffer[0].position.y;
			bool is_inverted = false;
			if (is_vertical && vertices->buffer[0].position.y < vertices->buffer[vertices->size - 1].position.y) {
				is_inverted = true;
			}
			else if (vertices->buffer[0].position.x > vertices->buffer[vertices->size - 1].position.x) {
				is_inverted = true;
			}

			if (is_x_different && !is_y_different) {
				ScaleTextX(*vertices, output_buffer, output_count, is_vertical, is_inverted, element->GetInverseZoomX(), zoom_ptr->x, character_spacing);
			}
			else if (is_x_different && is_y_different) {
				ScaleTextXY(*vertices, output_buffer, output_count, is_vertical, is_inverted, element->GetInverseZoom(), { zoom_ptr->x, zoom_ptr->y }, character_spacing);
			}
			else if (is_y_different) {
				ScaleTextY(*vertices, output_buffer, output_count, is_vertical, is_inverted, element->GetInverseZoomY(), zoom_ptr->y, character_spacing);
			}
		}

		// it only scales the sprites, it does not translate them
		template<typename Stream>
		void ScaleText(
			Stream& input,
			float2 input_zoom,
			float2 input_inverse_zoom,
			UISpriteVertex* output_buffer,
			size_t* output_count,
			const float2* zoom_ptr,
			float character_spacing
		);

		// returns the number of valid characters
		ECSENGINE_API size_t ParseStringIdentifier(Stream<char> string);

		// returns the position of the lowest and highest vertex
		template<typename Buffer>
		float2 GetSectionYBounds(const Buffer* buffer, size_t starting_index, size_t end_index);

		template<typename Buffer>
		float2 GetRectangleSectionYBounds(const Buffer* buffer, size_t starting_index, size_t end_index);

		ECSENGINE_API bool IsClickableTrigger(ActionData* action_data, ECS_MOUSE_BUTTON button_type = ECS_MOUSE_LEFT);

	}
}