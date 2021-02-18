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

template <template<typename> typename Allocator, size_t MaxIDLength, size_t DefaultInitialCapacity>
void basic_profiler<Allocator, MaxIDLength, DefaultInitialCapacity>::push(id_t id)
{
	if (curr != null_index && (ids_equal(id, tree[curr].id.data())))
	{
		tree[curr].recursive_calls++;
	}
	else
	{
		tree.emplace_back();
		curr_pops = 0;
		Node & new_node = tree.back();
		if (curr != null_index)
			add_child(tree[curr], new_node);
		curr = index_of(new_node);
		const auto id_len = std::min(id.size(), size_t(max_id_length));
		memcpy(new_node.id.data(), id.data(), id_len);
		new_node.id[id_len] = 0;
		new_node.cpu_cycles = start_measure();
	}
}

template <template<typename> typename Allocator, size_t MaxIDLength, size_t DefaultInitialCapacity>
auto basic_profiler<Allocator, MaxIDLength, DefaultInitialCapacity>::pop(id_t id) -> cpu_time_t
{
	const cpu_time_t delta_t = end_measure() - tree[curr].cpu_cycles;
	assert(ids_equal(id, tree[curr].id.data()));
	static_cast<void>(id); // For the unused parameter warning in release

	curr_pops++;
	if (curr_pops == tree[curr].recursive_calls)
	{
		tree[curr].cpu_cycles = delta_t;
		curr = tree[curr].parent;
		curr_pops = 0;
	}

	return delta_t;
}
