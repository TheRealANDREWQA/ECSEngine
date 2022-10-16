#include "ecspch.h"
#include "ApplicationUtilities.h"
#include "Allocators\MemoryManager.h"
#include "Rendering\Graphics.h"

namespace ECSEngine {

	void CreateGraphicsForProcess(Graphics* graphics, HWND hWnd, GlobalMemoryManager* global_manager, bool discrete_gpu)
	{
		GraphicsDescriptor graphics_descriptor;
		RECT window_rectangle;
		GetClientRect(hWnd, &window_rectangle);
		
		float monitor_scaling = 100.0f;
	#ifdef EDITOR_GET_MONITOR_SCALE
		HMONITOR monitor = MonitorFromPoint({ 0, 0 }, MONITOR_DEFAULTTOPRIMARY);
		//HMONITOR monitor = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONULL);
		UINT dpiX, dpiY;
		HRESULT dpi_result = GetDpiForMonitor(monitor, MDT_EFFECTIVE_DPI, &dpiX, &dpiY);
		ECS_ASSERT(!FAILED(dpi_result));
		monitor_scaling = dpiY / 96.0f * 100.0f;
		
	#else
		monitor_scaling = 150.0f;
	#endif
		
		LONG width = window_rectangle.right - window_rectangle.left;
		LONG height = window_rectangle.bottom - window_rectangle.top;
		
		/*unsigned int new_width = static_cast<unsigned int>(width * monitor_scaling / 100.0f);
		unsigned int new_height = static_cast<unsigned int>(height * monitor_scaling / 100.0f);*/
		
		unsigned int new_width = width;
		unsigned int new_height = height;

		MemoryManager* graphics_allocator = new MemoryManager(500'000, 2048, 100'000, global_manager);
		
		graphics_descriptor.window_size = { new_width, new_height };
		graphics_descriptor.gamma_corrected = false;
		graphics_descriptor.allocator = graphics_allocator;
		graphics_descriptor.hWnd = hWnd;
		graphics_descriptor.discrete_gpu = discrete_gpu;
		graphics_descriptor.create_swap_chain = true;
		new (graphics) Graphics(&graphics_descriptor);
	}

}