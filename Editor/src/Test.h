#pragma once
//#include "ECSEngine.h"
//
//using namespace ECSEngine;
//using namespace ECSEngine::Tools;
//
////[=](float* directx_values, float* ecs_values) { 
////	for (size_t index = 0; index < matrix_count; index++) { 
////		for (size_t subindex = 0; subindex < validate_count; subindex++) { 
////			assert(fabs(directx_values[index * 16 + subindex] - ecs_values[index * 16 + subindex]) < epsilon); 
////		} 
////	}  
////}
//
//template<typename DirectXFunction, typename ECSFunction, typename ScalarFunction>
//void TestMatrices(size_t max_iterations, size_t matrix_count, MemoryManager& memory_manager, Timer& timer,
//	DirectXFunction&& directx_function, ECSFunction&& ecs_function, ScalarFunction&& scalar_function) {
//	size_t* directx_times = (size_t*)memory_manager.Allocate(sizeof(size_t) * max_iterations);
//	size_t* ecs_times = (size_t*)memory_manager.Allocate(sizeof(size_t) * max_iterations);
//	size_t* scalar_times = (size_t*)memory_manager.Allocate(sizeof(size_t) * max_iterations);
//
//	for (size_t iteration_count = 0; iteration_count < max_iterations; iteration_count++) {
//		float* matrix_values = (float*)memory_manager.Allocate(sizeof(float) * 16 * matrix_count, 64);
//		void* dummy_allocation = memory_manager.Allocate(100000);
//		float* second_matrix_values = (float*)memory_manager.Allocate(sizeof(float) * 16 * matrix_count, 64);
//		void* dummy_allocation2 = memory_manager.Allocate(100000);
//		float* third_matrix_values = (float*)memory_manager.Allocate(sizeof(float) * 16 * matrix_count, 64);
//		for (size_t index = 0; index < 16 * matrix_count; index++) {
//			matrix_values[index] = static_cast<float>(rand()) / 467373.46f;
//			second_matrix_values[index] = matrix_values[index];
//			third_matrix_values[index] = matrix_values[index];
//		}
//
//		timer.SetMarker();
//		directx_function(matrix_values);
//		size_t total_time = timer.GetDurationSinceMarker_us();
//		directx_times[iteration_count] = total_time;
//		char converted_number[128];
//		converted_number[0] = '\n';
//		Stream<char> temp_number = Stream<char>(converted_number, 1);
//		function::ConvertIntToChars(temp_number, total_time);
//		wchar_t* converted_string = function::ConvertASCIIToWide(converted_number);
//		OutputDebugString(converted_string);
//
//		timer.SetMarker();
//		ecs_function(second_matrix_values);
//		total_time = timer.GetDurationSinceMarker_us();
//		ecs_times[iteration_count] = total_time;
//		converted_number[0] = ' ';
//		temp_number = Stream<char>(converted_number, 1);
//		function::ConvertIntToChars(temp_number, total_time);
//		converted_string = function::ConvertASCIIToWide(converted_number);
//		OutputDebugString(converted_string);
//
//		timer.SetMarker();
//		scalar_function(third_matrix_values);
//		total_time = timer.GetDurationSinceMarker_us();
//		scalar_times[iteration_count] = total_time;
//		temp_number[0] = ' ';
//		temp_number.size = 1;
//		function::ConvertIntToChars(temp_number, total_time);
//		converted_string = function::ConvertASCIIToWide(converted_number);
//		OutputDebugString(converted_string);
//
//		memory_manager.Deallocate(matrix_values);
//		memory_manager.Deallocate(second_matrix_values);
//		memory_manager.Deallocate(dummy_allocation);
//		memory_manager.Deallocate(dummy_allocation2);
//		memory_manager.Deallocate(third_matrix_values);
//	}
//
//	char temp_characters[128];
//	size_t total_directx_time = 0;
//	size_t total_ecs_time = 0;
//	size_t total_scalar_time = 0;
//	for (size_t index = 0; index < max_iterations; index++) {
//		total_directx_time += directx_times[index];
//		total_ecs_time += ecs_times[index];
//		total_scalar_time += scalar_times[index];
//	}
//
//	total_directx_time /= max_iterations;
//	total_ecs_time /= max_iterations;
//	total_scalar_time /= max_iterations;
//
//	temp_characters[0] = '\n';
//	Stream<char> temp_stream = Stream<char>(temp_characters, 1);
//	function::ConvertIntToChars(temp_stream, total_directx_time);
//
//	temp_stream.Add(' ');
//	function::ConvertIntToChars(temp_stream, total_ecs_time);
//
//	temp_stream.Add(' ');
//	function::ConvertIntToChars(temp_stream, total_scalar_time);
//	wchar_t* w_string = function::ConvertASCIIToWide(temp_characters);
//	OutputDebugString(w_string);
//
//	memory_manager.Deallocate(directx_times);
//	memory_manager.Deallocate(ecs_times);
//	memory_manager.Deallocate(scalar_times);
//}
//
//template<typename Function>
//void ValidateFunction(size_t max_iterations, size_t matrix_count, MemoryManager& memory_manager, Timer& timer, Function&& validate_function) {
//	for (size_t iteration_count = 0; iteration_count < max_iterations; iteration_count++) {
//		float* matrix_values = (float*)memory_manager.Allocate(sizeof(float) * 16 * matrix_count, 64);
//		void* dummy_allocation = memory_manager.Allocate(100000);
//		float* second_matrix_values = (float*)memory_manager.Allocate(sizeof(float) * 16 * matrix_count, 64);
//		for (size_t index = 0; index < 16 * matrix_count; index++) {
//			//srand(timer.GetDuration_ns());
//			matrix_values[index] = static_cast<float>(rand()) / 11263.46f - 2.52f;
//			second_matrix_values[index] = matrix_values[index];
//		}
//
//		validate_function(matrix_values, second_matrix_values);
//
//		memory_manager.Deallocate(matrix_values);
//		memory_manager.Deallocate(second_matrix_values);
//		memory_manager.Deallocate(dummy_allocation);
//	}
//}
//
//void TestMatricesMultiply(MemoryManager& memory_manager, Timer& timer) {
//	constexpr size_t max_iterations = 100;
//
//	constexpr size_t matrix_count = 2000;
//	constexpr size_t subindex_count = 50;
//
//	auto directx_function = [&](float* matrix_values) {
//		DirectX::XMMATRIX matrix;
//		DirectX::XMMATRIX second_matrix;
//		DirectX::XMMATRIX matrix_result;
//
//		DirectX::XMMATRIX matrixu;
//		for (size_t index = 0; index < matrix_count / 2; index++) {
//			matrix = DirectX::XMMATRIX(matrix_values + index * 32);
//			second_matrix = DirectX::XMMATRIX(matrix_values + index * 32 + 16);
//			matrix_result = DirectX::XMMatrixMultiply(matrix, second_matrix);
//			matrix_result = DirectX::XMMatrixTranspose(matrix_result);
//			for (size_t subindex = 0; subindex < subindex_count; subindex++) {
//				//matrix_result = DirectX::XMMatrixMultiply(matrix, matrix_result);
//				matrix_result = DirectX::XMMatrixMultiply(second_matrix, matrix_result);
//				matrix_result = DirectX::XMMatrixTranspose(matrix_result);
//			}
//			for (size_t subindex = 0; subindex < 4; subindex++) {
//				_mm_store_ps(matrix_values + index * 32 + subindex * 4, matrix_result.r[subindex]);
//			}
//		}
//	};
//
//	auto ecs_function = [&](float* second_matrix_values) {
//		ECSEngine::Matrix ecs_matrix;
//		ECSEngine::Matrix ecs_second_matrix;
//		ECSEngine::Matrix ecs_result;
//
//		ECSEngine::Matrix ecs_matrixu;
//		for (size_t index = 0; index < matrix_count / 2; index++) {
//			ecs_matrix.LoadAligned(second_matrix_values + index * 32);
//			ecs_second_matrix.LoadAligned(second_matrix_values + index * 32 + 16);
//			ecs_result = ECSEngine::MatrixMultiply(ecs_matrix, ecs_second_matrix);
//			ecs_result = ECSEngine::MatrixTranspose(ecs_result);
//			for (size_t subindex = 0; subindex < subindex_count; subindex++) {
//				//ecs_result = ECSEngine::MatrixMultiplyInlined(ecs_matrix, ecs_result);
//				ecs_result = ECSEngine::MatrixMultiply(ecs_second_matrix, ecs_result);
//				ecs_result = ECSEngine::MatrixTranspose(ecs_result);
//			}
//
//			ecs_result.StoreAligned(second_matrix_values + index * 32);
//		}
//	};
//
//	TestMatrices(max_iterations, matrix_count, memory_manager, timer, directx_function, ecs_function, [](float* third_matrix_values) {});
//}
//
//void ValidateDeterminant(MemoryManager& manager, Timer& timer) {
//	constexpr size_t max_iterations = 100;
//	constexpr size_t matrix_count = 5000; 
//
//	constexpr float epsilon = 0.000001f;
//	constexpr size_t validate_count = 1;
//
//	auto function = [=](float* directx_values, float* ecs_values) {
//		for (size_t index = 0; index < matrix_count; index++) {
//			auto vector = DirectX::XMMatrixDeterminant(DirectX::XMMATRIX(directx_values + index * 16));
//			_mm_store_ps(directx_values + index * 16, vector);
//
//			Matrix matrix = Matrix(ecs_values + index * 16);
//			ecs_values[index * 16] = ECSEngine::MatrixDeterminant(matrix);
//
//			for (size_t subindex = 0; subindex < validate_count; subindex++) {
//				assert(fabs(directx_values[index * 16 + subindex] - ecs_values[index * 16 + subindex]) < epsilon);
//			}
//		}
//	};
//
//	ValidateFunction(max_iterations, matrix_count, manager, timer, function);
//}
//
//void ValidateTranspose(MemoryManager& manager, Timer& timer) {
//	constexpr size_t max_iterations = 100;
//	constexpr size_t matrix_count = 5000;
//	constexpr float epsilon = 0.0001f;
//	constexpr size_t validate_count = 16;
//
//	auto function = [=](float* directx_values, float* ecs_values) {
//		for (size_t index = 0; index < matrix_count; index++) {
//			DirectX::XMMATRIX matrix = DirectX::XMMATRIX(directx_values + index * 16);
//			matrix = DirectX::XMMatrixTranspose(matrix);
//			_mm_store_ps(directx_values + index * 16, matrix.r[0]);
//			_mm_store_ps(directx_values + index * 16 + 4, matrix.r[1]);
//			_mm_store_ps(directx_values + index * 16 + 8, matrix.r[2]);
//			_mm_store_ps(directx_values + index * 16 + 12, matrix.r[3]);
//
//			ECSEngine::Matrix ecs_matrix = ECSEngine::Matrix(ecs_values + index * 16);
//			ecs_matrix = ECSEngine::MatrixTranspose(ecs_matrix);
//			ecs_matrix.StoreAligned(ecs_values + index * 16);
//
//			for (size_t subindex = 0; subindex < validate_count; subindex++) {
//				assert(fabs(directx_values[index * 16 + subindex] - ecs_values[index * 16 + subindex]) < epsilon);
//			}
//		}
//	};
//
//	ValidateFunction(max_iterations, matrix_count, manager, timer, function);
//}
//
//void ValidateInverse(MemoryManager& manager, Timer& timer) {
//	constexpr size_t max_iterations = 100;
//	constexpr size_t matrix_count = 5000;
//
//	constexpr float epsilon = 0.08f;
//	constexpr size_t validate_count = 16;
//
//	auto function = [=](float* directx_values, float* ecs_values) {
//		for (size_t index = 0; index < matrix_count; index++) {
//			alignas(32) float values[48];
//
//			DirectX::XMMATRIX matrix = DirectX::XMMATRIX(directx_values + index * 16);
//			DirectX::XMVECTOR vec;
//			DirectX::XMMATRIX inverse_matrix = DirectX::XMMatrixInverse(&vec, matrix);
//			DirectX::XMMATRIX multi = matrix * inverse_matrix;
//			_mm_store_ps(values + 16, multi.r[0]);
//			_mm_store_ps(values + 20, multi.r[1]);
//			_mm_store_ps(values + 24, multi.r[2]);
//			_mm_store_ps(values + 28, multi.r[3]);
//
//			ECSEngine::Matrix ecs_matrix = ECSEngine::Matrix(ecs_values + index * 16);
//			float determinant_value = ECSEngine::MatrixDeterminant(ecs_matrix);
//			float inverse_determinant = 1.0f / determinant_value;
//			ECSEngine::Matrix ecs_inverse_matrix = ECSEngine::MatrixInversePrecise(ecs_matrix);
//			//ecs_inverse_matrix.StoreAligned(values);
//			ECSEngine::Matrix multiplied = ECSEngine::MatrixMultiply(ecs_inverse_matrix, ecs_matrix);
//			multiplied.StoreAligned(values);
//			alignas(32) float tempu[16];
//			ecs_inverse_matrix.StoreAligned(tempu);
//			DirectX::XMMATRIX d_inverse = DirectX::XMMATRIX(tempu);
//			DirectX::XMMATRIX d_inverse_with_their = d_inverse * matrix;
//			_mm_store_ps(values + 32, d_inverse_with_their.r[0]);
//			_mm_store_ps(values + 36, d_inverse_with_their.r[1]);
//			_mm_store_ps(values + 40, d_inverse_with_their.r[2]);
//			_mm_store_ps(values + 44, d_inverse_with_their.r[3]);
//
//			/*for (size_t subindex = 0; subindex < validate_count; subindex++) {
//				assert(fabs(values[subindex] - values[subindex + 16]) < epsilon);
//			}*/
//			assert(fabs(values[0] - 1.0f) < epsilon);
//			assert(fabs(values[1]) < epsilon);
//			assert(fabs(values[2]) < epsilon);
//			assert(fabs(values[3]) < epsilon);
//			assert(fabs(values[4]) < epsilon);
//			assert(fabs(values[5] - 1.0f) < epsilon);
//			assert(fabs(values[6]) < epsilon);
//			assert(fabs(values[7]) < epsilon);
//			assert(fabs(values[8]) < epsilon);
//			assert(fabs(values[9]) < epsilon);
//			assert(fabs(values[10] - 1.0f) < epsilon);
//			assert(fabs(values[11]) < epsilon);
//			assert(fabs(values[12]) < epsilon);
//			assert(fabs(values[13]) < epsilon);
//			assert(fabs(values[14]) < epsilon);
//			assert(fabs(values[15] - 1.0f) < epsilon);
//		}
//	};
//
//	ValidateFunction(max_iterations, matrix_count, manager, timer, function);
//}
//
//void TestMatrixDeterminant(MemoryManager& manager, Timer& timer) {
//	constexpr size_t max_iterations = 100;
//	constexpr size_t matrix_count = 5000;
//	constexpr size_t subindex_count = 50;
//
//	auto directx_function = [=](float* values) {
//		for (size_t index = 0; index < matrix_count; index++) {
//			alignas(16) float temp_memory[4];
//			float value = 0.0f;
//			for (size_t subindex = 0; subindex < subindex_count; subindex++) {
//				DirectX::XMMATRIX matrix = DirectX::XMMATRIX(values + subindex * 16 + index * subindex_count * 16);
//				matrix *= 1.25f;
//				auto vector = DirectX::XMMatrixDeterminant(matrix);
//				_mm_store_ps(temp_memory, vector);
//				value += temp_memory[0];
//			}
//			values[index * subindex_count * 16] = value;
//		}
//	};
//
//	auto ecs_function = [=](float* values) {
//		for (size_t index = 0; index < matrix_count; index++) {
//			float determinant_value = 0.0f;
//			for (size_t subindex = 0; subindex < subindex_count; subindex++) {
//				Matrix matrix = Matrix(values + subindex * 16 + index * subindex_count * 16);
//				matrix *= 1.25f;
//				determinant_value += MatrixDeterminant(matrix);
//			}
//			values[index * subindex_count * 16] = determinant_value;
//		}
//	};
//
//	auto scalar_function = [=](float* values) {
//
//	};
//
//	TestMatrices(max_iterations, matrix_count, manager, timer, directx_function, ecs_function, scalar_function);
//}
//
//void TestMatrixInverse(MemoryManager& manager, Timer& timer) {
//	constexpr size_t max_iterations = 100;
//	constexpr size_t matrix_count = 5000;
//	constexpr size_t subindex_count = 50;
//
//	auto directx_function = [=](float* directx_values) {
//		for (size_t index = 0; index < matrix_count; index++) {
//			DirectX::XMMATRIX matrix = DirectX::XMMATRIX(directx_values + index * 16);
//			DirectX::XMVECTOR vec;
//			for (size_t subindex = 0; subindex < subindex_count; subindex++) {
//				matrix = DirectX::XMMatrixInverse(&vec, matrix);
//			}
//			_mm_store_ps(directx_values + index * 16, matrix.r[0]);
//			_mm_store_ps(directx_values + index * 16 + 4, matrix.r[1]);
//			_mm_store_ps(directx_values + index * 16 + 8, matrix.r[2]);
//			_mm_store_ps(directx_values + index * 16 + 12, matrix.r[3]);
//		}
//	};
//
//	auto ecs_function = [=](float* ecs_values) {
//		for (size_t index = 0; index < matrix_count; index++) {
//			ECSEngine::Matrix matrix = ECSEngine::Matrix(ecs_values + index * 16);
//			for (size_t subindex = 0; subindex < subindex_count; subindex++) {
//				matrix = MatrixInverse(matrix);
//			}
//			matrix.StoreAligned(ecs_values + index * 16);
//		}
//	};
//
//	auto scalar_function = [=](float* scalar_values) {
//
//	};
//
//	TestMatrices(max_iterations, matrix_count, manager, timer, directx_function, ecs_function, scalar_function);
//}