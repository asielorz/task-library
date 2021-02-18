#include "profiler.hh"
#include <cassert>

uint16_t push_node(std::vector<TaskProfile::Node> & nodes, std::string_view name, uint16_t parent)
{
	auto index = static_cast<uint16_t>(nodes.size());
	nodes.push_back(TaskProfile::Node{
		.name = name,
		.time_start = std::chrono::steady_clock::now().time_since_epoch(),
		.parent = parent
	});
}

void Profiler::start_main_task(std::string_view name, uint32_t id)
{
	assert(current_node == TaskProfile::invalid_node_index);
	start_sub_task(name, id, TaskProfile::no_parent_id);
}

void Profiler::start_sub_task(std::string_view name, uint32_t id, uint32_t parent_id)
{
	assert(current_node == TaskProfile::invalid_node_index);
	current_profile.id = id;
	current_profile.parent_id = parent_id;
	// push
}

void Profiler::end_task()
{
	pop();
	auto const g = std::lock_guard(finished_profiles_mutex);
	finished_profiles.push_back(std::move(current_profile));
}

void Profiler::push(std::string_view name)
{
	
}

void Profiler::pop()
{

}
