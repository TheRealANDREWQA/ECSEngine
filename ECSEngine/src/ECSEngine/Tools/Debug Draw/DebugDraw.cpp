#include "ecspch.h"
#include "DebugDraw.h"
#include "../../Resources/ResourceManager.h"
#include "../../Rendering/GraphicsHelpers.h"
#include "../../Math/Quaternion.h"
#include "../../Math/Conversion.h"
#include "../../Rendering/RenderingEffects.h"
#include "../../Utilities/FilePreprocessor.h"
#include "../../Rendering/Camera.h"

#define SMALL_VERTEX_BUFFER_CAPACITY 8
#define PER_THREAD_RESOURCES 32
#define GRID_THREAD_CAPACITY 4

#define LARGE_BUFFER_CAPACITY 100 * ECS_KB

#define POINT_SIZE 0.0175f
#define CIRCLE_TESSELATION 32
#define ARROW_HEAD_DARKEN_COLOR 1.0f
#define AXES_X_SCALE 3.0f

#define STRING_SPACE_SIZE 0.2f
#define STRING_TAB_SIZE STRING_SPACE_SIZE * 4
#define STRING_NEW_LINE_SIZE 0.85f
#define STRING_CHARACTER_SPACING 0.05f

#define DECK_POWER_OF_TWO 7
#define DECK_CHUNK_SIZE (1 << DECK_POWER_OF_TWO)

#define OOBB_CROSS_Y_SCALE 0.02f
#define OOBB_CROSS_Z_SCALE 0.02f

namespace ECSEngine {

	// In consort with DebugVertexBuffers
	const wchar_t* ECS_DEBUG_PRIMITIVE_MESH_FILE[] = {
		L"Resources/DebugPrimitives/Sphere.glb",
		L"Resources/DebugPrimitives/Point.glb",
		L"Resources/DebugPrimitives/Cross.glb",
		L"Resources/DebugPrimitives/Arrow.glb",
		L"Resources/DebugPrimitives/Cube.glb"
	};

	static_assert(ECS_COUNTOF(ECS_DEBUG_PRIMITIVE_MESH_FILE) == ECS_DEBUG_VERTEX_BUFFER_COUNT);

	const wchar_t* STRING_MESH_FILE = L"Resources/DebugPrimitives/Alphabet.glb";

	enum ElementType {
		WIREFRAME_DEPTH,
		WIREFRAME_NO_DEPTH,
		SOLID_DEPTH,
		SOLID_NO_DEPTH,
		ELEMENT_COUNT
	};

	struct InstancedTransformData {
		float4 matrix_0;
		float4 matrix_1;
		float4 matrix_2;
		float4 matrix_3;
		ColorFloat color;
	};

	struct InstancedVertex {
		float3 position;
		unsigned int instance_thickness;
	};

	union DebugDrawerOutput {
		ECS_INLINE DebugDrawerOutput() {}
		ECS_INLINE DebugDrawerOutput(Color _color) : color(_color) {}
		ECS_INLINE DebugDrawerOutput(unsigned int _instance_index) : instance_thickness(_instance_index) {}

		Color color;
		unsigned int instance_thickness;
	};

	// ----------------------------------------------------------------------------------------------------------------------

	ECS_INLINE unsigned int* GetThreadRedirectCount(DebugDrawer* debug_drawer, unsigned int thread_index) {
		return debug_drawer->redirect_thread_counts + thread_index * ECS_DEBUG_PRIMITIVE_COUNT;
	}

	template<typename SizeExtractor, typename LINE, typename SPHERE, typename POINT, typename RECTANGLE, typename CROSS, typename CIRCLE,
		typename ARROW, typename TRIANGLE, typename AABB_, typename OOBB, typename STRING, typename GRID>
	static void SetRedirectCounts(unsigned int* counts, SizeExtractor&& extractor, const LINE& line, const SPHERE& sphere,
			const POINT& point, const RECTANGLE& rectangle, const CROSS& cross, const CIRCLE& circle, const ARROW& arrow,
			const TRIANGLE& triangle, const AABB_& aabb, const OOBB& oobb, const STRING& string, const GRID& grid)
	{
		static_assert(ECS_DEBUG_PRIMITIVE_COUNT == 12, "Update the AddRedirects code to account for the changes!");

		counts[ECS_DEBUG_PRIMITIVE_AABB] = extractor(aabb);
		counts[ECS_DEBUG_PRIMITIVE_ARROW] = extractor(arrow);
		counts[ECS_DEBUG_PRIMITIVE_CIRCLE] = extractor(circle);
		counts[ECS_DEBUG_PRIMITIVE_CROSS] = extractor(cross);
		counts[ECS_DEBUG_PRIMITIVE_GRID] = extractor(grid);
		counts[ECS_DEBUG_PRIMITIVE_LINE] = extractor(line);
		counts[ECS_DEBUG_PRIMITIVE_OOBB] = extractor(oobb);
		counts[ECS_DEBUG_PRIMITIVE_POINT] = extractor(point);
		counts[ECS_DEBUG_PRIMITIVE_RECTANGLE] = extractor(rectangle);
		counts[ECS_DEBUG_PRIMITIVE_SPHERE] = extractor(sphere);
		counts[ECS_DEBUG_PRIMITIVE_STRING] = extractor(string);
		counts[ECS_DEBUG_PRIMITIVE_TRIANGLE] = extractor(triangle);
	}
	
	struct RedirectThreadSizeExtractor {
		template<typename Parameter>
		ECS_INLINE unsigned int operator()(const Parameter& parameter) const {
			return parameter[thread_index].size;
		}
		
		unsigned int thread_index;
	};

	struct RedirectGlobalSizeExtractor {
		template<typename Parameter>
		ECS_INLINE unsigned int operator()(const Parameter& parameter) const {
			return parameter.GetElementCount();
		}
	};

	// It will add all the redirects into the specified drawer
	// For the global add, use thread_index of -1
	template<typename SizeExtractor, typename ValueExtractor, typename LINE, typename SPHERE, typename POINT, typename RECTANGLE, 
		typename CROSS, typename CIRCLE, typename ARROW, typename TRIANGLE, typename AABB_, typename OOBB, typename STRING, typename GRID>
	static void AddRedirects(const unsigned int* counts, unsigned int thread_index, SizeExtractor&& size_extractor, ValueExtractor&& value_extractor, 
		const LINE& line, const SPHERE& sphere, const POINT& point, const RECTANGLE& rectangle, const CROSS& cross, 
		const CIRCLE& circle, const ARROW& arrow, const TRIANGLE& triangle, const AABB_& aabb, const OOBB& oobb, 
		const STRING& string, const GRID& grid, DebugDrawer* redirect_drawer) 
	{
		static_assert(ECS_DEBUG_PRIMITIVE_COUNT == 12, "Update the AddRedirects code to account for the changes!");
		
		if (thread_index == -1) {
			thread_index = 0;
		}

		// Use the AddThread
		for (unsigned int index = counts[ECS_DEBUG_PRIMITIVE_LINE]; index < size_extractor(line); index++) {
			DebugLine value = value_extractor(line, index);
			redirect_drawer->AddLineThread(thread_index, value.start, value.end, value.color, value.options);
		}
		
		for (unsigned int index = counts[ECS_DEBUG_PRIMITIVE_SPHERE]; index < size_extractor(sphere); index++) {
			DebugSphere value = value_extractor(sphere, index);
			redirect_drawer->AddSphereThread(thread_index, value.position, value.radius, value.color, value.options);
		}

		//for (unsigned int index = counts[ECS_DEBUG_PRIMITIVE_POINT]; index < size_extractor(point); index++) {
		//	DebugPoint value = value_extractor(point, index);
		//	redirect_drawer->AddPointThread(thread_index, value.position, value.size, value.color, value.options);
		//}

		for (unsigned int index = counts[ECS_DEBUG_PRIMITIVE_RECTANGLE]; index < size_extractor(rectangle); index++) {
			DebugRectangle value = value_extractor(rectangle, index);
			redirect_drawer->AddRectangleThread(thread_index, value.corner0, value.corner1, value.color, value.options);
		}

		for (unsigned int index = counts[ECS_DEBUG_PRIMITIVE_CROSS]; index < size_extractor(cross); index++) {
			DebugCross value = value_extractor(cross, index);
			redirect_drawer->AddCrossThread(thread_index, value.position, value.rotation, value.size, value.color, value.options);
		}

		for (unsigned int index = counts[ECS_DEBUG_PRIMITIVE_CIRCLE]; index < size_extractor(circle); index++) {
			DebugCircle value = value_extractor(circle, index);
			redirect_drawer->AddCircleThread(thread_index, value.position, value.rotation, value.radius, value.color, value.options);
		}

		for (unsigned int index = counts[ECS_DEBUG_PRIMITIVE_ARROW]; index < size_extractor(arrow); index++) {
			DebugArrow value = value_extractor(arrow, index);
			redirect_drawer->AddArrowRotationThread(thread_index, value.translation, value.rotation, value.length, value.size, value.color, value.options);
		}

		for (unsigned int index = counts[ECS_DEBUG_PRIMITIVE_TRIANGLE]; index < size_extractor(triangle); index++) {
			DebugTriangle value = value_extractor(triangle, index);
			redirect_drawer->AddTriangleThread(thread_index, value.point0, value.point1, value.point2, value.color, value.options);
		}

		for (unsigned int index = counts[ECS_DEBUG_PRIMITIVE_AABB]; index < size_extractor(aabb); index++) {
			DebugAABB value = value_extractor(aabb, index);
			redirect_drawer->AddAABBThread(thread_index, value.translation - value.scale * float3::Splat(0.5f), 
				value.translation + value.scale * float3::Splat(0.5f), value.color, value.options);;
		}

		for (unsigned int index = counts[ECS_DEBUG_PRIMITIVE_OOBB]; index < size_extractor(oobb); index++) {
			DebugOOBB value = value_extractor(oobb, index);
			redirect_drawer->AddOOBBThread(thread_index, value.translation, value.rotation, value.scale, value.color, value.options);
		}

		for (unsigned int index = counts[ECS_DEBUG_PRIMITIVE_STRING]; index < size_extractor(string); index++) {
			DebugString value = value_extractor(string, index);
			redirect_drawer->AddStringThread(thread_index, value.position, value.direction, value.size, value.text, value.color, value.options);
		}

		for (unsigned int index = counts[ECS_DEBUG_PRIMITIVE_GRID]; index < size_extractor(grid); index++) {
			DebugGrid value = value_extractor(grid, index);
			// Retrieve the values if the has_valid_cells is set
			redirect_drawer->AddGridThread(thread_index, &value, value.has_valid_cells);
		}
	}

	struct RedirectThreadValueExtractor {
		template<typename Parameter>
		ECS_INLINE auto operator()(const Parameter& parameter, unsigned int index) const {
			return parameter[thread_index][index];
		}

		unsigned int thread_index;
	};

	struct RedirectGlobalValueExtractor {
		template<typename Parameter>
		ECS_INLINE auto operator()(const Parameter& parameter, unsigned int index) const {
			return parameter[index];
		}
	};

	// ----------------------------------------------------------------------------------------------------------------------

	ECS_INLINE unsigned char GetMaskFromOptions(DebugDrawOptions options) {
		return (unsigned char)options.ignore_depth | (((unsigned char)(!options.wireframe) << 1) & 2);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	ECS_INLINE unsigned int GetMaximumCount(unsigned int* counts) {
		return max(max(counts[WIREFRAME_DEPTH], counts[WIREFRAME_NO_DEPTH]), max(counts[SOLID_DEPTH], counts[SOLID_NO_DEPTH]));
	}

	// ----------------------------------------------------------------------------------------------------------------------

	ECS_INLINE float3 OOBBCrossSize(float length, float size, float override_value) {
		float size_value = override_value == 0.0f ? size : override_value;
		return float3(length, size_value * OOBB_CROSS_Y_SCALE, size_value * OOBB_CROSS_Z_SCALE);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	template<typename Element>
	ushort2* CalculateElementTypeCounts(
		MemoryManager* allocator, 
		unsigned int* counts, 
		ushort2** indices, 
		DeckPowerOfTwo<Element>* deck, 
		DebugShaderOutput shader_output
	) {
		unsigned int total_count = deck->GetElementCount();
		ushort2* type_indices = (ushort2*)allocator->Allocate(sizeof(ushort2) * total_count * 4);
		SetIndicesTypeMask(indices, type_indices, total_count);

		auto for_each_lambda = [&](ulong2 deck_indices, auto is_output_id) {
			const auto* value = deck->GetValuePtr(deck_indices.x, deck_indices.y);
			if constexpr (is_output_id) {
				if (GetTypeInstanceThickness(value) == -1) {
					return;
				}
			}
			unsigned char mask = GetMaskFromOptions(value->options);
			// Assigned the indices and update the count
			indices[mask][counts[mask]++] = { (unsigned short)deck_indices.x, (unsigned short)deck_indices.y };
		};

		if (shader_output == ECS_DEBUG_SHADER_OUTPUT_ID) {
			deck->ForEachIndex([&](ulong2 deck_indices) {
				for_each_lambda(deck_indices, std::true_type{});
			});
		}
		else {
			deck->ForEachIndex([&](ulong2 deck_indices) {
				for_each_lambda(deck_indices, std::false_type{});
			});
		}

		return type_indices;
	}

	// ----------------------------------------------------------------------------------------------------------------------

	static void SetLinePositions(InstancedVertex* positions, unsigned int instance_thickness, float3 start, float3 end) {
		unsigned int offset = instance_thickness * 2;

		positions[offset].position = start;
		positions[offset + 1].position = end;

		positions[offset].instance_thickness = instance_thickness;
		positions[offset + 1].instance_thickness = instance_thickness;
	}

	static void SetTrianglePositions(InstancedVertex* positions, unsigned int instance_thickness, float3 corner0, float3 corner1, float3 corner2) {
		unsigned int offset = instance_thickness * 3;

		positions[offset].position = corner0;
		positions[offset + 1].position = corner1;
		positions[offset + 2].position = corner2;

		positions[offset].instance_thickness = instance_thickness;
		positions[offset + 1].instance_thickness = instance_thickness;
		positions[offset + 2].instance_thickness = instance_thickness;
	}

	static void SetRectanglePositionsFront(InstancedVertex* positions, unsigned int instance_thickness, float3 corner0, float3 corner1) {
		unsigned int offset = instance_thickness * 6;

		// First rectangle triangle
		positions[offset + 0].position = corner0;
		positions[offset + 1].position = { corner1.x, corner0.y, corner1.z };
		positions[offset + 2].position = { corner0.x, corner1.y, corner0.z };

		// Second rectangle triangle
		positions[offset + 3].position = positions[offset + 1].position;
		positions[offset + 4].position = corner1;
		positions[offset + 5].position = positions[offset + 2].position;

		// The instance index
		positions[offset + 0].instance_thickness = instance_thickness;
		positions[offset + 1].instance_thickness = instance_thickness;
		positions[offset + 2].instance_thickness = instance_thickness;
		positions[offset + 3].instance_thickness = instance_thickness;
		positions[offset + 4].instance_thickness = instance_thickness;
		positions[offset + 5].instance_thickness = instance_thickness;
	}

	// ----------------------------------------------------------------------------------------------------------------------

	template<typename Element>
	void UpdateElementDurations(DeckPowerOfTwo<Element>* deck, float time_delta) {
		deck->ForEach<false, true>([time_delta](auto& element) {
			element.options.duration -= time_delta;
			if (element.options.duration < 0.0f) {
				return true;
			}
			return false;
		});
	}

	template<typename DebugType>
	ECS_INLINE Color GetTypeColor(const DebugType* type) {
		return type->color;
	}

	template<typename DebugType>
	ECS_INLINE unsigned int GetTypeInstanceThickness(const DebugType* type) {
		return type->options.instance_thickness;
	}

	template<typename Deck>
	ECS_INLINE bool DeckHasOutputID(const Deck* deck) {
		return deck->ForEach<true>([](const auto* element) {
			return GetTypeInstanceThickness(element) != -1;
			});
	}

	static void SetStructuredPositions(InstancedVertex* positions, unsigned int index, const DebugLine* line) {
		SetLinePositions(positions, index, line->start, line->end);
	}

	static void SetStructuredPositions(InstancedVertex* positions, unsigned int index, const DebugTriangle* triangle) {
		SetTrianglePositions(positions, index, triangle->point0, triangle->point1, triangle->point2);
	}

	static void SetStructuredPositions(InstancedVertex* positions, unsigned int index, const DebugRectangle* rectangle) {
		SetRectanglePositionsFront(positions, index, rectangle->corner0, rectangle->corner1);
	}

	// If the dynamic counts is set to true, it will keep retrieving items and filling
	// The GPU buffers directly without the need for allocations. This works only for a single
	// Type of draw - the options cannot be intermingled. It will also increment
	// the counts as needed. The functor receives (unsigned int index, ElementType type, bool* should_stop)
	// In both cases. When dynamic counts are activated, the functor must return nullptr
	// when the elements have been exhausted
	template<bool dynamic_counts, size_t flags = 0, typename GetElementFunctor>
	static void DrawStructuredDeckCore(
		DebugDrawer* drawer,
		unsigned int instance_count,
		unsigned int vertex_count,
		unsigned int* per_type_counts,
		DebugShaderOutput shader_output,
		GetElementFunctor&& get_element_functor,
		ElementType dynamic_options_type = ELEMENT_COUNT
	) {
		if constexpr (dynamic_counts) {
			instance_count = 1;
		}
		if (instance_count > 0) {
			Timer timer;

			if constexpr (dynamic_counts) {
				instance_count = 0;
				memset(per_type_counts, 0, sizeof(unsigned int) * ELEMENT_COUNT);
			}

			// Create temporary buffers to hold the data
			VertexBuffer position_buffer;
			if (instance_count * vertex_count <= LARGE_BUFFER_CAPACITY) {
				position_buffer = drawer->positions_large_vertex_buffer;
			}
			else {
				position_buffer = drawer->graphics->CreateVertexBuffer(sizeof(InstancedVertex), instance_count * vertex_count, true);
			}
			// Bind the vertex buffer and the structured buffer now
			drawer->graphics->BindVertexBuffer(position_buffer);

			StructuredBuffer instanced_buffer;
			StructuredBuffer matrix_buffer;
			StructuredBuffer instance_pixel_thickness_buffer;
			ResourceView instanced_view;
			ResourceView matrix_view;
			ResourceView instance_pixel_thickness_view;

			if (shader_output == ECS_DEBUG_SHADER_OUTPUT_COLOR) {
				if (instance_count <= LARGE_BUFFER_CAPACITY) {
					instanced_buffer = drawer->output_large_instanced_structured_buffer;
					instanced_view = drawer->output_large_instanced_buffer_view;
				}
				else {
					instanced_buffer = drawer->graphics->CreateStructuredBuffer(sizeof(InstancedTransformData), instance_count, true);
					instanced_view = drawer->graphics->CreateBufferView(instanced_buffer, true);
				}
				drawer->graphics->BindVertexResourceView(instanced_view);
			}
			else {
				if (instance_count <= LARGE_BUFFER_CAPACITY) {
					matrix_buffer = drawer->output_large_matrix_structured_buffer;
					instance_pixel_thickness_buffer = drawer->output_large_id_structured_buffer;
					matrix_view = drawer->output_large_matrix_buffer_view;
					instance_pixel_thickness_view = drawer->output_large_id_buffer_view;
				}
				else {
					matrix_buffer = drawer->graphics->CreateStructuredBuffer(sizeof(Matrix), instance_count, true);
					instance_pixel_thickness_buffer = drawer->graphics->CreateStructuredBuffer(sizeof(unsigned int), instance_count, true);
					matrix_view = drawer->graphics->CreateBufferView(matrix_buffer, true);
					instance_pixel_thickness_view = drawer->graphics->CreateBufferView(instance_pixel_thickness_buffer, true);
				}
				ResourceView vertex_views[] = {
					matrix_view,
					instance_pixel_thickness_view
				};
				drawer->graphics->BindVertexResourceViews({ vertex_views, ECS_COUNTOF(vertex_views) });
			}

			Matrix gpu_camera = MatrixGPU(drawer->camera_matrix);
			timer.SetNewStart();

			InstancedVertex* positions = nullptr;
			void* instanced_data = nullptr;
			Matrix* matrix_data = nullptr;
			unsigned int* instance_pixel_thickness_data = nullptr;

			auto draw_call = [&](DebugDrawOptions options, ElementType element_type) {
				unsigned int current_count = per_type_counts[element_type];
				bool exit_dynamic_loop = false;
				unsigned int overall_dynamic_count = 0;
				if constexpr (dynamic_counts) {
					if (element_type == dynamic_options_type) {
						current_count = 1;
					}
				}

				while (current_count > 0 && !exit_dynamic_loop) {
					if constexpr (dynamic_counts) {
						current_count = 0;
					}

					// Map the buffers
					positions = (InstancedVertex*)drawer->graphics->MapBuffer(position_buffer.buffer);
					if (shader_output == ECS_DEBUG_SHADER_OUTPUT_COLOR) {
						instanced_data = drawer->graphics->MapBuffer(instanced_buffer.buffer);
					}
					else {
						matrix_data = (Matrix*)drawer->graphics->MapBuffer(matrix_buffer.buffer);
						instance_pixel_thickness_data = (unsigned int*)drawer->graphics->MapBuffer(instance_pixel_thickness_buffer.buffer);
					}

					// Returns a bool2 where the x says whether or not an element was drawn or not
					// And in the y if the iteration should stop
					auto iteration = [&](unsigned int index) {
						bool should_stop = false;
						const auto* element = get_element_functor(index, element_type, &should_stop);
						if constexpr (dynamic_counts) {
							if (element == nullptr) {
								return bool2(false, !should_stop);
							}
						}
						SetStructuredPositions(positions, index, element);
						if (shader_output == ECS_DEBUG_SHADER_OUTPUT_COLOR) {
							SetInstancedColor(instanced_data, index, GetTypeColor(element));
							SetInstancedMatrix(instanced_data, index, gpu_camera);
						}
						else {
							gpu_camera.Store(matrix_data + index);
							instance_pixel_thickness_data[index] = GetTypeInstanceThickness(element);
						}
						return bool2(true, !should_stop);
					};

					if constexpr (dynamic_counts) {
						bool2 iteration_result = iteration(overall_dynamic_count);
						while (iteration_result.y && current_count < LARGE_BUFFER_CAPACITY) {
							if (iteration_result.x) {
								current_count++;
								overall_dynamic_count++;
							}
							iteration_result = iteration(overall_dynamic_count);
						}
						if (!iteration_result.y) {
							exit_dynamic_loop = true;
						}
					}
					else {
						// Fill the buffers
						for (unsigned int index = 0; index < current_count; index++) {
							iteration(index);
						}
					}

					// Unmap the buffers
					drawer->graphics->UnmapBuffer(position_buffer.buffer);
					if (shader_output == ECS_DEBUG_SHADER_OUTPUT_COLOR) {
						drawer->graphics->UnmapBuffer(instanced_buffer.buffer);
					}
					else {
						drawer->graphics->UnmapBuffer(matrix_buffer.buffer);
						drawer->graphics->UnmapBuffer(instance_pixel_thickness_buffer.buffer);
					}

					// Set the state
					BindStructuredShaderState<flags>(drawer, options, shader_output);

					// Issue the draw call
					drawer->DrawCallStructured(vertex_count, current_count);
					if constexpr (!dynamic_counts) {
						// Exit the loop
						exit_dynamic_loop = true;
					}
					else {
						current_count = 1;
					}
				}
			};

			// Draw all the Wireframe depth elements
			draw_call({ true, false }, WIREFRAME_DEPTH);

			// Draw all the Wireframe no depth elements
			draw_call({ true, true }, WIREFRAME_NO_DEPTH);

			// Draw all the Solid depth elements
			draw_call({ false, false }, SOLID_DEPTH);

			// Draw all the Solid no depth elements
			draw_call({ false, true }, SOLID_NO_DEPTH);

			// Release the temporary vertex buffer, structured buffer and the temporary allocation
			if (instance_count * vertex_count > LARGE_BUFFER_CAPACITY) {
				position_buffer.Release();
			}
			if (shader_output == ECS_DEBUG_SHADER_OUTPUT_COLOR) {
				if (instance_count > LARGE_BUFFER_CAPACITY) {
					instanced_view.Release();
					instanced_buffer.Release();
				}
			}
			else {
				if (instance_count > LARGE_BUFFER_CAPACITY) {
					matrix_view.Release();
					matrix_buffer.Release();
					instance_pixel_thickness_view.Release();
					instance_pixel_thickness_buffer.Release();
				}
			}
		}
	}

	// The functor receives as parameters (InstancedVertex* positions, unsigned int index, const auto* element)
	// and must fill in the positions accordingly
	template<size_t flags = 0, typename DeckType, typename SetPositionsFunctor>
	void DrawStructuredDeck(DebugDrawer* drawer, DeckType* deck, unsigned int vertex_count, float time_delta, DebugShaderOutput shader_output, SetPositionsFunctor&& set_positions_functor) {
		// Calculate the maximum amount of vertices needed for every type
		// and also get an integer mask to indicate the elements for each type
		unsigned int counts[ELEMENT_COUNT] = { 0 };
		ushort2* indices[ELEMENT_COUNT];
		unsigned int total_count = deck->GetElementCount();

		if (total_count > 0) {
			// Allocate 4 times the memory needed to be sure that each type has enough indices
			ushort2* type_indices = CalculateElementTypeCounts(drawer->allocator, counts, indices, deck, shader_output);

			// Get the max
			unsigned int instance_count = GetMaximumCount(counts);

			if (instance_count > 0) {
				DrawStructuredDeckCore<false, flags>(drawer, instance_count, vertex_count, counts, shader_output, 
					[deck, indices](unsigned int index, ElementType element_type, bool* should_stop) {
					return &deck->buffers[indices[element_type][index].x][indices[element_type][index].y];
				});
				// Update the duration and remove those elements that expired
				if (time_delta != 0.0f) {
					UpdateElementDurations(deck, time_delta);
				}
			}
			drawer->allocator->Deallocate(type_indices);
		}
	}

	// If the dynamic counts is set to true, it will keep retrieving items and filling
	// The GPU buffers directly without the need for allocations. This works only for a single
	// Type of draw - the options cannot be intermingled. It will also increment
	// the counts as needed. The functor receives (unsigned int index, ElementType type, bool* should_stop)
	// In both cases. When dynamic counts are activated, the functor must return nullptr
	// when the elements have been exhausted
	template<bool dynamic_counts, size_t flags = 0, typename GetElementFunctor>
	static void DrawTransformCallCore(
		DebugDrawer* drawer,
		unsigned int instance_count,
		unsigned int* per_type_counts,
		DebugShaderOutput shader_output,
		DebugVertexBuffers debug_vertex_buffer,
		GetElementFunctor&& get_element_functor,
		ElementType dynamic_options_type = ELEMENT_COUNT
	) {
		if constexpr (dynamic_counts) {
			instance_count = 1;
		}
		if (instance_count > 0) {
			if constexpr (dynamic_counts) {
				instance_count = 0;
				memset(per_type_counts, 0, sizeof(unsigned int) * ELEMENT_COUNT);
			}

			VertexBuffer instanced_buffer;
			VertexBuffer matrix_buffer;
			VertexBuffer instance_id_buffer;

			// Create temporary buffers to hold the data
			if (shader_output == ECS_DEBUG_SHADER_OUTPUT_COLOR) {
				if (instance_count <= LARGE_BUFFER_CAPACITY) {
					instanced_buffer = drawer->instanced_large_vertex_buffer;
				}
				else {
					instanced_buffer = drawer->graphics->CreateVertexBuffer(sizeof(InstancedTransformData), instance_count, true);
				}
				drawer->graphics->BindVertexBuffer(instanced_buffer, 1);
			}
			else {
				if (instance_count <= LARGE_BUFFER_CAPACITY) {
					matrix_buffer = drawer->output_large_matrix_vertex_buffer;
					instance_id_buffer = drawer->output_large_id_vertex_buffer;
				}
				else {
					matrix_buffer = drawer->graphics->CreateVertexBuffer(sizeof(Matrix), instance_count, true);
					instance_id_buffer = drawer->graphics->CreateVertexBuffer(sizeof(unsigned int), instance_count, true);
				}

				drawer->graphics->BindVertexBuffer(matrix_buffer, 1);
				drawer->graphics->BindVertexBuffer(instance_id_buffer, 2);
			}

			void* instanced_data = nullptr;
			Matrix* matrix_data = nullptr;
			unsigned int* instance_id_data = nullptr;

			unsigned int mesh_index_count = BindTransformVertexBuffers<flags>(drawer, debug_vertex_buffer);

			auto draw_call = [&](DebugDrawOptions options, ElementType element_type) {
				unsigned int current_count = per_type_counts[element_type];
				bool exit_dynamic_loop = false;
				unsigned int overall_dynamic_count = 0;
				if constexpr (dynamic_counts) {
					if (element_type == dynamic_options_type) {
						current_count = 1;
					}
				}
				while (current_count > 0 && !exit_dynamic_loop) {
					if constexpr (dynamic_counts) {
						current_count = 0;
					}

					// Map the buffers
					if (shader_output == ECS_DEBUG_SHADER_OUTPUT_COLOR) {
						instanced_data = drawer->graphics->MapBuffer(instanced_buffer.buffer);
					}
					else {
						matrix_data = (Matrix*)drawer->graphics->MapBuffer(matrix_buffer.buffer);
						instance_id_data = (unsigned int*)drawer->graphics->MapBuffer(instance_id_buffer.buffer);
					}

					// Returns a bool2 where the x says whether or not an element was drawn or not
					// And in the y if the iteration should stop
					auto iteration = [&](unsigned int index) {
						bool should_stop = false;
						const auto* element = get_element_functor(index, element_type, &should_stop);
						if constexpr (dynamic_counts) {
							if (element == nullptr) {
								return bool2(false, !should_stop);
							}
						}
						Matrix current_matrix = element->GetMatrix(drawer->camera_matrix);
						if (shader_output == ECS_DEBUG_SHADER_OUTPUT_COLOR) {
							SetInstancedColor(instanced_data, index, GetTypeColor(element));
							SetInstancedMatrix(instanced_data, index, current_matrix);
						}
						else {
							current_matrix.Store(matrix_data + index);
							instance_id_data[index] = GetTypeInstanceThickness(element);
						}
						return bool2(true, !should_stop);
					};

					if constexpr (dynamic_counts) {
						bool2 iteration_result = iteration(overall_dynamic_count);
						while (iteration_result.y && current_count < LARGE_BUFFER_CAPACITY) {
							if (iteration_result.x) {
								current_count++;
								overall_dynamic_count++;
							}
							iteration_result = iteration(overall_dynamic_count);
						}
						if (!iteration_result.y) {
							exit_dynamic_loop = true;
						}
					}
					else {
						// Fill the buffers
						for (unsigned int index = 0; index < current_count; index++) {
							iteration(index);
						}
					}

					// Unmap the buffers
					if (shader_output == ECS_DEBUG_SHADER_OUTPUT_COLOR) {
						drawer->graphics->UnmapBuffer(instanced_buffer.buffer);
					}
					else {
						drawer->graphics->UnmapBuffer(matrix_buffer.buffer);
						drawer->graphics->UnmapBuffer(instance_id_buffer.buffer);
					}

					drawer->SetTransformShaderState(options, shader_output);
					DrawCallTransformFlags<flags>(drawer, mesh_index_count, current_count);
					if constexpr (!dynamic_counts) {
						// Exit the loop
						exit_dynamic_loop = true;
					}
					else {
						current_count = 1;
					}
				}
			};

			// Draw all the Wireframe depth elements
			draw_call({ true, false }, WIREFRAME_DEPTH);

			// Draw all the Wireframe no depth elements
			draw_call({ true, true }, WIREFRAME_NO_DEPTH);

			// Draw all the Solid depth elements
			draw_call({ false, false }, SOLID_DEPTH);

			// Draw all the Solid no depth elements
			draw_call({ false, true }, SOLID_NO_DEPTH);

			// Release the temporary vertex buffers and the temporary allocation
			if (shader_output == ECS_DEBUG_SHADER_OUTPUT_COLOR) {
				if (instance_count > LARGE_BUFFER_CAPACITY) {
					instanced_buffer.Release();
				}
			}
			else {
				if (instance_count > LARGE_BUFFER_CAPACITY) {
					matrix_buffer.Release();
					instance_id_buffer.Release();
				}
			}
		}
	}

	// The functor receives as parameters (const auto* element) and must return a matrix
	template<size_t flags = 0, typename DeckElement>
	static void DrawTransformDeck(
		DebugDrawer* drawer,
		DeckPowerOfTwo<DeckElement>* deck,
		DebugVertexBuffers debug_vertex_buffer,
		float time_delta,
		DebugShaderOutput shader_output
	) {
		// Calculate the maximum amount of vertices needed for every type
		// and also get an integer mask to indicate the elements for each type
		unsigned int counts[ELEMENT_COUNT] = { 0 };
		ushort2* indices[ELEMENT_COUNT];
		unsigned int total_count = deck->GetElementCount();

		if (total_count > 0) {
			// Allocate 4 times the memory needed to be sure that each type has enough indices
			ushort2* type_indices = CalculateElementTypeCounts(drawer->allocator, counts, indices, deck, shader_output);

			// Get the max
			unsigned int instance_count = GetMaximumCount(counts);

			if (instance_count > 0) {
				// Bind the vertex buffers now
				DrawTransformCallCore<false, flags>(drawer, instance_count, counts, shader_output, debug_vertex_buffer, 
					[deck, indices](unsigned int index, ElementType element_type, bool* should_stop) {
					return &deck->buffers[indices[element_type][index].x][indices[element_type][index].y];
					});
				// Update the duration and remove those elements that expired
				if (time_delta != 0.0f) {
					UpdateElementDurations(deck, time_delta);
				}
			}
			drawer->allocator->Deallocate(type_indices);
		}
	}

	// ----------------------------------------------------------------------------------------------------------------------

	ECS_INLINE InstancedVertex* MapPositions(DebugDrawer* drawer) {
		return (InstancedVertex*)drawer->graphics->MapBuffer(drawer->positions_small_vertex_buffer.buffer);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	// Can be used as InstancedTransformData or plain Color information for world position shader
	ECS_INLINE InstancedTransformData* MapInstancedTransformVertex(DebugDrawer* drawer) {
		return (InstancedTransformData*)drawer->graphics->MapBuffer(drawer->instanced_small_transform_vertex_buffer.buffer);
	}

	ECS_INLINE InstancedTransformData* MapInstancedStructured(DebugDrawer* drawer) {
		return (InstancedTransformData*)drawer->graphics->MapBuffer(drawer->instanced_small_structured_buffer.buffer);
	}

	ECS_INLINE Matrix* MapOutputMatrixVertex(DebugDrawer* drawer) {
		return (Matrix*)drawer->graphics->MapBuffer(drawer->output_instance_small_matrix_buffer.buffer);
	}

	ECS_INLINE Matrix* MapOutputMatrixStructured(DebugDrawer* drawer) {
		return (Matrix*)drawer->graphics->MapBuffer(drawer->output_instance_matrix_small_structured_buffer.buffer);
	}
	
	ECS_INLINE unsigned int* MapOutputIDVertex(DebugDrawer* drawer) {
		return (unsigned int*)drawer->graphics->MapBuffer(drawer->output_instance_small_id_buffer.buffer);
	}

	ECS_INLINE unsigned int* MapOutputIDStructured(DebugDrawer* drawer) {
		return (unsigned int*)drawer->graphics->MapBuffer(drawer->output_instance_id_small_structured_buffer.buffer);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	ECS_INLINE void UnmapPositions(DebugDrawer* drawer) {
		drawer->graphics->UnmapBuffer(drawer->positions_small_vertex_buffer.buffer);
	}

	ECS_INLINE void UnmapInstancedVertex(DebugDrawer* drawer) {
		drawer->graphics->UnmapBuffer(drawer->instanced_small_transform_vertex_buffer.buffer);
	}

	ECS_INLINE void UnmapInstancedStructured(DebugDrawer* drawer) {
		drawer->graphics->UnmapBuffer(drawer->instanced_small_structured_buffer.buffer);
	}

	ECS_INLINE void UnmapOutputMatrixVertex(DebugDrawer* drawer) {
		drawer->graphics->UnmapBuffer(drawer->output_instance_small_matrix_buffer.buffer);
	}

	ECS_INLINE void UnmapOutputMatrixStructured(DebugDrawer* drawer) {
		drawer->graphics->UnmapBuffer(drawer->output_instance_matrix_small_structured_buffer.buffer);
	}

	ECS_INLINE void UnmapOutputIDVertex(DebugDrawer* drawer) {
		drawer->graphics->UnmapBuffer(drawer->output_instance_small_id_buffer.buffer);
	}

	ECS_INLINE void UnmapOutputIDStructured(DebugDrawer* drawer) {
		drawer->graphics->UnmapBuffer(drawer->output_instance_id_small_structured_buffer.buffer);
	}

	ECS_INLINE void UnmapAllStructuredColor(DebugDrawer* drawer) {
		UnmapPositions(drawer);
		UnmapInstancedStructured(drawer);
	}

	ECS_INLINE void UnmapAllTransformOutputID(DebugDrawer* drawer) {
		UnmapOutputIDVertex(drawer);
		UnmapOutputMatrixVertex(drawer);
	}

	ECS_INLINE void UnmapAllStructuredOutputID(DebugDrawer* drawer) {
		UnmapPositions(drawer);
		UnmapOutputIDStructured(drawer);
		UnmapOutputMatrixStructured(drawer);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	// Sets the color of the instaced data if the stream has a color and matrix as instanced data
	void SetInstancedColor(void* pointer, unsigned int index, Color color) {
		InstancedTransformData* data = (InstancedTransformData*)pointer;
		data[index].color = color;
	}

	// Sets the matrix of the instaced data if the stream has a color and matrix as instanced data
	void ECS_VECTORCALL SetInstancedMatrix(void* pointer, unsigned int index, Matrix matrix) {
		InstancedTransformData* data = (InstancedTransformData*)pointer;
		matrix.Store(data + index);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void HandleOptions(DebugDrawer* drawer, DebugDrawOptions options) {
		drawer->graphics->BindRasterizerState(drawer->rasterizer_states[options.wireframe]);
		if (options.ignore_depth) {
			drawer->graphics->DisableDepth();
		}
		else {
			drawer->graphics->EnableDepth();
		}
	}

	// ----------------------------------------------------------------------------------------------------------------------

	ECS_INLINE void BindSmallStructuredBuffersColor(DebugDrawer* drawer) {
		drawer->graphics->BindVertexBuffer(drawer->positions_small_vertex_buffer);
		drawer->graphics->BindVertexResourceView(drawer->instanced_structured_view);
	}

	ECS_INLINE void BindSmallStructuredBuffersOutputID(DebugDrawer* drawer) {
		drawer->graphics->BindVertexBuffer(drawer->positions_small_vertex_buffer);
		drawer->graphics->BindVertexResourceView(drawer->output_instance_matrix_structured_view);
		drawer->graphics->BindVertexResourceView(drawer->output_instance_id_structured_view, 1);
	}

	ECS_INLINE void BindSmallTransformBuffersOuptutID(DebugDrawer* drawer) {
		VertexBuffer buffers[2] = {
			drawer->output_instance_small_matrix_buffer,
			drawer->output_instance_small_id_buffer
		};
		drawer->graphics->BindVertexBuffers({ buffers, ECS_COUNTOF(buffers) }, 1);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	ECS_INLINE Matrix ECS_VECTORCALL SphereMatrix(float3 translation, float size, Matrix camera_matrix) {
		return MatrixMVPToGPU(MatrixTS(MatrixTranslation(translation), MatrixScale(float3::Splat(size))), camera_matrix);
	}

	ECS_INLINE Matrix ECS_VECTORCALL PointMatrix(float3 translation, Matrix camera_matrix) {
		// The same as the sphere
		return SphereMatrix(translation, POINT_SIZE, camera_matrix);
	}

	ECS_INLINE Matrix ECS_VECTORCALL CrossMatrix(float3 position, QuaternionScalar rotation, float size, Matrix camera_matrix) {
		return MatrixMVPToGPU(
			MatrixTranslation(position),
			QuaternionToMatrix(rotation),
			MatrixScale(float3::Splat(size)),
			camera_matrix
		);
	}

	ECS_INLINE Matrix ECS_VECTORCALL CircleMatrix(float3 position, QuaternionScalar rotation, float radius, Matrix camera_matrix) {
		// The same as the cross
		return CrossMatrix(position, rotation, radius, camera_matrix);
	}

	ECS_INLINE Matrix ECS_VECTORCALL AABBMatrix(float3 position, float3 scale, Matrix camera_matrix) {
		return MatrixMVPToGPU(MatrixTS(MatrixTranslation(position), MatrixScale(scale)), camera_matrix);
	}

	ECS_INLINE Matrix ECS_VECTORCALL OOBBMatrix(float3 position, QuaternionScalar rotation, float3 scale, Matrix camera_matrix) {
		return MatrixMVPToGPU(
			MatrixTranslation(position),
			QuaternionToMatrix(rotation),
			MatrixScale(scale),
			camera_matrix
		);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void ConstructTesselatedCircle(float3* positions, float radius, size_t tesselation_count) {
		float angle_increment = 2 * PI / tesselation_count;
		for (size_t index = 0; index < tesselation_count; index++) {
			float current_angle = angle_increment * index;
			positions[index].x = cos(current_angle);
			positions[index].y = 0.0f;
			positions[index].z = sin(current_angle);
		}
	}

	// ----------------------------------------------------------------------------------------------------------------------

	// QuaternionScalar rotation and the length is in the w component
	void ArrowStartEndToRotation(float3 start, float3 end, QuaternionScalar* rotation, float* length) {
		float4 result;

		float3 direction = end - start;
		QuaternionScalar rotation_quat = DirectionToQuaternion(Normalize(direction));
		*rotation = rotation_quat;
		*length = Length(direction);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	// The ushort2* indices that will indicate the positions inside the deck
	void SetIndicesTypeMask(ushort2** pointers, ushort2* indices, unsigned int total_count) {
		pointers[WIREFRAME_DEPTH] = indices;
		pointers[WIREFRAME_NO_DEPTH] = indices + total_count;
		pointers[SOLID_DEPTH] = indices + 2 * total_count;
		pointers[SOLID_NO_DEPTH] = indices + 3 * total_count;
	}

	// ----------------------------------------------------------------------------------------------------------------------

	ECS_INLINE Matrix ECS_VECTORCALL ArrowMatrix(float3 translation, QuaternionScalar rotation, float length, float size, Matrix camera_matrix) {
		Matrix rotation_matrix = QuaternionToMatrix(rotation);
		float3 current_translation = translation;
		return MatrixMVPToGPU(
			MatrixTranslation(current_translation),
			rotation_matrix,
			MatrixScale(length, size, size),
			camera_matrix
		);
	}

	ECS_INLINE Matrix ECS_VECTORCALL OOBBCrossMatrix(
		float3 translation, 
		QuaternionScalar rotation, 
		float length, 
		float size, 
		float size_override, 
		Matrix camera_matrix
	) {
		Matrix rotation_matrix = QuaternionToMatrix(rotation);
		float3 current_translation = translation;
		return MatrixMVPToGPU(
			MatrixTranslation(current_translation),
			rotation_matrix,
			MatrixScale(OOBBCrossSize(length, size, size_override)),
			camera_matrix
		);
	}

	ECS_INLINE QuaternionScalar ECS_VECTORCALL AxesArrowXRotationSIMD(QuaternionScalar rotation) {
		return rotation;
	}

	ECS_INLINE QuaternionScalar AxesArrowXRotation(QuaternionScalar rotation) {
		return rotation;
	}

	ECS_INLINE QuaternionScalar ECS_VECTORCALL AxesArrowYRotationSIMD(QuaternionScalar rotation) {
		return QuaternionAngleFromAxis(GetForwardVector(), 90.0f) * rotation;
	}

	ECS_INLINE QuaternionScalar AxesArrowYRotation(QuaternionScalar rotation) {
		return AxesArrowYRotationSIMD(rotation);
	}

	ECS_INLINE QuaternionScalar ECS_VECTORCALL AxesArrowZRotationSIMD(QuaternionScalar rotation) {
		return QuaternionAngleFromAxis(GetUpVector(), -90.0f) * rotation;
	}

	ECS_INLINE QuaternionScalar AxesArrowZRotation(QuaternionScalar rotation) {
		return AxesArrowZRotationSIMD(rotation);
	}

	float3 OOBBCrossTranslation(float3 translation, QuaternionScalar rotation, float length, bool start_from_same_point) {
		// Offset in the direction of the by half of the length - if start from same point
		if (start_from_same_point) {
			float3 direction = RotateVector(GetRightVector(), rotation);
			return translation + direction * float3::Splat(length);
		}
		else {
			return translation;
		}
	}

	ECS_INLINE float3 OOBBCrossXTranslation(float3 translation, QuaternionScalar rotation, float length, bool start_from_same_point) {
		return OOBBCrossTranslation(translation, AxesArrowXRotationSIMD(rotation), length, start_from_same_point);
	}

	ECS_INLINE float3 OOBBCrossYTranslation(float3 translation, QuaternionScalar rotation, float length, bool start_from_same_point) {
		return OOBBCrossTranslation(translation, AxesArrowYRotationSIMD(rotation), length, start_from_same_point);
	}

	ECS_INLINE float3 OOBBCrossZTranslation(float3 translation, QuaternionScalar rotation, float length, bool start_from_same_point) {
		return OOBBCrossTranslation(translation, AxesArrowZRotationSIMD(rotation), length, start_from_same_point);
	}

	void ConvertTranslationLineIntoStartEnd(float3 translation, QuaternionScalar rotation, float size, float3& start, float3& end) {
		start = translation;
		float3 direction = RotateVector(GetRightVector(), rotation);
		end = translation + float3::Splat(size) * direction;
	}

	DebugLine ConvertTranslationLineIntoStartEnd(float3 translation, QuaternionScalar rotation, float size, Color color, DebugDrawOptions options) {
		DebugLine debug_line;

		ConvertTranslationLineIntoStartEnd(translation, rotation, size, debug_line.start, debug_line.end);
		debug_line.color = color;
		debug_line.options = options;
			
		return debug_line;
	}

	// ----------------------------------------------------------------------------------------------------------------------

	DebugDrawer::DebugDrawer(MemoryManager* allocator, ResourceManager* resource_manager, size_t thread_count) {
		Initialize(allocator, resource_manager, thread_count);
	}

#pragma region Add to the queue - single threaded

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::AddLine(float3 start, float3 end, Color color, DebugDrawOptions options)
	{
		lines.Add({ start, end, color, options });
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::AddLine(float3 translation, QuaternionScalar rotation, float size, Color color, DebugDrawOptions options)
	{
		float3 start, end;
		ConvertTranslationLineIntoStartEnd(translation, rotation, size, start, end);
		AddLine(start, end, color, options);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::AddSphere(float3 position, float radius, Color color, DebugDrawOptions options)
	{
		spheres.Add({ position, radius, color, options });
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::AddPoint(float3 position, float size, Color color, DebugDrawOptions options)
	{
		// If using points again, update the code at AddRedirect to consider points!!

		// Make wireframe false
		options.wireframe = false;
		AddSphere(position, POINT_SIZE * size, color, options);
		//points.Add({ position, color, options });
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::AddRectangle(float3 corner0, float3 corner1, Color color, DebugDrawOptions options)
	{
		rectangles.Add({ corner0, corner1, color, options });
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::AddCross(float3 position, QuaternionScalar rotation, float size, Color color, DebugDrawOptions options)
	{
		crosses.Add({ position, rotation, size, color, options });
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::AddOOBBCross(
		float3 position, 
		QuaternionScalar rotation, 
		float length,
		float size, 
		bool start_from_same_point, 
		const DebugOOBBCrossInfo* info,
		DebugDrawOptions options
	)
	{
		QuaternionScalar rotation_quaternion = rotation;
		float3 complete_size_x = OOBBCrossSize(length, size, info->size_override_x);
		float3 complete_size_y = OOBBCrossSize(length, size, info->size_override_y);
		float3 complete_size_z = OOBBCrossSize(length, size, info->size_override_z);

		DebugDrawOptions options_x = options;
		DebugDrawOptions options_y = options;
		DebugDrawOptions options_z = options;

		options_x.instance_thickness = info->instance_thickness_x;
		options_y.instance_thickness = info->instance_thickness_y;
		options_z.instance_thickness = info->instance_thickness_z;

		AddOOBB(
			OOBBCrossXTranslation(position, rotation_quaternion, length, start_from_same_point),
			AxesArrowXRotation(rotation), 
			complete_size_x, 
			info->color_x, 
			options_x
		);
		AddOOBB(
			OOBBCrossYTranslation(position, rotation_quaternion, length, start_from_same_point), 
			AxesArrowYRotation(rotation), 
			complete_size_y, 
			info->color_y, 
			options_y
		);
		AddOOBB(
			OOBBCrossZTranslation(position, rotation_quaternion, length, start_from_same_point), 
			AxesArrowZRotation(rotation), 
			complete_size_z, 
			info->color_z, 
			options_z
		);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::AddCircle(float3 position, QuaternionScalar rotation, float radius, Color color, DebugDrawOptions options)
	{
		circles.Add({ position, rotation, radius, color, options });
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::AddArrow(float3 start, float3 end, float size, Color color, DebugDrawOptions options)
	{
		QuaternionScalar rotation;
		float length;
		ArrowStartEndToRotation(start, end, &rotation, &length);
		AddArrowRotation(start, rotation, length, size, color, options);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::AddArrowRotation(float3 translation, QuaternionScalar rotation, float length, float size, Color color, DebugDrawOptions options)
	{
		arrows.Add({ translation, rotation, length, size, color, options });
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::AddAxes(float3 translation, QuaternionScalar rotation, float size, const DebugAxesInfo* info, DebugDrawOptions options)
	{
		DebugDrawOptions options_x = options;
		DebugDrawOptions options_y = options;
		DebugDrawOptions options_z = options;

		options_x.instance_thickness = info->instance_thickness_x;
		options_y.instance_thickness = info->instance_thickness_y;
		options_z.instance_thickness = info->instance_thickness_z;

		// The same as adding 3 arrows
		AddArrowRotation(translation, AxesArrowXRotation(rotation), size, size, info->color_x, options_x);
		AddArrowRotation(translation, AxesArrowYRotation(rotation), size, size, info->color_y, options_y);
		AddArrowRotation(translation, AxesArrowZRotation(rotation), size, size, info->color_z, options_z);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::AddTriangle(float3 point0, float3 point1, float3 point2, Color color, DebugDrawOptions options)
	{
		triangles.Add({ point0, point1, point2, color, options });
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::AddAABB(float3 min_coordinates, float3 max_coordinates, Color color, DebugDrawOptions options)
	{
		aabbs.Add({ min_coordinates, max_coordinates, color, options });
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::AddOOBB(float3 translation, QuaternionScalar rotation, float3 scale, Color color, DebugDrawOptions options)
	{
		oobbs.Add({ translation, rotation, scale, color, options });
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::AddString(float3 position, float3 direction, float size, Stream<char> text, Color color, DebugDrawOptions options)
	{
		Stream<char> text_copy = StringCopy(Allocator(), text);
		strings.Add({ position, direction, size, text_copy, color, options });
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::AddStringRotation(float3 position, QuaternionScalar rotation, float size, Stream<char> text, Color color, DebugDrawOptions options)
	{
		Stream<char> text_copy = StringCopy(Allocator(), text);
		float3 direction = RotateVector(GetRightVector(), rotation);
		AddString(position, direction, size, text, color, options);
	}

	void DebugDrawer::AddGrid(const DebugGrid* grid, bool retrieve_entries_now)
	{
		DebugGrid temp_grid = *grid;
		if (retrieve_entries_now) {
			temp_grid.ExtractResidentCells(Allocator());
		}
		grids.Add(&temp_grid);
	}


	// ----------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Add to queue - multi threaded

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::AddLineThread(unsigned int thread_index, float3 start, float3 end, Color color, DebugDrawOptions options)
	{
		// Check to see if the queue is full
		// If it is, then a flush is needed to clear the queue
		if (thread_lines[thread_index].IsFull()) {
			FlushLine(thread_index);
		}
		thread_lines[thread_index].Add({ start, end, color, options });
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::AddLineThread(unsigned int thread_index, float3 translation, QuaternionScalar rotation, float size, Color color, DebugDrawOptions options)
	{
		float3 start, end;
		ConvertTranslationLineIntoStartEnd(translation, rotation, size, start, end);
		AddLineThread(thread_index, start, end, color, options);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::AddSphereThread(unsigned int thread_index, float3 position, float radius, Color color, DebugDrawOptions options)
	{
		// Check to see if the queue is full
		// If it is, then a flush is needed to clear the queue
		if (thread_spheres[thread_index].IsFull()) {
			FlushSphere(thread_index);
		}
		thread_spheres[thread_index].Add({ position, radius, color, options });
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::AddPointThread(unsigned int thread_index, float3 position, float size, Color color, DebugDrawOptions options)
	{
		// TODO: Make the point depth size independent? For a quick momentary fix, let the user specify the size
		// If using points again, update the code at AddRedirect to consider points!!

		// Make wireframe false
		options.wireframe = false;
		AddSphereThread(thread_index, position, POINT_SIZE * size, color, options);

	//	// Check to see if the queue is full
	//	// If it is, then a flush is needed to clear the queue
	//	if (thread_points[thread_index].IsFull()) {
	//		FlushPoint(thread_index);
	//	}
	//	thread_points[thread_index].Add({ position, color, options });
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::AddRectangleThread(unsigned int thread_index, float3 corner0, float3 corner1, Color color, DebugDrawOptions options)
	{
		// Check to see if the queue is full
		// If it is, then a flush is needed to clear the queue
		if (thread_rectangles[thread_index].IsFull()) {
			FlushRectangle(thread_index);
		}
		thread_rectangles[thread_index].Add({ corner0, corner1, color, options });
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::AddCrossThread(unsigned int thread_index, float3 position, QuaternionScalar rotation, float size, Color color, DebugDrawOptions options)
	{
		// Check to see if the queue is full
		// If it is, then a flush is needed to clear the queue
		if (thread_crosses[thread_index].IsFull()) {
			FlushCross(thread_index);
		}
		thread_crosses[thread_index].Add({ position, rotation, size, color, options });
	}
	
	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::AddOOBBCrossThread(
		unsigned int thread_index, 
		float3 position, 
		QuaternionScalar rotation,
		float length,
		float size, 
		bool start_from_same_point, 
		const DebugOOBBCrossInfo* info,
		DebugDrawOptions options
	)
	{
		QuaternionScalar rotation_quaternion = rotation;
		float3 complete_size_x = OOBBCrossSize(length, size, info->size_override_x);
		float3 complete_size_y = OOBBCrossSize(length, size, info->size_override_y);
		float3 complete_size_z = OOBBCrossSize(length, size, info->size_override_z);

		DebugDrawOptions options_x = options;
		DebugDrawOptions options_y = options;
		DebugDrawOptions options_z = options;

		options_x.instance_thickness = info->instance_thickness_x;
		options_y.instance_thickness = info->instance_thickness_y;
		options_z.instance_thickness = info->instance_thickness_z;

		AddOOBBThread(
			thread_index,
			OOBBCrossXTranslation(position, rotation_quaternion, length, start_from_same_point),
			AxesArrowXRotation(rotation),
			complete_size_x,
			info->color_x,
			options_x
		);
		AddOOBBThread(
			thread_index,
			OOBBCrossYTranslation(position, rotation_quaternion, length, start_from_same_point),
			AxesArrowYRotation(rotation),
			complete_size_y,
			info->color_y,
			options_y
		);
		AddOOBBThread(
			thread_index,
			OOBBCrossZTranslation(position, rotation_quaternion, length, start_from_same_point),
			AxesArrowZRotation(rotation),
			complete_size_z,
			info->color_z,
			options_z
		);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::AddCircleThread(unsigned int thread_index, float3 position, QuaternionScalar rotation, float radius, Color color, DebugDrawOptions options)
	{
		// Check to see if the queue is full
		// If it is, then a flush is needed to clear the queue
		if (thread_circles[thread_index].IsFull()) {
			FlushCircle(thread_index);
		}
		thread_circles[thread_index].Add({ position, rotation, radius, color, options });
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::AddArrowThread(unsigned int thread_index, float3 start, float3 end, float size, Color color, DebugDrawOptions options)
	{
		// Check to see if the queue is full
		// If it is, then a flush is needed to clear the queue
		if (thread_arrows[thread_index].IsFull()) {
			FlushArrow(thread_index);
		}
		QuaternionScalar rotation;
		float length;
		ArrowStartEndToRotation(start, end, &rotation, &length);
		float3 direction = QuaternionVectorMultiply(rotation, GetRightVector());
		float3 end_position = start + direction * float3::Splat(length);
		thread_arrows[thread_index].Add({ start, rotation, length, size, color, options });
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::AddArrowRotationThread(unsigned int thread_index, float3 translation, QuaternionScalar rotation, float length, float size, Color color, DebugDrawOptions options)
	{
		// Check to see if the queue is full
		// If it is, then a flush is needed to clear the queue
		if (thread_arrows[thread_index].IsFull()) {
			FlushArrow(thread_index);
		}
		thread_arrows[thread_index].Add({ translation, rotation, length, size, color, options });
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::AddAxesThread(unsigned int thread_index, float3 translation, QuaternionScalar rotation, float size, const DebugAxesInfo* info, DebugDrawOptions options)
	{
		// Check to see if the queue is full
		// If it is, then a flush is needed to clear the queue
		if (thread_arrows[thread_index].size + 3 > thread_arrows[thread_index].capacity) {
			FlushArrow(thread_index);
		}

		DebugDrawOptions options_x = options;
		DebugDrawOptions options_y = options;
		DebugDrawOptions options_z = options;

		options_x.instance_thickness = info->instance_thickness_x;
		options_y.instance_thickness = info->instance_thickness_y;
		options_z.instance_thickness = info->instance_thickness_z;

		thread_arrows[thread_index].Add({ translation, AxesArrowXRotation(rotation), size, size, info->color_x, options_x });
		thread_arrows[thread_index].Add({ translation, AxesArrowYRotation(rotation), size, size, info->color_y, options_y });
		thread_arrows[thread_index].Add({ translation, AxesArrowZRotation(rotation), size, size, info->color_z, options_z });
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::AddTriangleThread(unsigned int thread_index, float3 point0, float3 point1, float3 point2, Color color, DebugDrawOptions options)
	{
		// Check to see if the queue is full
		// If it is, then a flush is needed to clear the queue
		if (thread_triangles[thread_index].IsFull()) {
			FlushTriangle(thread_index);
		}
		thread_triangles[thread_index].Add({ point0, point1, point2, color, options });
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::AddAABBThread(unsigned int thread_index, float3 min_coordinates, float3 max_coordinates, Color color, DebugDrawOptions options)
	{
		// Check to see if the queue is full
		// If it is, then a flush is needed to clear the queue
		if (thread_aabbs[thread_index].IsFull()) {
			FlushAABB(thread_index);
		}
		thread_aabbs[thread_index].Add({ min_coordinates, max_coordinates, color, options });
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::AddOOBBThread(unsigned int thread_index, float3 translation, QuaternionScalar rotation, float3 scale, Color color, DebugDrawOptions options)
	{
		// Check to see if the queue is full
		// If it is, then a flush is needed to clear the queue
		if (thread_oobbs[thread_index].IsFull()) {
			FlushOOBB(thread_index);
		}
		thread_oobbs[thread_index].Add({ translation, rotation, scale, color, options });
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::AddStringThread(unsigned int thread_index, float3 position, float3 direction, float size, Stream<char> text, Color color, DebugDrawOptions options)
	{
		// Check to see if the queue is full
		// If it is, then a flush is needed to clear the queue
		if (thread_strings[thread_index].IsFull()) {
			FlushString(thread_index);
		}
		Stream<char> string_copy = StringCopy(AllocatorTs(), text);
		thread_strings[thread_index].Add({ position, direction, size, string_copy, color, options });
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::AddStringRotationThread(unsigned int thread_index, float3 position, QuaternionScalar rotation, float size, Stream<char> text, Color color, DebugDrawOptions options)
	{
		// Check to see if the queue is full
		// If it is, then a flush is needed to clear the queue
		if (thread_strings[thread_index].IsFull()) {
			FlushString(thread_index);
		}
		Stream<char> string_copy = StringCopy(AllocatorTs(), text);
		thread_strings[thread_index].Add({ position, RotateVector(GetRightVector(), rotation), size, string_copy, color, options });
	}

	void DebugDrawer::AddGridThread(unsigned int thread_index, const DebugGrid* grid, bool retrieve_entries_now)
	{
		if (thread_grids[thread_index].IsFull()) {
			FlushGrid(thread_index);
		}
		DebugGrid temp_grid = *grid;
		if (retrieve_entries_now) {
			temp_grid.ExtractResidentCells(Allocator());
		}
		thread_grids[thread_index].Add(&temp_grid);
	}

	// ----------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Dynamic call types

	// ----------------------------------------------------------------------------------------------------------------------

#define DISPATCH_DYNAMIC_CALL(primitive_name, ...) switch (call_type.type) { \
		case ECS_DEBUG_DRAWER_CALL_IMMEDIATE: \
			Draw##primitive_name(__VA_ARGS__); \
			break; \
		case ECS_DEBUG_DRAWER_CALL_DEFERRED: \
			Add##primitive_name(__VA_ARGS__); \
			break; \
		case ECS_DEBUG_DRAWER_CALL_DEFERRED_THREAD: \
			Add##primitive_name##Thread(call_type.thread_index, __VA_ARGS__); \
			break; \
		default: \
			ECS_ASSERT(false, "Debug drawer invalid call type!"); \
	}

	void DebugDrawer::CallLine(DebugDrawCallType call_type, float3 start, float3 end, Color color, DebugDrawOptions options) {
		DISPATCH_DYNAMIC_CALL(Line, start, end, color, options);
	}

	void DebugDrawer::CallLine(DebugDrawCallType call_type, float3 translation, QuaternionScalar rotation, float size, Color color, DebugDrawOptions options) {
		DISPATCH_DYNAMIC_CALL(Line, translation, rotation, size, color, options);
	}

	void DebugDrawer::CallSphere(DebugDrawCallType call_type, float3 position, float radius, Color color, DebugDrawOptions options) {
		DISPATCH_DYNAMIC_CALL(Sphere, position, radius, color, options);
	}

	void DebugDrawer::CallPoint(DebugDrawCallType call_type, float3 position, float size, Color color, DebugDrawOptions options) {
		DISPATCH_DYNAMIC_CALL(Point, position, size, color, options);
	}

	void DebugDrawer::CallRectangle(DebugDrawCallType call_type, float3 corner0, float3 corner1, Color color, DebugDrawOptions options) {
		DISPATCH_DYNAMIC_CALL(Rectangle, corner0, corner1, color, options);
	}

	void DebugDrawer::CallCross(DebugDrawCallType call_type, float3 position, QuaternionScalar rotation, float size, Color color, DebugDrawOptions options) {
		DISPATCH_DYNAMIC_CALL(Cross, position, rotation, size, color, options);
	}

	void DebugDrawer::CallOOBBCross(
		DebugDrawCallType call_type,
		float3 position,
		QuaternionScalar rotation,
		float length,
		float size,
		bool start_from_same_point,
		const DebugOOBBCrossInfo* info,
		DebugDrawOptions options
	) {
		DISPATCH_DYNAMIC_CALL(OOBBCross, position, rotation, length, size, start_from_same_point, info, options);
	}

	void DebugDrawer::CallCircle(DebugDrawCallType call_type, float3 position, QuaternionScalar rotation, float radius, Color color, DebugDrawOptions options) {
		DISPATCH_DYNAMIC_CALL(Circle, position, rotation, radius, color, options);
	}

	void DebugDrawer::CallArrow(DebugDrawCallType call_type, float3 start, float3 end, float size, Color color, DebugDrawOptions options) {
		DISPATCH_DYNAMIC_CALL(Arrow, start, end, size, color, options);
	}

	void DebugDrawer::CallArrowRotation(
		DebugDrawCallType call_type,
		float3 translation,
		QuaternionScalar rotation,
		float length,
		float size,
		Color color,
		DebugDrawOptions options
	) {
		DISPATCH_DYNAMIC_CALL(ArrowRotation, translation, rotation, length, size, color, options);
	}

	void DebugDrawer::CallAxes(
		DebugDrawCallType call_type,
		float3 translation,
		QuaternionScalar rotation,
		float size,
		const DebugAxesInfo* info,
		DebugDrawOptions options
	) {
		DISPATCH_DYNAMIC_CALL(Axes, translation, rotation, size, info, options);
	}

	void DebugDrawer::CallTriangle(DebugDrawCallType call_type, float3 point0, float3 point1, float3 point2, Color color, DebugDrawOptions options) {
		DISPATCH_DYNAMIC_CALL(Triangle, point0, point1, point2, color, options);
	}

	void DebugDrawer::CallAABB(DebugDrawCallType call_type, float3 translation, float3 scale, Color color, DebugDrawOptions options) {
		DISPATCH_DYNAMIC_CALL(AABB, translation, scale, color, options);
	}

	void DebugDrawer::CallOOBB(DebugDrawCallType call_type, float3 translation, QuaternionScalar rotation, float3 scale, Color color, DebugDrawOptions options) {
		DISPATCH_DYNAMIC_CALL(OOBB, translation, rotation, scale, color, options);
	}

	void DebugDrawer::CallString(
		DebugDrawCallType call_type,
		float3 translation,
		float3 direction,
		float size,
		Stream<char> text,
		Color color,
		DebugDrawOptions options
	) {
		DISPATCH_DYNAMIC_CALL(String, translation, direction, size, text, color, options);
	}

	void DebugDrawer::CallStringRotation(
		DebugDrawCallType call_type,
		float3 translation,
		QuaternionScalar rotation,
		float size,
		Stream<char> text,
		Color color,
		DebugDrawOptions options
	) {
		DISPATCH_DYNAMIC_CALL(StringRotation, translation, rotation, size, text, color, options);
	}

	// ----------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Draw immediately

	// ----------------------------------------------------------------------------------------------------------------------

#define DRAW_STRUCTURED_LINE_TOPOLOGY (1 << 0)
#define DRAW_TRANSFORM_CIRCLE_BUFFER (1 << 0)

	template<size_t flags>
	void BindStructuredShaderState(DebugDrawer* drawer, DebugDrawOptions options, DebugShaderOutput output = ECS_DEBUG_SHADER_OUTPUT_COLOR) {
		drawer->SetStructuredShaderState(options, output);
		if constexpr (flags & DRAW_STRUCTURED_LINE_TOPOLOGY) {
			drawer->graphics->BindTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
		}
	}

	template<size_t flags>
	unsigned int BindTransformVertexBuffers(DebugDrawer* drawer, DebugVertexBuffers debug_vertex_buffer) {
		unsigned int draw_count = 0;
		if constexpr (flags & DRAW_TRANSFORM_CIRCLE_BUFFER) {
			drawer->graphics->BindVertexBuffer(drawer->circle_buffer);
			draw_count = CIRCLE_TESSELATION + 1;
		}
		else {
			ECS_MESH_INDEX mesh_index = ECS_MESH_POSITION;
			drawer->graphics->BindMesh(*drawer->primitive_meshes[debug_vertex_buffer], Stream<ECS_MESH_INDEX>(&mesh_index, 1));
			draw_count = drawer->primitive_meshes[debug_vertex_buffer]->index_buffer.count;
		}
		return draw_count;
	}

	template<size_t flags>
	void DrawCallTransformFlags(DebugDrawer* drawer, unsigned int mesh_index_count, unsigned int instance_count) {
		if constexpr (flags & DRAW_TRANSFORM_CIRCLE_BUFFER) {
			drawer->graphics->BindTopology(D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP);
			drawer->graphics->DrawInstanced(mesh_index_count, instance_count);
		}
		else {
			drawer->DrawCallTransform(mesh_index_count, instance_count);
		}
	}
	
	// This is for a single element draw
	template<DebugShaderOutput shader_output, size_t flags = 0, typename SetPositionFunctor>
	void DrawStructuredImmediate(
		DebugDrawer* drawer, 
		unsigned int instance_count,
		const DebugDrawerOutput* outputs, 
		DebugDrawOptions options, 
		unsigned int vertex_count, 
		SetPositionFunctor&& set_position_functor
	) {
		InstancedVertex* positions = MapPositions(drawer);
		InstancedTransformData* instanced;
		Matrix* matrix;
		unsigned int* ids;
		
		if constexpr (shader_output == ECS_DEBUG_SHADER_OUTPUT_COLOR) {
			instanced = MapInstancedTransformVertex(drawer);
		}
		else {
			matrix = MapOutputMatrixStructured(drawer);
			ids = MapOutputIDStructured(drawer);
		}

		Matrix gpu_matrix = MatrixGPU(drawer->camera_matrix);
		for (unsigned int index = 0; index < instance_count; index++) {
			set_position_functor(positions, index);
			if constexpr (shader_output == ECS_DEBUG_SHADER_OUTPUT_COLOR) {
				SetInstancedMatrix(instanced, index, gpu_matrix);
				SetInstancedColor(instanced, index, outputs[index].color);
			}
			else {
				gpu_matrix.Store(matrix + index);
				ids[index] = outputs[index].instance_thickness;
			}
		}

		if constexpr (shader_output == ECS_DEBUG_SHADER_OUTPUT_COLOR) {
			UnmapAllStructuredColor(drawer);
		}
		else {
			UnmapAllStructuredOutputID(drawer);
		}

		BindStructuredShaderState<flags>(drawer, options, shader_output);
		if constexpr (shader_output == ECS_DEBUG_SHADER_OUTPUT_COLOR) {
			BindSmallStructuredBuffersColor(drawer);
		}
		else {
			BindSmallStructuredBuffersOutputID(drawer);
		}

		drawer->DrawCallStructured(vertex_count, instance_count);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	template<DebugShaderOutput shader_output, size_t flags = 0, typename SetMatrixFunctor>
	void DrawTransformImmediate(
		DebugDrawer* drawer, 
		unsigned int instance_count,
		const DebugDrawerOutput* outputs,
		DebugDrawOptions options, 
		DebugVertexBuffers debug_vertex_buffer, 
		SetMatrixFunctor&& set_matrix_functor
	) {
		Matrix* matrix_data;
		unsigned int* id;
		InstancedTransformData* instanced;

		if constexpr (shader_output == ECS_DEBUG_SHADER_OUTPUT_COLOR) {
			instanced = MapInstancedTransformVertex(drawer);
		}
		else {
			matrix_data = MapOutputMatrixVertex(drawer);
			id = MapOutputIDVertex(drawer);
		}

		for (unsigned int index = 0; index < instance_count; index++) {
			Matrix matrix = set_matrix_functor(index);
			if constexpr (shader_output == ECS_DEBUG_SHADER_OUTPUT_COLOR) {
				SetInstancedColor(instanced, index, outputs[index].color);
				SetInstancedMatrix(instanced, index, matrix);
			}
			else {
				matrix.Store(matrix_data + index);
				id[index] = outputs[index].instance_thickness;
			}
		}

		if constexpr (shader_output == ECS_DEBUG_SHADER_OUTPUT_COLOR) {
			UnmapInstancedVertex(drawer);
		}
		else {
			UnmapAllTransformOutputID(drawer);
		}

		drawer->SetTransformShaderState(options, shader_output);
		unsigned int draw_count = BindTransformVertexBuffers<flags>(drawer, debug_vertex_buffer);
		if constexpr (shader_output == ECS_DEBUG_SHADER_OUTPUT_COLOR) {
			drawer->graphics->BindVertexBuffer(drawer->instanced_small_transform_vertex_buffer, 1);
		}
		else {
			drawer->graphics->BindVertexBuffer(drawer->output_instance_small_matrix_buffer, 1);
			drawer->graphics->BindVertexBuffer(drawer->output_instance_small_id_buffer, 2);
		}

		DrawCallTransformFlags<flags>(drawer, draw_count, instance_count);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::DrawLine(float3 start, float3 end, Color color, DebugDrawOptions options)
	{
		DrawStructuredImmediate<ECS_DEBUG_SHADER_OUTPUT_COLOR, DRAW_STRUCTURED_LINE_TOPOLOGY>(
			this, 
			1, 
			(const DebugDrawerOutput*)&color, 
			options, 
			2, 
			[&](InstancedVertex* positions, unsigned int index) {
			SetLinePositions(positions, 0, start, end);
		});
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::DrawLine(float3 translation, QuaternionScalar rotation, float size, Color color, DebugDrawOptions options)
	{
		float3 start, end;
		ConvertTranslationLineIntoStartEnd(translation, rotation, size, start, end);
		DrawLine(start, end, color, options);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::DrawSphere(float3 position, float radius, Color color, DebugDrawOptions options)
	{
		DrawTransformImmediate<ECS_DEBUG_SHADER_OUTPUT_COLOR>(
			this, 
			1, 
			(const DebugDrawerOutput*)&color, 
			options, 
			ECS_DEBUG_VERTEX_BUFFER_SPHERE, 
			[&](unsigned int index) {
			return SphereMatrix(position, radius, camera_matrix);
		});
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::DrawPoint(float3 position, float size, Color color, DebugDrawOptions options)
	{
		options.wireframe = false;
		DrawSphere(position, POINT_SIZE * size, color, options);

		//DrawTransformImmediate<ECS_DEBUG_SHADER_OUTPUT_COLOR>(
		//	this, 
		//	1, 
		//	(const DebugDrawerOutput*)&color, 
		//	options, 
		//	ECS_DEBUG_VERTEX_BUFFER_POINT, 
		//	[&](unsigned int index) {
		//	return PointMatrix(position, camera_matrix);
		//});
	}

	// ----------------------------------------------------------------------------------------------------------------------

	// Draw it double sided, so it appears on both sides
	void DebugDrawer::DrawRectangle(float3 corner0, float3 corner1, Color color, DebugDrawOptions options)
	{
		DrawStructuredImmediate<ECS_DEBUG_SHADER_OUTPUT_COLOR>(
			this, 
			1, 
			(const DebugDrawerOutput*)&color, 
			options, 
			6, 
			[&](InstancedVertex* positions, unsigned int index) {
			SetRectanglePositionsFront(positions, 0, corner0, corner1);
		});
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::DrawCross(float3 position, QuaternionScalar rotation, float size, Color color, DebugDrawOptions options)
	{
		DrawTransformImmediate<ECS_DEBUG_SHADER_OUTPUT_COLOR>(
			this, 
			1, 
			(const DebugDrawerOutput*)&color, 
			options, 
			ECS_DEBUG_VERTEX_BUFFER_CROSS, 
			[&](unsigned int index) {
			return CrossMatrix(position, rotation, size, camera_matrix);
		});
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::DrawOOBBCross(
		float3 position, 
		QuaternionScalar rotation,
		float length,
		float size, 
		bool start_from_same_point, 
		const DebugOOBBCrossInfo* info,
		DebugDrawOptions options
	)
	{
		// The same as drawing 3 arrows for each axis
		DebugDrawerOutput colors[] = {
			info->color_x,
			info->color_y,
			info->color_z
		};

		QuaternionScalar rotation_quaternion = rotation;

		// Hopefully since the function is marked as inline the compiler will notice the same function being
		// called multiple times and fold the calls into a single one (with optimizations for sure, in release
		// I hope so)
		Matrix matrices[] = {
			OOBBCrossMatrix(
				OOBBCrossXTranslation(position, rotation_quaternion, length, start_from_same_point), 
				AxesArrowXRotation(rotation), 
				length, 
				size, 
				info->size_override_x, 
				camera_matrix
			),
			OOBBCrossMatrix(
				OOBBCrossYTranslation(position, rotation_quaternion, length, start_from_same_point), 
				AxesArrowYRotation(rotation), 
				length, 
				size,
				info->size_override_y,
				camera_matrix
			),
			OOBBCrossMatrix(
				OOBBCrossZTranslation(position, rotation_quaternion, length, start_from_same_point), 
				AxesArrowZRotation(rotation), 
				length, 
				size, 
				info->size_override_z,
				camera_matrix
			)
		};

		DrawTransformImmediate<ECS_DEBUG_SHADER_OUTPUT_COLOR>(
			this,
			3,
			colors,
			options,
			ECS_DEBUG_VERTEX_BUFFER_CUBE,
			[&](unsigned int index) {
				return matrices[index];
		});
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::DrawCircle(float3 position, QuaternionScalar rotation, float radius, Color color, DebugDrawOptions options)
	{
		options.wireframe = false;
		DrawTransformImmediate<ECS_DEBUG_SHADER_OUTPUT_COLOR, DRAW_TRANSFORM_CIRCLE_BUFFER>(
			this, 
			1, 
			(const DebugDrawerOutput*)&color,
			options, 
			ECS_DEBUG_VERTEX_BUFFER_COUNT, 
			[&](unsigned int index) {
			return CircleMatrix(position, rotation, radius, camera_matrix);
		});
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::DrawArrow(float3 start, float3 end, float size, Color color, DebugDrawOptions options)
	{
		QuaternionScalar rotation;
		float length;
		ArrowStartEndToRotation(start, end, &rotation, &length);
		DrawArrowRotation(start, rotation, length, size, color, options);
	}

	void DebugDrawer::DrawArrowRotation(float3 translation, QuaternionScalar rotation, float length, float size, Color color, DebugDrawOptions options)
	{
		DrawTransformImmediate<ECS_DEBUG_SHADER_OUTPUT_COLOR>(
			this, 
			1, 
			(const DebugDrawerOutput*)&color, 
			options, 
			ECS_DEBUG_VERTEX_BUFFER_ARROW, 
			[&](unsigned int index) {
			return ArrowMatrix(translation, rotation, length, size, camera_matrix);
		});
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::DrawAxes(float3 translation, QuaternionScalar rotation, float size, const DebugAxesInfo* info, DebugDrawOptions options)
	{
		// The same as drawing 3 arrows for each axis
		DebugDrawerOutput colors[] = {
			info->color_x,
			info->color_y,
			info->color_z
		};

		// Hopefully since the function is marked as inline the compiler will notice the same function being
		// called multiple times and fold the calls into a single one (with optimizations for sure, in release
		// I hope so)
		Matrix matrices[] = {
			ArrowMatrix(translation, AxesArrowXRotation(rotation), size, size, camera_matrix),
			ArrowMatrix(translation, AxesArrowYRotation(rotation), size, size, camera_matrix),
			ArrowMatrix(translation, AxesArrowZRotation(rotation), size, size, camera_matrix)
		};

		DrawTransformImmediate<ECS_DEBUG_SHADER_OUTPUT_COLOR>(
			this, 
			3, 
			colors, 
			options, 
			ECS_DEBUG_VERTEX_BUFFER_ARROW, 
			[&](unsigned int index) {
			return matrices[index];
		});
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::DrawTriangle(float3 point0, float3 point1, float3 point2, Color color, DebugDrawOptions options)
	{
		DrawStructuredImmediate<ECS_DEBUG_SHADER_OUTPUT_COLOR>(
			this, 
			1, 
			(const DebugDrawerOutput*)&color,
			options, 
			3, 
			[&](InstancedVertex* positions, unsigned int index) {
			SetTrianglePositions(positions, index, point0, point1, point2);
		});
	}

	// ----------------------------------------------------------------------------------------------------------------------

	// It uses a scaled cube to draw it
	void DebugDrawer::DrawAABB(float3 translation, float3 scale, Color color, DebugDrawOptions options)
	{
		DrawOOBB(translation, QuaternionIdentityScalar(), scale, color, options);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::DrawOOBB(float3 translation, QuaternionScalar rotation, float3 scale, Color color, DebugDrawOptions options)
	{
		DrawTransformImmediate<ECS_DEBUG_SHADER_OUTPUT_COLOR>(
			this, 
			1, 
			(const DebugDrawerOutput*)&color, 
			options, 
			ECS_DEBUG_VERTEX_BUFFER_CUBE, 
			[&](unsigned int index) {
			return OOBBMatrix(translation, rotation, scale, camera_matrix);
		});
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::DrawString(float3 position, float3 direction, float size, Stream<char> text, Color color, DebugDrawOptions options)
	{
		ECS_ASSERT(text.size < 10'000);

		// Normalize the direction
		direction = Normalize(direction);

		float3 rotation_from_direction = DirectionToRotationEuler(direction);
		// Invert the y axis 
		rotation_from_direction.y = -rotation_from_direction.y;
		float3 up_direction = RotateVector(GetUpVector(), QuaternionFromEuler(rotation_from_direction));

		// Splat these early
		float3 space_size = float3::Splat(STRING_SPACE_SIZE * size) * direction;
		float3 tab_size = float3::Splat(STRING_TAB_SIZE * size) * direction;
		float3 character_spacing = float3::Splat(STRING_CHARACTER_SPACING * size) * direction;
		float3 new_line_size = float3::Splat(STRING_NEW_LINE_SIZE * -size) * up_direction;

		Matrix rotation_matrix = QuaternionRotationMatrix({rotation_from_direction.x, rotation_from_direction.y, rotation_from_direction.z});
		Matrix scale_matrix = MatrixScale(float3::Splat(size));
		Matrix scale_rotation = MatrixRS(rotation_matrix, scale_matrix);

		// Create the instanced vertex buffer
		VertexBuffer instanced_data_buffer = graphics->CreateVertexBuffer(sizeof(InstancedTransformData), text.size, true);

		// Keep track of already checked elements - the same principle as the Eratosthenes sieve
		bool* checked_characters = (bool*)ECS_STACK_ALLOC(sizeof(bool) * text.size);
		memset(checked_characters, 0, sizeof(bool) * text.size);

		// Horizontal offsets as in the offset alongside the direction
		float3* horizontal_offsets = (float3*)ECS_STACK_ALLOC(sizeof(float3) * text.size);
		// Vertical offsets as in the offset alongside the -up direction
		float3* vertical_offsets = (float3*)ECS_STACK_ALLOC(sizeof(float3) * text.size);

		// Create a mask of spaces and tabs offsets for each character

		// Do the first character separately
		horizontal_offsets[0] = { 0.0f, 0.0f, 0.0f };
		vertical_offsets[0] = { 0.0f, 0.0f, 0.0f };

		if (text[0] == ' ' || text[0] == '\n' || text[0] == '\t') {
			checked_characters[0] = true;
		}

		for (size_t index = 1; index < text.size; index++) {
			// Hoist the checking outside the if
			checked_characters[index] = text[index] == ' ' || text[index] == '\t' || text[index] == '\n';
			
			if (text[index - 1] == '\n') {
				vertical_offsets[index] = vertical_offsets[index - 1] + new_line_size;

				// If a new character has been found - set to 0 all the other offsets - the tab and the space
				horizontal_offsets[index] = { 0.0f, 0.0f, 0.0f };
			}
			else {
				// It doesn't matter if the current character is \n because it will reset the horizontal offset anyway
				unsigned int previous_alphabet_index = GetAlphabetIndex(text[index - 1]);
				unsigned int current_alphabet_index = GetAlphabetIndex(text[index]);

				float previous_offset = string_character_bounds[previous_alphabet_index].y;
				float current_offset = string_character_bounds[current_alphabet_index].x;

				// The current offset will be negative
				float3 character_span = float3::Splat((previous_offset - current_offset) * size) * direction;
				horizontal_offsets[index] = horizontal_offsets[index - 1] + character_spacing + character_span;
				vertical_offsets[index] = vertical_offsets[index - 1];
			}
		}

		SetTransformShaderState(options, ECS_DEBUG_SHADER_OUTPUT_COLOR);
		ECS_MESH_INDEX mesh_position = ECS_MESH_POSITION;
		graphics->BindMesh(string_mesh->mesh, Stream<ECS_MESH_INDEX>(&mesh_position, 1));
		graphics->BindVertexBuffer(instanced_data_buffer, 1);

		for (size_t index = 0; index < text.size; index++) {
			if (!checked_characters[index]) {
				// Map the vertex buffer
				void* instance_data = graphics->MapBuffer(instanced_data_buffer.buffer);

				unsigned int current_character_count = 0;
				// Do a search for the same character
				for (size_t subindex = index; subindex < text.size; subindex++) {
					if (text[subindex] == text[index]) {
						float3 translation = position + horizontal_offsets[subindex] + vertical_offsets[subindex];
						checked_characters[subindex] = true;
						
						SetInstancedColor(instance_data, current_character_count, color);
						SetInstancedMatrix(instance_data, current_character_count, MatrixGPU(MatrixMVP(scale_rotation * MatrixTranslation(translation), camera_matrix)));
						current_character_count++;
					}
				}

				// Draw the current characters
				graphics->UnmapBuffer(instanced_data_buffer.buffer);

				unsigned int alphabet_index = GetAlphabetIndex(text[index]);
				// Draw the current character
				graphics->DrawIndexedInstanced(string_mesh->submeshes[alphabet_index].index_count, current_character_count, string_mesh->submeshes[alphabet_index].index_buffer_offset);
			}
		}

		// Release the temporary buffer
		instanced_data_buffer.Release();
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::DrawStringRotation(
		float3 translation,
		QuaternionScalar rotation,
		float size,
		Stream<char> text,
		Color color,
		DebugDrawOptions options
	) {
		DrawString(translation, RotateVector(GetRightVector(), rotation), size, text, color, options);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DrawDebugVertexLine(
		DebugDrawer* drawer,
		VertexBuffer model_position,
		VertexBuffer attribute, 
		unsigned int float_step, 
		float size, 
		Color color,
		Matrix world_matrix, 
		DebugDrawOptions options
	) {
		// Draw them as lines
		Graphics* graphics = drawer->graphics;

		// Allocate temporary constant buffer and position buffers
		ConstantBuffer constant_buffer = graphics->CreateConstantBuffer(sizeof(InstancedTransformData), true);
		VertexBuffer line_position_buffer = graphics->CreateVertexBuffer(sizeof(float3), attribute.size * 2, true);

		// Create staging buffers for positions and normals
		VertexBuffer staging_normals = BufferToStaging(graphics, attribute);
		VertexBuffer staging_positions = BufferToStaging(graphics, model_position);

		float* attribute_data = (float*)graphics->MapBuffer(staging_normals.buffer, ECS_GRAPHICS_MAP_READ);
		float3* model_positions = (float3*)graphics->MapBuffer(staging_positions.buffer, ECS_GRAPHICS_MAP_READ);
		void* constant_data = graphics->MapBuffer(constant_buffer.buffer);
		float3* line_positions = (float3*)graphics->MapBuffer(line_position_buffer.buffer);

		SetInstancedColor(constant_data, 0, color);
		SetInstancedMatrix(constant_data, 0, MatrixGPU(MatrixMVP(world_matrix, drawer->camera_matrix)));

		// Set the line positions
		for (size_t index = 0; index < attribute.size; index++) {
			size_t offset = index * 2;
			size_t attribute_offset = index * float_step;
			float3 direction = { attribute_data[attribute_offset], attribute_data[attribute_offset + 1], attribute_data[attribute_offset + 2] };
			line_positions[offset] = model_positions[index];
			line_positions[offset + 1] = model_positions[index] + direction * size;
		}

		// Unmap the constant buffer and the staging ones
		graphics->UnmapBuffer(staging_normals.buffer);
		graphics->UnmapBuffer(staging_positions.buffer);
		graphics->UnmapBuffer(constant_buffer.buffer);
		graphics->UnmapBuffer(line_position_buffer.buffer);

		// Special shader
		unsigned int shader_type = ECS_DEBUG_SHADER_NORMAL_SINGLE_DRAW;
		drawer->BindShaders(shader_type, ECS_DEBUG_SHADER_OUTPUT_COLOR);
		HandleOptions(drawer, options);
		graphics->BindTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

		// The positions will be taken from the model positions buffer
		graphics->BindVertexBuffer(line_position_buffer);
		graphics->BindVertexConstantBuffer(constant_buffer);

		graphics->Draw(attribute.size * 2);

		// Release the temporary buffer and the staging normals
		staging_normals.Release();
		staging_positions.Release();
		constant_buffer.Release();
		line_position_buffer.Release();
	}

	void DrawDebugVertexLine(
		DebugDrawer* drawer,
		VertexBuffer model_position,
		VertexBuffer attribute,
		unsigned int float_step,
		float size,
		Color color,
		Stream<Matrix> world_matrices,
		DebugDrawOptions options
	) {
		// Draw them as lines
		Graphics* graphics = drawer->graphics;

		// Allocate temporary instanced buffer and position buffers
		VertexBuffer instanced_buffer = graphics->CreateVertexBuffer(sizeof(InstancedTransformData), world_matrices.size, true);
		VertexBuffer line_position_buffer = graphics->CreateVertexBuffer(sizeof(float3), attribute.size * 2, true);

		// Create staging buffers for positions and normals
		VertexBuffer staging_normals = BufferToStaging(graphics, attribute);
		VertexBuffer staging_positions = BufferToStaging(graphics, model_position);

		float* attribute_data = (float*)graphics->MapBuffer(staging_normals.buffer, ECS_GRAPHICS_MAP_READ);
		float3* model_positions = (float3*)graphics->MapBuffer(staging_positions.buffer, ECS_GRAPHICS_MAP_READ);
		void* instanced_data = graphics->MapBuffer(instanced_buffer.buffer);
		float3* line_positions = (float3*)graphics->MapBuffer(line_position_buffer.buffer);

		// Set the line positions
		for (size_t index = 0; index < attribute.size; index++) {
			size_t offset = index * 2;
			size_t attribute_offset = index * float_step;
			float3 direction = { attribute_data[attribute_offset], attribute_data[attribute_offset + 1], attribute_data[attribute_offset + 2] };
			line_positions[offset] = model_positions[index];
			line_positions[offset + 1] = model_positions[index] + direction * size;
		}

		for (size_t index = 0; index < world_matrices.size; index++) {
			SetInstancedColor(instanced_data, index, color);
			SetInstancedMatrix(instanced_data, index, MatrixGPU(MatrixMVP(world_matrices[index], drawer->camera_matrix)));
		}

		// Unmap the constant buffer and the staging ones
		graphics->UnmapBuffer(staging_normals.buffer);
		graphics->UnmapBuffer(staging_positions.buffer);
		graphics->UnmapBuffer(instanced_buffer.buffer);
		graphics->UnmapBuffer(line_position_buffer.buffer);

		// Instanced draw
		unsigned int shader_type = ECS_DEBUG_SHADER_TRANSFORM;
		drawer->BindShaders(shader_type, ECS_DEBUG_SHADER_OUTPUT_COLOR);
		HandleOptions(drawer, options);
		graphics->BindTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

		// The positions will be taken from the model positions buffer
		VertexBuffer vertex_buffers[2];
		vertex_buffers[0] = line_position_buffer;
		vertex_buffers[1] = instanced_buffer;
		graphics->BindVertexBuffers({ vertex_buffers, ECS_COUNTOF(vertex_buffers) });

		graphics->DrawInstanced(attribute.size * 2, world_matrices.size);

		// Release the temporary buffer and the staging normals
		staging_normals.Release();
		staging_positions.Release();
		instanced_buffer.Release();
		line_position_buffer.Release();
	}

	void DebugDrawer::DrawNormals(
		VertexBuffer model_position,
		VertexBuffer model_normals, 
		float size,
		Color color, 
		Matrix world_matrix,
		DebugDrawOptions options
	)
	{
		DrawDebugVertexLine(this, model_position, model_normals, 3, size, color, world_matrix, options);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::DrawNormals(
		VertexBuffer model_position,
		VertexBuffer model_normals,
		float size,
		Color color,
		Stream<Matrix> world_matrices,
		DebugDrawOptions options
	) {
		DrawDebugVertexLine(this, model_position, model_normals, 3, size, color, world_matrices, options);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::DrawTangents(
		VertexBuffer model_position,
		VertexBuffer model_tangents,
		float size, 
		Color color, 
		Matrix world_matrix, 
		DebugDrawOptions options
	)
	{
		DrawDebugVertexLine(this, model_position, model_tangents, 4, size, color, world_matrix, options);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::DrawTangents(
		VertexBuffer model_position,
		VertexBuffer model_tangents, 
		float size,
		Color color,
		Stream<Matrix> world_matrices,
		DebugDrawOptions options
	)
	{
		DrawDebugVertexLine(this, model_position, model_tangents, 4, size, color, world_matrices, options);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::DrawGrid(const DebugGrid* grid, DebugShaderOutput shader_output)
	{
		DebugAABB aabb;
		aabb.color = grid->color;
		aabb.options = grid->options;
		aabb.options.wireframe = true;
		aabb.scale = grid->cell_size;
		unsigned int per_type_counts[ELEMENT_COUNT];
		unsigned int x = 0;
		unsigned int y = 0;
		unsigned int z = 0;
		float3 cell_offsets = aabb.scale * float3::Splat(2.0f);
		if (grid->has_valid_cells) {
			if (grid->valid_cells.GetChunkCount() > 0) {
				memset(per_type_counts, 0, sizeof(per_type_counts));
				size_t element_count = grid->valid_cells.GetElementCount();
				if (element_count > 0) {
					// We have the cells already determined
					unsigned int instance_count = element_count;
					per_type_counts[grid->options.ignore_depth ? WIREFRAME_NO_DEPTH : WIREFRAME_DEPTH] = instance_count;
					DrawTransformCallCore<false>(this, instance_count, per_type_counts, shader_output, ECS_DEBUG_VERTEX_BUFFER_CUBE,
						[&](unsigned int index, ElementType type, bool* should_stop) {
							aabb.translation = grid->translation + float3(grid->valid_cells[index]) * cell_offsets;
							return &aabb;
						});
				}
			}
		}	
		else {
			if (grid->residency_function != nullptr) {
				DrawTransformCallCore<true>(this, 0, per_type_counts, shader_output, ECS_DEBUG_VERTEX_BUFFER_CUBE,
					[&](unsigned int index, ElementType type, bool* should_stop) {
						bool is_resident = grid->residency_function(uint3(x, y, z), grid->residency_data);
						const DebugAABB* return_value = &aabb;
						if (is_resident) {
							aabb.translation = grid->translation + float3(uint3(x, y, z)) * cell_offsets;
						}
						else {
							return_value = nullptr;
						}
						z++;
						if (z == grid->dimensions.z) {
							z = 0;
							y++;
							if (y == grid->dimensions.y) {
								y = 0;
								x++;
								if (x == grid->dimensions.x) {
									*should_stop = true;
								}
							}
						}
						return return_value;
					}, grid->options.ignore_depth ? WIREFRAME_NO_DEPTH : WIREFRAME_DEPTH
					);
			}
			else {
				unsigned int instance_count = grid->dimensions.x * grid->dimensions.y * grid->dimensions.z;
				per_type_counts[grid->options.ignore_depth ? WIREFRAME_NO_DEPTH : WIREFRAME_DEPTH] = instance_count;
				DrawTransformCallCore<false>(this, instance_count, per_type_counts, shader_output, ECS_DEBUG_VERTEX_BUFFER_CUBE,
					[&](unsigned int index, ElementType type, bool* should_stop) {
						aabb.translation = grid->translation + float3(uint3(x, y, z)) * cell_offsets;
						z++;
						if (z == grid->dimensions.z) {
							z = 0;
							y++;
							if (y == grid->dimensions.y) {
								y = 0;
								x++;
							}
						}
						return &aabb;
					}
				);
			}
		}
	}

	// ----------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Draw Deck elements

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::DrawLineDeck(float time_delta, DebugShaderOutput shader_output, DeckPowerOfTwo<DebugLine>* custom_source) {
		DeckPowerOfTwo<DebugLine>* source = custom_source != nullptr ? custom_source : &lines;
		DrawStructuredDeck<DRAW_STRUCTURED_LINE_TOPOLOGY>(this, source, 2, time_delta, shader_output,
			[](InstancedVertex* positions, unsigned int index, const DebugLine* line) {
				SetLinePositions(positions, index, line->start, line->end);
		});
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::DrawSphereDeck(float time_delta, DebugShaderOutput shader_output, DeckPowerOfTwo<DebugSphere>* custom_source) {
		DeckPowerOfTwo<DebugSphere>* source = custom_source != nullptr ? custom_source : &spheres;
		DrawTransformDeck(this, source, ECS_DEBUG_VERTEX_BUFFER_SPHERE, time_delta, shader_output);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::DrawPointDeck(float time_delta, DebugShaderOutput shader_output, DeckPowerOfTwo<DebugPoint>* custom_source) {
		// Deactivate this for the time being - until we decide how the points should behave
		//DeckPowerOfTwo<DebugPoint>* source = custom_source != nullptr ? custom_source : &points;
		//DrawTransformDeck(this, source, ECS_DEBUG_VERTEX_BUFFER_POINT, time_delta, shader_output);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::DrawRectangleDeck(float time_delta, DebugShaderOutput shader_output, DeckPowerOfTwo<DebugRectangle> * custom_source) {
		DeckPowerOfTwo<DebugRectangle>* source = custom_source != nullptr ? custom_source : &rectangles;
		DrawStructuredDeck(this, source, 6, time_delta, shader_output,
			[](InstancedVertex* positions, unsigned int index, const DebugRectangle* rectangle) {
				SetRectanglePositionsFront(positions, index, rectangle->corner0, rectangle->corner1);
		});
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::DrawCrossDeck(float time_delta, DebugShaderOutput shader_output, DeckPowerOfTwo<DebugCross>* custom_source) {
		DeckPowerOfTwo<DebugCross>* source = custom_source != nullptr ? custom_source : &crosses;
		DrawTransformDeck(this, source, ECS_DEBUG_VERTEX_BUFFER_CROSS, time_delta, shader_output);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::DrawCircleDeck(float time_delta, DebugShaderOutput shader_output, DeckPowerOfTwo<DebugCircle>* custom_source) {
		DeckPowerOfTwo<DebugCircle>* source = custom_source != nullptr ? custom_source : &circles;
		DrawTransformDeck<DRAW_TRANSFORM_CIRCLE_BUFFER>(this, source, ECS_DEBUG_VERTEX_BUFFER_COUNT, time_delta, shader_output);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::DrawArrowDeck(float time_delta, DebugShaderOutput shader_output, DeckPowerOfTwo<DebugArrow>* custom_source) {
		DeckPowerOfTwo<DebugArrow>* source = custom_source != nullptr ? custom_source : &arrows;
		DrawTransformDeck(this, source, ECS_DEBUG_VERTEX_BUFFER_ARROW, time_delta, shader_output);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::DrawTriangleDeck(float time_delta, DebugShaderOutput shader_output, DeckPowerOfTwo<DebugTriangle>* custom_source) {
		DeckPowerOfTwo<DebugTriangle>* source = custom_source != nullptr ? custom_source : &triangles;
		DrawStructuredDeck(this, source, 3, time_delta, shader_output,
			[](InstancedVertex* positions, unsigned int index, const DebugTriangle* triangle) {
				SetTrianglePositions(positions, index, triangle->point0, triangle->point1, triangle->point2);
		});
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::DrawAABBDeck(float time_delta, DebugShaderOutput shader_output, DeckPowerOfTwo<DebugAABB>* custom_source) {
		DeckPowerOfTwo<DebugAABB>* source = custom_source != nullptr ? custom_source : &aabbs;
		DrawTransformDeck(this, source, ECS_DEBUG_VERTEX_BUFFER_CUBE, time_delta, shader_output);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::DrawOOBBDeck(float time_delta, DebugShaderOutput shader_output, DeckPowerOfTwo<DebugOOBB>* custom_source) {
		DeckPowerOfTwo<DebugOOBB>* source = custom_source != nullptr ? custom_source : &oobbs;
		DrawTransformDeck(this, source, ECS_DEBUG_VERTEX_BUFFER_CUBE, time_delta, shader_output);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::DrawStringDeck(float time_delta, DebugShaderOutput shader_output, DeckPowerOfTwo<DebugString>* custom_source) {
		DeckPowerOfTwo<DebugString>* source = custom_source != nullptr ? custom_source : &strings;
		
		// Calculate the maximum amount of vertices needed for every type
		// and also get an integer mask to indicate the elements for each type
		unsigned int counts[ELEMENT_COUNT] = { 0 };
		ushort2* indices[ELEMENT_COUNT];
		auto deck_pointer = source;
		unsigned int total_count = deck_pointer->GetElementCount();

		if (total_count > 0) {
			/*if (shader_output == ECS_DEBUG_SHADER_OUTPUT_ID) {
				ECS_ASSERT(false);
			}*/
			// TODO: At the moment, we just skip in case we have to output IDs
			// For strings. Add that later on when the need arises
			if (shader_output == ECS_DEBUG_SHADER_OUTPUT_ID) {
				// Just exit
				return;
			}

			// Allocate 4 times the memory needed to be sure that each type has enough indices
			ushort2* type_indices = CalculateElementTypeCounts(allocator, counts, indices, deck_pointer, ECS_DEBUG_SHADER_OUTPUT_COLOR);

			// Get the maximum count of characters for each string
			size_t instance_count = 0;
			for (size_t index = 0; index < strings.buffers.size; index++) {
				for (size_t subindex = 0; subindex < strings.buffers[index].size; subindex++) {
					instance_count = max(strings.buffers[index][subindex].text.size, instance_count);
				}
			}
			// Sanity check
			ECS_ASSERT(instance_count < 10'000);

			// Create temporary buffers to hold the data
			VertexBuffer instanced_buffer = graphics->CreateVertexBuffer(sizeof(InstancedTransformData), instance_count, true);
			graphics->BindVertexBuffer(instanced_buffer, 1);
				
			unsigned int current_count = counts[WIREFRAME_DEPTH];
			ushort2* current_indices = indices[WIREFRAME_DEPTH];

			bool* checked_characters = (bool*)allocator->Allocate(sizeof(bool) * instance_count);
			float3* horizontal_offsets = (float3*)allocator->Allocate(sizeof(float3) * instance_count);
			float3* vertical_offsets = (float3*)allocator->Allocate(sizeof(float3) * instance_count);

			auto map_for = [&]() {
				for (size_t index = 0; index < current_count; index++) {
					const DebugString* string = &deck_pointer->buffers[current_indices[index].x][current_indices[index].y];

					// Normalize the direction
					float3 direction = Normalize(string->direction);

					float3 rotation_from_direction = DirectionToRotationEuler(direction);
					// Invert the y axis 
					rotation_from_direction.y = -rotation_from_direction.y;
					float3 up_direction = RotateVector(GetUpVector(), QuaternionFromEuler(rotation_from_direction));

					// Splat these early
					float3 space_size = float3::Splat(STRING_SPACE_SIZE * string->size) * direction;
					float3 tab_size = float3::Splat(STRING_TAB_SIZE * string->size) * direction;
					float3 character_spacing = float3::Splat(STRING_CHARACTER_SPACING * string->size) * direction;
					float3 new_line_size = float3::Splat(STRING_NEW_LINE_SIZE * -string->size) * up_direction;

					Matrix rotation_matrix = QuaternionRotationMatrix({ rotation_from_direction.x, rotation_from_direction.y, rotation_from_direction.z });
					Matrix scale_matrix = MatrixScale(float3::Splat(string->size));
					Matrix scale_rotation = MatrixRS(rotation_matrix, scale_matrix);

					// Keep track of already checked elements - the same principle as the Eratosthenes sieve
					memset(checked_characters, 0, sizeof(bool) * string->text.size);

					// Create a mask of spaces and tabs offsets for each character

					// Do the first character separately
					horizontal_offsets[0] = { 0.0f, 0.0f, 0.0f };
					vertical_offsets[0] = { 0.0f, 0.0f, 0.0f };

					if (string->text[0] == ' ' || string->text[0] == '\n' || string->text[0] == '\t') {
						checked_characters[0] = true;
					}

					for (size_t text_index = 1; text_index < string->text.size; text_index++) {
						// Hoist the checking outside the if
						checked_characters[text_index] = string->text[text_index] == ' ' || string->text[text_index] == '\t' || string->text[text_index] == '\n';

						if (string->text[text_index - 1] == '\n') {
							vertical_offsets[text_index] = vertical_offsets[text_index - 1] + new_line_size;

							// If a new character has been found - set to 0 all the other offsets - the tab and the space
							horizontal_offsets[text_index] = { 0.0f, 0.0f, 0.0f };
						}
						else {
							// It doesn't matter if the current character is \n because it will reset the horizontal offset anyway
							unsigned int previous_alphabet_index = GetAlphabetIndex(string->text[text_index - 1]);
							unsigned int current_alphabet_index = GetAlphabetIndex(string->text[text_index]);

							float previous_offset = string_character_bounds[previous_alphabet_index].y;
							float current_offset = string_character_bounds[current_alphabet_index].x;

							// The current offset will be negative
							float3 character_span = float3::Splat((previous_offset - current_offset) * string->size) * direction;
							horizontal_offsets[text_index] = horizontal_offsets[text_index - 1] + character_spacing + character_span;
							vertical_offsets[text_index] = vertical_offsets[text_index - 1];
						}
					}

					for (size_t text_index = 0; text_index < string->text.size; text_index++) {
						if (!checked_characters[text_index]) {
							unsigned int current_character_count = 0;

							void* instanced_data = graphics->MapBuffer(instanced_buffer.buffer);

							// Do a search for the same character
							for (size_t subindex = text_index; subindex < string->text.size; subindex++) {
								if (string->text[subindex] == string->text[text_index]) {
									float3 translation = string->position + horizontal_offsets[subindex] + vertical_offsets[subindex];
									checked_characters[subindex] = true;

									SetInstancedColor(instanced_data, current_character_count, string->color);
									SetInstancedMatrix(instanced_data, current_character_count, MatrixGPU(MatrixMVP(
										scale_rotation * MatrixTranslation(translation), camera_matrix
									)));
									current_character_count++;
								}
							}

							graphics->UnmapBuffer(instanced_buffer.buffer);

							unsigned int alphabet_index = GetAlphabetIndex(string->text[text_index]);
							// Draw the current character
							Submesh submesh = string_mesh->submeshes[alphabet_index];
							// Here the buffer offset needs to be 0
							submesh.vertex_buffer_offset = 0;
							graphics->DrawSubmeshCommand(submesh, current_character_count);
						}
					}
				}
			};

			ECS_MESH_INDEX mesh_position = ECS_MESH_POSITION;
			graphics->BindMesh(string_mesh->mesh, Stream<ECS_MESH_INDEX>(&mesh_position, 1));

			auto draw_call = [&](DebugDrawOptions options) {
				if (current_count > 0) {
					// Set the state
					SetTransformShaderState(options, ECS_DEBUG_SHADER_OUTPUT_COLOR);

					// Issues the draw calls once per string - too difficult to batch multiple strings at once
					map_for();
				}
			};

			// Draw all the Wireframe depth elements
			draw_call({ true, false });

			// Draw all the Wireframe no depth elements
			current_count = counts[WIREFRAME_NO_DEPTH];
			current_indices = indices[WIREFRAME_NO_DEPTH];
			draw_call({ true, true });

			// Draw all the Solid depth elements
			current_count = counts[SOLID_DEPTH];
			current_indices = indices[SOLID_DEPTH];
			draw_call({ false, false });

			// Draw all the Solid no depth elements
			current_count = counts[SOLID_NO_DEPTH];
			current_indices = indices[SOLID_NO_DEPTH];
			draw_call({ false, true });

			// Update the duration and remove those elements that expired;
			// Also deallocate the string buffer
			deck_pointer->ForEach<false, true>([this, time_delta](DebugString& string) {
				string.options.duration -= time_delta;
				if (string.options.duration < 0.0f) {
					allocator->Deallocate(string.text.buffer);
					return true;
				}
				return false;
			});

			// Release the temporary vertex buffers and the temporary allocation
			instanced_buffer.Release();
			allocator->Deallocate(type_indices);
			allocator->Deallocate(horizontal_offsets);
			allocator->Deallocate(vertical_offsets);
			allocator->Deallocate(checked_characters);
		}
	}

	void DebugDrawer::DrawGridDeck(float time_delta, DebugShaderOutput shader_output, DeckPowerOfTwo<DebugGrid>* custom_source)
	{
		DeckPowerOfTwo<DebugGrid>* source = custom_source != nullptr ? custom_source : &grids;
		source->ForEach([&](DebugGrid& grid) {
			DrawGrid(&grid, shader_output);
			grid.options.duration -= time_delta;
			if (grid.options.duration < 0.0f) {
				// Deallocate the valid cells if they have been retrieved
				if (grid.has_valid_cells) {
					grid.valid_cells.Deallocate();
				}
				return true;
			}
			return false;
		});
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::DrawAll(float time_delta, DebugShaderOutput shader_output)
	{
		if (is_redirect) {
			DeactivateRedirect(false);
			is_redirect = true;
		}
		for (unsigned int index = 0; index < thread_count; index++) {
			if (is_redirect_thread_count[index]) {
				DeactivateRedirect(false);
				is_redirect_thread_count[index] = true;
			}
		}

		FlushAll();
		DrawLineDeck(time_delta, shader_output);
		DrawSphereDeck(time_delta, shader_output);
		DrawPointDeck(time_delta, shader_output);
		DrawRectangleDeck(time_delta, shader_output);
		DrawCrossDeck(time_delta, shader_output);
		DrawCircleDeck(time_delta, shader_output);
		DrawArrowDeck(time_delta, shader_output);
		DrawTriangleDeck(time_delta, shader_output);
		DrawAABBDeck(time_delta, shader_output);
		DrawOOBBDeck(time_delta, shader_output);
		DrawStringDeck(time_delta, shader_output);
		DrawGridDeck(time_delta, shader_output);
	}

	// ----------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Output Instance Index

	// ----------------------------------------------------------------------------------------------------------------------

	ECS_INLINE const DebugDrawerOutput* ToDrawerOutput(const DebugDrawOptions* options) {
		return (const DebugDrawerOutput*)&options->instance_thickness;
	}

	void DebugDrawer::OutputInstanceIndexLine(
		float3 start, 
		float3 end, 
		DebugDrawOptions options,
		AdditionStreamAtomic<DebugLine>* addition_stream
	)
	{
		if (addition_stream != nullptr) {
			addition_stream->Add({ start, end, Color(), options });
		}
		else {
			DrawStructuredImmediate<ECS_DEBUG_SHADER_OUTPUT_ID, DRAW_STRUCTURED_LINE_TOPOLOGY>(
				this,
				1,
				ToDrawerOutput(&options),
				options,
				2,
				[&](InstancedVertex* positions, unsigned int index) {
					SetLinePositions(positions, index, start, end);
			});
		}
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::OutputInstanceIndexLine(
		float3 translation, 
		QuaternionScalar rotation, 
		float size, 
		DebugDrawOptions options,
		AdditionStreamAtomic<DebugLine>* addition_stream
	)
	{
		float3 start, end;
		ConvertTranslationLineIntoStartEnd(translation, rotation, size, start, end);
		OutputInstanceIndexLine(start, end, options, addition_stream);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::OutputInstanceIndexSphere(
		float3 translation, 
		float radius, 
		DebugDrawOptions options,
		AdditionStreamAtomic<DebugSphere>* addition_stream
	)
	{
		if (addition_stream != nullptr) {
			addition_stream->Add({ translation, radius, Color(), options });
		}
		else {
			DrawTransformImmediate<ECS_DEBUG_SHADER_OUTPUT_ID>(this, 1, ToDrawerOutput(&options), options, ECS_DEBUG_VERTEX_BUFFER_SPHERE, [&](unsigned int index) {
				return SphereMatrix(translation, radius, camera_matrix);
			});
		}
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::OutputInstanceIndexPoint(
		float3 translation, 
		DebugDrawOptions options, 
		AdditionStreamAtomic<DebugPoint>* addition_stream
	)
	{
		DebugDrawOptions temp_options = options;
		temp_options.wireframe = false;

		if (addition_stream != nullptr) {
			addition_stream->Add({ translation, Color(), temp_options });
		}
		else {
			DrawTransformImmediate<ECS_DEBUG_SHADER_OUTPUT_COLOR>(this, 1, ToDrawerOutput(&options), temp_options, ECS_DEBUG_VERTEX_BUFFER_POINT, [&](unsigned int index) {
				return PointMatrix(translation, camera_matrix);
			});
		}
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::OutputInstanceIndexRectangle(
		float3 corner0, 
		float3 corner1, 
		DebugDrawOptions options,
		AdditionStreamAtomic<DebugRectangle>* addition_stream
	)
	{
		if (addition_stream != nullptr) {
			addition_stream->Add({ corner0, corner1, Color(), options });
		}
		else {
			DrawStructuredImmediate<ECS_DEBUG_SHADER_OUTPUT_ID>(this, 1, ToDrawerOutput(&options), options, 6, [&](InstancedVertex* positions, unsigned int index) {
				SetRectanglePositionsFront(positions, index, corner0, corner1);
			});
		}
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::OutputInstanceIndexCross(
		float3 position, 
		QuaternionScalar rotation, 
		float size, 
		DebugDrawOptions options,
		AdditionStreamAtomic<DebugCross>* addition_stream
	)
	{
		if (addition_stream != nullptr) {
			addition_stream->Add({ position, rotation, size, Color(), options });
		}
		else {
			DrawTransformImmediate<ECS_DEBUG_SHADER_OUTPUT_ID>(this, 1, ToDrawerOutput(&options), options, ECS_DEBUG_VERTEX_BUFFER_CROSS, [&](unsigned int index) {
				return CrossMatrix(position, rotation, size, camera_matrix);
			});
		}
	}

	void DebugDrawer::OutputInstanceIndexOOBBCross(
		float3 position, 
		QuaternionScalar rotation,
		float length,
		float size, 
		bool start_from_same_point, 
		const DebugOOBBCrossInfo* info,
		DebugDrawOptions options, 
		AdditionStreamAtomic<DebugOOBB>* addition_stream
	)
	{
		QuaternionScalar rotation_quaternion = rotation;
		float3 translations[] = {
			OOBBCrossXTranslation(position, rotation_quaternion, length, start_from_same_point),
			OOBBCrossYTranslation(position, rotation_quaternion, length, start_from_same_point),
			OOBBCrossZTranslation(position, rotation_quaternion, length, start_from_same_point),
		};
		QuaternionScalar rotations[] = {
			AxesArrowXRotation(rotation),
			AxesArrowYRotation(rotation),
			AxesArrowZRotation(rotation)
		};
		float3 complete_sizes[] = {
			OOBBCrossSize(length, size, info->size_override_x),
			OOBBCrossSize(length, size, info->size_override_y),
			OOBBCrossSize(length, size, info->size_override_z),
		};

		if (addition_stream != nullptr) {
			unsigned int instance_thickness_values[] = {
				info->instance_thickness_x,
				info->instance_thickness_y,
				info->instance_thickness_z,
			};

			DebugOOBB oobb_crosses[3] = {};
			for (size_t index = 0; index < ECS_COUNTOF(oobb_crosses); index++) {
				DebugDrawOptions current_option = options;
				current_option.instance_thickness = instance_thickness_values[index];
				oobb_crosses[index] = {
					translations[index],
					rotations[index],
					complete_sizes[index],
					Color(),
					current_option
				};
			}

			addition_stream->AddStream({ oobb_crosses, ECS_COUNTOF(oobb_crosses) });
		}
		else {
			DebugDrawerOutput outputs[] = {
				info->instance_thickness_x,
				info->instance_thickness_y,
				info->instance_thickness_z
			};

			// Hopefully since the function is marked as inline the compiler will notice the same function being
			// called multiple times and fold the calls into a single one (with optimizations for sure, in release
			// I hope so)
			Matrix matrices[3] = {};
			for (size_t index = 0; index < ECS_COUNTOF(matrices); index++) {
				matrices[index] = OOBBCrossMatrix(
					translations[index],
					rotations[index],
					length,
					size,
					info->GetSizeOverrides()[index],
					camera_matrix
				);
			}

			DrawTransformImmediate<ECS_DEBUG_SHADER_OUTPUT_ID>(this, 3, outputs, options, ECS_DEBUG_VERTEX_BUFFER_CUBE, [&](unsigned int index) {
				return matrices[index];
			});
		}
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::OutputInstanceIndexCircle(
		float3 position, 
		QuaternionScalar rotation, 
		float radius, 
		DebugDrawOptions options,
		AdditionStreamAtomic<DebugCircle>* addition_stream
	)
	{
		if (addition_stream != nullptr) {
			addition_stream->Add({ position, rotation, radius, Color(), options });
		}
		else {
			DrawTransformImmediate<ECS_DEBUG_SHADER_OUTPUT_ID, DRAW_TRANSFORM_CIRCLE_BUFFER>(this, 1, ToDrawerOutput(&options), options, ECS_DEBUG_VERTEX_BUFFER_COUNT, 
				[&](unsigned int index) {
				return CircleMatrix(position, rotation, radius, camera_matrix);
			});
		}
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::OutputInstanceIndexArrow(
		float3 start, 
		float3 end, 
		float size, 
		DebugDrawOptions options,
		AdditionStreamAtomic<DebugArrow>* addition_stream
	)
	{
		QuaternionScalar rotation;
		float length;
		ArrowStartEndToRotation(start, end, &rotation, &length);
		OutputInstanceIndexArrowRotation(start, rotation, length, size, options, addition_stream);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::OutputInstanceIndexArrowRotation(
		float3 translation, 
		QuaternionScalar rotation, 
		float length, 
		float size, 
		DebugDrawOptions options,
		AdditionStreamAtomic<DebugArrow>* addition_stream
	)
	{
		options.instance_thickness = GenerateRenderInstanceValue(options.instance_thickness, 0);
		if (addition_stream != nullptr) {
			addition_stream->Add({ translation, rotation, length, size, Color(), options });
		}
		else {
			DrawTransformImmediate<ECS_DEBUG_SHADER_OUTPUT_ID>(this, 1, ToDrawerOutput(&options), options, ECS_DEBUG_VERTEX_BUFFER_ARROW, [&](unsigned int index) {
				return ArrowMatrix(translation, rotation, length, size, camera_matrix);
			});
		}
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::OutputInstanceIndexAxes(
		float3 translation,
		QuaternionScalar rotation,
		float size,
		unsigned int instance_thickness_x,
		unsigned int instance_thickness_y,
		unsigned int instance_thickness_z,
		DebugDrawOptions options,
		AdditionStreamAtomic<DebugArrow>* addition_stream
	)
	{
		if (addition_stream != nullptr) {
			DebugDrawOptions options_x = options;
			options_x.instance_thickness = instance_thickness_x;

			DebugDrawOptions options_y = options;
			options_y.instance_thickness = instance_thickness_y;

			DebugDrawOptions options_z = options;
			options_z.instance_thickness = instance_thickness_z;

			DebugArrow axes_arrows[] = {
				{ translation, AxesArrowXRotation(rotation), size, size, Color(), options_x },
				{ translation, AxesArrowYRotation(rotation), size, size, Color(), options_y },
				{ translation, AxesArrowZRotation(rotation), size, size, Color(), options_z }
			};

			addition_stream->AddStream({ axes_arrows, ECS_COUNTOF(axes_arrows)});
		}
		else {
			DebugDrawerOutput outputs[] = {
				instance_thickness_x,
				instance_thickness_y,
				instance_thickness_z
			};

			// Hopefully since the function is marked as inline the compiler will notice the same function being
			// called multiple times and fold the calls into a single one (with optimizations for sure, in release
			// I hope so)
			Matrix matrices[] = {
				ArrowMatrix(translation, AxesArrowXRotation(rotation), size, size, camera_matrix),
				ArrowMatrix(translation, AxesArrowYRotation(rotation), size, size, camera_matrix),
				ArrowMatrix(translation, AxesArrowZRotation(rotation), size, size, camera_matrix)
			};

			DrawTransformImmediate<ECS_DEBUG_SHADER_OUTPUT_ID>(this, 3, outputs, options, ECS_DEBUG_VERTEX_BUFFER_ARROW, [&](unsigned int index) {
				return matrices[index];
			});
		}
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::OutputInstanceIndexTriangle(
		float3 point0, 
		float3 point1, 
		float3 point2, 
		DebugDrawOptions options,
		AdditionStreamAtomic<DebugTriangle>* addition_stream
	)
	{
		if (addition_stream != nullptr) {
			addition_stream->Add({ point0, point1, point2, Color(), options });
		}
		else {
			DrawStructuredImmediate<ECS_DEBUG_SHADER_OUTPUT_ID>(this, 1, ToDrawerOutput(&options), options, 3, [&](InstancedVertex* positions, unsigned int index) {
				SetTrianglePositions(positions, index, point0, point1, point2);
			});
		}
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::OutputInstanceIndexAABB(
		float3 translation, 
		float3 scale, 
		DebugDrawOptions options,
		AdditionStreamAtomic<DebugAABB>* addition_stream
	)
	{
		if (addition_stream != nullptr) {
			addition_stream->Add({ translation, scale, Color(), options });
		}
		else {
			DrawTransformImmediate<ECS_DEBUG_SHADER_OUTPUT_ID>(this, 1, ToDrawerOutput(&options), options, ECS_DEBUG_VERTEX_BUFFER_CUBE, [&](unsigned int index) {
				return AABBMatrix(translation, scale, camera_matrix);
			});
		}
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::OutputInstanceIndexOOBB(
		float3 translation, 
		QuaternionScalar rotation, 
		float3 scale, 
		DebugDrawOptions options,
		AdditionStreamAtomic<DebugOOBB>* addition_stream
	)
	{
		if (addition_stream != nullptr) {
			addition_stream->Add({ translation, rotation, scale, Color(), options });
		}
		else {
			DrawTransformImmediate<ECS_DEBUG_SHADER_OUTPUT_ID>(this, 1, ToDrawerOutput(&options), options, ECS_DEBUG_VERTEX_BUFFER_CUBE, [&](unsigned int index) {
				return OOBBMatrix(translation, rotation, scale, camera_matrix);
			});
		}
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::OutputInstanceIndexString(
		float3 translation, 
		float3 direction, 
		float size, 
		Stream<char> text, 
		DebugDrawOptions options,
		AdditionStreamAtomic<DebugString>* addition_stream, 
		AllocatorPolymorphic _allocator
	)
	{
		ECS_ASSERT(false, "Output ID for Debug Strings is not yet implemented");
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::OutputInstanceIndexStringRotation(
		float3 translation, 
		QuaternionScalar rotation, 
		float size, 
		Stream<char> text, 
		DebugDrawOptions options,
		AdditionStreamAtomic<DebugString>* addition_stream, 
		AllocatorPolymorphic _allocator
	)
	{
		OutputInstanceIndexString(translation, RotateVector(GetRightVector(), rotation), size, text, options, addition_stream, _allocator);
	}


	// ----------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Output Instance Bulk

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::OutputInstanceIndexLineBulk(const AdditionStreamAtomic<DebugLine>* addition_stream, float time_delta)
	{
		ECS_STACK_VOID_STREAM(temp_memory, 256);
		DeckPowerOfTwo<DebugLine> temp_deck = DeckPowerOfTwo<DebugLine>::InitializeTempReference(addition_stream->ToStream(), temp_memory.buffer);
		DrawStructuredDeck<DRAW_STRUCTURED_LINE_TOPOLOGY>(this, &temp_deck, 2, time_delta, ECS_DEBUG_SHADER_OUTPUT_ID,
			[&](InstancedVertex* positions, unsigned int index, const DebugLine* line) {
				SetLinePositions(positions, index, line->start, line->end);
		});
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::OutputInstanceIndexSphereBulk(const AdditionStreamAtomic<DebugSphere>* addition_stream, float time_delta)
	{
		ECS_STACK_VOID_STREAM(temp_memory, 256);
		DeckPowerOfTwo<DebugSphere> temp_deck = DeckPowerOfTwo<DebugSphere>::InitializeTempReference(addition_stream->ToStream(), temp_memory.buffer);
		DrawTransformDeck(this, &temp_deck, ECS_DEBUG_VERTEX_BUFFER_SPHERE, time_delta, ECS_DEBUG_SHADER_OUTPUT_ID);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::OutputInstanceIndexPointBulk(const AdditionStreamAtomic<DebugPoint>* addition_stream, float time_delta)
	{
		ECS_STACK_VOID_STREAM(temp_memory, 256);
		DeckPowerOfTwo<DebugPoint> temp_deck = DeckPowerOfTwo<DebugPoint>::InitializeTempReference(addition_stream->ToStream(), temp_memory.buffer);
		DrawTransformDeck(this, &temp_deck, ECS_DEBUG_VERTEX_BUFFER_POINT, time_delta, ECS_DEBUG_SHADER_OUTPUT_ID);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::OutputInstanceIndexRectangleBulk(const AdditionStreamAtomic<DebugRectangle>* addition_stream, float time_delta)
	{
		ECS_STACK_VOID_STREAM(temp_memory, 256);
		DeckPowerOfTwo<DebugRectangle> temp_deck = DeckPowerOfTwo<DebugRectangle>::InitializeTempReference(addition_stream->ToStream(), temp_memory.buffer);
		DrawStructuredDeck(this, &temp_deck, 6, time_delta, ECS_DEBUG_SHADER_OUTPUT_ID, [](InstancedVertex* positions, unsigned int index, const DebugRectangle* rectangle) {
			SetRectanglePositionsFront(positions, index, rectangle->corner0, rectangle->corner1);
		});
	}
	
	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::OutputInstanceIndexCrossBulk(const AdditionStreamAtomic<DebugCross>* addition_stream, float time_delta)
	{
		ECS_STACK_VOID_STREAM(temp_memory, 256);
		DeckPowerOfTwo<DebugCross> temp_deck = DeckPowerOfTwo<DebugCross>::InitializeTempReference(addition_stream->ToStream(), temp_memory.buffer);
		DrawTransformDeck(this, &temp_deck, ECS_DEBUG_VERTEX_BUFFER_CROSS, time_delta, ECS_DEBUG_SHADER_OUTPUT_ID);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::OutputInstanceIndexCircleBulk(const AdditionStreamAtomic<DebugCircle>* addition_stream, float time_delta)
	{
		ECS_STACK_VOID_STREAM(temp_memory, 256);
		DeckPowerOfTwo<DebugCircle> temp_deck = DeckPowerOfTwo<DebugCircle>::InitializeTempReference(addition_stream->ToStream(), temp_memory.buffer);
		DrawTransformDeck<DRAW_TRANSFORM_CIRCLE_BUFFER>(this, &temp_deck, ECS_DEBUG_VERTEX_BUFFER_COUNT, time_delta, ECS_DEBUG_SHADER_OUTPUT_ID);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::OutputInstanceIndexArrowBulk(const AdditionStreamAtomic<DebugArrow>* addition_stream, float time_delta)
	{
		ECS_STACK_VOID_STREAM(temp_memory, 256);
		DeckPowerOfTwo<DebugArrow> temp_deck = DeckPowerOfTwo<DebugArrow>::InitializeTempReference(addition_stream->ToStream(), temp_memory.buffer);
		DrawTransformDeck(this, &temp_deck, ECS_DEBUG_VERTEX_BUFFER_ARROW, time_delta, ECS_DEBUG_SHADER_OUTPUT_ID);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::OutputInstanceIndexTriangleBulk(const AdditionStreamAtomic<DebugTriangle>* addition_stream, float time_delta)
	{
		ECS_STACK_VOID_STREAM(temp_memory, 256);
		DeckPowerOfTwo<DebugTriangle> temp_deck = DeckPowerOfTwo<DebugTriangle>::InitializeTempReference(addition_stream->ToStream(), temp_memory.buffer);
		DrawStructuredDeck(this, &temp_deck, 3, time_delta, ECS_DEBUG_SHADER_OUTPUT_ID, [](InstancedVertex* positions, unsigned int index, const DebugTriangle* triangle) {
			SetTrianglePositions(positions, index, triangle->point0, triangle->point1, triangle->point2);
		});
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::OutputInstanceIndexAABBBulk(const AdditionStreamAtomic<DebugAABB>* addition_stream, float time_delta)
	{
		ECS_STACK_VOID_STREAM(temp_memory, 256);
		DeckPowerOfTwo<DebugAABB> temp_deck = DeckPowerOfTwo<DebugAABB>::InitializeTempReference(addition_stream->ToStream(), temp_memory.buffer);
		DrawTransformDeck(this, &temp_deck, ECS_DEBUG_VERTEX_BUFFER_CUBE, time_delta, ECS_DEBUG_SHADER_OUTPUT_ID);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::OutputInstanceIndexOOBBBulk(const AdditionStreamAtomic<DebugOOBB>* addition_stream, float time_delta)
	{
		ECS_STACK_VOID_STREAM(temp_memory, 256);
		DeckPowerOfTwo<DebugOOBB> temp_deck = DeckPowerOfTwo<DebugOOBB>::InitializeTempReference(addition_stream->ToStream(), temp_memory.buffer);
		DrawTransformDeck(this, &temp_deck, ECS_DEBUG_VERTEX_BUFFER_CUBE, time_delta, ECS_DEBUG_SHADER_OUTPUT_ID);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::OutputInstanceIndexStringBulk(const AdditionStreamAtomic<DebugString>* addition_stream, float time_delta)
	{
		ECS_ASSERT(false, "Output instance index bulk write for debug strings is not yet implemented");
	}

	// ----------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Flush

	template<typename Deck, typename ThreadStream>
	void FlushType(DebugDrawer* drawer, Deck* deck, ThreadStream* thread_buffering, DebugPrimitive debug_primitive, unsigned int thread_index) {
		if (thread_buffering[thread_index].size > 0) {
			drawer->thread_locks[debug_primitive]->Lock();
			bool needs_allocator_lock = !deck->CanAddNoResize(thread_buffering[thread_index].size);
			if (needs_allocator_lock) {
				drawer->allocator->Lock();
			}
			// This will take care of the reserve
			deck->AddStream(thread_buffering[thread_index]);
			if (needs_allocator_lock) {
				drawer->allocator->Unlock();
			}

			// Before resetting the size, if the redirect is activated, we need to redirect
			// The entries that were added
			if (drawer->redirect_thread_counts[thread_index]) {
				AddRedirects(GetThreadRedirectCount(drawer, thread_index), thread_index, RedirectThreadSizeExtractor{ thread_index },
					RedirectThreadValueExtractor{ thread_index }, drawer->thread_lines, drawer->thread_spheres, drawer->thread_points,
					drawer->thread_rectangles, drawer->thread_crosses, drawer->thread_circles, drawer->thread_arrows, drawer->thread_triangles,
					drawer->thread_aabbs, drawer->thread_oobbs, drawer->thread_strings, drawer->thread_grids, drawer->redirect_drawer);
				// Set the counts to 0, to indicate that these entries have been added
				memset(GetThreadRedirectCount(drawer, thread_index), 0, sizeof(unsigned int) * ECS_DEBUG_PRIMITIVE_COUNT);
			}

			thread_buffering[thread_index].size = 0;
			drawer->thread_locks[debug_primitive]->Unlock();
		}
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::FlushAll()
	{
		for (size_t index = 0; index < thread_count; index++) {
			FlushLine(index);
			FlushSphere(index);
			FlushPoint(index);
			FlushRectangle(index);
			FlushCross(index);
			FlushCircle(index);
			FlushArrow(index);
			FlushTriangle(index);
			FlushAABB(index);
			FlushOOBB(index);
			FlushString(index);
			FlushGrid(index);
		}
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::FlushLine(unsigned int thread_index)
	{
		FlushType(this, &lines, thread_lines, ECS_DEBUG_PRIMITIVE_LINE, thread_index);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::FlushSphere(unsigned int thread_index)
	{
		FlushType(this, &spheres, thread_spheres, ECS_DEBUG_PRIMITIVE_SPHERE, thread_index);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::FlushPoint(unsigned int thread_index)
	{
		FlushType(this, &points, thread_points, ECS_DEBUG_PRIMITIVE_POINT, thread_index);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::FlushRectangle(unsigned int thread_index)
	{
		FlushType(this, &rectangles, thread_rectangles, ECS_DEBUG_PRIMITIVE_RECTANGLE, thread_index);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::FlushCross(unsigned int thread_index)
	{
		FlushType(this, &crosses, thread_crosses, ECS_DEBUG_PRIMITIVE_CROSS, thread_index);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::FlushCircle(unsigned int thread_index)
	{
		FlushType(this, &circles, thread_circles, ECS_DEBUG_PRIMITIVE_CIRCLE, thread_index);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::FlushArrow(unsigned int thread_index)
	{
		FlushType(this, &arrows, thread_arrows, ECS_DEBUG_PRIMITIVE_ARROW, thread_index);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::FlushTriangle(unsigned int thread_index)
	{
		FlushType(this, &triangles, thread_triangles, ECS_DEBUG_PRIMITIVE_TRIANGLE, thread_index);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::FlushAABB(unsigned int thread_index)
	{
		FlushType(this, &aabbs, thread_aabbs, ECS_DEBUG_PRIMITIVE_AABB, thread_index);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::FlushOOBB(unsigned int thread_index)
	{
		FlushType(this, &oobbs, thread_oobbs, ECS_DEBUG_PRIMITIVE_OOBB, thread_index);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::FlushString(unsigned int thread_index)
	{
		FlushType(this, &strings, thread_strings, ECS_DEBUG_PRIMITIVE_STRING, thread_index);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::FlushGrid(unsigned int thread_index)
	{
		FlushType(this, &grids, thread_grids, ECS_DEBUG_PRIMITIVE_GRID, thread_index);
	}

	// ----------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Set state

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::SetTransformShaderState(DebugDrawOptions options, DebugShaderOutput output)
	{
		unsigned int shader_type = ECS_DEBUG_SHADER_TRANSFORM;
		BindShaders(shader_type, output);
		HandleOptions(this, options);
		graphics->BindTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::SetStructuredShaderState(DebugDrawOptions options, DebugShaderOutput output)
	{
		unsigned int shader_type = ECS_DEBUG_SHADER_STRUCTURED_INSTANCED_DATA;
		BindShaders(shader_type, output);
		HandleOptions(this, options);
		graphics->BindTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	}

	// ----------------------------------------------------------------------------------------------------------------------

#pragma endregion

#pragma region Initialize

	// It assumes that the allocator was set
	static void InitializeMemoryResources(DebugDrawer* drawer, size_t thread_count) {
		size_t total_memory = 0;
		size_t thread_capacity_stream_size = sizeof(CapacityStream<void>) * thread_count;

		total_memory += drawer->thread_lines->MemoryOf(PER_THREAD_RESOURCES) * thread_count;
		total_memory += drawer->thread_spheres->MemoryOf(PER_THREAD_RESOURCES) * thread_count;
		total_memory += drawer->thread_points->MemoryOf(PER_THREAD_RESOURCES) * thread_count;
		total_memory += drawer->thread_rectangles->MemoryOf(PER_THREAD_RESOURCES) * thread_count;
		total_memory += drawer->thread_crosses->MemoryOf(PER_THREAD_RESOURCES) * thread_count;
		total_memory += drawer->thread_circles->MemoryOf(PER_THREAD_RESOURCES) * thread_count;
		total_memory += drawer->thread_arrows->MemoryOf(PER_THREAD_RESOURCES) * thread_count;
		total_memory += drawer->thread_triangles->MemoryOf(PER_THREAD_RESOURCES) * thread_count;
		total_memory += drawer->thread_aabbs->MemoryOf(PER_THREAD_RESOURCES) * thread_count;
		total_memory += drawer->thread_oobbs->MemoryOf(PER_THREAD_RESOURCES) * thread_count;
		total_memory += drawer->thread_strings->MemoryOf(PER_THREAD_RESOURCES) * thread_count;
		total_memory += drawer->thread_grids->MemoryOf(GRID_THREAD_CAPACITY) * thread_count;

		// Add the actual capacity stream sizes
		total_memory += thread_capacity_stream_size * ECS_DEBUG_PRIMITIVE_COUNT;

		// The locks will be padded to different cache lines
		total_memory += ECS_CACHE_LINE_SIZE * thread_count + sizeof(SpinLock*) * ECS_DEBUG_PRIMITIVE_COUNT;

		// The string character bounds
		total_memory += sizeof(float2) * (unsigned int)AlphabetIndex::Unknown;

		void* allocation = drawer->allocator->Allocate(total_memory);
		uintptr_t buffer = (uintptr_t)allocation;

		drawer->thread_lines = (CapacityStream<DebugLine>*)buffer;
		buffer += thread_capacity_stream_size;

		drawer->thread_spheres = (CapacityStream<DebugSphere>*)buffer;
		buffer += thread_capacity_stream_size;

		drawer->thread_points = (CapacityStream<DebugPoint>*)buffer;
		buffer += thread_capacity_stream_size;

		drawer->thread_rectangles = (CapacityStream<DebugRectangle>*)buffer;
		buffer += thread_capacity_stream_size;

		drawer->thread_crosses = (CapacityStream<DebugCross>*)buffer;
		buffer += thread_capacity_stream_size;

		drawer->thread_circles = (CapacityStream<DebugCircle>*)buffer;
		buffer += thread_capacity_stream_size;

		drawer->thread_arrows = (CapacityStream<DebugArrow>*)buffer;
		buffer += thread_capacity_stream_size;

		drawer->thread_triangles = (CapacityStream<DebugTriangle>*)buffer;
		buffer += thread_capacity_stream_size;

		drawer->thread_aabbs = (CapacityStream<DebugAABB>*)buffer;
		buffer += thread_capacity_stream_size;

		drawer->thread_oobbs = (CapacityStream<DebugOOBB>*)buffer;
		buffer += thread_capacity_stream_size;

		drawer->thread_strings = (CapacityStream<DebugString>*)buffer;
		buffer += thread_capacity_stream_size;

		drawer->thread_grids = (CapacityStream<DebugGrid>*)buffer;
		buffer += thread_capacity_stream_size;

		for (size_t index = 0; index < thread_count; index++) {
			drawer->thread_lines[index].InitializeFromBuffer(buffer, 0, PER_THREAD_RESOURCES);
			drawer->thread_spheres[index].InitializeFromBuffer(buffer, 0, PER_THREAD_RESOURCES);
			drawer->thread_points[index].InitializeFromBuffer(buffer, 0, PER_THREAD_RESOURCES);
			drawer->thread_rectangles[index].InitializeFromBuffer(buffer, 0, PER_THREAD_RESOURCES);
			drawer->thread_crosses[index].InitializeFromBuffer(buffer, 0, PER_THREAD_RESOURCES);
			drawer->thread_circles[index].InitializeFromBuffer(buffer, 0, PER_THREAD_RESOURCES);
			drawer->thread_arrows[index].InitializeFromBuffer(buffer, 0, PER_THREAD_RESOURCES);
			drawer->thread_triangles[index].InitializeFromBuffer(buffer, 0, PER_THREAD_RESOURCES);
			drawer->thread_aabbs[index].InitializeFromBuffer(buffer, 0, PER_THREAD_RESOURCES);
			drawer->thread_oobbs[index].InitializeFromBuffer(buffer, 0, PER_THREAD_RESOURCES);
			drawer->thread_strings[index].InitializeFromBuffer(buffer, 0, PER_THREAD_RESOURCES);
			drawer->thread_grids[index].InitializeFromBuffer(buffer, 0, GRID_THREAD_CAPACITY);
		}

		drawer->thread_locks = (SpinLock**)buffer;
		buffer += sizeof(SpinLock*) * ECS_DEBUG_PRIMITIVE_COUNT;

		// Padd the locks to different cache lines
		for (size_t index = 0; index < ECS_DEBUG_PRIMITIVE_COUNT; index++) {
			drawer->thread_locks[index] = (SpinLock*)buffer;
			drawer->thread_locks[index]->Clear();
			buffer += ECS_CACHE_LINE_SIZE;
		}
		drawer->string_character_bounds = (float2*)buffer;

		AllocatorPolymorphic polymorphic_allocator = drawer->allocator;

		drawer->lines.Initialize(polymorphic_allocator, 1, DECK_CHUNK_SIZE, DECK_POWER_OF_TWO);
		drawer->spheres.Initialize(polymorphic_allocator, 1, DECK_CHUNK_SIZE, DECK_POWER_OF_TWO);
		drawer->points.Initialize(polymorphic_allocator, 1, DECK_CHUNK_SIZE, DECK_POWER_OF_TWO);
		drawer->rectangles.Initialize(polymorphic_allocator, 1, DECK_CHUNK_SIZE, DECK_POWER_OF_TWO);
		drawer->crosses.Initialize(polymorphic_allocator, 1, DECK_CHUNK_SIZE, DECK_POWER_OF_TWO);
		drawer->circles.Initialize(polymorphic_allocator, 1, DECK_CHUNK_SIZE, DECK_POWER_OF_TWO);
		drawer->arrows.Initialize(polymorphic_allocator, 1, DECK_CHUNK_SIZE, DECK_POWER_OF_TWO);
		drawer->triangles.Initialize(polymorphic_allocator, 1, DECK_CHUNK_SIZE, DECK_POWER_OF_TWO);
		drawer->aabbs.Initialize(polymorphic_allocator, 1, DECK_CHUNK_SIZE, DECK_POWER_OF_TWO);
		drawer->oobbs.Initialize(polymorphic_allocator, 1, DECK_CHUNK_SIZE, DECK_POWER_OF_TWO);
		drawer->strings.Initialize(polymorphic_allocator, 1, DECK_CHUNK_SIZE, DECK_POWER_OF_TWO);
		drawer->grids.Initialize(polymorphic_allocator, 1, DECK_CHUNK_SIZE, DECK_POWER_OF_TWO);

		drawer->is_redirect = false;
		drawer->redirect_drawer = nullptr;
		memset(drawer->redirect_counts, 0, sizeof(drawer->redirect_counts));
		drawer->redirect_thread_counts = (unsigned int*)drawer->allocator->Allocate(sizeof(unsigned int) * ECS_DEBUG_PRIMITIVE_COUNT * thread_count);
		memset(drawer->redirect_thread_counts, 0, sizeof(unsigned int) * ECS_DEBUG_PRIMITIVE_COUNT * thread_count);
		drawer->is_redirect_thread_count = (bool*)drawer->allocator->Allocate(sizeof(bool) * thread_count);
		memset(drawer->is_redirect_thread_count, false, sizeof(bool) * thread_count);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::Initialize(MemoryManager* _allocator, ResourceManager* resource_manager, size_t _thread_count) {
		allocator = _allocator;
		graphics = resource_manager->m_graphics;
		thread_count = _thread_count;

		// Initialize the small vertex buffers
		positions_small_vertex_buffer = graphics->CreateVertexBuffer(sizeof(InstancedVertex), SMALL_VERTEX_BUFFER_CAPACITY);
		instanced_small_transform_vertex_buffer = graphics->CreateVertexBuffer(sizeof(InstancedTransformData), SMALL_VERTEX_BUFFER_CAPACITY);
		instanced_small_structured_buffer = graphics->CreateStructuredBuffer(sizeof(InstancedTransformData), SMALL_VERTEX_BUFFER_CAPACITY);
		instanced_structured_view = graphics->CreateBufferView(instanced_small_structured_buffer);

		output_instance_small_matrix_buffer = graphics->CreateVertexBuffer(sizeof(Matrix), SMALL_VERTEX_BUFFER_CAPACITY);
		output_instance_small_id_buffer = graphics->CreateVertexBuffer(sizeof(unsigned int), SMALL_VERTEX_BUFFER_CAPACITY);
		output_instance_matrix_small_structured_buffer = graphics->CreateStructuredBuffer(sizeof(Matrix), SMALL_VERTEX_BUFFER_CAPACITY);
		output_instance_matrix_structured_view = graphics->CreateBufferView(output_instance_matrix_small_structured_buffer);
		output_instance_id_small_structured_buffer = graphics->CreateStructuredBuffer(sizeof(unsigned int), SMALL_VERTEX_BUFFER_CAPACITY);
		output_instance_id_structured_view = graphics->CreateBufferView(output_instance_id_small_structured_buffer);

		// Initialize the large vertex buffers
		positions_large_vertex_buffer = graphics->CreateVertexBuffer(sizeof(InstancedVertex), LARGE_BUFFER_CAPACITY);
		instanced_large_vertex_buffer = graphics->CreateVertexBuffer(sizeof(InstancedTransformData), LARGE_BUFFER_CAPACITY);
		output_large_matrix_vertex_buffer = graphics->CreateVertexBuffer(sizeof(Matrix), LARGE_BUFFER_CAPACITY);
		output_large_id_vertex_buffer = graphics->CreateVertexBuffer(sizeof(unsigned int), LARGE_BUFFER_CAPACITY);
		output_large_instanced_structured_buffer = graphics->CreateStructuredBuffer(sizeof(InstancedTransformData), LARGE_BUFFER_CAPACITY);
		output_large_instanced_buffer_view = graphics->CreateBufferView(output_large_instanced_structured_buffer);
		output_large_matrix_structured_buffer = graphics->CreateStructuredBuffer(sizeof(Matrix), LARGE_BUFFER_CAPACITY);
		output_large_matrix_buffer_view = graphics->CreateBufferView(output_large_matrix_structured_buffer);
		output_large_id_structured_buffer = graphics->CreateStructuredBuffer(sizeof(unsigned int), LARGE_BUFFER_CAPACITY);
		output_large_id_buffer_view = graphics->CreateBufferView(output_large_id_structured_buffer);

		InputLayout loaded_shader_input_layout;
		LoadShaderExtraInformation load_shader_extra_information;
		load_shader_extra_information.input_layout = &loaded_shader_input_layout;

#define REGISTER_VERTEX_SHADER(type, output_type, source_name) vertex_shaders[type][output_type] = \
		resource_manager->LoadShaderImplementation( \
			ECS_VERTEX_SHADER_SOURCE(source_name), \
			ECS_SHADER_VERTEX, \
			false, \
			load_shader_extra_information \
		); \
		/*shader_source = PreprocessCFile(shader_source);*/ \
		layout_shaders[type][output_type] = loaded_shader_input_layout;

#define REGISTER_SHADER(index, output_type, name) REGISTER_VERTEX_SHADER(index, output_type, name); \
		pixel_shaders[index][output_type] = resource_manager->LoadShaderImplementation(ECS_PIXEL_SHADER_SOURCE(name), ECS_SHADER_PIXEL, false); \


		// Initialize the shaders and input layouts
		REGISTER_SHADER(ECS_DEBUG_SHADER_TRANSFORM, ECS_DEBUG_SHADER_OUTPUT_COLOR, DebugTransform);
		REGISTER_SHADER(ECS_DEBUG_SHADER_STRUCTURED_INSTANCED_DATA, ECS_DEBUG_SHADER_OUTPUT_COLOR, DebugStructured);
		REGISTER_SHADER(ECS_DEBUG_SHADER_NORMAL_SINGLE_DRAW, ECS_DEBUG_SHADER_OUTPUT_COLOR, DebugNormalSingleDraw);

		REGISTER_VERTEX_SHADER(ECS_DEBUG_SHADER_TRANSFORM, ECS_DEBUG_SHADER_OUTPUT_ID, DebugTransformInstance);
		pixel_shaders[ECS_DEBUG_SHADER_TRANSFORM][ECS_DEBUG_SHADER_OUTPUT_ID] = resource_manager->LoadShaderImplementation(
			ECS_PIXEL_SHADER_SOURCE(DebugMousePick),
			ECS_SHADER_PIXEL,
			false
		);

		REGISTER_VERTEX_SHADER(ECS_DEBUG_SHADER_STRUCTURED_INSTANCED_DATA, ECS_DEBUG_SHADER_OUTPUT_ID, DebugStructuredInstance);
		pixel_shaders[ECS_DEBUG_SHADER_STRUCTURED_INSTANCED_DATA][ECS_DEBUG_SHADER_OUTPUT_ID] = pixel_shaders[ECS_DEBUG_SHADER_TRANSFORM][ECS_DEBUG_SHADER_OUTPUT_ID];

		// This is not available
		vertex_shaders[ECS_DEBUG_SHADER_NORMAL_SINGLE_DRAW][ECS_DEBUG_SHADER_OUTPUT_ID] = nullptr;
		pixel_shaders[ECS_DEBUG_SHADER_NORMAL_SINGLE_DRAW][ECS_DEBUG_SHADER_OUTPUT_ID] = nullptr;
		layout_shaders[ECS_DEBUG_SHADER_NORMAL_SINGLE_DRAW][ECS_DEBUG_SHADER_OUTPUT_ID] = nullptr;

#undef REGISTER_SHADER
#undef REGISTER_VERTEX_SHADER

		// Initialize the raster states

		// Wireframe
		RasterizerDescriptor rasterizer_desc = {};
		rasterizer_desc.cull_mode = ECS_CULL_NONE;
		rasterizer_desc.solid_fill = false;
		rasterizer_states[ECS_DEBUG_RASTERIZER_WIREFRAME] = graphics->CreateRasterizerState(rasterizer_desc);

		// Solid - disable the back face culling
		rasterizer_desc.solid_fill = true;
		rasterizer_desc.cull_mode = ECS_CULL_NONE;
		rasterizer_states[ECS_DEBUG_RASTERIZER_SOLID] = graphics->CreateRasterizerState(rasterizer_desc);

		// Solid - with back face culling
		rasterizer_desc.cull_mode = ECS_CULL_BACK;
		rasterizer_states[ECS_DEBUG_RASTERIZER_SOLID_CULL] = graphics->CreateRasterizerState(rasterizer_desc);

		InitializeMemoryResources(this, thread_count);

#pragma region Primitive buffers

		// The string meshes - in the gltf mesh they are stored in reverse order.
		// Change their order
		string_mesh = resource_manager->LoadCoalescedMesh<true>(STRING_MESH_FILE);

		size_t submesh_count = string_mesh->submeshes.size;
		// Change the order of the submeshes such that they mirror alphabet index
		unsigned int* string_submesh_mask = (unsigned int*)ECS_STACK_ALLOC(sizeof(unsigned int) * submesh_count);
		memset(string_submesh_mask, 0, sizeof(unsigned int) * submesh_count);

		// Blender export bug - " gets exported as \" - must account for that
		for (size_t index = 0; index < (unsigned int)AlphabetIndex::Space; index++) {
			for (size_t subindex = 0; subindex < submesh_count; subindex++) {
				// If it is not the invalid character, " or backslash
				if (string_mesh->submeshes[subindex].name.size == 1) {
					unsigned int alphabet_index = GetAlphabetIndex(string_mesh->submeshes[subindex].name[0]);
					if (alphabet_index == index) {
						string_submesh_mask[index] = subindex;
						break;
					}
				}
			}
		}

		// Invalid character, " and \\ handled separately

		// "
		for (size_t index = 0; index < submesh_count; index++) {
			if (string_mesh->submeshes[index].name.size > 1 && string_mesh->submeshes[index].name[1] == '\"') {
				string_submesh_mask[(unsigned int)AlphabetIndex::Quotes] = index;
				break;
			}
		}

		// backslash
		for (size_t index = 0; index < submesh_count; index++) {
			if (string_mesh->submeshes[index].name.size > 1 && string_mesh->submeshes[index].name[1] == '\\') {
				string_submesh_mask[(unsigned int)AlphabetIndex::Backslash] = index;
				break;
			}
		}

		// Invalid character
		for (size_t index = 0; index < submesh_count; index++) {
			if (string_mesh->submeshes[index].name.size > 1 && string_mesh->submeshes[index].name[1] != '\"' && string_mesh->submeshes[index].name[1] != '\\') {
				string_submesh_mask[submesh_count - 1] = index;
				break;
			}
		}

		// Validate that we do not repeat a mesh
		for (size_t index = 0; index < submesh_count; index++) {
			ECS_ASSERT(SearchBytes(string_submesh_mask + index + 1, submesh_count - index - 1, string_submesh_mask[index], sizeof(unsigned int)) == -1);
		}

		// Swap the submeshes according to the mask
		Submesh* backup_submeshes = (Submesh*)ECS_STACK_ALLOC(sizeof(Submesh) * submesh_count);
		memcpy(backup_submeshes, string_mesh->submeshes.buffer, sizeof(Submesh) * submesh_count);
		for (size_t index = 0; index < submesh_count; index++) {
			string_mesh->submeshes[index] = backup_submeshes[string_submesh_mask[index]];
		}

		// Transform the vertex buffer to a staging buffer and read the values
		VertexBuffer staging_buffer = BufferToStaging(graphics, string_mesh->mesh.GetBuffer(ECS_MESH_POSITION));

		// Map the buffer
		float3* values = (float3*)graphics->MapBuffer(staging_buffer.buffer, ECS_GRAPHICS_MAP_READ);

		// Determine the string character spans
		for (size_t index = 0; index < submesh_count; index++) {
			float maximum = -10000.0f;
			float minimum = 10000.0f;
			// Walk through the vertices and record the minimum and maximum for the X axis
			for (size_t vertex_index = 0; vertex_index < string_mesh->submeshes[index].vertex_count; vertex_index++) {
				size_t offset = string_mesh->submeshes[index].vertex_buffer_offset + vertex_index;
				maximum = max(values[offset].x, maximum);
				minimum = min(values[offset].x, minimum);
			}

			// The character span is the difference between maximum and minimum
			string_character_bounds[index] = { minimum, maximum };
		}
		// Copy the unknown character to the last character
		string_character_bounds[(unsigned int)AlphabetIndex::Unknown - 1] = string_character_bounds[submesh_count - 1];

		// Place the space and tab dimensions
		string_character_bounds[(unsigned int)AlphabetIndex::Space] = { -STRING_SPACE_SIZE, STRING_SPACE_SIZE };
		string_character_bounds[(unsigned int)AlphabetIndex::Tab] = { -STRING_TAB_SIZE, STRING_TAB_SIZE };

		graphics->UnmapBuffer(staging_buffer.buffer);

		// Release the staging buffer
		staging_buffer.Release();

		for (size_t index = 0; index < ECS_DEBUG_VERTEX_BUFFER_COUNT; index++) {
			primitive_meshes[index] = ((Stream<Mesh>*)resource_manager->LoadMeshes<true>(ECS_DEBUG_PRIMITIVE_MESH_FILE[index]))->buffer;
		}

		float3 circle_positions[CIRCLE_TESSELATION + 1];

		// The circle
		float step = 2.0f * PI / CIRCLE_TESSELATION;
		size_t index = 0;
		float float_index = 0.0f;
		for (; index < CIRCLE_TESSELATION; index++) {
			float z = cos(float_index);
			float x = sin(float_index);

			circle_positions[index].x = x;
			circle_positions[index].y = 0.0f;
			circle_positions[index].z = z;

			float_index += step;
		}
		// Last point must be the same as the first
		circle_positions[index] = circle_positions[0];

		circle_buffer = graphics->CreateVertexBuffer(sizeof(float3), ECS_COUNTOF(circle_positions), circle_positions);

#pragma endregion

	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::InitializeRedirect(MemoryManager* _allocator, size_t _thread_count) {
		memset(this, 0, sizeof(*this));
		allocator = _allocator;
		thread_count = _thread_count;

		InitializeMemoryResources(this, thread_count);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	bool DebugDrawer::ActivateRedirect() {
		// Don't do anything if the redirect drawer is not set
		if (redirect_drawer == nullptr) {
			return false;
		}

		bool has_redirect = is_redirect;
		// Flush all the entries from the threads
		FlushAll();
		// Record the count of the main storage
		SetRedirectCounts(redirect_counts, RedirectGlobalSizeExtractor{}, lines, spheres, points, rectangles, crosses, circles,
			arrows, triangles, aabbs, oobbs, strings, grids);
		is_redirect = true;
		return has_redirect;
	}

	// ----------------------------------------------------------------------------------------------------------------------

	bool DebugDrawer::ActivateRedirectThread(unsigned int thread_index) {
		// Don't do anything if the redirect drawer is not set
		if (redirect_drawer == nullptr) {
			return false;
		}

		bool has_redirect = is_redirect_thread_count[thread_index];
		is_redirect_thread_count[thread_index] = true;
		// Record the counts for this thread
		SetRedirectCounts(GetThreadRedirectCount(this, thread_index), RedirectThreadSizeExtractor{ thread_index }, thread_lines, thread_spheres, thread_points,
			thread_rectangles, thread_crosses, thread_circles, thread_arrows, thread_triangles, thread_aabbs, 
			thread_oobbs, thread_strings, thread_grids);
		return has_redirect;
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::AddTo(DebugDrawer* other) {
		FlushAll();

		// Add each deck to its correspondent
		size_t initial_string_size = other->strings.GetElementCount();
		size_t initial_grid_size = other->grids.GetElementCount();

		other->lines.AddDeck(lines);
		other->spheres.AddDeck(spheres);
		other->points.AddDeck(points);
		other->rectangles.AddDeck(rectangles);
		other->crosses.AddDeck(crosses);
		other->circles.AddDeck(circles);
		other->arrows.AddDeck(arrows);
		other->triangles.AddDeck(triangles);
		other->aabbs.AddDeck(aabbs);
		other->oobbs.AddDeck(oobbs);
		other->strings.AddDeck(strings);
		other->grids.AddDeck(grids);

		// For the added strings and grids, we need to make sure that the allocations are carried over
		for (size_t index = initial_string_size; index < other->strings.GetElementCount(); index++) {
			other->strings[index].text = other->strings[index].text.Copy(other->Allocator());
		}
		for (size_t index = initial_grid_size; index < other->grids.GetElementCount(); index++) {
			if (other->grids[index].has_valid_cells) {
				other->grids[index].valid_cells = other->grids[index].valid_cells.Copy(other->Allocator());
			}
		}
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::BindShaders(unsigned int index, DebugShaderOutput output)
	{
		graphics->BindVertexShader(vertex_shaders[index][output]);
		graphics->BindPixelShader(pixel_shaders[index][output]);
		graphics->BindInputLayout(layout_shaders[index][output]);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::Clear()
	{
		static_assert(ECS_DEBUG_PRIMITIVE_COUNT == 12, "Update the clear method for DebugDrawer!");

		lines.Deallocate();
		spheres.Deallocate();
		points.Deallocate();
		rectangles.Deallocate();
		crosses.Deallocate();
		circles.Deallocate();
		arrows.Deallocate();
		triangles.Deallocate();
		aabbs.Deallocate();
		oobbs.Deallocate();
		// For strings, we need to deallocate their text
		for (size_t index = 0; index < strings.size; index++) {
			strings[index].text.Deallocate(allocator);
		}
		strings.Deallocate();
		// For the grids, there are some possible deallocation that need to be made
		size_t grid_count = grids.GetElementCount();
		for (size_t index = 0; index < grid_count; index++) {
			if (grids[index].has_valid_cells) {
				grids[index].valid_cells.Deallocate();
			}
		}
		grids.Deallocate();

		for (unsigned int index = 0; index < thread_count; index++) {
			thread_lines[index].Clear();
			thread_spheres[index].Clear();
			thread_points[index].Clear();
			thread_rectangles[index].Clear();
			thread_crosses[index].Clear();
			thread_circles[index].Clear();
			thread_arrows[index].Clear();
			thread_triangles[index].Clear();
			thread_aabbs[index].Clear();
			thread_oobbs[index].Clear();
			for (unsigned int subindex = 0; subindex < thread_strings[index].size; subindex++) {
				thread_strings[index][subindex].text.Deallocate(allocator);
			}
			thread_strings[index].Clear();
			for (unsigned int grid_index = 0; grid_index < thread_grids[index].size; grid_index++) {
				if (thread_grids[index][grid_index].has_valid_cells) {
					thread_grids[index][grid_index].valid_cells.Deallocate();
				}
			}
			thread_grids[index].Clear();
		}
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::CopyCharacterSubmesh(IndexBuffer index_buffer, unsigned int& buffer_offset, unsigned int alphabet_index) {
		CopyBufferSubresource(
			index_buffer,
			buffer_offset,
			string_mesh->mesh.index_buffer,
			string_mesh->submeshes[alphabet_index].index_buffer_offset,
			string_mesh->submeshes[alphabet_index].index_count,
			graphics->GetContext()
		);

		buffer_offset += string_mesh->submeshes[alphabet_index].index_count;
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::DeactivateRedirect(bool previously_set) {
		// Don't do anything if the redirect drawer is not set
		if (redirect_drawer == nullptr) {
			return;
		}

		if (previously_set) {
			return;
		}

		// Perform something only on the call where the boolean is false
		// Flush everything again in the main storage and then use the counts
		// To determine what has been added
		FlushAll();

		AddRedirects(redirect_counts, -1, RedirectGlobalSizeExtractor{}, RedirectGlobalValueExtractor{}, lines, spheres, points, rectangles, crosses,
			circles, arrows, triangles, aabbs, oobbs, strings, grids, redirect_drawer);
		// We don't need to clear these because the activate will set the counts to the appropriate values
		is_redirect = false;
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::DeactivateRedirectThread(unsigned int thread_index, bool previously_set) {
		// Don't do anything if the redirect drawer is not set
		if (redirect_drawer == nullptr) {
			return;
		}

		if (previously_set) {
			return;
		}

		// Perform something only on the call where the boolean is false
		// Here, the flush is not desired
		AddRedirects(GetThreadRedirectCount(this, thread_index), thread_index, RedirectThreadSizeExtractor{ thread_index },
			RedirectThreadValueExtractor{ thread_index }, thread_lines, thread_spheres, thread_points,
			thread_rectangles, thread_crosses, thread_circles, thread_arrows, thread_triangles, thread_aabbs,
			thread_oobbs, thread_strings, thread_grids, redirect_drawer);

		is_redirect_thread_count[thread_index] = false;
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::UpdateCameraMatrix(Matrix new_matrix)
	{
		camera_matrix = new_matrix;
	}

	// ----------------------------------------------------------------------------------------------------------------------

	GraphicsPipelineRenderState DebugDrawer::GetPreviousRenderState() const
	{
		return graphics->GetPipelineRenderState();
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::RestorePreviousRenderState(const GraphicsPipelineRenderState* state)
	{
		graphics->RestorePipelineRenderState(state);
	}

	// ----------------------------------------------------------------------------------------------------------------------

#pragma endregion

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::DrawCallStructured(unsigned int vertex_count, unsigned int instance_count)
	{
		graphics->Draw(vertex_count * instance_count);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugDrawer::DrawCallTransform(unsigned int index_count, unsigned int instance_count)
	{
		graphics->DrawIndexedInstanced(index_count, instance_count);
	}

	// ----------------------------------------------------------------------------------------------------------------------
	
	void DebugDrawerAddToDrawShapes(DebugDrawer* drawer) {
		DebugDrawOptions debug_options;
		debug_options.ignore_depth = true;
		debug_options.wireframe = true;
		drawer->AddAABB({ 0.0f, 0.0f, 0.0f }, float3::Splat(1.0f), AxisXColor(), debug_options);
		drawer->AddOOBB({ 0.0f, 0.0f, 0.0f }, QuaternionFromEuler(float3::Splat(45.0f)), float3::Splat(1.0f), AxisZColor(), debug_options);
		drawer->AddLine({ 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, -5.0f }, AxisYColor());
		drawer->AddRectangle({ 5.0f, 0.0f, -5.0f }, { 0.0f, 5.0f, -5.0f }, Color(100, 120, 160), debug_options);

		debug_options.wireframe = false;
		drawer->AddArrow({ 5.0f, 0.0f, -5.0f }, { 10.0f, 0.0f, -5.0f }, 10.0f, Color(200, 100, 150), debug_options);
		drawer->AddCircle({ -5.0f, 0.0f, -5.0f }, QuaternionFromEuler(float3(0.0f, 0.0f, 0.0f)), 1.0f, Color(100, 150, 200), debug_options);
		drawer->AddCircle({ -5.0f, 0.0f, -5.0f }, QuaternionFromEuler(float3(90.0f, 0.0f, 0.0f)), 1.0f, Color(100, 150, 200), debug_options);
		drawer->AddCircle({ -5.0f, 0.0f, -5.0f }, QuaternionFromEuler(float3(0.0f, 0.0f, 90.0f)), 1.0f, Color(100, 150, 200), debug_options);

		drawer->AddCross({ 10.0f, 10.0f, 0.0f }, QuaternionFromEuler(float3(0.0f, 0.0f, 0.0f)), 1.0f, Color(200, 200, 200), debug_options);
		drawer->AddPoint({ 0.0f, 0.0f, -10.0f }, 1.0f, Color(255, 255, 255), debug_options);
		drawer->AddTriangle({ 2.0f, 0.0f, 0.0f }, { 5.0f, 0.0f, 0.0f }, { 0.0f, 5.0f, 0.0f }, Color(10, 50, 200), debug_options);

		drawer->AddString({ 0.0f, 10.0f, 0.0f }, { 1.0f, 0.0f, 0.0f }, 1.0f, "Hey there fellas!\nNice to see this working!\nPogU", Color(255, 255, 255), debug_options);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	// The functor is called for each line that should be drawn/added
	template<typename Functor>
	static void DrawDebugFrustumImpl(const FrustumPoints& frustum, Functor&& functor) {
		// The near plane
		functor(frustum.near_plane.top_left, frustum.near_plane.top_right);
		functor(frustum.near_plane.top_right, frustum.near_plane.bottom_right);
		functor(frustum.near_plane.bottom_right, frustum.near_plane.bottom_left);
		functor(frustum.near_plane.bottom_left, frustum.near_plane.top_left);
	
		// The far plane
		functor(frustum.far_plane.top_left, frustum.far_plane.top_right);
		functor(frustum.far_plane.top_right, frustum.far_plane.bottom_right);
		functor(frustum.far_plane.bottom_right, frustum.far_plane.bottom_left);
		functor(frustum.far_plane.bottom_left, frustum.far_plane.top_left);

		// The lateral lines
		functor(frustum.near_plane.top_left, frustum.far_plane.top_left);
		functor(frustum.near_plane.top_right, frustum.far_plane.top_right);
		functor(frustum.near_plane.bottom_left, frustum.far_plane.bottom_left);
		functor(frustum.near_plane.bottom_right, frustum.far_plane.bottom_right);
	}

	void DrawDebugFrustum(const FrustumPoints& frustum, DebugDrawer* drawer, Color color, DebugDrawOptions options)
	{
		DrawDebugFrustumImpl(frustum, [&](float3 start, float3 end) {
			drawer->DrawLine(start, end, color, options);
		});
	}

	void AddDebugFrustum(const FrustumPoints& frustum, DebugDrawer* drawer, Color color, DebugDrawOptions options)
	{
		DrawDebugFrustumImpl(frustum, [&](float3 start, float3 end) {
			drawer->AddLine(start, end, color, options);
		});
	}

	void AddDebugFrustumThread(const FrustumPoints& frustum, DebugDrawer* drawer, unsigned int thread_id, Color color, DebugDrawOptions options)
	{
		DrawDebugFrustumImpl(frustum, [&](float3 start, float3 end) {
			drawer->AddLineThread(thread_id, start, end, color, options);
		});
	}

	// ----------------------------------------------------------------------------------------------------------------------

	template<typename Functor>
	static void DrawDebugGridImpl(const DebugGrid* grid, DebugDrawOptions options, Functor&& functor) {
		options.wireframe = true;
		auto iterate = [&](auto has_residency_function) {
			for (unsigned int x = 0; x < grid->dimensions.x; x++) {
				for (unsigned int y = 0; y < grid->dimensions.y; y++) {
					for (unsigned int z = 0; z < grid->dimensions.z; z++) {
						bool is_resident = true;
						if constexpr (has_residency_function) {
							is_resident = grid->residency_function({ x, y, z }, grid->residency_function);
						}
						if (is_resident) {
							float3 translation = grid->translation + float3{ uint3{x, y, z} } *	grid->cell_size;
							functor(translation, options);
						}
					}
				}
			}
		};

		if (grid->residency_function != nullptr) {
			iterate(std::true_type{});
		}
		else {
			iterate(std::false_type{});
		}
	}

	// ----------------------------------------------------------------------------------------------------------------------

	Matrix ECS_VECTORCALL DebugSphere::GetMatrix(Matrix camera_matrix) const
	{
		return SphereMatrix(position, radius, camera_matrix);
	}

	Matrix ECS_VECTORCALL DebugPoint::GetMatrix(Matrix camera_matrix) const
	{
		return PointMatrix(position, camera_matrix);
	}

	Matrix ECS_VECTORCALL DebugCross::GetMatrix(Matrix camera_matrix) const
	{
		return CrossMatrix(position, rotation, size, camera_matrix);
	}

	Matrix ECS_VECTORCALL DebugCircle::GetMatrix(Matrix camera_matrix) const
	{
		return CircleMatrix(position, rotation, radius, camera_matrix);
	}

	Matrix ECS_VECTORCALL DebugArrow::GetMatrix(Matrix camera_matrix) const
	{
		return ArrowMatrix(translation, rotation, length, size, camera_matrix);
	}

	Matrix ECS_VECTORCALL DebugAABB::GetMatrix(Matrix camera_matrix) const
	{
		return AABBMatrix(translation, scale, camera_matrix);
	}

	Matrix ECS_VECTORCALL DebugOOBB::GetMatrix(Matrix camera_matrix) const
	{
		return OOBBMatrix(translation, rotation, scale, camera_matrix);
	}

	// ----------------------------------------------------------------------------------------------------------------------

	void DebugGrid::ExtractResidentCells(AllocatorPolymorphic allocator)
	{
		// Use a reasonably small power of two exponent such that we don't
		// Overcommit too much
		const size_t POWER_OF_TWO_EXPONENT = 7; // 128

		if (residency_function != nullptr) {
			valid_cells.Initialize(allocator, 1, 1 << POWER_OF_TWO_EXPONENT, POWER_OF_TWO_EXPONENT);
			float3 cell_offset = cell_size * float3::Splat(2.0f);

			for (unsigned int x = 0; x < dimensions.x; x++) {
				for (unsigned int y = 0; y < dimensions.y; y++) {
					for (unsigned int z = 0; z < dimensions.z; z++) {
						if (residency_function({ x, y, z }, residency_data)) {
							valid_cells.Add(translation + float3(uint3(x, y, z)) * cell_offset);
						}
					}
				}
			}
			if (valid_cells.GetElementCount() == 0) {
				valid_cells.Deallocate();
			}
			has_valid_cells = true;
		}
	}

	// ----------------------------------------------------------------------------------------------------------------------
}