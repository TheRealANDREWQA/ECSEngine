#include "ecspch.h"
#include "Benchmark.h"
#include "StreamUtilities.h"
#include "StringUtilities.h"
#include "ECSEngineReflectionMacros.h"

namespace ECSEngine {

#pragma region Example Use

        //Stream<char> names[] = {
        //	"SearchBytes",
        //	"FindFirstToken",
        //	"Std",
        //	"CString",
        //	"Loop"
        //};

        //typedef unsigned short t;
        //BenchmarkState benchmark_state(editor_state.EditorAllocator(), 5, nullptr, names);
        //benchmark_state.options.element_size = sizeof(t);
        //benchmark_state.options.max_step_count = 18;
        ////benchmark_state.options.timed_run = 50;

        //while (benchmark_state.KeepRunning()) {
        //	Stream<void> iteration_buffer = benchmark_state.GetIterationBuffer();

        //	Stream<t> ts = { iteration_buffer.buffer, iteration_buffer.size / sizeof(t) };
        //	//MakeSequence(ts);
        //	char* it = (char*)iteration_buffer.buffer;
        //	it[iteration_buffer.size - 1] = '\0';
        //	for (size_t index = 0; index < iteration_buffer.size - 1; index++) {
        //		if (it[index] > 1) {
        //			it[index] = 'a';
        //		}
        //		else {
        //			it[index] = 'b';
        //		}
        //	}
        //	//t ptr = (t)ts.buffer;
        //	t ptr = -1;

        //	while (benchmark_state.Run()) {
        //		Stream<void> current_buffer = benchmark_state.GetCurrentBuffer();
        //		size_t indexu = SearchBytes(current_buffer.buffer, current_buffer.size / sizeof(ptr), (size_t)ptr);
        //		benchmark_state.DoNotOptimize(indexu);
        //	}

        //	while (benchmark_state.Run()) {
        //		Stream<void> current_buffer = benchmark_state.GetCurrentBuffer();
        //		auto resultu = FindFirstToken(Stream<char>(current_buffer.buffer, current_buffer.size), { &ptr, sizeof(ptr) });
        //		benchmark_state.DoNotOptimize(resultu.buffer == nullptr ? -1 : resultu.size);
        //	}

        //	while (benchmark_state.Run()) {
        //		Stream<void> current_buffer = benchmark_state.GetCurrentBuffer();
        //		std::string_view view = { (const char*)current_buffer.buffer, current_buffer.size };
        //		auto resultuu = view.find((const char*)&ptr, sizeof(ptr));
        //		benchmark_state.DoNotOptimize(resultuu);
        //	}

        //	char null_terminated[16];
        //	memcpy(null_terminated, &ptr, sizeof(ptr));
        //	null_terminated[sizeof(ptr)] = '\0';
        //	while (benchmark_state.Run()) {
        //		Stream<void> current_buffer = benchmark_state.GetCurrentBuffer();
        //		const char* c_string = (const char*)current_buffer.buffer;
        //		const char* result = strstr(c_string, null_terminated);
        //		benchmark_state.DoNotOptimize((size_t)result);
        //	}

        //	while (benchmark_state.Run()) {
        //		Stream<void> current_buffer = benchmark_state.GetCurrentBuffer();

        //		Stream<t> ts = { current_buffer.buffer, current_buffer.size / sizeof(t) };
        //		size_t subindex = 0;
        //		for (size_t i = 0; i < ts.size; i++) {
        //			if (ts[i] == ptr) {
        //				subindex = i;
        //				break;
        //			}
        //		}
        //		benchmark_state.DoNotOptimize(subindex);
        //	}
        //}

        //ECS_STACK_CAPACITY_STREAM(char, bench_string, ECS_KB * 64);
        //benchmark_state.GetString(bench_string, false);
        //bench_string[bench_string.size] = '\0';
        //OutputDebugStringA(bench_string.buffer);

#pragma endregion


    BenchmarkState::BenchmarkState(AllocatorPolymorphic allocator, size_t functor_count, const BenchmarkOptions* _options, Stream<char>* _names)
    {
        durations.Initialize(allocator, 0, functor_count);
        names = _names;
        
        if (_options != nullptr) {
            memcpy(&options, _options, sizeof(options));
        }
        else {
            options = BenchmarkOptions{};
        }

        options.iteration_count = options.iteration_count == 0 ? 10 : options.iteration_count;

        unsigned int capacity = options.timed_run == 0 ? (options.max_step_count - options.min_step_count) * options.iteration_count : 0;
        for (size_t index = 0; index < functor_count; index++) {
            durations[index].Initialize(allocator, capacity);
        }
    }

    Stream<void> BenchmarkState::GetIterationBuffer()
    {
        size_t step_index = options.step_index + options.min_step_count - 1;

        float buffer_size = pow(options.buffer_step_jump, step_index);
        size_t int_size = (size_t)buffer_size;
        int_size *= options.element_size;
        iteration_buffer = { Malloc(int_size), int_size };
        return iteration_buffer;
    }

    Stream<void> BenchmarkState::GetCurrentBuffer() {
        return current_buffer;
    }

    bool BenchmarkState::KeepRunning()
    {
        options.step_index++;
        bool keep_running = options.step_index <= (options.max_step_count - options.min_step_count + 1);

        if (keep_running) {
            if (options.timed_run != 0) {
                for (size_t index = 0; index < durations.capacity; index++) {
                    durations[index].Add((size_t)0);
                }
            }
        }

        return keep_running;
    }

    void BenchmarkState::GetString(CapacityStream<char>& report, bool verbose) const
    {
        size_t steps = options.max_step_count - options.min_step_count + 1;
        size_t total_iterations = options.iteration_count * steps;

        ECS_STACK_CAPACITY_STREAM_DYNAMIC(unsigned int, iteration_values, options.iteration_count);
        ECS_STACK_CAPACITY_STREAM_DYNAMIC(unsigned int, new_valid_values, iteration_values.size);
        for (size_t index = 0; index < durations.capacity; index++) {
            report.AddStream("Benchmark Run:");
            if (names != nullptr) {
                report.Add(' ');
                report.AddStream(names[index]);
            }
            report.Add('\n');
            
            auto get_value = [&](size_t step_index, size_t iteration_index) {
                return durations[index][step_index * options.iteration_count + iteration_index];
            };

            if (options.timed_run == 0) {
                ECS_ASSERT(durations[index].size == total_iterations);
            }
            else {
                ECS_ASSERT(durations[index].size == options.max_step_count - options.min_step_count + 1);
            }

            for (size_t step_index = 0; step_index < steps; step_index++) {
                if (options.timed_run == 0) {
                    // Iterations. We need to eliminate the big spikes by repeatedly removing the values outside the tolerancy threshold
                    iteration_values.size = options.iteration_count;
                    MakeSequence(iteration_values);

                    unsigned int valid_count = 0;
                    size_t median = 0;

                    // Remove all the values which are egregious
                    for (size_t iteration = 0; iteration < options.iteration_count; iteration++) {
                        unsigned int difference_count = 0;
                        for (unsigned int subiteration = 0; subiteration < iteration_values.size; subiteration++) {
                            size_t value = get_value(step_index, iteration);
                            size_t subiteration_index = iteration_values[subiteration];
                            if (subiteration_index != iteration) {
                                size_t second_value = get_value(step_index, subiteration_index);
                                if ((value > second_value * 2) || (value < second_value / 2)) {
                                    difference_count++;
                                }
                            }
                        }

                        if (difference_count > options.iteration_count / 2) {
                            // Eliminate it, it's a spike
                            // Find the index where it is located
                            for (unsigned int subindex = 0; subindex < iteration_values.size; subindex++) {
                                if (iteration_values[subindex] == iteration) {
                                    iteration_values.RemoveSwapBack(subindex);
                                    break;
                                }
                            }
                        }
                    }

                    while (valid_count != iteration_values.size) {
                        median = 0;

                        size_t min = 0;
                        for (size_t iteration = 0; iteration < iteration_values.size; iteration++) {
                            median += get_value(step_index, iteration_values[iteration]);
                        }

                        median /= iteration_values.size;

                        float float_median = (float)median;
                        // Eliminate the big spikes
                        new_valid_values.size = 0;
                        for (size_t iteration = 0; iteration < iteration_values.size; iteration++) {
                            size_t value = get_value(step_index, iteration_values[iteration]);
                            size_t difference = (size_t)abs((int64_t)(median - value));
                            if ((float)difference / float_median * 100.0f <= options.spike_tolerancy) {
                                new_valid_values.Add(iteration_values[iteration]);
                            }
                        }

                        valid_count = new_valid_values.size;
                        iteration_values.CopyOther(new_valid_values);
                    }

                    // Recalculate the median
                    median = 0;
                    size_t lowest_valid = 100000000000000;
                    size_t highest_valid = 0;
                    for (size_t iteration = 0; iteration < iteration_values.size; iteration++) {
                        size_t value = get_value(step_index, iteration_values[iteration]);
                        lowest_valid = min(lowest_valid, value);
                        highest_valid = max(highest_valid, value);
                        median += value;
                    }

                    if (iteration_values.size > 0) {
                        median /= iteration_values.size;
                    }

                    auto format_value = [](size_t value, const char*& time_unit) {
                        if (value > 10'000'000) {
                            value /= 1'000'000;
                            time_unit = "ms";
                        }
                        else if (value > 10'000) {
                            value /= 1'000;
                            time_unit = "us";
                        }
                        else {
                            time_unit = "ns";
                        }
                        return value;
                    };

                    // Present as nanoseconds
                    const char* time_unit;
                    median = format_value(median, time_unit);

                    const char* highest_unit;
                    highest_valid = format_value(highest_valid, highest_unit);

                    const char* lowest_unit;
                    lowest_valid = format_value(lowest_valid, lowest_unit);

                    float step_elements = pow(options.buffer_step_jump, step_index + options.min_step_count);
                    size_t elements = (size_t)step_elements;

                    FormatString(report, "Step {#} - elements {#} - accepted iterations {#}: {#} {#} (lowest valid {#} {#}, highest valid {#} {#})", step_index, elements, iteration_values.size, median, time_unit, lowest_valid, lowest_unit, highest_valid, highest_unit);
                    if (verbose) {
                        // Print all values
                        report.AddStream(". Values: ");
                        for (size_t iteration = 0; iteration < options.iteration_count; iteration++) {
                            const char* iteration_time_unit;
                            size_t value = format_value(get_value(step_index, iteration), iteration_time_unit);
                            ConvertIntToChars(report, value);
                            bool is_valid = false;
                            for (unsigned int valid_index = 0; valid_index < iteration_values.size; valid_index++) {
                                if (iteration_values[valid_index] == iteration) {
                                    is_valid = true;
                                    break;
                                }
                            }

                            report.Add(' ');
                            report.AddStream(iteration_time_unit);
                            report.Add(' ');
                            report.Add('(');
                            report.Add(is_valid ? 'V' : 'S');
                            report.Add(')');
                            report.Add(' ');
                        }
                    }

                    report.Add('\n');
                }
                else {
                    // Print the number of times only
                    FormatString(report, "Step {#} - ran {#}.\n", step_index, durations[index][step_index]);
                }
            }
        }

        report.AssertCapacity();
    }

    bool BenchmarkState::Run() {
        bool should_run;

        bool timed_end_timer = false;
        if (options.timed_run) {
            if (options.timed_run_elapsed == -1) {
                EndTimer();
                timed_end_timer = true;
            }
            // Timed run elapsed is in nanoseconds
            should_run = options.timed_run_elapsed / 1'000'000 < options.timed_run;        
        }
        else {
            options.iteration_index++;
            should_run = options.iteration_index <= options.iteration_count;
        }

        if (!should_run) {
            EndTimer();

            // Divide the count by the elapsed count
            if (options.timed_run) {
                size_t elapsed_milliseconds = options.timed_run_elapsed / 1'000'000;
                size_t last_index = durations[durations.size].size - 1;
                durations[durations.size][last_index] /= elapsed_milliseconds;
            }

            options.timed_run_elapsed = -1;
            options.iteration_index = 0;
            if (current_buffer.size > 0) {
                // Allocate a new one such that is different
                Free(current_buffer.buffer);
                current_buffer.size = 0;
            }

            // Advance to the next functor
            durations.size++;
            if (durations.size == durations.capacity) {
                durations.size = 0;
                if (iteration_buffer.size > 0) {
                    Free(iteration_buffer.buffer);
                    iteration_buffer.size = 0;
                }
            }
        }
        else {
            if ((options.timed_run == 0 && options.iteration_index > 1) || (options.timed_run != 0 && !timed_end_timer)) {
                EndTimer();
            }
            StartTimer();
        }
        return should_run;
    }

    void BenchmarkState::StartTimer()
    {
        if (current_buffer.size == 0) {
            // Allocate it
            current_buffer.buffer = Malloc(iteration_buffer.size);
            current_buffer.size = iteration_buffer.size;
            memcpy(current_buffer.buffer, iteration_buffer.buffer, iteration_buffer.size);
        }

        timer.SetMarker();
    }

    void BenchmarkState::EndTimer()
    {
        size_t duration = timer.GetDurationSinceMarker(ECS_TIMER_DURATION_NS);
        if (options.timed_run) {
            size_t last = durations[durations.size].size - 1;
            durations[durations.size][last]++;
            options.timed_run_elapsed += duration == 0 ? 50 : duration;
        }
        else {
            durations[durations.size].Add(duration);
        }
    }

    void BenchmarkState::DoNotOptimize(size_t value) {}

    void BenchmarkState::DoNotOptimizeFloat(float float_value) {}

}
