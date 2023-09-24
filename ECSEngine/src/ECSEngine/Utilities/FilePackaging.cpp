#include "ecspch.h"
#include "FilePackaging.h"
#include "Encryption.h"

#include "Serialization/SerializationHelpers.h"
#include "FunctionInterfaces.h"

#define TEMPORARY_BUFFER_SIZE ECS_MB * 50

namespace ECSEngine {

	// -------------------------------------------------------------------------------------------------------------------

	struct TableHeader {
		unsigned int table_size;
		unsigned int string_total_size;
		unsigned int table_capacity;
	};

	// The table is located at the end of the file. The last 4 bytes indicate the table size.
	// The table can then be memcpy'ed from that point on
	unsigned int PackFiles(Stream<Stream<wchar_t>> input, Stream<bool> input_is_text_file, Stream<wchar_t> output, PackFileFunctor functor, void* data)
	{
		ECS_ASSERT(input.size == input_is_text_file.size);

		PackFilesLookupTable lookup_table;
		size_t table_capacity = function::PowerOfTwoGreater((size_t)((float)input.size * (100 / ECS_HASHTABLE_MAXIMUM_LOAD_FACTOR)));
		size_t table_size = lookup_table.MemoryOf(table_capacity);

		// Open a file handle to the output file
		ECS_FILE_HANDLE output_file = -1;
		ECS_FILE_STATUS_FLAGS status = FileCreate(output, &output_file, ECS_FILE_ACCESS_WRITE_BINARY_TRUNCATE);
		if (status != ECS_FILE_STATUS_OK) {
			return -2;
		}

		// Use malloc - the table might occupy a lot of space due to the
		void* allocation = malloc(table_size);
		lookup_table.InitializeFromBuffer(allocation, table_capacity);

		// We need an extra buffer in order to store the offsets and the sizes for each file
		uint2* offsets = (uint2*)malloc(sizeof(uint2) * input.size);

		void* temporary_buffer = malloc(TEMPORARY_BUFFER_SIZE);

		auto error_lambda = [=]() {
			CloseFile(output_file);
			RemoveFile(output);
			free(offsets);
			free(temporary_buffer);
			free(allocation);
		};

		ECS_FILE_HANDLE current_file_handle = -1;
		bool close_success = false;
		for (size_t index = 0; index < input.size; index++) {
			ECS_FILE_ACCESS_FLAGS access_flags = ECS_FILE_ACCESS_READ_ONLY | ECS_FILE_ACCESS_OPTIMIZE_SEQUENTIAL;
			access_flags |= input_is_text_file[index] ? ECS_FILE_ACCESS_TEXT : ECS_FILE_ACCESS_BINARY;
			status = OpenFile(input[index], &current_file_handle, access_flags);
			if (status != ECS_FILE_STATUS_OK) {
				error_lambda();
				return index;
			}

			// Read the file data
			size_t file_size = GetFileByteSize(current_file_handle);
			if (file_size == 0) {
				error_lambda();
				CloseFile(current_file_handle);
				return index;
			}

			void* file_buffer = temporary_buffer;
			unsigned int buffer_size = TEMPORARY_BUFFER_SIZE;

			if (file_size >= TEMPORARY_BUFFER_SIZE) {
				file_buffer = malloc(file_size);
				buffer_size = file_size;
			}

			// Read the whole data
			unsigned int bytes_read = ReadFromFile(current_file_handle, { file_buffer, buffer_size });
			if (bytes_read == -1) {
				error_lambda();
				CloseFile(current_file_handle);
				return index;
			}

			if (bytes_read > 0) {
				Stream<void> contents(temporary_buffer, bytes_read);
				// Call the functor if any
				if (functor != nullptr) {
					contents = functor(contents, data);
				}

				size_t file_offset = GetFileCursor(output_file);
				if (file_offset == -1) {
					error_lambda();
					CloseFile(current_file_handle);
					return index;
				}
				if (file_offset > UINT_MAX) {
					error_lambda();
					CloseFile(current_file_handle);
					return index;
				}

				unsigned int uint_file_offset = (unsigned int)file_offset;
				unsigned int bytes_written = WriteToFile(output_file, contents);
				if (bytes_written == -1 || bytes_written != bytes_read) {
					error_lambda();
					CloseFile(current_file_handle);
					return index;
				}

				// Record now the offsets into the array
				offsets[index] = { uint_file_offset, bytes_written };
			}

			if (file_size >= TEMPORARY_BUFFER_SIZE) {
				free(file_buffer);	
			}		

			close_success = CloseFile(current_file_handle);
		}

		// Now write the lookup table - the first things that must be serialized are the name of the files
		// They will be encrypted in order to not expose the strings to the outer world
		ECS_STACK_CAPACITY_STREAM(wchar_t, encrypted_input, 1024);

		unsigned int string_bytes_written = 0;
		// Populate it first
		for (size_t index = 0; index < input.size; index++) {
			encrypted_input.CopyOther(input[index]);

			unsigned int input_hash = input[index].size * sizeof(wchar_t);
			// Encrypt the input now
			EncryptBufferByte(encrypted_input, input_hash);

			// Write the buffer now
			unsigned int bytes_written = WriteToFile(output_file, { encrypted_input.buffer, encrypted_input.size * sizeof(wchar_t) });
			if (bytes_written == -1 || bytes_written != encrypted_input.size * sizeof(wchar_t)) {
				error_lambda();
				return -3;
			}

			// For the identifier - use it as a uint2 - the offset and the size of the string
			unsigned int insert_position;
			ECS_ASSERT(!lookup_table.Insert(offsets[index], { input[index].buffer, (unsigned int)input[index].size * sizeof(wchar_t) }, insert_position));
			ResourceIdentifier* identifier = lookup_table.GetIdentifierPtrFromIndex(insert_position);
			identifier->ptr = (void*)((uintptr_t)string_bytes_written);

			string_bytes_written += sizeof(wchar_t) * input[index].size;
		}

		// Now write the actual table
		unsigned int bytes_written = WriteToFile(output_file, { allocation, table_size });
		if (bytes_written == -1 || bytes_written != table_size) {
			error_lambda();
			return -3;
		}

		// Write the table size and the string total size
		TableHeader header;
		header.table_size = (unsigned int)table_size;
		header.table_capacity = table_capacity;
		header.string_total_size = string_bytes_written;

		bytes_written = WriteToFile(output_file, { &header, sizeof(header) });
		if (bytes_written == -1 || bytes_written != sizeof(header)) {
			error_lambda();
			return -3;
		}

		CloseFile(output_file);
		free(temporary_buffer);
		free(allocation);
		free(offsets);

		return -1;
	}

	// -------------------------------------------------------------------------------------------------------------------

	PackFilesLookupTable GetPackedFileLookupTable(ECS_FILE_HANDLE file_handle, AllocatorPolymorphic allocator)
	{
		PackFilesLookupTable table;
		memset(&table, 0, sizeof(table));

		// Read the last 4 bytes - the size of the table
		size_t status = SetFileCursor(file_handle, -sizeof(TableHeader), ECS_FILE_SEEK_END);

		// Failed to seek
		if (status == -1) {
			return table;
		}

		TableHeader header;

		unsigned int bytes_read = ReadFromFile(file_handle, { &header, sizeof(header) });
		// Incorrect reading
		if (bytes_read != sizeof(header)) {
			return table;
		}

		if (header.string_total_size == 0 || header.table_capacity == 0 || header.table_size == 0) {
			return table;
		}

		if (!function::IsPowerOfTwo(header.table_capacity)) {
			return table;
		}

		// Now read the whole data into a buffer - the string and the actual table
		void* allocation = AllocateEx(allocator, header.table_size + header.string_total_size);
		status = SetFileCursor(file_handle, -sizeof(header) - header.table_size - header.string_total_size, ECS_FILE_SEEK_END);
		if (status == -1) {
			DeallocateEx(allocator, allocation);
			return table;
		}

		bytes_read = ReadFromFile(file_handle, { allocation, header.table_size + header.string_total_size });
		if (bytes_read == -1) {
			DeallocateEx(allocator, allocation);
			return table;
		}

		// Create now the table 
		table.SetBuffers(function::OffsetPointer(allocation, header.string_total_size), header.table_capacity);
		// The identifiers must be patched to form an absolute value from the relative ones
		// and then be decrypted

		table.ForEach<true>([&](auto value, ResourceIdentifier& identifier) {
			// Patch the reference, and then decrypt
			// Check to see if the identifier is still valid
			if ((uintptr_t)identifier.ptr + identifier.size > header.string_total_size) {
				memset(&table, 0, sizeof(table));
				DeallocateEx(allocator, allocation);
				// Fail
				return true;
			}
			identifier.ptr = function::OffsetPointer(allocation, (uintptr_t)identifier.ptr);
			// Decrypt the buffer now
			unsigned int decrypt_key = identifier.size;
			DecryptBufferByte({ identifier.ptr, identifier.size }, decrypt_key);

			return false;
		});

		return table;
	}

	// -------------------------------------------------------------------------------------------------------------------

	unsigned int GetMultiPackedFileIndex(Stream<wchar_t> file, const MultiPackedFile* multi_packed_file)
	{
		return multi_packed_file->lookup_table.GetValue(file);
	}

	// -------------------------------------------------------------------------------------------------------------------

#define MULTI_PACKED_FILE_VERSION (0)
#define MULTI_PACKED_FILE_MAX_COUNT (64)
#define MULTI_PACKED_FILE_MAX_INPUT_COUNT ECS_KB * 16

	struct MultiPackedFileHeader {
		unsigned int version;
		// The number of packed files (the number of output files)
		unsigned int element_count;
		// The total count of individual input file (those that are packed)
		unsigned int input_file_count;
	};

	MultiPackedFile LoadMultiPackedFile(Stream<wchar_t> file, AllocatorPolymorphic allocator)
	{
		// Use the heap for the file
		Stream<void> data = ReadWholeFileBinary(file);
		if (data.size > 0) {
			uintptr_t ptr = (uintptr_t)data.buffer;
			MultiPackedFile result = LoadMultiPackedFile(&ptr, data.size, allocator);
			free(data.buffer);
			return result;
		}
		MultiPackedFile failed_file;
		failed_file.packed_files = { nullptr, 0 };
		return failed_file;
	}

	// -------------------------------------------------------------------------------------------------------------------

	MultiPackedFile LoadMultiPackedFile(uintptr_t* file, size_t size, AllocatorPolymorphic allocator)
	{
		MultiPackedFile packed_file;
		packed_file.packed_files = { nullptr, 0 };

		// Decrypt the buffer first
		void* buffer = (void*)*file;
		DecryptBuffer({ buffer, size }, size);

		MultiPackedFileHeader header;
		Read<true>(file, &header, sizeof(header));

		if (header.version != MULTI_PACKED_FILE_VERSION || header.element_count > MULTI_PACKED_FILE_MAX_COUNT) {
			return packed_file;
		}

		packed_file.packed_files.Initialize(allocator, header.element_count);
		size_t table_capacity = function::PowerOfTwoGreater(HashTableCapacityForElements(header.input_file_count));
		packed_file.lookup_table.Initialize(allocator, table_capacity);

		for (size_t index = 0; index < header.element_count; index++) {
			// Read the count
			unsigned int current_count = 0;
			Read<true>(file, &current_count, sizeof(current_count));
			if (current_count > MULTI_PACKED_FILE_MAX_INPUT_COUNT) {
				memset(&packed_file, 0, sizeof(packed_file));
				return packed_file;
			}

			// Read the name
			unsigned short name_size = 0;
			Read<true>(file, &name_size, sizeof(name_size));
			name_size /= sizeof(wchar_t);
			ECS_STACK_CAPACITY_STREAM(wchar_t, current_name, 512);
			if (name_size > 512) {
				memset(&packed_file, 0, sizeof(packed_file));
				return packed_file;
			}

			Read<true>(file, current_name.buffer, name_size * sizeof(wchar_t));
			current_name.size = name_size;
			Stream<wchar_t> allocated_name = function::StringCopy(allocator, current_name);
			packed_file.packed_files[index] = allocated_name;

			for (unsigned int input_index = 0; input_index < current_count; input_index++) {
				Read<true>(file, &name_size, sizeof(name_size));
				name_size /= sizeof(wchar_t);
				if (name_size > 512) {
					memset(&packed_file, 0, sizeof(packed_file));
					return packed_file;
				}

				Read<true>(file, current_name.buffer, name_size * sizeof(wchar_t));
				current_name.size = name_size;
				allocated_name = function::StringCopy(allocator, current_name);
				ECS_ASSERT(!packed_file.lookup_table.Insert(index, allocated_name));
			}
		}

		return packed_file;
	}

	// -------------------------------------------------------------------------------------------------------------------

	Stream<void> UnpackFile(Stream<wchar_t> file, const PackedFile* packed_file, AllocatorPolymorphic allocator)
	{
		ResourceIdentifier identifier(file);

		size_t file_size = GetFileByteSize(packed_file->file_handle);
		if (file_size == 0) {
			return { nullptr, 0 };
		}
		uint2 file_offsets;
		if (packed_file->lookup_table.TryGetValue(identifier, file_offsets)) {
			size_t size_t_offset = file_offsets.x;
			size_t size_t_size = file_offsets.y;
			if (size_t_offset + size_t_size >= file_size) {
				// The values have been corrupted
				return { nullptr, 0 };
			}

			// Seek to that position
			size_t status = SetFileCursor(packed_file->file_handle, size_t_offset, ECS_FILE_SEEK_BEG);
			if (status == -1) {
				return { nullptr , 0 };
			}

			// Read the data now
			void* allocation = AllocateEx(allocator, size_t_size);
			if (allocation == nullptr) {
				return { nullptr, 0 };
			}

			unsigned int bytes_read = ReadFromFile(packed_file->file_handle, { allocation, size_t_size });
			if (bytes_read != file_offsets.y) {
				DeallocateEx(allocator, allocation);
				return { nullptr , 0 };
			}

			return { allocation, size_t_size };
		}

		return { nullptr, 0 };
	}

	// -------------------------------------------------------------------------------------------------------------------

	bool WriteMultiPackedFile(Stream<wchar_t> file, Stream<MultiPackedFileElement> elements)
	{
		size_t allocation_size = WriteMultiPackedFileSize(elements);
		void* buffer = nullptr;

		// Use the stack
		if (allocation_size < ECS_KB * 128) {
			buffer = ECS_STACK_ALLOC(allocation_size);
		}
		// Use the heap
		else {
			buffer = malloc(allocation_size);
		}

		uintptr_t ptr = (uintptr_t)buffer;
		WriteMultiPackedFile(&ptr, elements);

		bool success = WriteBufferToFileBinary(file, { buffer, allocation_size }) ==  ECS_FILE_STATUS_OK;
		if (allocation_size >= ECS_KB * 128) {
			// Free the heap allocation
			free(buffer);
		}

		return success;
	}

	// -------------------------------------------------------------------------------------------------------------------

	void WriteMultiPackedFile(uintptr_t* ptr, Stream<MultiPackedFileElement> elements)
	{
		ECS_ASSERT(elements.size < MULTI_PACKED_FILE_MAX_COUNT, "Too many packed files for multi packed file.");

		MultiPackedFileHeader header;
		header.version = MULTI_PACKED_FILE_VERSION;
		header.element_count = elements.size;

		uintptr_t initial_ptr = *ptr;
		Write<true>(ptr, &header, sizeof(header));

		for (size_t index = 0; index < elements.size; index++) {
			ECS_ASSERT(elements[index].input.size < MULTI_PACKED_FILE_MAX_INPUT_COUNT, "Too many input files per packed file when writing multi packed file.");

			// Write it only as a uint
			Write<true>(ptr, &elements[index].input.size, sizeof(unsigned int));
			// This will be written as a byte size, will be divided upon load
			WriteWithSizeShort<true>(ptr, elements[index].output.buffer, elements[index].output.size * sizeof(wchar_t));
			ECS_ASSERT(elements[index].output.size < 512, "Output file name too large when writing multi packed file.");

			for (size_t input_index = 0; input_index < elements[index].input.size; input_index++) {
				WriteWithSizeShort<true>(ptr, elements[index].input[input_index].buffer, elements[index].input[input_index].size);
				ECS_ASSERT(elements[index].input[input_index].size < 512, "Input file name too large when writing multi packed file.");
			}
		}

		uintptr_t final_ptr = *ptr;
		size_t difference = final_ptr - initial_ptr;
		// Encrypt the entire data with the total written size
		EncryptBuffer({ (void*)initial_ptr, difference }, difference);
	}

	// -------------------------------------------------------------------------------------------------------------------

	size_t WriteMultiPackedFileSize(Stream<MultiPackedFileElement> elements)
	{
		size_t total_size = sizeof(MultiPackedFileHeader);

		for (size_t index = 0; index < elements.size; index++) {
			total_size += elements[index].output.MemoryOf(elements[index].output.size) + sizeof(unsigned short) + sizeof(unsigned int);
			for (size_t input_index = 0; input_index < elements[index].input.size; input_index++) {
				total_size += sizeof(unsigned short) + sizeof(wchar_t) * elements[index].input[input_index].size;
			}
		}

		return total_size;
	}

	// -------------------------------------------------------------------------------------------------------------------

	MultiPackedFile MultiPackedFile::Copy(AllocatorPolymorphic allocator) const
	{
		MultiPackedFile result;

		HashTableCopy<false, true>(lookup_table, result.lookup_table, allocator);

		// Copy the stream now - can't use a deep copy because the call does a coallescing
		// allocation and the deallocate does not. (in the main case it is segragated)
		result.packed_files.Initialize(allocator, packed_files.size);
		for (size_t index = 0; index < packed_files.size; index++) {
			result.packed_files[index] = function::StringCopy(allocator, packed_files[index]);
		}
		
		return result;
	}

	// -------------------------------------------------------------------------------------------------------------------

	void MultiPackedFile::Deallocate(AllocatorPolymorphic allocator)
	{
		lookup_table.ForEachConst([&](unsigned int index, ResourceIdentifier identifier) {
			ECSEngine::Deallocate(allocator, identifier.ptr);
		});
		for (size_t index = 0; index < packed_files.size; index++) {
			ECSEngine::Deallocate(allocator, packed_files[index].buffer);
		}

		ECSEngine::Deallocate(allocator, packed_files.buffer);
		ECSEngine::Deallocate(allocator, lookup_table.GetAllocatedBuffer());
	}

	// -------------------------------------------------------------------------------------------------------------------

}