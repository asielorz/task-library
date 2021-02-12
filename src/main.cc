#include "thread_pool.hh"
#include "task.hh"
#include "async.hh"
#include "when_all.hh"
#include <chrono>

#define CATCH_CONFIG_RUNNER
#include "catch/catch.hpp"

using namespace std::literals;

int main()
{
	int const tests_result = Catch::Session().run();
	if (tests_result != 0)
		system("pause");
}

TEST_CASE("Can push a task to a queue and run it later")
{
	auto task_queue = TaskQueue(1);

	int i = 0;
	task_queue.push_task([&i]() { i = 5; });

	REQUIRE(i == 0); // task hasn't run yet

	this_thread::work_until_no_tasks_left_for(task_queue);

	REQUIRE(i == 5); // task has run now
}

TEST_CASE("A continuation can be attached to a task and will receive the result of the task")
{
	auto task_queue = TaskQueue(1);

	int i = 0;
	auto t = task([]() { return 5; })
		.then(continuation([&i](int x) { i = x; }, task_queue));

	task_queue.push_task(std::move(t));

	REQUIRE(i == 0); // task hasn't run yet

	this_thread::work_until_no_tasks_left_for(task_queue);

	REQUIRE(i == 5); // task has run now
}

TEST_CASE("store_in is a continuation that stores the result of a task in a future")
{
	auto task_queue = TaskQueue(1);

	std::future<int> future;
	auto t = task([]() { return 5; })
		.then(store_in(future));

	task_queue.push_task(std::move(t));

	REQUIRE(!is_ready(future)); // task hasn't run yet

	this_thread::work_until_no_tasks_left_for(task_queue);

	REQUIRE(future.get() == 5); // task has run now
}

TEST_CASE("A task may have several continuations")
{
	auto task_queue = TaskQueue(1);

	int i = 0;
	int j = 0;
	int k = 0;
	auto t = task([]() { return 5; })
		.then(continuation([&i](int x) { i = x; }, task_queue))
		.then(continuation([&j](int x) { j = x + 1; }, task_queue))
		.then(continuation([&k](int x) { k = x - 1; }, task_queue));
	
	task_queue.push_task(std::move(t));

	// task hasn't run yet
	REQUIRE(i == 0);
	REQUIRE(j == 0);
	REQUIRE(k == 0);

	this_thread::work_until_no_tasks_left_for(task_queue);

	// task has run now
	REQUIRE(i == 5);
	REQUIRE(j == 6);
	REQUIRE(k == 4);
}

TEST_CASE("A continuation may have continuations")
{
	auto task_queue = TaskQueue(1);

	int i = 0;
	auto t = task([]() { return 5; })
		.then(continuation([](int x) { return x + 1; }, task_queue)
			.then(continuation([&i](int x) { i = x; }, task_queue))
		);

	task_queue.push_task(std::move(t));

	REQUIRE(i == 0); // task hasn't run yet

	this_thread::work_until_no_tasks_left_for(task_queue);

	REQUIRE(i == 6); // task has run now
}

TEST_CASE("A continuation may run in a different executor")
{
	auto task_queue_1 = TaskQueue(1);
	auto task_queue_2 = TaskQueue(1);

	int i = 0;
	auto t = task([]() { return 5; })
		.then(continuation([&i](int x) { i = x; }, task_queue_2));

	task_queue_1.push_task(std::move(t));

	// task hasn't run yet
	REQUIRE(i == 0);
	REQUIRE(task_queue_1.number_of_queued_tasks() == 1);
	REQUIRE(task_queue_2.number_of_queued_tasks() == 0);

	this_thread::work_until_no_tasks_left_for(task_queue_1);

	// first task has run and pushed its continuation to second queue
	REQUIRE(i == 0); // i is still 0 because i is written only by the continuation
	REQUIRE(task_queue_1.number_of_queued_tasks() == 0);
	REQUIRE(task_queue_2.number_of_queued_tasks() == 1);

	this_thread::work_until_no_tasks_left_for(task_queue_2);

	// continuation has run now
	REQUIRE(i == 5);
	REQUIRE(task_queue_1.number_of_queued_tasks() == 0);
	REQUIRE(task_queue_2.number_of_queued_tasks() == 0);
}

TEST_CASE("async pushes a task to an executor and returns a future that will hold the result of the task")
{
	auto task_queue = TaskQueue(1);

	std::future<int> future = async(task_queue, task([]() { return 5; }));

	REQUIRE(!is_ready(future)); // task hasn't run yet

	this_thread::work_until_no_tasks_left_for(task_queue);

	REQUIRE(future.get() == 5); // task has run now
}

TEST_CASE("task can bind parameters in order to generate a callable that takes nothing")
{
	auto task_queue = TaskQueue(1);

	int i = 0;
	auto const some_function = [&i](int a, int b) { i = a + b; };

	task_queue.push_task(task(some_function, 3, 4));
	
	REQUIRE(i == 0); // task hasn't run yet

	this_thread::work_until_no_tasks_left_for(task_queue);

	REQUIRE(i == 7); // task has run now
}

TEST_CASE("continuation can bind parameters, starting from the second. The first parameter will be the result of the task") 
{
	auto task_queue = TaskQueue(1);

	std::string s;
	auto const some_function = [&s](int a, std::string b) { s = std::to_string(a) + b; };

	auto const t = task([]() { return 5; })
		.then(continuation(some_function, task_queue, " foo")); // Binds " foo" to the second parameter. The result of task will be the first, calling some_function(5, " foo")

	task_queue.push_task(t);

	REQUIRE(s == ""); // task hasn't run yet

	this_thread::work_until_no_tasks_left_for(task_queue);

	REQUIRE(s == "5 foo"); // task has run now
}

TEST_CASE("A continuation may run in the same task instead of creating a new one. "
	"However, this is not the intended way of using continuations, but an efficient basis for implementing things like ScheduledContinuation or store_in")
{
	auto task_queue = TaskQueue(1);

	int i = 0;
	auto t = task([]() { return 5; })
		.then([&i](int x) { i = x; });

	task_queue.push_task(std::move(t));

	REQUIRE(i == 0); // task hasn't run yet

	this_thread::work_until_no_tasks_left_for(task_queue);

	REQUIRE(i == 5); // task has run now
}

TEST_CASE("get_if_ready returns the content of a future if ready and nothing otherwise")
{
	std::promise<int> promise;
	std::future<int> future = promise.get_future();

	REQUIRE(get_if_ready(future) == std::nullopt);

	promise.set_value(-123);

	REQUIRE(get_if_ready(future) == -123);
}

TEST_CASE("when_all lets the program execute a task after several tasks has finished")
{
	auto task_queue = TaskQueue(1);

	int i = 0;
	auto [t1, t2, t3] = when_all([&i](int a, int b, int c) { i = a + b + c; }, task_queue,
		task([]() { return 1; }),
		task([]() { return 2; }),
		task([]() { return 4; })
	);

	task_queue.push_task(std::move(t1));
	task_queue.push_task(std::move(t2));
	task_queue.push_task(std::move(t3));

	// task hasn't run yet
	REQUIRE(i == 0);
	REQUIRE(task_queue.number_of_queued_tasks() == 3);

	REQUIRE(this_thread::perform_task_for(task_queue));
	REQUIRE(task_queue.number_of_queued_tasks() == 2);
	REQUIRE(i == 0);

	REQUIRE(this_thread::perform_task_for(task_queue));
	REQUIRE(task_queue.number_of_queued_tasks() == 1);
	REQUIRE(i == 0);

	// After doing the third task a new task has been pushed so the count keeps at 1.
	REQUIRE(this_thread::perform_task_for(task_queue));
	REQUIRE(task_queue.number_of_queued_tasks() == 1);
	REQUIRE(i == 0);

	// The fourth task is the continuation.
	REQUIRE(this_thread::perform_task_for(task_queue));
	REQUIRE(task_queue.number_of_queued_tasks() == 0);
	REQUIRE(i == 1 + 2 + 4);
}

TEST_CASE("Continuation function of when_all may take arguments of different types")
{
	auto task_queue = TaskQueue(1);

	std::string s;
	auto [t1, t2, t3] = when_all([&s](std::string a, int b, std::chrono::seconds c) { s = a + " " + std::to_string(b) + " " + std::to_string(c.count()); }, task_queue,
		task([]() { return "Hello!"; }),
		task([]() { return 2; }),
		task([]() { return 4s; })
	);

	task_queue.push_task(std::move(t1));
	task_queue.push_task(std::move(t2));
	task_queue.push_task(std::move(t3));

	REQUIRE(s == ""); // task hasn't run yet
	
	this_thread::work_until_no_tasks_left_for(task_queue);

	REQUIRE(s == "Hello! 2 4");
}

TEST_CASE("A task with a joint continuation cannot be copied because that would be a potential data race")
{
	auto task_queue = TaskQueue(1);

	int i = 0;
	auto [t1, t2, t3] = when_all([&i](int a, int b, int c) { i = a + b + c; }, task_queue,
		task([]() { return 1; }),
		task([]() { return 2; }),
		task([]() { return 4; })
	);

	static_assert(!std::is_copy_constructible_v<decltype(t1)>);
	static_assert(!std::is_copy_constructible_v<decltype(t2)>);
	static_assert(!std::is_copy_constructible_v<decltype(t3)>);
}
