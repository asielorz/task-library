#pragma once

#include "polymorphic_task.hh"
#include <vector>
#include <mutex>
#include <atomic>
#include <queue>
#include <functional>
#include <optional>
#include <span>

struct atomic_flag_lock_guard
{
	[[nodiscard]] explicit atomic_flag_lock_guard(std::atomic_flag & flag_) noexcept;
	~atomic_flag_lock_guard();

	atomic_flag_lock_guard(atomic_flag_lock_guard const &) = delete;
	atomic_flag_lock_guard & operator = (atomic_flag_lock_guard const &) = delete;

	explicit operator bool() const noexcept;

private:
	std::atomic_flag & flag;
	bool locked;
};

struct TaskQueue
{
	explicit TaskQueue(int queue_count);

	auto push_task(PolymorphicTask task) -> void;
	auto push_task(PolymorphicTask task, int preferred_queue_index) -> int;

	auto pop_task(int preferred_queue_index) -> std::optional<PolymorphicTask>;

	// To make it satisfy the executor concept.
	auto run_task(PolymorphicTask task) -> void { push_task(std::move(task)); }

	auto number_of_queues() const noexcept -> int { return static_cast<int>(queues.size()); }
	auto number_of_queued_tasks() const noexcept -> int { return queued_tasks; }
	auto has_work_queued() const noexcept -> bool { return number_of_queued_tasks() > 0; }

private:
	struct LockQueue
	{
		auto push(PolymorphicTask && task) -> bool;
		auto pop() -> std::optional<PolymorphicTask>;

	private:
		std::queue<PolymorphicTask> queue;
		std::atomic_flag mutex;
	};

	std::vector<LockQueue> queues;
	int round_robin_next_index = 0;
	std::atomic<int> queued_tasks;
};

inline auto as_work_source(TaskQueue & queue, int preferred_queue_index);

namespace this_thread
{
	auto perform_task_for(TaskQueue & task_queue) -> bool;
	auto perform_task_for(TaskQueue & task_queue, int preferred_queue_index) -> bool;

	auto work_until_no_tasks_left_for(TaskQueue & task_queue) -> int;
	auto work_until_no_tasks_left_for(TaskQueue & task_queue, int preferred_queue_index) -> int;
}

struct WorkerThread
{
	using WorkSource = std::function<std::optional<PolymorphicTask>()>;

	WorkerThread(WorkSource work_source);
	~WorkerThread();

	WorkerThread(WorkerThread const &) = delete;
	WorkerThread & operator = (WorkerThread const &) = delete;

	WorkerThread(WorkerThread && other) noexcept;
	WorkerThread & operator = (WorkerThread &&) noexcept;

	auto work_for(WorkSource source) -> void;

	auto join() -> void;

	auto joinable() const noexcept -> bool { return state != nullptr; }
	explicit operator bool() const noexcept { return joinable(); }

private:
	struct WorkerState
	{
		std::mutex work_source_mutex;
		WorkSource work_source;
		bool stop_token = false;
		bool work_source_changed = false;
	};

	static auto worker_main(WorkerState * & state_ptr, WorkSource initial_work_source) -> void;

	std::thread thread;
	WorkerState * state = nullptr;
};

auto make_workers_for_queue(TaskQueue & task_queue) -> std::vector<WorkerThread>;
auto make_workers_for_queue(TaskQueue & task_queue, int worker_count) -> std::vector<WorkerThread>;
auto assign_thread_pool_to_workers(std::span<WorkerThread> workers, TaskQueue & task_queue) -> void;

#include "thread_pool.inl"
