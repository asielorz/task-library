#include "thread_pool.hh"
#include <cassert>
#include <random>

atomic_flag_lock_guard::atomic_flag_lock_guard(std::atomic_flag & flag_) noexcept
	: flag(flag_)
	, locked(flag_.test_and_set() == false)
{}

atomic_flag_lock_guard::~atomic_flag_lock_guard()
{
	if (locked)
		flag.clear();
}

atomic_flag_lock_guard::operator bool() const noexcept
{
	return locked;
}

TaskQueue::TaskQueue(int queue_count)
	: queues(queue_count)
{
	assert(queue_count > 0);
}

auto TaskQueue::push_task(PolymorphicTask task) -> void
{
	int const preferred_index = round_robin_next_index;
	round_robin_next_index = (round_robin_next_index + 1) % number_of_queues();
	int const insertion_index = push_task(std::move(task), preferred_index);
	if (preferred_index != insertion_index)
		round_robin_next_index = (preferred_index + 1) % number_of_queues();
}

auto TaskQueue::push_task(PolymorphicTask task, int preferred_queue_index) -> int
{
	int const n = number_of_queues();
	for (int i = preferred_queue_index; true; i = (i + 1) % n)
	{
		if (queues[i].push(std::move(task)))
		{
			queued_tasks++;
			return i;
		}
	}
}

auto TaskQueue::pop_task(int preferred_queue_index) -> std::optional<PolymorphicTask>
{
	int const n = number_of_queues();

	while (queued_tasks > 0)
	{
		for (int i = 0; i < n; ++i)
		{
			int const index = (preferred_queue_index + i) % n;
			auto task = queues[index].pop();
			if (task)
			{
				queued_tasks--;
				return task;
			}
		}
	}
	return std::nullopt;
}

auto TaskQueue::LockQueue::push(PolymorphicTask && task) -> bool
{
	if (auto const g = atomic_flag_lock_guard(mutex))
	{
		queue.push(std::move(task));
		return true;
	}
	else return false;
}

auto TaskQueue::LockQueue::pop() -> std::optional<PolymorphicTask>
{
	if (auto const g = atomic_flag_lock_guard(mutex))
	{
		if (queue.empty())
			return std::nullopt;

		auto task = std::move(queue.front());
		queue.pop();
		return task;
	}
	else return std::nullopt;
}

namespace this_thread
{
	auto perform_task_for(TaskQueue & task_queue) -> bool
	{
		return perform_task_for(task_queue, std::random_device()() % task_queue.number_of_queues());
	}

	auto perform_task_for(TaskQueue & task_queue, int preferred_queue_index) -> bool
	{
		auto task = task_queue.pop_task(preferred_queue_index);
		if (task)
		{
			(*task)();
			return true;
		}
		else return false;
	}

	auto work_until_no_tasks_left_for(TaskQueue & task_queue) -> int
	{
		return work_until_no_tasks_left_for(task_queue, std::random_device()() % task_queue.number_of_queues());
	}

	auto work_until_no_tasks_left_for(TaskQueue & task_queue, int preferred_queue_index) -> int
	{
		int tasks_done = 0;
		while (true)
		{
			bool const work_done = perform_task_for(task_queue, preferred_queue_index);
			if (work_done)
				tasks_done++;
			else
				break;
		}
		return tasks_done;
	}
}

//******************************************************************************

WorkerThread::WorkerThread(WorkSource work_source)
	: thread(&WorkerThread::worker_main, std::ref(state), std::move(work_source))
{
	// Not sure if this is the best idea but thread creation is already rare 
	// and expensive and it is a good idea to always hold the invariant of 
	// (state != nullptr) <-> has a working thread.
	while (!state)
		std::this_thread::yield();
}

WorkerThread::WorkerThread(WorkerThread && other) noexcept
	: thread(std::move(other.thread))
	, state(std::exchange(other.state, nullptr))
{}

WorkerThread & WorkerThread::operator = (WorkerThread && other) noexcept
{
	join();
	thread = std::move(other.thread);
	state = std::exchange(other.state, nullptr);
	return *this;
}

WorkerThread::~WorkerThread()
{
	join();
}

auto WorkerThread::work_for(WorkSource source) -> void
{
	assert(state);
	auto const g = std::lock_guard(state->work_source_mutex);
	state->work_source = std::move(source);
	state->work_source_changed = true;
}

auto WorkerThread::join() -> void
{
	if (state)
	{
		state->stop_token = true;
		thread.join();
		state = nullptr;
	}
}

auto work(bool & stop_token, bool & work_source_changed, WorkerThread::WorkSource const & work_source)
{
	while (!work_source_changed)
	{
		auto task = work_source();
		if (task)
			(*task)();
		else if (stop_token)
			break;
		else
			std::this_thread::yield();
	}
}

auto WorkerThread::worker_main(WorkerState * & state_ptr, WorkSource initial_work_source) -> void
{
	WorkerState thread_state;
	state_ptr = &thread_state;
	thread_state.work_source = std::move(initial_work_source);

	while (true)
	{
		if (thread_state.stop_token && !thread_state.work_source_changed)
			break;

		WorkSource current_work_source;
		{
			auto const g = std::lock_guard(thread_state.work_source_mutex);
			current_work_source = thread_state.work_source;
			thread_state.work_source_changed = false;
		}
		work(thread_state.stop_token, thread_state.work_source_changed, current_work_source);
	}
}

auto make_workers_for_queue(TaskQueue & task_queue) -> std::vector<WorkerThread>
{
	return make_workers_for_queue(task_queue, task_queue.number_of_queues());
}

auto make_workers_for_queue(TaskQueue & queue, int worker_count) -> std::vector<WorkerThread>
{
	std::vector<WorkerThread> workers;
	workers.reserve(worker_count);
	for (int i = 0; i < worker_count; ++i)
		workers.emplace_back(as_work_source(queue, i));
	return workers;
}

auto assign_thread_pool_to_workers(std::span<WorkerThread> workers, TaskQueue & task_queue) -> void
{
	int const n = static_cast<int>(workers.size());
	for (int i = 0; i < n; ++i)
		workers[i].work_for(as_work_source(task_queue, i));
}
