#pragma once

#include "task.hh"
#include <string_view>
#include <chrono>
#include <vector>
#include <mutex>
#include <span>
#include <map>
#include <cassert>

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
		
		[[nodiscard]] auto duration() const noexcept -> std::chrono::nanoseconds { return time_end - time_start; }

		[[nodiscard]] auto operator == (Node const & other) const noexcept -> bool = default;
	};

	template <std::invocable<TaskProfile::Node const &> Enter, std::invocable<TaskProfile::Node const &> Exit>
	void traverse(Enter && enter, Exit && exit) const;

	static constexpr std::string_view no_parent_id = "";

	[[nodiscard]] std::string_view id() const noexcept { return nodes[0].name; }
	[[nodiscard]] bool is_main_task() const noexcept { return parent_id == no_parent_id; }

	[[nodiscard]] auto operator == (TaskProfile const & other) const noexcept -> bool = default;

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
// Does not support appending.
void save_profiles_and_strings(std::span<TaskProfile const> profiles, std::ostream & out);

std::vector<TaskProfile> load_profiles(std::istream & in, std::span<char const> strings);
std::pair<std::vector<TaskProfile>, std::vector<char>> load_profiles_and_strings(std::istream & in);

struct Profiler
{
	void start_main_task(std::string_view name);
	void start_sub_task(std::string_view name, std::string_view parent_id);
	void end_task();

	void push(std::string_view name);
	void pop() noexcept;

	[[nodiscard]] auto is_profiling() const noexcept -> bool { return current_node != TaskProfile::invalid_node_index; }
	[[nodiscard]] auto current_task_id() const noexcept -> std::string_view { assert(is_profiling()); return current_profile.id(); }

	[[nodiscard]] auto get_finished_profiles() noexcept -> std::vector<TaskProfile>;

private:
	TaskProfile current_profile;
	uint16_t current_node = TaskProfile::invalid_node_index;
	uint16_t current_insertion_point = TaskProfile::invalid_node_index;

	std::mutex finished_profiles_mutex;
	std::vector<TaskProfile> finished_profiles;
};

struct ProfileScope
{
	#if ENABLE_GLOBAL_PROFILER
		explicit ProfileScope(std::string_view name);
	#endif

	explicit ProfileScope(Profiler & profiler_, std::string_view name);
	ProfileScope(ProfileScope const &) = delete;
	ProfileScope & operator = (ProfileScope const &) = delete;
	~ProfileScope();

private:
	Profiler & profiler;
};

struct ProfileScopeAsTask
{
	#if ENABLE_GLOBAL_PROFILER
		explicit ProfileScopeAsTask(std::string_view name);
		explicit ProfileScopeAsTask(std::string_view name, std::string_view parent_id);
	#endif

	explicit ProfileScopeAsTask(Profiler & profiler_, std::string_view name);
	explicit ProfileScopeAsTask(Profiler & profiler_, std::string_view name, std::string_view parent_id);
	ProfileScopeAsTask(ProfileScopeAsTask const &) = delete;
	ProfileScopeAsTask & operator = (ProfileScopeAsTask const &) = delete;
	~ProfileScopeAsTask();

private:
	Profiler & profiler;
};

#if ENABLE_GLOBAL_PROFILER
namespace global_profiler
{
	Profiler & instance() noexcept;

	void start_main_task(std::string_view name);
	void start_sub_task(std::string_view name, std::string_view parent_id);
	void end_task();

	void push(std::string_view name);
	void pop() noexcept;

	[[nodiscard]] auto is_profiling() noexcept -> bool;
	[[nodiscard]] auto current_task_id() noexcept -> std::string_view;

	[[nodiscard]] auto get_finished_profiles() noexcept -> std::vector<TaskProfile>;

} // namespace global_profiler

#define PROFILE_FUNCTION auto const zzz_profile_scope_guard = ProfileScope(__FUNCTION__)

template <typename F, typename ... Args> requires std::invocable<F, Args...>
auto main_task(std::string_view name, F && f, Args && ... args);

template <typename F, typename ... Args> requires std::invocable<F, Args...>
auto sub_task(std::string_view name, F && f, Args && ... args);

template <typename F, is_task_executor TaskExecutor, typename ... Args>
auto main_continuation(std::string_view name, F f, TaskExecutor & executor, Args && ... args);

template <typename F, is_task_executor TaskExecutor, typename ... Args>
auto sub_continuation(std::string_view name, F f, TaskExecutor & executor, Args && ... args);

#endif // ENABLE_GLOBAL_PROFILER

#include "profiler.inl"
