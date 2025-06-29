#include "ecspch.h"
#include "RuntimeSystems.h"
#include "../Multithreading/TaskScheduler.h"
#include "../Utilities/Serialization/Binary/Serialization.h"
#include "../Utilities/Reflection/Reflection.h"
#include "../Utilities/BufferedFileReaderWriter.h"
#include "World.h"

namespace ECSEngine {

	// In order to make the registration API friendlier, we are going to use a global to record the options per each task scheduler 
	// (such that there can be multiple different instances at the same time). We don't want each individual task allocate for the 
	// options such that they carried over. Also, we don't really care about this being exposed to the outer world, as it is local to this file.

	// -----------------------------------------------------------------------------------------------------------------------------

	struct PerEntryOptions {
		// This is going to be used as a key to lookup the options for
		const TaskScheduler* key;
		RegisterECSRuntimeSystemsOptions options;
	};

	// Initialize this with a capacity of 2
	static ResizableStream<PerEntryOptions> PER_ENTRY_OPTIONS(ECS_MALLOC_ALLOCATOR, 2);

	static RegisterECSRuntimeSystemsOptions* GetOptions(const TaskScheduler* key) {
		for (unsigned int index = 0; index < PER_ENTRY_OPTIONS.size; index++) {
			if (PER_ENTRY_OPTIONS[index].key == key) {
				return &PER_ENTRY_OPTIONS[index].options;
			}
		}

		return nullptr;
	}

	// Returns the options that correpond to the provided world
	static RegisterECSRuntimeSystemsOptions* GetOptions(const World* world) {
		return GetOptions(world->task_scheduler);
	}

	// Sets the options for a specific key
	static void SetOptions(const TaskScheduler* key, const RegisterECSRuntimeSystemsOptions& options) {
		const AllocatorPolymorphic allocator = ECS_MALLOC_ALLOCATOR;

		// If the key already exists, overwrite the data from that entry, else create a new entry
		RegisterECSRuntimeSystemsOptions* container_options = GetOptions(key);
		if (container_options != nullptr) {
			container_options->Deallocate(allocator);
			*container_options = options.Copy(allocator);
		}
		else {
			// Add a new entry
			PER_ENTRY_OPTIONS.Add({ key, options.Copy(allocator) });
		}
	}

	// -----------------------------------------------------------------------------------------------------------------------------

	static Stream<wchar_t> GetMonitoredValuesFilePath(const World* world, unsigned int file_index, CapacityStream<wchar_t>& file_path_storage) {
		const RegisterECSRuntimeSystemsOptions* options = GetOptions(world);
		if (options != nullptr && options->project_path.size > 0) {
			file_path_storage.CopyOther(options->project_path);
			file_path_storage.AddAssert(ECS_OS_PATH_SEPARATOR);
		}

		file_path_storage.AddStreamAssert(L".monitored_values");
		// If the folder doesn't exist, create it
		if (!ExistsFileOrFolder(file_path_storage)) {
			ECS_CRASH_CONDITION_RETURN_DEDUCE(CreateFolder(file_path_storage), "Failed to create monitored values folder");
		}

		// If the options are present, assume that an absolute path was given, else use the relative separator
		file_path_storage.AddAssert(options != nullptr ? ECS_OS_PATH_SEPARATOR : ECS_OS_PATH_SEPARATOR_REL);
		ConvertIntToChars(file_path_storage, file_index);
		file_path_storage.AddStreamAssert(L".txt");

		return file_path_storage;
	}

	struct WriteMonitoredValuesData {
		ECS_INLINE static Stream<char> Key() {
			return "__WriteMonitoredValuesData";
		}

		// Returns the file path of the file where data should be written
		ECS_INLINE Stream<wchar_t> GetFilePath(const World* world, CapacityStream<wchar_t>& file_path_storage) const {
			return GetMonitoredValuesFilePath(world, file_index, file_path_storage);
		}
		
		void EnsureFileIsOpened(const World* world) {
			if (write_instrument.file == -1) {
				// We need to open the file
				ECS_STACK_CAPACITY_STREAM(wchar_t, file_path_storage, ECS_KB);
				Stream<wchar_t> file_path = GetFilePath(world, file_path_storage);
				
				ECS_CRASH_CONDITION_RETURN_VOID(FileCreate(file_path, &write_instrument.file, ECS_FILE_ACCESS_WRITE_ONLY | ECS_FILE_ACCESS_TEXT | ECS_FILE_ACCESS_TRUNCATE_FILE) == ECS_FILE_STATUS_OK, "Failed to create/open monitored values file");
				// Call the constructor since it might do some other operations
				write_instrument = BufferedFileWriteInstrument(write_instrument.file, write_instrument.buffering, 0);

				// Each frame must have a header with the write size, so write a header now
				frame_size_header_offset = 0;
				ECS_CRASH_CONDITION_RETURN_VOID(write_instrument.AppendUninitialized(sizeof(unsigned int)), "Failed to append monitored values frame header");
			}
		}

		// Does not ensure that the file is opened previously
		ECS_INLINE void AddFloat(float value) {
			ECS_CRASH_CONDITION(write_instrument.Write(&value), "Failed to write monitored value float");
		}

		// Does not ensure that the file is opened previously
		ECS_INLINE void AddInteger(int64_t value) {
			ECS_CRASH_CONDITION(SerializeIntVariableLengthBool(&write_instrument, value), "Failed to write monitored value integer");
		}

		// Does not ensure that the file is opened previously
		ECS_INLINE void AddDouble(double value) {
			ECS_CRASH_CONDITION(write_instrument.Write(&value), "Failed to write monitored value double");
		}

		// Does not ensure that the file is opened previously
		void AddStruct(const World* world, Stream<char> type_name, const void* data) {
			// Ensure that we have the reflection manager set in the options
			const RegisterECSRuntimeSystemsOptions* options = GetOptions(world);
			ECS_CRASH_CONDITION_RETURN_VOID(options != nullptr && options->reflection_manager != nullptr, "Writing a monitored value struct requires the reflection manager to be set in the register options!");
			// Use default serialize options
			ECS_CRASH_CONDITION_RETURN_VOID(Serialize(options->reflection_manager, options->reflection_manager->GetType(type_name), data, &write_instrument) == ECS_SERIALIZE_OK, 
				"Failed to write monitored value struct");
		}

		void FinishFrame() {
			if (write_instrument.file != -1) {
				// Write the current's frame write size header and append the unitialized header for the next frame.
				size_t current_offset = write_instrument.GetOffset();
				size_t frame_write_size = current_offset - frame_size_header_offset - sizeof(unsigned int);
				ECS_CRASH_CONDITION_RETURN_VOID(EnsureUnsignedIntegerIsInRange<unsigned int>(frame_write_size), "Monitored value frame header integer width is not sufficient");
				unsigned int int_frame_write_size = (unsigned int)frame_write_size;
				ECS_CRASH_CONDITION_RETURN_VOID(write_instrument.WriteUninitializedData(frame_size_header_offset, &int_frame_write_size, sizeof(int_frame_write_size)), "Failed to write monitored values the current frame's header write size");

				frame_size_header_offset = current_offset;
				ECS_CRASH_CONDITION_RETURN_VOID(write_instrument.AppendUninitialized(sizeof(int_frame_write_size)), "Failed to append monitored values new frame header");
			}
		}

		BufferedFileWriteInstrument write_instrument;
		// This is the index of the current opened file, in case multiple recordings are desired
		unsigned int file_index;
		size_t frame_size_header_offset;
	};

	static void WriteMonitoredValuesInitialize(World* world, StaticThreadTaskInitializeInfo* initialize_data) {
		WriteMonitoredValuesData data;
		data.file_index = 0;
		// Initialize the write instrument's buffering now, using the entity manager's allocator, which will guarantee the proper
		// Deallocation when the world is stopped or crashed
		data.write_instrument.buffering.Initialize(world->entity_manager->MainAllocator(), ECS_KB * 64);
		world->system_manager->BindDataUnique(data.Key(), &data, sizeof(data));
	}

	// This task will write the monitored values
	static ECS_THREAD_TASK(WriteMonitoredValues) {
		WriteMonitoredValuesData* data = world->system_manager->GetData<WriteMonitoredValuesData>();
		// The only thing to be done is to finish the frame, the actual writes themselves will do the rest of the work
		data->FinishFrame();
	}

	// -----------------------------------------------------------------------------------------------------------------------------

	struct ReadMonitoredValuesData {
		ECS_INLINE static Stream<char> Key() {
			return "__ReadMonitoredValuesData";
		}

		// Returns the file path of the file where data should be written
		ECS_INLINE Stream<wchar_t> GetFilePath(const World* world, CapacityStream<wchar_t>& file_path_storage) const {
			return GetMonitoredValuesFilePath(world, file_index, file_path_storage);
		}

		ECS_INLINE size_t GetCurrentFrameInstrumentEndOffset() const {
			return last_frame_header_offset + current_frame_header_size + sizeof(current_frame_header_size);
		}

		void EnsureFileIsOpened(const World* world) {
			if (read_instrument.file == -1) {
				// We need to open the file
				ECS_STACK_CAPACITY_STREAM(wchar_t, file_path_storage, ECS_KB);
				Stream<wchar_t> file_path = GetFilePath(world, file_path_storage);

				ECS_CRASH_CONDITION_RETURN_VOID(OpenFile(file_path, &read_instrument.file, ECS_FILE_ACCESS_READ_ONLY | ECS_FILE_ACCESS_TEXT) == ECS_FILE_STATUS_OK, "Failed to create/open monitored values file");
				// Call the constructor since it might do some other operations
				read_instrument = BufferedFileReadInstrument(read_instrument.file, read_instrument.buffering, 0);

				if (read_instrument.IsEndReached()) {
					return;
				}

				// Each frame must have a header with the write size, so write a header now
				current_frame_header_size = 0;
				ECS_CRASH_CONDITION_RETURN_VOID(read_instrument.Read(&current_frame_header_size), "Failed to read monitored values first frame header");

				// Create a subinstrument to ensure that we don't go overboard the frame's data
				read_instrument.PushSubinstrumentNoDeallocator(&subinstrument_data, current_frame_header_size);
			}
		}

		// Does not ensure that the file is opened previously
		float GetFloat() {
			float value = 0.0f;
			// If the end was reached, early exit
			if (read_instrument.IsEndReached()) {
				return value;
			}

			ECS_CRASH_CONDITION(read_instrument.Read(&value), "Failed to read monitored value float");
			return value;
		}

		// Does not ensure that the file is opened previously
		int64_t GetInteger() {
			int64_t value = 0;
			// If the end was reached, early exit
			if (read_instrument.IsEndReached()) {
				return value;
			}

			ECS_CRASH_CONDITION(DeserializeIntVariableLengthBool(&read_instrument, value), "Failed to read monitored value integer");
			return value;
		}

		// Does not ensure that the file is opened previously
		double GetDouble() {
			double value = 0.0;
			// If the end was reached, early exit
			if (read_instrument.IsEndReached()) {
				return value;
			}

			ECS_CRASH_CONDITION(read_instrument.Read(&value), "Failed to read monitored value double");
			return value;
		}

		// Does not ensure that the file is opened previously
		void GetStruct(const World* world, Stream<char> type_name, void* data, AllocatorPolymorphic allocator) {
			// If the end was reached, early exit
			if (read_instrument.IsEndReached()) {
				return;
			}

			// Ensure that we have the reflection manager set in the options
			const RegisterECSRuntimeSystemsOptions* options = GetOptions(world);
			ECS_CRASH_CONDITION_RETURN_VOID(options != nullptr && options->reflection_manager != nullptr, "Reading a monitored value struct requires the reflection manager to be set in the register options!");
			
			DeserializeOptions deserialize_options;
			deserialize_options.field_allocator = allocator;
			ECS_CRASH_CONDITION_RETURN_VOID(Deserialize(options->reflection_manager, options->reflection_manager->GetType(type_name), data, &read_instrument, &deserialize_options) == ECS_DESERIALIZE_OK,
				"Failed to write monitored value struct");
		}

		void FinishFrame() {
			if (read_instrument.file != -1) {
				// We can pop the current subinstrument
				read_instrument.PopSubinstrument();

				// Ensure that we got to the end of the frame's range, and read the next header offset
				size_t current_offset = read_instrument.GetOffset();
				size_t final_frame_offset = GetCurrentFrameInstrumentEndOffset();
				if (current_offset != final_frame_offset) {
					ECS_CRASH_CONDITION_RETURN_VOID(read_instrument.Seek(ECS_INSTRUMENT_SEEK_START, final_frame_offset), "Failed to seek monitored value reader to the end of the frame's offset");
				}

				// If we reached the end, exit now
				if (read_instrument.IsEndReached()) {
					return;
				}

				last_frame_header_offset = final_frame_offset;
				// Read the new frame's header size
				ECS_CRASH_CONDITION_RETURN_VOID(read_instrument.Read(&current_frame_header_size), "Failed to read monitored value next frame header");
				// Use another subinstrument for the new frame
				read_instrument.PushSubinstrumentNoDeallocator(&subinstrument_data, current_frame_header_size);
			}
		}

		BufferedFileReadInstrument read_instrument;
		// This is the index of the current opened file, in case multiple recordings are desired
		unsigned int file_index;
		unsigned int current_frame_header_size;
		size_t last_frame_header_offset;

		// This is underlying storage used by the instrument's subinstrument
		ReadInstrument::SubinstrumentData subinstrument_data;
	};

	static void ReadMonitoredValuesInitialize(World* world, StaticThreadTaskInitializeInfo* initialize_data) {
		ReadMonitoredValuesData data;
		data.file_index = 0;
		// Initialize the write instrument's buffering now, using the entity manager's allocator, which will guarantee the proper
		// Deallocation when the world is stopped or crashed
		data.read_instrument.buffering.Initialize(world->entity_manager->MainAllocator(), ECS_KB * 64);
		world->system_manager->BindDataUnique(data.Key(), &data, sizeof(data));
	}

	// This task will write the monitored values
	static ECS_THREAD_TASK(ReadMonitoredValues) {
		ReadMonitoredValuesData* data = world->system_manager->GetData<ReadMonitoredValuesData>();
		// The only thing to be done is to finish the frame, the actual reads themselves will do the rest of the work
		data->FinishFrame();
	}

	// -----------------------------------------------------------------------------------------------------------------------------

	void RegisterRuntimeMonitoredFloat(const World* world, float value) {
		WriteMonitoredValuesData* data = world->system_manager->GetData<WriteMonitoredValuesData>();
		data->EnsureFileIsOpened(world);
		data->AddFloat(value);
	}

	void RegisterRuntimeMonitoredInt(const World* world, int64_t value) {
		WriteMonitoredValuesData* data = world->system_manager->GetData<WriteMonitoredValuesData>();
		data->EnsureFileIsOpened(world);
		data->AddInteger(value);
	}

	void RegisterRuntimeMonitoredDouble(const World* world, double value) {
		WriteMonitoredValuesData* data = world->system_manager->GetData<WriteMonitoredValuesData>();
		data->EnsureFileIsOpened(world);
		data->AddDouble(value);
	}

	void RegisterRuntimeMonitoredStruct(const World* world, Stream<char> reflection_type_name, const void* struct_data) {
		WriteMonitoredValuesData* data = world->system_manager->GetData<WriteMonitoredValuesData>();
		data->EnsureFileIsOpened(world);
		data->AddStruct(world, reflection_type_name, struct_data);
	}

	void SetWriteRuntimeMonitoredStructFileIndex(const World* world, unsigned int file_index) {
		WriteMonitoredValuesData* data = world->system_manager->GetData<WriteMonitoredValuesData>();
		data->file_index = file_index;
	}

	// -----------------------------------------------------------------------------------------------------------------------------

	float GetRuntimeMonitoredFloat(const World* world) {
		ReadMonitoredValuesData* data = world->system_manager->GetData<ReadMonitoredValuesData>();
		data->EnsureFileIsOpened(world);
		return data->GetFloat();
	}

	int64_t GetRuntimeMonitoredInt(const World* world) {
		ReadMonitoredValuesData* data = world->system_manager->GetData<ReadMonitoredValuesData>();
		data->EnsureFileIsOpened(world);
		return data->GetInteger();
	}

	double GetRuntimeMonitoredDouble(const World* world) {
		ReadMonitoredValuesData* data = world->system_manager->GetData<ReadMonitoredValuesData>();
		data->EnsureFileIsOpened(world);
		return data->GetDouble();
	}

	void GetRuntimeMonitoredStruct(const World* world, Stream<char> reflection_type_name, void* struct_data, AllocatorPolymorphic field_allocator) {
		ReadMonitoredValuesData* data = world->system_manager->GetData<ReadMonitoredValuesData>();
		data->EnsureFileIsOpened(world);
		data->GetStruct(world, reflection_type_name, struct_data, field_allocator);
	}

	void SetReadRuntimeMonitoredStructFileIndex(const World* world, unsigned int file_index) {
		ReadMonitoredValuesData* data = world->system_manager->GetData<ReadMonitoredValuesData>();
		data->file_index = file_index;
	}

	// -----------------------------------------------------------------------------------------------------------------------------

	void RegisterECSRuntimeSystems(TaskScheduler* task_scheduler, const RegisterECSRuntimeSystemsOptions& options) {
		SetOptions(task_scheduler, options);

		TaskSchedulerElement write_monitored_values;
		write_monitored_values.task_group = ECS_THREAD_TASK_FINALIZE_LATE;
		write_monitored_values.task_function = WriteMonitoredValues;
		write_monitored_values.initialize_task_function = WriteMonitoredValuesInitialize;
		write_monitored_values.task_name = STRING(WriteMonitoredValues);
		task_scheduler->Add(write_monitored_values);

		TaskSchedulerElement read_monitored_values;
		read_monitored_values.task_group = ECS_THREAD_TASK_FINALIZE_LATE;
		read_monitored_values.task_function = ReadMonitoredValues;
		read_monitored_values.initialize_task_function = ReadMonitoredValuesInitialize;
		read_monitored_values.task_name = STRING(ReadMonitoredValues);
		task_scheduler->Add(read_monitored_values);
	}

	// -----------------------------------------------------------------------------------------------------------------------------

	void UnregisterECSRuntimeSystems(World* world) {
		WriteMonitoredValuesData* write_monitored_values_data = world->system_manager->GetData<WriteMonitoredValuesData>();
		// No need to deallocate the buffering itself, that will be done by the pause or crash, just close the file, if it was opened
		if (write_monitored_values_data->write_instrument.file != -1) {
			write_monitored_values_data->write_instrument.Flush();
			CloseFile(write_monitored_values_data->write_instrument.file);
		}

		ReadMonitoredValuesData* read_monitored_values_data = world->system_manager->GetData<ReadMonitoredValuesData>();
		if (read_monitored_values_data->read_instrument.file != -1) {
			CloseFile(read_monitored_values_data->read_instrument.file);
		}
	}

	// -----------------------------------------------------------------------------------------------------------------------------

	RegisterECSRuntimeSystemsOptions RegisterECSRuntimeSystemsOptions::Copy(AllocatorPolymorphic allocator) const {
		RegisterECSRuntimeSystemsOptions copy;

		copy.reflection_manager = reflection_manager;
		copy.project_path = project_path.Copy(allocator);

		return copy;
	}

	void RegisterECSRuntimeSystemsOptions::Deallocate(AllocatorPolymorphic allocator) {
		project_path.Deallocate(allocator);
	}

	// -----------------------------------------------------------------------------------------------------------------------------

}