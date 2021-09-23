#pragma once

#include <memory>
#include <stdint.h>
#include <math.h>
#include <immintrin.h>
#include <iostream>
#include <execution>
#include <string>
#include <chrono>
#include <random>
#include <sstream>
#include <comdef.h>
#include <atomic>
#include <fstream>
#include <condition_variable>
#include <assert.h>
#include <filesystem>
#include <type_traits>

#include "../../Dependencies/VCL-version2/vectorclass.h"
#include "../../Dependencies/VCL-version2/vectormath_trig.h"
#include "../../Dependencies/VCL-version2/vectormath_exp.h"
#include "../../Dependencies/VCL-version2/vectormath_hyp.h"

#ifdef ECSENGINE_PLATFORM_WINDOWS

#include <wrl.h>
#include <DirectXMath.h>
#include "../../Dependencies/DirectXTK/Inc/WICTextureLoader.h"

// target Windows 7 or later
#define _WIN32_WINNT 0x0601

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#define NOSYSMETRICS
#define NOMENUS
#define NOGDICAPSMASKS
#define NOICONS
#define NOSYSCOMMANDS
#define NORASTEROPS
#define OEMRESOURCE
#define NOATOM
#define NOCLIPBOARD
#define NODRAWTEXT
#define NOCOLOR
#define NOCTLMGR
#define NOKERNEL
#define NONLS
#define NOMEMMGR
#define NOMETAFILE
#define NOMINMAX
#define NOOPENFILE
#define NOSCROLL
#define NOSERVICE
#define NOSOUND
#define NOTEXTMETRIC
#define NOWH
#define NOCOMM
#define NOKANJI
#define NOHELP
#define NOPROFILER
#define NODEFERWINDOWPOS
#define NOMCX
#define NORPC
#define NOPROXYSTUB
#define NOIMAGE
#define NOTAPE

#define STRICT

#include <sdkddkver.h>
#include <Windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgidebug.h>
#include <shellapi.h>
#include <ShlObj_core.h>
#include <ShellScalingApi.h>

#endif // ifdef ECSENGINE_PLATFORM_WINDOWS