#pragma once
#include "ECSEngineContainers.h"

#define EDITOR_INPUT_RECORDING_FILE_EXTENSION L".input"
#define EDITOR_STATE_RECORDING_FILE_EXTENSION L".replay"
#define EDITOR_INPUT_RECORDER_TYPE_STRING "input"
#define EDITOR_STATE_RECORDER_TYPE_STRING "state"

enum EDITOR_SANDBOX_RECORDING_TYPE : unsigned char;

EDITOR_SANDBOX_RECORDING_TYPE GetRecordingTypeFromExtension(ECSEngine::Stream<wchar_t> extension);