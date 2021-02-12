#pragma once

#include "task.hh"
#include <future>
#include <optional>

// Returns a continuation function that stores the result of the task in the given future.
template <typename T>
auto store_in(std::future<T> & future);

// Launches a task in an executor and returns a future that will hold the result of the task.
template <is_task_executor TaskExecutor, is_task T>
auto async(TaskExecutor & executor, PackagedTask<T> t) -> std::future<task_result_type<T>>;

template <typename T>
auto is_ready(std::future<T> const & future) -> bool;

template <typename T>
auto get_if_ready(std::future<T> & future) -> std::optional<T>;

#include "async.inl"
