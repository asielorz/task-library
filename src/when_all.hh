#pragma once

#include "task.hh"
#include <tuple>
#include <optional>
#include <atomic>
#include <memory>

template <typename F, is_task_executor Executor, typename ... ArgProducers> requires std::invocable<F, result_type<ArgProducers>...>
[[nodiscard]] auto when_all(F f, Executor & executor, ArgProducers ... arg_producers);

#include "when_all.inl"
