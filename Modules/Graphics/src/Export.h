#pragma once

#ifdef Graphics_BUILD_DLL
#define	Graphics_API __declspec(dllexport)
#else
#define Graphics_API __declspec(dllimport)
#endif 