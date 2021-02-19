#include "profiler.hh"
#include "catch/catch.hpp"

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
