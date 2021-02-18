#pragma once

#include <string_view>
#include <chrono>
#include <vector>
#include <mutex>

struct TaskProfile
{
	static constexpr uint16_t invalid_node_index = uint16_t(-1);

	struct Node
	{
		std::string_view name;
		std::chrono::nanoseconds time_start;
		std::chrono::nanoseconds time_end;
		uint16_t parent;
		uint16_t first_child = invalid_node_index;
		uint16_t next_sibling = invalid_node_index;
	};

	template <std::invocable<Node const &> Enter, std::invocable<Node const &> Exit>
	void traverse(Enter && enter, Exit && exit) const;

	static constexpr uint32_t no_parent_id = uint32_t(-1);

	uint32_t id;
	uint32_t parent_id = no_parent_id;
	std::vector<Node> nodes;
};

struct Profiler
{
	void start_main_task(std::string_view name, uint32_t id);
	void start_sub_task(std::string_view name, uint32_t id, uint32_t parent_id);
	void end_task();

	void push(std::string_view name);
	void pop();

private:
	TaskProfile current_profile;
	uint16_t current_node = TaskProfile::invalid_node_index;

	std::mutex finished_profiles_mutex;
	std::vector<TaskProfile> finished_profiles;
};
