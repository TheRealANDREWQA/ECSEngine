#include "editorpch.h"
#include "SandboxRecordingFileExtension.h"
#include "SandboxTypes.h"

EDITOR_SANDBOX_RECORDING_TYPE GetRecordingTypeFromExtension(ECSEngine::Stream<wchar_t> extension) {
	if (extension == EDITOR_INPUT_RECORDING_FILE_EXTENSION) {
		return EDITOR_SANDBOX_RECORDING_INPUT;
	}
	else if (extension == EDITOR_STATE_RECORDING_FILE_EXTENSION) {
		return EDITOR_SANDBOX_RECORDING_STATE;
	}
	return EDITOR_SANDBOX_RECORDING_TYPE_COUNT;
}