#include "ecspch.h"
#include "Statistic.h"
#include "../Utilities/Reflection/Reflection.h"

namespace ECSEngine {
	using namespace Reflection;

	size_t StatisticCountForReflectionType(const ReflectionManager* reflection_manager, Stream<char> type_name)
	{
		return StatisticCountForReflectionType(reflection_manager, reflection_manager->GetType(type_name));
	}

	Statistic<float>* FillStatisticsForReflectionType(
		const ReflectionManager* reflection_manager, 
		Stream<char> type_name, 
		const void* instance, 
		Statistic<float>* statistics
	)
	{
		return FillStatisticsForReflectionType(reflection_manager, reflection_manager->GetType(type_name), instance, statistics);
	}

	size_t StatisticCountForReflectionType(const ReflectionManager* reflection_manager, const ReflectionType* type)
	{
		size_t field_count = 0;
		for (size_t index = 0; index < type->fields.size; index++) {
			if (type->fields[index].info.basic_type != ReflectionBasicFieldType::UserDefined) {
				size_t basic_field_count = (size_t)BasicTypeComponentCount(type->fields[index].info.basic_type);
				if (type->fields[index].info.stream_type == ReflectionStreamFieldType::Basic) {
					field_count += basic_field_count;
				}
				else if (type->fields[index].info.stream_type == ReflectionStreamFieldType::BasicTypeArray) {
					field_count += basic_field_count * (size_t)type->fields[index].info.basic_type_count;
				}
				else {
					ECS_ASSERT(false, "Cannot get statistic count for reflected types with pointers or stream types");
				}
			}
			else {
				const ReflectionType* nested_type = reflection_manager->TryGetType(type->fields[index].definition);
				if (nested_type != nullptr) {
					field_count += StatisticCountForReflectionType(reflection_manager, nested_type);
				}
				else {
					ECS_ASSERT(false, "Cannot get statistic count for reflected types with enums, reflection container types or blittable types");
				}
			}
		}

		return field_count;
	}

	Statistic<float>* FillStatisticsForReflectionType(
		const ReflectionManager* reflection_manager, 
		const ReflectionType* type, 
		const void* instance, 
		Statistic<float>* statistics
	)
	{
		for (size_t index = 0; index < type->fields.size; index++) {
			if (type->fields[index].info.basic_type != ReflectionBasicFieldType::UserDefined) {
				unsigned char basic_count = BasicTypeComponentCount(type->fields[index].info.basic_type);
				if (type->fields[index].info.stream_type == ReflectionStreamFieldType::Basic) {
					double4 value = ConvertToDouble4FromBasic(type->fields[index].info.basic_type, type->GetField(instance, index));
					double* double_values = (double*)&value;
					for (unsigned char basic_index = 0; basic_index < basic_count; basic_index++) {
						statistics[basic_index].Add(double_values[basic_index]);
					}
					statistics += basic_count;
				}
				else if (type->fields[index].info.stream_type == ReflectionStreamFieldType::BasicTypeArray) {
					const void* array_start = type->GetField(instance, index);
					size_t basic_byte_size = GetBasicTypeArrayElementSize(type->fields[index].info);
					for (unsigned short array_index = 0; array_index < type->fields[index].info.basic_type_count; array_index++) {
						double4 value = ConvertToDouble4FromBasic(type->fields[index].info.basic_type, OffsetPointer(array_start, array_index * basic_byte_size));
						double* double_values = (double*)&value;
						for (unsigned char basic_index = 0; basic_index < basic_count; basic_index++) {
							statistics[basic_index].Add(double_values[basic_index]);
						}
						statistics += basic_count;
					}
				}
				else {
					ECS_ASSERT(false, "Cannot update statistics for reflection types with pointers or stream types");
				}
			}
			else {
				const ReflectionType* nested_type = reflection_manager->TryGetType(type->fields[index].definition);
				if (nested_type != nullptr) {
					statistics = FillStatisticsForReflectionType(reflection_manager, type, type->GetField(instance, index), statistics);
				}
				else {
					ECS_ASSERT(false, "Cannot update statistics for reflection types with enums, reflection container types or blittable types");
				}
			}
		}
		return statistics;
	}

}