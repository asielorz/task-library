#include "thread_pool.hh"
#include "profiler.hh"
#include "catch/catch.hpp"
#include <sstream>
#include <fstream>

TEST_CASE("Can start and finish a task, and retrieve the result")
{
	Profiler profiler;

	profiler.start_main_task("Test task");
	profiler.end_task();

	auto profiles = profiler.get_finished_profiles();
	REQUIRE(profiles.size() == 1);
	REQUIRE(profiles[0].id() == "Test task");
	REQUIRE(profiles[0].parent_id == TaskProfile::no_parent_id);
	REQUIRE(profiles[0].nodes.size() == 1);
	REQUIRE(profiles[0].nodes[0].name == "Test task");
	REQUIRE(profiles[0].nodes[0].time_end > profiles[0].nodes[0].time_start);
	REQUIRE(profiles[0].nodes[0].first_child == TaskProfile::invalid_node_index);
	REQUIRE(profiles[0].nodes[0].next_sibling == TaskProfile::invalid_node_index);
}

TEST_CASE("A subtask is a task that knows its parent task")
{
	Profiler profiler;

	profiler.start_sub_task("Test task", "Parent task");
	profiler.end_task();

	auto profiles = profiler.get_finished_profiles();
	REQUIRE(profiles.size() == 1);
	REQUIRE(profiles[0].id() == "Test task");
	REQUIRE(profiles[0].parent_id == "Parent task");
	REQUIRE(profiles[0].nodes.size() == 1);
	REQUIRE(profiles[0].nodes[0].name == "Test task");
	REQUIRE(profiles[0].nodes[0].time_end > profiles[0].nodes[0].time_start);
	REQUIRE(profiles[0].nodes[0].first_child == TaskProfile::invalid_node_index);
	REQUIRE(profiles[0].nodes[0].next_sibling == TaskProfile::invalid_node_index);
}

TEST_CASE("Parts of a task can be profiled with calls to push and pop")
{
	Profiler profiler;

	profiler.start_main_task("Test task");
	profiler.push("Step 1");
	profiler.pop();
	profiler.push("Step 2");
	profiler.pop();
	profiler.push("Step 3");
	profiler.pop();
	profiler.end_task();

	auto profiles = profiler.get_finished_profiles();
	REQUIRE(profiles.size() == 1);
	REQUIRE(profiles[0].id() == "Test task");
	REQUIRE(profiles[0].parent_id == TaskProfile::no_parent_id);
	REQUIRE(profiles[0].nodes.size() == 4);
	REQUIRE(profiles[0].nodes[0].name == "Test task");
	REQUIRE(profiles[0].nodes[0].time_end > profiles[0].nodes[0].time_start);
	REQUIRE(profiles[0].nodes[0].first_child != TaskProfile::invalid_node_index);
	REQUIRE(profiles[0].nodes[0].next_sibling == TaskProfile::invalid_node_index);
}

TEST_CASE("Several tasks can be profiled")
{
	Profiler profiler;

	profiler.start_main_task("Task 1");
	profiler.end_task();

	profiler.start_main_task("Task 2");
	profiler.end_task();

	profiler.start_main_task("Task 3");
	profiler.end_task();

	auto profiles = profiler.get_finished_profiles();
	REQUIRE(profiles.size() == 3);

	REQUIRE(profiles[0].id() == "Task 1");
	REQUIRE(profiles[1].id() == "Task 2");
	REQUIRE(profiles[2].id() == "Task 3");
}

TEST_CASE("A profiler can be queried for the task it is currently profiling")
{
	Profiler profiler;

	profiler.start_main_task("Test task");

	REQUIRE(profiler.current_task_id() == "Test task");

	profiler.end_task();
}

TEST_CASE("Profiles can be serialized and read back")
{
	Profiler profiler;

	profiler.start_main_task("Task 1");
		profiler.push("Step 1");
		profiler.pop();
		profiler.push("Step 2");
		profiler.pop();
		profiler.push("Step 3");
		profiler.pop();
	profiler.end_task();

	profiler.start_main_task("Task 2");
		profiler.push("Step 1");
			profiler.push("Step 1.1");
				profiler.push("Step 1.1.1");
				profiler.pop();
				profiler.push("Step 1.1.2");
				profiler.pop();
				profiler.push("Step 1.1.3");
				profiler.pop();
			profiler.pop();
		profiler.pop();
	profiler.end_task();

	profiler.start_main_task("Task 3");
	profiler.end_task();

	auto profiles = profiler.get_finished_profiles();

	auto out_stream = std::ostringstream(std::ios::out | std::ios::binary);
	save_profiles_and_strings(profiles, out_stream);
	auto in_stream = std::istringstream(out_stream.str(), std::ios::in | std::ios::binary);
	auto [saved_and_loaded_profiles, strings] = load_profiles_and_strings(in_stream);

	REQUIRE(profiles == saved_and_loaded_profiles);
}

TEST_CASE("ProfileScope is a scope guard for profiling a scope without being able to forget or mismatch push and pop calls")
{
	Profiler profiler;
	{
		auto const task_guard = ProfileScopeAsTask(profiler, "Test task");
		{
			auto const step1_guard = ProfileScope(profiler, "Step 1");
		}
		{
			auto const step2_guard = ProfileScope(profiler, "Step 2");
		}
		{
			auto const step3_guard = ProfileScope(profiler, "Step 3");
			{
				auto const step3_1_guard = ProfileScope(profiler, "Step 3.1");
			}
			{
				auto const step3_2_guard = ProfileScope(profiler, "Step 3.2");
			}
		}
	}

	profiler.start_main_task("Test task");
		profiler.push("Step 1");
		profiler.pop();
		profiler.push("Step 2");
		profiler.pop();
		profiler.push("Step 3");
			profiler.push("Step 3.1");
			profiler.pop();
			profiler.push("Step 3.2");
			profiler.pop();
		profiler.pop();
	profiler.end_task();

	auto profiles = profiler.get_finished_profiles();

	REQUIRE(std::equal(
		profiles[0].nodes.begin(), profiles[0].nodes.end(),
		profiles[1].nodes.begin(), profiles[1].nodes.end(),
		// Compare everything except for the timestamps (which are obviously different)
		[](TaskProfile::Node const & a, TaskProfile::Node const & b) { return a.name == b.name && a.parent == b.parent && a.first_child == b.first_child && a.next_sibling == b.next_sibling; }
	));
}

TEST_CASE("is_profiling returns true if the profiler is in the middle of a task")
{
	Profiler profiler;
	
	REQUIRE(!profiler.is_profiling());

	profiler.start_main_task("Test task");

	REQUIRE(profiler.is_profiling());

	profiler.end_task();
	
	REQUIRE(!profiler.is_profiling());
}

#if ENABLE_GLOBAL_PROFILER
TEST_CASE("main_task creates a task that is profiled automatically")
{
	auto task_queue = TaskQueue(1);

	int i = 0;
	task_queue.push_task(main_task("Test task", [&i]() { i = 5; }));

	REQUIRE(i == 0); // task hasn't run yet

	this_thread::work_until_no_tasks_left_for(task_queue);

	REQUIRE(i == 5); // task has run now

	auto profiles = global_profiler::get_finished_profiles();

	REQUIRE(profiles.size() == 1);
	REQUIRE(profiles[0].id() == "Test task");
	REQUIRE(profiles[0].parent_id == TaskProfile::no_parent_id);
	REQUIRE(profiles[0].nodes.size() == 1);
	REQUIRE(profiles[0].nodes[0].name == "Test task");
	REQUIRE(profiles[0].nodes[0].time_end > profiles[0].nodes[0].time_start);
	REQUIRE(profiles[0].nodes[0].first_child == TaskProfile::invalid_node_index);
	REQUIRE(profiles[0].nodes[0].next_sibling == TaskProfile::invalid_node_index);
}

TEST_CASE("main_continuation creates a continuation that is profiled automatically")
{
	auto task_queue = TaskQueue(1);

	int i = 0;
	auto t = main_task("Test task", []() { return 5; })
		.then(main_continuation("Test continuation", [&i](int x) { i = x; }, task_queue));

	task_queue.push_task(std::move(t));

	REQUIRE(i == 0); // task hasn't run yet

	this_thread::work_until_no_tasks_left_for(task_queue);

	REQUIRE(i == 5); // task has run now

	auto profiles = global_profiler::get_finished_profiles();

	REQUIRE(profiles.size() == 2);
	REQUIRE(profiles[0].id() == "Test task");
	REQUIRE(profiles[0].parent_id == TaskProfile::no_parent_id);
	REQUIRE(profiles[1].id() == "Test continuation");
	REQUIRE(profiles[1].parent_id == TaskProfile::no_parent_id);
}

TEST_CASE("sub_task creates a task that is automatically profiled and that has as a parent the task active when calling sub_task")
{
	auto task_queue = TaskQueue(1);

	int i = 0;
	{
		auto const g = ProfileScopeAsTask("Main task");
		task_queue.push_task(sub_task("Sub task", [&i]() { i = 5; }));
	}

	REQUIRE(i == 0); // task hasn't run yet

	this_thread::work_until_no_tasks_left_for(task_queue);

	REQUIRE(i == 5); // task has run now

	auto profiles = global_profiler::get_finished_profiles();

	REQUIRE(profiles.size() == 2);
	REQUIRE(profiles[0].id() == "Main task");
	REQUIRE(profiles[0].parent_id == TaskProfile::no_parent_id);
	REQUIRE(profiles[1].id() == "Sub task");
	REQUIRE(profiles[1].parent_id == "Main task");
}

TEST_CASE("sub_continuation creates a continuation that is automatically profiled and that has as a parent the task active when calling sub_continuation")
{
	auto task_queue = TaskQueue(1);

	int i = 0;
	{
		auto const g = ProfileScopeAsTask("Main task");
		auto t = sub_task("Sub task", []() { return 5; })
			.then(sub_continuation("Sub continuation", [&i](int x) { i = x; }, task_queue));

		task_queue.push_task(std::move(t));
	}

	REQUIRE(i == 0); // task hasn't run yet

	this_thread::work_until_no_tasks_left_for(task_queue);

	REQUIRE(i == 5); // task has run now

	auto profiles = global_profiler::get_finished_profiles();

	REQUIRE(profiles.size() == 3);
	REQUIRE(profiles[0].id() == "Main task");
	REQUIRE(profiles[0].parent_id == TaskProfile::no_parent_id);
	REQUIRE(profiles[1].id() == "Sub task");
	REQUIRE(profiles[1].parent_id == "Main task");
	REQUIRE(profiles[2].id() == "Sub continuation");
	REQUIRE(profiles[2].parent_id == "Main task");
}
#endif // ENABLE_GLOBAL_PROFILER

#if 0
void node_traversal_algorithms_that_should_be_turned_into_iterators_at_some_point()
{
	TaskProfile p;

	int tabs = 0;
	auto const enter = [&tabs](TaskProfile::Node const & node)
	{
		for (int i = 0; i < tabs; ++i) putchar('\t');
		printf("Enter: ");
		for (char c : node.name) putchar(c);
		putchar('\n');
		++tabs;
	};
	auto const exit = [&tabs](TaskProfile::Node const & node)
	{
		--tabs;
		for (int i = 0; i < tabs; ++i) putchar('\t');
		printf("Exit:  ");
		for (char c : node.name) putchar(c);
		putchar('\n');
	};

	{
		uint16_t i = 0;
		while (i != TaskProfile::invalid_node_index)
		{
			TaskProfile::Node const & node = p.nodes[i];
			enter(node);

			if (node.first_child != TaskProfile::invalid_node_index)
			{
				i = node.first_child;
			}
			else
			{
				exit(node);

				if (node.next_sibling != TaskProfile::invalid_node_index)
				{
					i = node.next_sibling;
				}
				else
				{
					while (true)
					{
						uint16_t const parent = p.nodes[i].parent;

						if (parent == TaskProfile::invalid_node_index)
						{
							i = parent;
							break;
						}

						TaskProfile::Node const & parent_node = p.nodes[parent];
						exit(parent_node);

						uint16_t const next_sibling = parent_node.next_sibling;
						if (next_sibling != TaskProfile::invalid_node_index)
						{
							i = next_sibling;
							break;
						}
						else
						{
							i = parent;
						}
					}
				}
			}
		}
	}

	{
		uint16_t i = 0;
		bool entering = true;
		while (i != TaskProfile::invalid_node_index)
		{
			TaskProfile::Node const & node = p.nodes[i];
			if (entering)
			{
				enter(node);

				if (node.first_child != TaskProfile::invalid_node_index)
				{
					i = node.first_child;
				}
				else
				{
					entering = false;
				}
			}
			else
			{
				exit(node);

				if (node.next_sibling != TaskProfile::invalid_node_index)
				{
					i = node.next_sibling;
					entering = true;
				}
				else
				{
					i = node.parent;
				}
			}
		}
	}
}
#endif
