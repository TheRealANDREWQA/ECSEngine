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

			Stream<Stream<unsigned int>> metadata_out_of_date_assets = GetOutOfDateAssetsMetadata(editor_state, allocator, true, false);
			ReloadAssetsMetadataChange(editor_state, metadata_out_of_date_assets);
			_stack_allocator.Clear();

			Stream<Stream<unsigned int>> target_file_out_of_date_assets = GetOutOfDateAssetsTargetFile(editor_state, allocator);
			ReloadAssets(editor_state, target_file_out_of_date_assets);

			_stack_allocator.ClearBackup();
		}
	}
}