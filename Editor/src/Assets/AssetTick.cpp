#include "editorpch.h"
#include "AssetTick.h"
#include "../Editor/EditorState.h"
#include "AssetManagement.h"
#include "EditorSandboxAssets.h"

using namespace ECSEngine;

void TickAsset(EditorState* editor_state) {
	// Every half a second check for updates to asset settings
	if (EditorStateLazyEvaluationTrue(editor_state, EDITOR_LAZY_EVALUATION_METADATA_FOR_ASSETS, 500)) {
		CreateAssetDefaultSetting(editor_state);
		DeleteMissingAssetSettings(editor_state);

		// Check to see if the assets have become out of date - only if we can indeed unload or load
		if (!EditorStateHasFlag(editor_state, EDITOR_STATE_PREVENT_RESOURCE_LOADING)) {
			// Determine the out of date assets
			ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(_stack_allocator, ECS_KB * 96, ECS_MB);
			AllocatorPolymorphic allocator = GetAllocatorPolymorphic(&_stack_allocator);

			ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(_temp_stack_allocator, ECS_KB * 32, ECS_MB);
			AllocatorPolymorphic temp_allocator = GetAllocatorPolymorphic(&_temp_stack_allocator);

			Stream<Stream<unsigned int>> out_of_date_assets = GetOutOfDateAssets(editor_state, allocator);

			// This contains all the assets which are out of date - including dependencies
			ResizableStream<unsigned int> overall_out_of_date_assets[ECS_ASSET_TYPE_COUNT];
			for (size_t index = 0; index < ECS_ASSET_TYPE_COUNT; index++) {
				overall_out_of_date_assets[index].allocator = allocator;
				overall_out_of_date_assets[index].capacity = 0;
				overall_out_of_date_assets[index].Copy(out_of_date_assets[index]);
			}

			for (size_t index = 0; index < ECS_ASSET_TYPE_COUNT; index++) {
				ECS_ASSET_TYPE current_type = (ECS_ASSET_TYPE)index;
				for (size_t subindex = 0; subindex < out_of_date_assets[index].size; subindex++) {
					unsigned int handle = out_of_date_assets[current_type][subindex];
					
					const void* metadata = editor_state->asset_database->GetAsset(handle, current_type);
					// Determine dependent assets on this current asset
					Stream<Stream<unsigned int>> current_extra_dependent_assets = GetDependentAssetsFor(editor_state, metadata, current_type, temp_allocator);
					for (size_t current_index = 0; current_index < current_extra_dependent_assets.size; current_index++) {
						for (size_t current_subindex = 0; current_subindex < current_extra_dependent_assets[current_index].size; current_subindex++) {
							unsigned int handle_to_search = current_extra_dependent_assets[current_index][current_subindex];
							size_t subindex = function::SearchBytes(
								overall_out_of_date_assets[current_index].buffer,
								overall_out_of_date_assets[current_index].size,
								handle_to_search, 
								sizeof(unsigned int)
							);
							
							if (subindex == -1) {
								// Add it
								overall_out_of_date_assets[current_index].Add(handle_to_search);
							}
						}
					}

					_temp_stack_allocator.Clear();
				}
			}

			size_t total_count = 0;
			Stream<unsigned int> assets_to_reload[ECS_ASSET_TYPE_COUNT];
			for (size_t index = 0; index < ECS_ASSET_TYPE_COUNT; index++) {
				assets_to_reload[index] = overall_out_of_date_assets[index].ToStream();
				total_count += assets_to_reload[index].size;
			}

			if (total_count > 0) {
				// Reload these assets
				ReloadAssets(editor_state, { assets_to_reload, ECS_ASSET_TYPE_COUNT });
			}

			_stack_allocator.ClearBackup();
			_temp_stack_allocator.ClearBackup();
		}
	}
}