#pragma once

#include <string_view>
#include <chrono>
#include <vector>
#include <mutex>
#include <span>
#include <map>

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

	template <std::invocable<TaskProfile::Node const &> Enter, std::invocable<TaskProfile::Node const &> Exit>
	void traverse(Enter && enter, Exit && exit) const;

	static constexpr std::string_view no_parent_id = "";

	[[nodiscard]] std::string_view id() const noexcept { return nodes[0].name; }
	[[nodiscard]] bool is_main_task() const noexcept { return parent_id == no_parent_id; }

	std::string_view parent_id = no_parent_id;
	std::vector<Node> nodes;
};

// Saves profiles to a very efficient binary format. Very fast to save and load. Strings are not saved to file. 
// It is the responsibility of the programmer to manage the strings.
struct StringInDisk
{
	uint32_t start;
	uint32_t length;
};

void save_profiles(
	std::span<TaskProfile const> profiles, std::ostream & out,
	std::vector<char> & strings,
	std::map<std::string_view, StringInDisk> & already_recorded_strings
);

std::vector<TaskProfile> load_profiles(std::istream & in, std::span<char const> strings);

struct Profiler
{
	void start_main_task(std::string_view name);
	void start_sub_task(std::string_view name, std::string_view parent_id);
	void end_task();

	void push(std::string_view name);
	void pop() noexcept;

	std::string_view current_task_id() const noexcept { return current_profile.id(); }

	std::vector<TaskProfile> get_finished_profiles() noexcept;

private:
	TaskProfile current_profile;
	uint16_t current_node = TaskProfile::invalid_node_index;

	std::mutex finished_profiles_mutex;
	std::vector<TaskProfile> finished_profiles;
};

#include "profiler.inl"
