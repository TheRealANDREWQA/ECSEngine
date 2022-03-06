#include "editorpch.h"
//#include "ECSEngine.h"
//#include "EntyPoint.h"
//#include "DrawFunction2.h"
//#include "DrawFunction.h"
//#include "../Resources/resource.h"
//#include "Test.h"
////#include "..\Dependencies\imgui-docking\imgui.h"
//
//#define ERROR_BOX_MESSAGE WM_USER + 1
//#define ERROR_BOX_CODE -2
//#define ECS_COMPONENT
//
//#define ECS_TOOLS_UI_DESCRIPTOR_FILE_PATH "UIDescriptors.txt"
//#define ECS_TOOLS_UI_FILE_PATH "UIFile.txt"
//
//constexpr size_t SLEEP_AMOUNT = 8;
//
//using namespace ECSEngine;
//using namespace ECSEngine::Tools;
//
//struct PrintStruct {
//	std::atomic<int>* val;
//	ECSEngine::SpinLock* lock;
//};
//struct WaitStruct {
//	ECSEngine::TaskManager* task_manager_ptr;
//};
//
//void DoSomething(unsigned int thread_id, void* data) {
//	std::atomic<int>* reinterpretation = (std::atomic<int>*)data;
//	reinterpretation->fetch_add(1, ECS_RELAXED);
//}
//void WaitTask(unsigned int thread_id, void* data) {
//	WaitStruct* reinterpretation = (WaitStruct*)data;
//	reinterpretation->task_manager_ptr->SleepThread(thread_id);
//	reinterpretation->task_manager_ptr->SetThreadTaskIndex(thread_id, 0);
//}
//void SleepThread(unsigned int thread_id, void* data) {
//	ECSEngine::TaskManager* task_manager = (ECSEngine::TaskManager*)data;
//	task_manager->SleepThread(thread_id);
//	task_manager->SetThreadTaskIndex(thread_id, task_manager->GetThreadTaskIndex(thread_id) - 1);
//}
//void PrintTask(unsigned int thread_id, void* data) {
//	PrintStruct* reinterpretation = (PrintStruct*)data;
//	reinterpretation->lock->lock();
//	// print function
//	//ECSENGINE_CORE_INFO("Integer is {#}. Thread: {#}", reinterpretation->val->load(ECS_ACQUIRE), thread_id);
//	std::cout << "Integer is " << reinterpretation->val->load(ECS_ACQUIRE) << ". Thread: " << thread_id << "\n";
//	reinterpretation->lock->unlock();
//	//std::this_thread::sleep_for(std::chrono::seconds(1));
//}
//
//struct dialog_data {
//	HWND hWnd;
//	HINSTANCE hInstance;
//};
//
//void OpenFilename(ActionData* action_data) {
//	UI_UNPACK_ACTION_DATA;
//
//	dialog_data* data = (dialog_data*)_data;
//	OPENFILENAME openfilename;
//	memset(&openfilename, 0, sizeof(OPENFILENAME));
//	wchar_t wide_chars[512];
//	openfilename.nMaxFile = 512;
//	openfilename.lpstrFile = wide_chars;
//	wide_chars[0] = L'\0';
//	openfilename.Flags = OFN_EXPLORER | OFN_EXTENSIONDIFFERENT;
//	openfilename.lStructSize = sizeof(OPENFILENAME);
//	BOOL succes = GetOpenFileName(&openfilename);
//	DWORD error_type = CommDlgExtendedError();
//};
//
//template<bool initialize>
//void OpenFile(void* window_data, void* drawer_descriptor) {
//	UI_PREPARE_DRAWER(initialize);
//
//	drawer.Button("open dialog", { OpenFilename, window_data, 0 });
//};
//
//class Editor : public ECSEngine::Application {
//public:
//	Editor(int width, int height, LPCWSTR name) noexcept;
//	Editor() : timer("Editor"), width(0), height(0) {};
//	~Editor();
//	Editor(const Editor&) = delete;
//	Editor& operator = (const Editor&) = delete;
//
//private:
//	static LRESULT CALLBACK HandleMessageSetup(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) noexcept;
//	static LRESULT CALLBACK HandleMessageForward(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) noexcept;
//	LRESULT HandleMessage(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) noexcept;
//
//	int width;
//	int height;
//	HWND hWnd;
//	ECSEngine::Graphics* graphics = nullptr;
//	ECSEngine::Timer timer;
//	ECSEngine::HID::Mouse mouse;
//	ECSEngine::HID::Keyboard<ECSEngine::GlobalMemoryManager> keyboard;
//	ECSEngine::Stream<HCURSOR> cursors;
//	ECSEngine::CursorType current_cursor;
//public:
//
//	void ChangeCursor(ECSEngine::CursorType type) override {
//		if (type != current_cursor) {
//			SetCursor(cursors[(unsigned int)type]);
//			SetClassLong(hWnd, -12, (LONG)cursors[(unsigned int)type]);
//			current_cursor = type;
//		}
//	}
//
//	ECSEngine::CursorType GetCurrentCursor() const override {
//		return current_cursor;
//	}
//
//	void WriteTextToClipboard(const char* text) override {
//		assert(OpenClipboard(hWnd) == TRUE);
//		assert(EmptyClipboard() == TRUE);
//
//		size_t text_size = strlen((const char*)text);
//		auto handle = GlobalAlloc(GMEM_MOVEABLE, text_size + 1);
//		auto new_handle = GlobalLock(handle);
//		memcpy(new_handle, text, sizeof(unsigned char) * text_size);
//		char* reinterpretation = (char*)new_handle;
//		reinterpretation[text_size] = '\0';
//		GlobalUnlock(handle);
//		SetClipboardData(CF_TEXT, handle);
//		assert(CloseClipboard() == TRUE);
//	}
//
//	unsigned int CopyTextFromClipboard(char* text, unsigned int max_size) override {
//		if (IsClipboardFormatAvailable(CF_TEXT)) {
//			assert(OpenClipboard(hWnd) == TRUE);
//			HANDLE global_handle = GetClipboardData(CF_TEXT);
//			HANDLE data_handle = GlobalLock(global_handle);
//			unsigned int copy_count = strlen((const char*)data_handle);
//			copy_count = ECSEngine::function::Select(copy_count > max_size - 1, max_size - 1, copy_count);
//			memcpy(text, data_handle, copy_count);
//			GlobalUnlock(global_handle);
//			assert(CloseClipboard() == TRUE);
//			return copy_count;
//		}
//		else {
//			text = (char*)"";
//			return 0;
//		}
//	}
//
//	int Run() override {
//		using namespace ECSEngine;
//		using namespace ECSEngine::containers;
//		using namespace ECSEngine::Tools;
//#ifdef ECSENGINE_CONSOLE
//		unsigned char* buffer = new unsigned char[2048 * 128 * 1280 * 6];
//		ECSEngine::LinearAllocator linearAllocator(buffer, 2048 * 128 * 1280 * 6);
//		GlobalMemoryManager global_memory_manager(10'000'000, 8, 10'000'000);
//		MemoryManager memory_manager(1'800'000, 65536 * 2, 9'000'000, &global_memory_manager);
//#if 1
//		TaskManager manager(nullptr, 8, &memory_manager, 8, 16);
//		ThreadTask do_task;
//		do_task.function = &DoSomething;
//		std::atomic<int> atomic_integer(0);
//		do_task.data = (void*)&atomic_integer;
//
//		ThreadTask wait_condition_variable;
//		WaitStruct wait_struct;
//		ConditionVariable cv(false);
//		wait_struct.task_manager_ptr = &manager;
//		wait_condition_variable.function = &WaitTask;
//		wait_condition_variable.data = (void*)&wait_struct;
//
//		PrintStruct print_struct;
//		SpinLock LOCK;
//		print_struct.val = &atomic_integer;
//		print_struct.lock = &LOCK;
//
//		ThreadTask print_task;
//		print_task.function = &PrintTask;
//		print_task.data = (void*)&print_struct;
//
//		//bool succes = manager.GetThreadQueue(0)->Pop(dummy);
//		manager.SetTask(do_task);
//		manager.SetTask(wait_condition_variable);
//		//manager.SetTask(print_task);
//		//manager.SetTask(print_task);
//		SpinLock lock;
//		SyncronizationEvent event(false);
//
//		/*for (unsigned int index = 0; index < 8; index++ ) {
//			unsigned int copy = index;
//			std::thread([&]() {TaskManager::ThreadProcedure(&manager, (LinearAllocator*)&lock, nullptr, index, &event); }).detach();
//			event.Wait();
//		}
//		std::cout << "HOURA" << "\n";*/
//		LinearAllocator* xd[16] = { nullptr };
//		MemoryManager* xd2[16] = { nullptr };
//		manager.CreateThreads(xd, xd2);
//
//		while (true) {
//			std::this_thread::sleep_for(std::chrono::seconds(1));
//			manager.AddDynamicTask(print_task, (unsigned int)0, true);
//			manager.AddDynamicTask(print_task, (unsigned int)0);
//			manager.AddDynamicTask(print_task, (unsigned int)0);
//			manager.AddDynamicTask(print_task, (unsigned int)0);
//			manager.AddDynamicTask(print_task, (unsigned int)0);
//			manager.AddDynamicTask(print_task, (unsigned int)0);
//			manager.AddDynamicTask(print_task, (unsigned int)0);
//			manager.AddDynamicTask(do_task, (unsigned int)1, true);
//			manager.AddDynamicTask(do_task, (unsigned int)2, true);
//			manager.AddDynamicTask(do_task, (unsigned int)3, true);
//			manager.AddDynamicTask(do_task, (unsigned int)4, true);
//			manager.AddDynamicTask(do_task, (unsigned int)5, true);
//			manager.AddDynamicTask(do_task, (unsigned int)6, true);
//			manager.AddDynamicTask(do_task, (unsigned int)7, true);
//			//manager.WakeThreads();
//		}
//#endif
//		/*EntityPool entity_pool(10, 8, 128, 1024, &memory_manager);
//		EntityManager entity_manager(&memory_manager, &entity_pool);
//		ArchetypeBaseDescriptor archetype_descriptor;
//		archetype_descriptor.chunk_size = 8;
//		archetype_descriptor.initial_chunk_count = 1;
//		archetype_descriptor.max_chunk_count = 8;
//		archetype_descriptor.max_sequence_count = 128;
//
//		DirectX::Mouse mouse;
//		mouse.SetWindow(hWnd);
//
//		struct Translation {
//			float x, y, z;
//		};
//		struct Rotation {
//			float m00, m01, m02;
//			float m10, m11, m12;
//			float m20, m21, m22;
//		};
//		struct LocalToWorld {
//			float x;
//		};
//		struct Index {
//			int index;
//		};
//		struct RenderMesh {
//			unsigned int value;
//		};
//		struct CollisionMesh {
//			float value;
//		};
//
//
//		entity_manager.RegisterComponent(sizeof(Translation), alignof(Translation));
//		entity_manager.RegisterComponent(sizeof(Rotation), alignof(Rotation));
//		entity_manager.RegisterComponent(sizeof(LocalToWorld), alignof(LocalToWorld));
//		entity_manager.RegisterComponent(sizeof(Index), alignof(Index));
//		entity_manager.RegisterSharedComponent(sizeof(RenderMesh), alignof(RenderMesh), 1);
//		entity_manager.RegisterSharedComponent(sizeof(CollisionMesh), alignof(CollisionMesh), 1);
//		RenderMesh render;
//		render.value = 5;
//		CollisionMesh collision;
//		collision.value = 10;
//		entity_manager.RegisterSharedComponentData(0, &render);
//		entity_manager.RegisterSharedComponentData(1, &collision);
//		const unsigned char components[] = { 1, 0 };
//		const unsigned char components2[] = { 2, 0, 1 };
//		const unsigned char components3[] = { 4, 0, 1, 2, 3 };
//		const unsigned char shared_components[] = { 1, 0 };
//		const unsigned char shared_components2[] = { 2, 0, 1 };
//		const unsigned short shared_subindices[] = { 1, 0 };
//		const unsigned short shared_subindices2[] = { 2, 0, 1 };
//		unsigned int archetype_index = entity_manager.CreateArchetype(archetype_descriptor, components, shared_components, 8);
//		unsigned int archetype_subindex = entity_manager.CreateArchetypeBase(
//			archetype_descriptor,
//			archetype_index,
//			shared_components,
//			shared_subindices
//		);
//
//		unsigned int new_entity = entity_manager.CreateEntities(10, archetype_index, archetype_subindex);
//
//		archetype_index = entity_manager.CreateArchetype(archetype_descriptor, components2, shared_components2, 8);
//		archetype_subindex = entity_manager.CreateArchetypeBase(
//			archetype_descriptor,
//			archetype_index,
//			shared_components2,
//			shared_subindices2
//		);
//
//		void* data_buffer[1];
//
//		for (size_t index = 0; index < 10; index++) {
//			EntityInfo info = entity_manager.GetEntityInfo(new_entity + index);
//			std::cout << (int)info.archetype << " " << (int)info.archetype_subindex << " " << (int)info.chunk << " " << (int)info.sequence << "\n";
//		}
//		std::cout << "\n";
//		Stream<void*> data(data_buffer, 0);
//		for (size_t index = 0; index < 10; index++) {
//			entity_manager.GetComponent(new_entity + index, components, data);
//			for (size_t subindex = 0; subindex < data.size; subindex++) {
//				Translation* translation = (Translation*)data[subindex];
//				translation->x = new_entity + index;
//				translation->y = new_entity + index;
//				translation->z = new_entity + index;
//				std::cout << new_entity + index << " " << translation->x << " " << translation->y << " " << translation->z << "\n";
//			}
//			entity_manager.GetComponent(new_entity + index, components, data);
//			for (size_t subindex = 0; subindex < data.size; subindex++) {
//				Translation* translation = (Translation*)data[subindex];
//				std::cout << new_entity + index << " " << translation->x << " " << translation->y << " " << translation->z << "\n";
//			}
//		}
//		std::cout << "\n";
//
//		new_entity = entity_manager.CreateEntities(10, 0, 0);
//
//		for (size_t index = 0; index < 10; index++) {
//			EntityInfo info = entity_manager.GetEntityInfo(new_entity + index);
//			std::cout << (int)info.archetype << " " << (int)info.archetype_subindex << " " << (int)info.chunk << " " << (int)info.sequence << "\n";
//		}
//		std::cout << "\n";
//
//		archetype_index = entity_manager.CreateArchetype(archetype_descriptor, components3, shared_components2, 8);
//		archetype_subindex = entity_manager.CreateArchetypeBase(
//			archetype_descriptor,
//			archetype_index,
//			shared_components2,
//			shared_subindices2
//		);
//
//		new_entity = entity_manager.CreateEntities(10, 0, 0);
//
//		for (size_t index = 0; index < 10; index++) {
//			EntityInfo info = entity_manager.GetEntityInfo(new_entity + index);
//			std::cout << (int)info.archetype << " " << (int)info.archetype_subindex << " " << (int)info.chunk << " " << (int)info.sequence << "\n";
//		}
//		std::cout << "\n";
//
//		ArchetypeQuery query;
//		query.unique = VectorComponentSignature(components2);
//		query.shared = VectorComponentSignature(shared_components2);
//		std::cout << "\n" << "\n";
//		ArchetypeBase* archetypes[10];
//		Stream<ArchetypeBase*> archetype_stream(archetypes, 0);
//		unsigned int entities[100];
//		Stream<unsigned> entity_stream(entities, 0);
//		entity_manager.GetArchetypes(&query, archetype_stream);
//		std::cout << "----------------------------------------------------------\n";
//		for (size_t index = 0; index < archetype_stream.size; index++) {
//			archetype_stream[index]->GetEntities(entity_stream);
//			for (size_t subindex = 0; subindex < entity_stream.size; subindex++) {
//				std::cout << entity_stream[subindex] << "\n";
//			}
//		}*/
//#if 0 
//		MSG message;
//		BOOL result = 0;
//		while (true) {
//			while (PeekMessage(&message, nullptr, 0, 0, PM_REMOVE) != 0) {
//				auto mouse_state = mouse.GetState();
//				switch (message.message) {
//				case WM_QUIT:
//					result = -1;
//					break;
//				}
//				TranslateMessage(&message);
//				DispatchMessage(&message);
//			}
//		}
//#endif
//
//		return 0;
//#endif	
//#ifdef ECSENGINE_RENDER
//#if 1
//		GlobalMemoryManager global_memory_manager(150'000'000, 256, 50'000'000);
//		MemoryManager memory_manager(100'800'000, 1024, 10'000'000, &global_memory_manager);
//		ResourceManager resource_manager(&memory_manager, graphics, std::thread::hardware_concurrency(), "Resources\\");
//		GraphicsSystem gs(graphics, &memory_manager, &resource_manager, graphics->GetProjection());
//		TaskManager task_manager(8, &memory_manager, nullptr, 32);
//
//		resource_manager.InitializeDefaultTypes();
//#ifdef ECS_TOOLS_UI_MULTI_THREADED
//		ThreadTask sleep_task;
//		sleep_task.function = SleepThread;
//		sleep_task.data = &task_manager;
//		task_manager.SetTask(sleep_task);
//		LinearAllocator* temp_allocators[16] = { nullptr };
//		MemoryManager* memory[16] = { nullptr };
//		task_manager.CreateThreads(temp_allocators, memory);
//#endif
//#if 0
//		EntityPool entity_pool(10, 8, 128, 1024, &memory_manager);
//		EntityManager entity_manager(&memory_manager, &entity_pool);
//		ArchetypeBaseDescriptor archetype_descriptor;
//		archetype_descriptor.chunk_size = 256;
//		archetype_descriptor.initial_chunk_count = 1;
//		archetype_descriptor.max_chunk_count = 8;
//		archetype_descriptor.max_sequence_count = 128;
//
//		ConditionVariable variable(false);
//		//std::this_thread::sleep_for(std::chrono::seconds(2));
//
//		const unsigned char box_shared_components[] = {
//			9,
//			VERTEX_BUFFER,
//			INDEX_BUFFER,
//			INPUT_LAYOUT,
//			VERTEX_SHADER,
//			PIXEL_SHADER,
//			PRIMITIVE_TOPOLOGY,
//			VERTEX_CONSTANT_BUFFER,
//			PIXEL_CONSTANT_BUFFER,
//			PS_RESOURCE_VIEW
//		};
//		entity_manager.RegisterSharedComponent(sizeof(VertexBuffer), alignof(VertexBuffer), 5);
//		entity_manager.RegisterSharedComponent(sizeof(IndexBuffer), alignof(IndexBuffer), 5);
//		entity_manager.RegisterSharedComponent(sizeof(InputLayout), alignof(InputLayout), 5);
//		entity_manager.RegisterSharedComponent(sizeof(VertexShader), alignof(VertexShader), 5);
//		entity_manager.RegisterSharedComponent(sizeof(PixelShader), alignof(PixelShader), 5);
//		entity_manager.RegisterSharedComponent(sizeof(Topology), alignof(Topology), 5);
//		entity_manager.RegisterSharedComponent(sizeof(VertexConstantBuffer), alignof(VertexConstantBuffer), 5);
//		entity_manager.RegisterSharedComponent(sizeof(PixelConstantBuffer), alignof(PixelConstantBuffer), 5);
//		entity_manager.RegisterSharedComponent(sizeof(PSResourceView), alignof(PSResourceView), 5);
//		entity_manager.RegisterComponent(sizeof(ECSEngine::Translation), alignof(ECSEngine::Translation));
//		void* box_shared_data[10];
//		Stream<void*> box_data_stream(box_shared_data, 0);
//		entity_manager.GetSharedComponentDataStream(box_data_stream, box_shared_components);
//		gs.ConstructTexturedBox(box_data_stream, L"Kappa.jpg");
//		for (size_t index = 0; index < 9; index++) {
//			entity_manager.m_shared_component_data[index].size++;
//		}
//
//		const unsigned char box_unique_components[] = { 1, TRANSLATION };
//		const unsigned short box_shared_subindices[] = { 0, 0, 0, 0, 0, 0, 0, 0 };
//
//		unsigned int archetype_index = entity_manager.CreateArchetype(
//			archetype_descriptor,
//			box_unique_components,
//			box_shared_components,
//			4
//		);
//		unsigned int base_archetype_index = entity_manager.CreateArchetypeBase(
//			archetype_descriptor,
//			archetype_index,
//			box_shared_components,
//			box_shared_subindices
//		);
//		unsigned int first = entity_manager.CreateEntities(10, archetype_index, base_archetype_index);
//		void* void_buffer[10];
//		Stream<void*> box_translation(void_buffer, 0);
//		for (size_t index = first; index < first + 10; index++) {
//			entity_manager.GetComponent(index, box_unique_components, box_translation);
//			*(ECSEngine::Translation*)box_translation[0] = ECSEngine::Translation(index * 0.5f, index * 0.5f, 2 + index * 1.25f);
//		}
//
//
//		ArchetypeBase* archetype_buffer[10];
//		Stream<ArchetypeBase*> base_archetype_stream(archetype_buffer, 0);
//		ArchetypeQuery graphics_query(box_unique_components, box_shared_components);
//		entity_manager.GetArchetypes(&graphics_query, base_archetype_stream);
//
//		Stream<Sequence> chunk_sequences[10];
//		Stream<Stream<Sequence>> sequence_stream(chunk_sequences, 0);
//		unsigned int* entity_buffers[10];
//		void* second_void[5];
//		Stream<void*> entity_data(second_void, 0);
//#endif
//
//		unsigned int arena_block_count = 256;
//		void* arena_allocation = memory_manager.Allocate(MemoryArena::MemoryOf(8, arena_block_count), 8);
//		void* arena_buffer_allocation = memory_manager.Allocate(5'500'000, 8);
//		void* debug_allocation = memory_manager.Allocate(BlockRange::MemoryOfDebug(arena_block_count) * 8);
//		memset(arena_allocation, 0, MemoryArena::MemoryOf(8, arena_block_count));
//		memset(arena_buffer_allocation, 0, 5'500'000);
//		MemoryArena arena = MemoryArena(arena_allocation, arena_buffer_allocation, 5'500'000, 8, arena_block_count);
//		arena.SetDebugBuffer((unsigned int*)debug_allocation, arena_block_count);
//		ResizableMemoryArena resizable_arena = ResizableMemoryArena(&global_memory_manager, 15'500'000, 8, 512, 5'000'000, 4, 256);
//
//		mouse.AttachToProcess({ hWnd });
//		mouse.SetAbsoluteMode();
//		keyboard = HID::Keyboard<GlobalMemoryManager>(
//			&global_memory_manager
//			);
//
//		UISystem ui(
//			this,
//#ifdef ECS_TOOLS_UI_MEMORY_ARENA
//			& arena,
//#else
//#ifdef ECS_TOOLS_UI_RESIZABLE_MEMORY_ARENA
//			& resizable_arena,
//#else
//			& memory_manager,
//#endif
//#endif
//			& keyboard,
//			&mouse,
//			graphics,
//			&resource_manager,
//			&task_manager,
//			L"Fontv5.tiff",
//			"FontDescription.txt"
//		);
//
//		ui.BindWindowHandler(WindowHandler, WindowHandlerInitializer, sizeof(ECSEngine::Tools::UIDefaultWindowHandler));
//
//		Color color(0, 255, 128);
//		Color new_color = RGBToHSV(color);
//		new_color = HSVToRGB(new_color);
//
//		unsigned char* WINDOW_CHARACTERS = (unsigned char*)resizable_arena.Allocate(2048);
//		CapacityStream<char>* characters = (CapacityStream<char>*)WINDOW_CHARACTERS;
//		characters->buffer = (char*)WINDOW_CHARACTERS + 16;
//		characters->size = 0;
//		characters->capacity = 32;
//		WINDOW_CHARACTERS[60] = 'z';
//		WINDOW_CHARACTERS[61] = 'z';
//		WINDOW_CHARACTERS[62] = 'z';
//		double* dou = (double*)(WINDOW_CHARACTERS + 76);
//		*dou = 1.0;
//		Color* coloru = (Color*)(WINDOW_CHARACTERS + 160);
//		*coloru = Color(50, 120, 0);
//
//		//TestMatricesMultiply(memory_manager, timer);
//
//		/*DirectX::XMMATRIX matrix = DirectX::XMMATRIX(2.0f, 5.0f, 32.0f, 2.0f, 5.0f, 23.0f, 53.0f, 239.0f, 1.0f, 1.0f, 9.0f, 58.0f, 89.0f, 23.0f, 1.0f, 23.0f);
//		DirectX::XMVECTOR vec;
//		DirectX::XMMATRIX inverted = DirectX::XMMatrixInverse(&vec, matrix);
//		ECSEngine::Matrix ecs_matrix = ECSEngine::Matrix(2.0f, 5.0f, 32.0f, 2.0f, 5.0f, 23.0f, 53.0f, 239.0f, 1.0f, 1.0f, 9.0f, 58.0f, 89.0f, 23.0f, 1.0f, 23.0f);
//		float determinant = ECSEngine::MatrixDeterminantInlined(ecs_matrix);
//		ECSEngine::Matrix ecs_inverted = ECSEngine::MatrixInverseInlined(ecs_matrix);*/
//
//		//ValidateTranspose(memory_manager, timer);
//		//ValidateDeterminant(memory_manager, timer);
//		//ValidateInverse(memory_manager, timer);
//		//TestMatrixDeterminant(memory_manager, timer);
//		//TestMatrixInverse(memory_manager, timer);
//
//		//constexpr size_t iterations = 100;
//		//float accumulator1 = 0.0f;
//		//float accumulator2 = 0.0f;
//		//for (size_t iteration = 0; iteration < iterations; iteration++) {
//		//	constexpr size_t value_count = 500'000;
//		//	void* allocation_1 = memory_manager.Allocate(sizeof(float) * value_count);
//		//	void* allocation_2 = memory_manager.Allocate(sizeof(float) * value_count);
//		//	//void* allocation_3 = memory_manager.Allocate(sizeof(float) * value_count);
//		//	Stream<float> values = Stream<float>(allocation_1, 0);
//		//	Stream<float> random = Stream<float>(allocation_2, 0);
//		//	//Stream<float> random2 = Stream<float>(allocation_3, 0);
//		//	std::vector<float> vector;
//		//	std::vector<float> random_vector;
//		//	vector.reserve(value_count);
//		//	random_vector.reserve(value_count);
//
//		//	for (size_t index = 0; index < value_count; index++) {
//		//		random.Add(rand() * rand());
//		//		random_vector.emplace_back(random[index]);
//		//	}
//
//		//	timer.SetMarker();
//		//	for (size_t index = 0; index < value_count; index++) {
//		//		float value = random_vector[index];
//		//		float power = (value * value) / 0.1f;
//		//		accumulator1 += power;
//		//	}
//		//	size_t duration1 = timer.GetDurationSinceMarker_us();
//
//		//	timer.SetMarker();
//		//	for (size_t index = 0; index < value_count; index++) {
//		//		float value = values[index];
//		//		float power = (value * value) / 0.1f;
//		//		accumulator2 += power;
//		//	}
//		//	size_t duration2 = timer.GetDurationSinceMarker_us();
//		//	float sum = values[523] + random_vector[1000];
//		//	char temps[128];
//		//	Stream<char> chars = Stream<char>(temps, 0);
//		//	function::ConvertFloatToChars(chars, sum);
//		//	chars[chars.size] = ' ';
//		//	chars[chars.size + 1] = '\0';
//		//	wchar_t* wide_temps = function::ConvertASCIIToWide(temps);
//		//	OutputDebugString(wide_temps);
//
//		//	chars.size = 0;
//		//	function::ConvertIntToChars(chars, static_cast<int64_t>(duration1));
//		//	chars[chars.size] = ' ';
//		//	chars[chars.size + 1] = '\0';
//		//	wide_temps = function::ConvertASCIIToWide(temps);
//		//	OutputDebugString(wide_temps);
//
//		//	chars.size = 0;
//		//	function::ConvertIntToChars(chars, static_cast<int64_t>(duration2));
//		//	chars[chars.size] = '\n';
//		//	chars[chars.size + 1] = '\0';
//		//	wide_temps = function::ConvertASCIIToWide(temps);
//		//	OutputDebugString(wide_temps);
//
//		//	memory_manager.Deallocate(allocation_1);
//		//	memory_manager.Deallocate(allocation_2);
//		//}
//
//		dialog_data dd;
//		dd.hInstance = Editor::EditorClass::GetInstance();
//		dd.hWnd = hWnd;
//
//		const char* paths[] = { ECS_TOOLS_UI_DESCRIPTOR_FILE_PATH, ECS_TOOLS_UI_FILE_PATH };
//
//		UIWindowDescriptor ui_descriptor;
//		ui_descriptor.draw = nullptr;
//		ui_descriptor.initial_position_x = 0.0f;
//		ui_descriptor.initial_position_y = 0.0f;
//		ui_descriptor.initial_size_x = 0.7f;
//		ui_descriptor.initial_size_y = 0.7f;
//		ui_descriptor.window_name = "Scene";
//		ui_descriptor.initialize = OpenFile<true>;
//		ui_descriptor.draw = OpenFile<false>;
//		ui_descriptor.window_data = &dd;
//		ui_descriptor.window_data_size = sizeof(dd);
//		ui_descriptor.private_action = SkipAction;
//
//		UIWindowDescriptor ui_descriptor2;
//		ui_descriptor2.draw = nullptr;
//		ui_descriptor2.initial_position_x = -1.0f;
//		ui_descriptor2.initial_position_y = -1.0f;
//		ui_descriptor2.initial_size_x = 0.7f;
//		ui_descriptor2.initial_size_y = 0.7f;
//		ui_descriptor2.window_name = "ECSEngine Editor";
//		ui_descriptor2.initialize = DrawSomething<true>;
//		ui_descriptor2.window_data = paths;
//		ui_descriptor2.private_action = SkipAction;
//
//		UIWindowDescriptor ui_descriptor3;
//		ui_descriptor3.draw = nullptr;
//		ui_descriptor3.initial_position_x = -0.5f;
//		ui_descriptor3.initial_position_y = -0.5f;
//		ui_descriptor3.initial_size_x = 0.5f;
//		ui_descriptor3.initial_size_y = 0.5f;
//		ui_descriptor3.window_name = "Properties";
//		ui_descriptor3.draw = DrawSomething2<false>;
//		ui_descriptor3.initialize = DrawSomething2<true>;
//		ui_descriptor3.private_action = SkipAction;
//		ui_descriptor3.window_data = WINDOW_CHARACTERS;
//		ui_descriptor3.window_data_size = 0;
//
//		UIWindowDescriptor ui_descriptor4;
//		ui_descriptor4.draw = nullptr;
//		ui_descriptor4.initial_position_x = -0.5f;
//		ui_descriptor4.initial_position_y = -0.5f;
//		ui_descriptor4.initial_size_x = 0.5f;
//		ui_descriptor4.initial_size_y = 0.5f;
//		ui_descriptor4.window_name = "Weird Characters: #ls add file";
//		ui_descriptor4.draw = DrawSomething<false>;
//		ui_descriptor4.initialize = DrawSomething<true>;
//		ui_descriptor4.private_action = SkipAction;
//		ui_descriptor4.window_data = paths;
//
//
////#define LOAD_FROM_FILE
//
//		const char* window_names[32];
//		Stream<const char*> window_names_stream = Stream<const char*>(window_names, 0);
//#ifdef LOAD_FROM_FILE
//		ui.LoadUIFile(ECS_TOOLS_UI_FILE_PATH, window_names_stream);
//
//		UIWindowSerializedMissingData serialized_data[32];
//		Stream<UIWindowSerializedMissingData> serialized_stream = Stream<UIWindowSerializedMissingData>(serialized_data, window_names_stream.size);
//		for (size_t index = 0; index < window_names_stream.size; index++) {
//			if (window_names_stream[index][0] == 'W') {
//				serialized_stream[index].draw = DrawSomething<false>;
//				serialized_stream[index].initialize = DrawSomething<true>;
//				serialized_stream[index].window_data = paths;
//			}
//			else if (window_names_stream[index][0] == 'E') {
//				serialized_stream[index].draw = nullptr;
//				serialized_stream[index].initialize = DrawSomething<true>;
//				serialized_stream[index].window_data = paths;
//			}
//			else if (window_names_stream[index][0] == 'P') {
//				serialized_stream[index].draw = DrawSomething2<false>;
//				serialized_stream[index].initialize = DrawSomething2<true>;
//				serialized_stream[index].window_data = WINDOW_CHARACTERS;
//			}
//			else {
//				serialized_stream[index].draw = nullptr;
//				serialized_stream[index].initialize = DrawSomething<true>;
//				serialized_stream[index].window_data = paths;
//			}
//		}
//		ui.FillWindowDataAfterFileLoad(serialized_stream);
//#else
//
//		ui.Create_Window(ui_descriptor);
//		ui.Create_Window(ui_descriptor2);
//		ui.Create_Window(ui_descriptor3);
//		ui.Create_Window(ui_descriptor4);
//		ui.Create_Window(ui_descriptor);
//		ui.Create_Window(ui_descriptor2);
//		ui.Create_Window(ui_descriptor3);
//		ui.Create_Window(ui_descriptor4);
//		ui.Create_Window(ui_descriptor);
//		ui.Create_Window(ui_descriptor2);
//		ui.Create_Window(ui_descriptor3);
//		ui.Create_Window(ui_descriptor4);
//
//		ui.CreateDockspace(
//			UIElementTransform(float2(-0.250f, -0.250f), float2(0.7f, 0.7f)),
//			DockspaceType::Horizontal,
//			0,
//			false
//		);
//		ui.CreateDockspace(
//			UIElementTransform(float2(-0.252f, 0.0f), float2(0.7f, 0.7f)),
//			DockspaceType::FloatingHorizontal,
//			1,
//			false
//		);
//		ui.CreateDockspace(
//			UIElementTransform(float2(0.0f, 0.0f), float2(0.5f, 1.0f)),
//			DockspaceType::Vertical,
//			2,
//			false
//		);
//		ui.AddElementToDockspace(3, false, 0, 0, DockspaceType::Horizontal);
//		//ui.AddWindowToDockspace(2, 0, ui.m_horizontal_dockspaces, 0);
//		ui.AddElementToDockspace(4, false, 1, 0, DockspaceType::Vertical);
//		//ui.AddWindowToDockspace(0, 0, ui.m_horizontal_dockspaces, 0);
//		ui.CreateDockspace(
//			UIElementTransform(float2(-0.750f, -0.9f), float2(1.5f, 1.5f)),
//			DockspaceType::FloatingVertical,
//			0,
//			//false
//			true
//		);
//		ui.CreateDockspace(
//			UIElementTransform({ 0.0f, -0.5f }, { 0.7f, 1.0f }),
//			DockspaceType::FloatingHorizontal,
//			11,
//			false
//		);
//		ui.AddElementToDockspace(5, false, 0, 0, DockspaceType::FloatingVertical);
//		ui.m_floating_vertical_dockspaces[0].borders[0].window_indices[1] = 10;
//		ui.m_floating_vertical_dockspaces[0].borders[0].window_indices.size++;
//		//ui.m_floating_vertical_dockspaces[0].borders[0].draw_region_header = false;
//		//ui.AddElementToDockspace(0, true, 1, 0, DockspaceType::FloatingHorizontal);
//		ui.CreateDockspace(
//			UIElementTransform(float2(0.5f, 0.5f), float2(0.5f, 0.5f)),
//			DockspaceType::Vertical,
//			6,
//			false
//		);
//		ui.AddElementToDockspace(7, false, 0, 1, DockspaceType::Vertical);
//		ui.CreateDockspace(
//			UIElementTransform(float2(0.8f, 0.8f), float2(0.5f, 0.7f)),
//			DockspaceType::Horizontal,
//			8,
//			false
//		);
//		ui.AddElementToDockspace(
//			9,
//			false,
//			1,
//			1,
//			DockspaceType::Horizontal
//		);
//
//		ui.m_floating_vertical_dockspaces[0].borders[0].draw_region_header = true;
//#endif
//
//		//ui.AddElementToDockspace(1, true, 0, 0, DockspaceType::FloatingHorizontal);
//
//		bool yea = false;
//		MSG message;
//		BOOL result = 0;
//
//		while (result == 0) {
//			while (PeekMessage(&message, nullptr, 0, 0, PM_REMOVE) != 0) {
//				switch (message.message) {
//				case WM_QUIT:
//					result = -1;
//					break;
//				}
//				TranslateMessage(&message);
//				DispatchMessage(&message);
//			}
//
//			auto mouse_state = mouse.GetState();
//			auto keyboard_state = keyboard.GetState();
//			mouse.UpdateState();
//			mouse.UpdateTracker();
//			keyboard.UpdateState();
//			keyboard.UpdateTracker();
//			bool left_button = mouse_state->LeftButton();
//
//			static bool yes = false;
//			static bool yes1 = false;
//			static bool yes2 = false;
//			static bool yes3 = false;
//			static bool yes4 = false;
//			static bool yes5 = false;
//			static bool yes6 = false;
//
//			std::this_thread::sleep_for(std::chrono::milliseconds(SLEEP_AMOUNT));
//
//			if (yes6 == false) {
//				graphics->ClearBackBuffer(0.0f, 0.0f, 0.0f);
//			}
//
//			float normalized_mouse_x = (float)mouse_state->PositionX() / width * 2 - 1.0f;
//			float normalized_mouse_y = (float)mouse_state->PositionY() / height * 2 - 1.0f;
//
//#ifndef LOAD_FROM_FILE
//			if (yes == false) {
//				yes = true;
//				ui.AddElementToDockspace(1, true, 0, 0, DockspaceType::Horizontal);
//				//ui.AddElementToDockspace(1, true, 1, 0, DockspaceType::FloatingHorizontal);
//				//ui.AddElementToDockspace(0, true, 1, 0, DockspaceType::Horizontal);
//			}
//			if (yes1 == false) {
//				yes1 = true;
//				ui.AddElementToDockspace(1, true, 1, 0, DockspaceType::FloatingVertical);
//			}
//			if (yes2 == false) {
//				yes2 = true;
//				ui.AddElementToDockspace(0, true, 1, 0, DockspaceType::Horizontal);
//			}
//#endif
//
//			if (yes6 == false) {
//				ui.DoFrame();
//#if 0
//				for (size_t index = 0; index < base_archetype_stream.size; index++) {
//					base_archetype_stream[index]->GetEntities(sequence_stream, entity_buffers);
//					for (size_t chunk_index = 0; chunk_index < sequence_stream.size; chunk_index++) {
//						for (size_t sequence_index = 0; sequence_index < sequence_stream[chunk_index].size; sequence_index++) {
//							for (size_t entity_index = 0; entity_index < sequence_stream[chunk_index][sequence_index].size; entity_index++) {
//								entity_manager.GetComponent(sequence_stream[chunk_index][sequence_index].first + entity_index, box_unique_components, box_translation);
//								ECSEngine::Translation* translation = (ECSEngine::Translation*)box_translation[0];
//								if (entity_index == 0) {
//									translation->x = mouse_state->PositionX() * 0.005f;
//									translation->y = mouse_state->PositionY() * -0.005f;
//								}
//								//gs.DrawTexturedBoxes(box_data_stream, *translation);
//							}
//						}
//					}
//				}
//#endif
//			}
//			if (yes6 == false) {
//				graphics->SwapBuffers(1);
//			}
//
//			mouse.SetPreviousPosition();
//		}
//
//		if (result == -1)
//			return -1;
//		else
//			return message.wParam;
//#endif
//#endif
//	}
//private:
//	// singleton that manages registering and unregistering the editor class
//	class EditorClass {
//	public:
//		static LPCWSTR GetName() noexcept;
//		static HINSTANCE GetInstance() noexcept;
//		EditorClass() noexcept;
//		~EditorClass();
//	private:
//		EditorClass(const EditorClass&) = delete;
//		EditorClass& operator = (const EditorClass&) = delete;
//		static constexpr LPCWSTR editorClassName = L"Editor Window";
//		static EditorClass editorInstance;
//		HINSTANCE hInstance;
//	};
//}; // Editor
//
//Editor::EditorClass Editor::EditorClass::editorInstance;
//Editor::EditorClass::EditorClass() noexcept : hInstance(GetModuleHandle(nullptr)) {
//	WNDCLASSEX editor_class = { 0 };
//	editor_class.cbSize = sizeof(editor_class);
//	editor_class.style = CS_OWNDC;
//	editor_class.lpfnWndProc = HandleMessageSetup;
//	editor_class.cbClsExtra = 0;
//	editor_class.cbWndExtra = 0;
//	editor_class.hInstance = GetInstance();
//	editor_class.hIcon = static_cast<HICON>(LoadImage(hInstance, MAKEINTRESOURCE(IDI_ICON1), IMAGE_ICON, 128, 128, 0));
//	editor_class.hCursor = nullptr;
//	editor_class.hbrBackground = nullptr;
//	editor_class.lpszMenuName = nullptr;
//	editor_class.lpszClassName = GetName();
//	editor_class.hIconSm = static_cast<HICON>(LoadImage(hInstance, MAKEINTRESOURCE(IDI_ICON1), IMAGE_ICON, 32, 32, 0));;
//
//	RegisterClassEx(&editor_class);
//}
//Editor::EditorClass::~EditorClass() {
//	UnregisterClass(editorClassName, GetInstance());
//}
//HINSTANCE Editor::EditorClass::GetInstance() noexcept {
//	return editorInstance.hInstance;
//}
//LPCWSTR Editor::EditorClass::GetName() noexcept {
//	return editorClassName;
//}
//
//Editor::Editor(int _width, int _height, LPCWSTR name) noexcept : timer("Editor"), width(0), height(0)
//{
//	// calculate window size based on desired client region
//	RECT windowRegion;
//	windowRegion.left = 0;
//	windowRegion.right = _width;
//	windowRegion.bottom = _height;
//	windowRegion.top = 0;
//	AdjustWindowRect(&windowRegion, WS_CAPTION | WS_MINIMIZEBOX | WS_SYSMENU, FALSE);
//
//	width = _width;
//	height = _height;
//
//	HCURSOR* cursor_stream = new HCURSOR[ECS_CURSOR_COUNT];
//
//	// hInstance is null because these are predefined cursors
//	cursors = ECSEngine::Stream<HCURSOR>(cursor_stream, ECS_CURSOR_COUNT);
//	cursors[(unsigned int)ECSEngine::CursorType::AppStarting] = LoadCursor(NULL, IDC_APPSTARTING);
//	cursors[(unsigned int)ECSEngine::CursorType::Cross] = LoadCursor(NULL, IDC_CROSS);
//	cursors[(unsigned int)ECSEngine::CursorType::Default] = LoadCursor(NULL, IDC_ARROW);
//	cursors[(unsigned int)ECSEngine::CursorType::Hand] = LoadCursor(NULL, IDC_HAND);
//	cursors[(unsigned int)ECSEngine::CursorType::Help] = LoadCursor(NULL, IDC_HELP);
//	cursors[(unsigned int)ECSEngine::CursorType::IBeam] = LoadCursor(NULL, IDC_IBEAM);
//	cursors[(unsigned int)ECSEngine::CursorType::SizeAll] = LoadCursor(NULL, IDC_SIZEALL);
//	cursors[(unsigned int)ECSEngine::CursorType::SizeEW] = LoadCursor(NULL, IDC_SIZEWE);
//	cursors[(unsigned int)ECSEngine::CursorType::SizeNESW] = LoadCursor(NULL, IDC_SIZENESW);
//	cursors[(unsigned int)ECSEngine::CursorType::SizeNS] = LoadCursor(NULL, IDC_SIZENS);
//	cursors[(unsigned int)ECSEngine::CursorType::SizeNWSE] = LoadCursor(NULL, IDC_SIZENWSE);
//	cursors[(unsigned int)ECSEngine::CursorType::Wait] = LoadCursor(NULL, IDC_WAIT);
//
//	ChangeCursor(ECSEngine::CursorType::Default);
//
//	// create window and get Handle
//	hWnd = CreateWindow(
//		EditorClass::GetName(),
//		name,
//		WS_OVERLAPPEDWINDOW /*^ WS_THICKFRAME*/,
//		CW_USEDEFAULT,
//		CW_USEDEFAULT,
//		windowRegion.right - windowRegion.left,
//		windowRegion.bottom - windowRegion.top,
//		nullptr,
//		nullptr,
//		EditorClass::GetInstance(),
//		this
//	);
//
//	// show window since default is hidden
//	ShowWindow(hWnd, SW_SHOWMAXIMIZED);
//	UpdateWindow(hWnd);
//	RECT rect;
//	GetClientRect(hWnd, &rect);
//	width = rect.right - rect.left;
//	height = rect.bottom - rect.top;
//
//	GraphicsDescriptor graphics_descriptor;
//	graphics_descriptor.window_size = { (unsigned int)width, (unsigned int)height };
//	graphics = new ECSEngine::Graphics(hWnd, &graphics_descriptor);
//}
//Editor::~Editor() {
//	delete[] cursors.buffer;
//	DestroyWindow(hWnd);
//}
//
//LRESULT WINAPI Editor::HandleMessageSetup(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) noexcept {
//
//	if (message == WM_NCCREATE) {
//
//		// extract pointer to window class from creation data
//		const CREATESTRUCTW* const createStruct = reinterpret_cast<CREATESTRUCTW*>(lParam);
//		Editor* const editor_pointer = static_cast<Editor*>(createStruct->lpCreateParams);
//
//		// set WinAPI managed user data to store pointer to editor class
//		SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(editor_pointer));
//
//		// set WinAPI procedure to forwarder
//		SetWindowLongPtr(hWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&Editor::HandleMessageForward));
//
//		// forward message
//		return editor_pointer->HandleMessageForward(hWnd, message, wParam, lParam);
//	}
//}
//
//LRESULT WINAPI Editor::HandleMessageForward(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) noexcept {
//
//	// retrieve pointer to editor class
//	Editor* editor = reinterpret_cast<Editor*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
//
//	if (message == WM_SIZE) {
//		float new_width = LOWORD(lParam);
//		float new_height = HIWORD(lParam);
//		if (new_width != 0.0f && new_height != 0.0f) {
//			if (editor->graphics != nullptr) {
//				editor->graphics->SetNewSize(hWnd, new_width, new_height);
//			}
//			editor->width = new_width;
//			editor->height = new_height;
//		}
//	}
//
//	return editor->HandleMessage(hWnd, message, wParam, lParam);
//}
//
//LRESULT Editor::HandleMessage(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) noexcept {
//
//	switch (message) {
//	case WM_CLOSE:
//		PostQuitMessage(0);
//		return 0;
//	case WM_ACTIVATEAPP:
//		keyboard.Procedure({ message, wParam, lParam });
//		mouse.Procedure({ message, wParam, lParam });
//		break;
//	case WM_INPUT:
//	case WM_MOUSEMOVE:
//	case WM_LBUTTONDOWN:
//	case WM_LBUTTONUP:
//	case WM_RBUTTONDOWN:
//	case WM_RBUTTONUP:
//	case WM_MBUTTONDOWN:
//	case WM_MBUTTONUP:
//	case WM_MOUSEWHEEL:
//	case WM_XBUTTONDOWN:
//	case WM_XBUTTONUP:
//	case WM_MOUSEHOVER:
//		mouse.Procedure({ message, wParam, lParam });
//		break;
//	case WM_CHAR:
//	case WM_KEYDOWN:
//	case WM_KEYUP:
//	case WM_SYSKEYUP:
//	case WM_SYSKEYDOWN:
//		keyboard.Procedure({ message, wParam, lParam });
//		break;
//	case WM_SETCURSOR:
//		if (cursors[(unsigned int)current_cursor] != GetCursor()) {
//			SetCursor(cursors[(unsigned int)current_cursor]);
//		}
//		break;
//	}
//
//	return DefWindowProc(hWnd, message, wParam, lParam);
//}
//
//ECSEngine::Application* ECSEngine::CreateApplication(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
//	return new Editor(ECS_TOOLS_UI_DESIGNED_WIDTH, ECS_TOOLS_UI_DESIGNED_HEIGHT, L"ECSEngine Editor");
//}
//ECSEngine::Application* ECSEngine::CreateApplication() {
//	return new Editor(ECS_TOOLS_UI_DESIGNED_WIDTH, ECS_TOOLS_UI_DESIGNED_HEIGHT, L"ECSEngine Editor");
//}