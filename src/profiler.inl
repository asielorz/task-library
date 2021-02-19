namespace detail
{
	template <std::invocable<TaskProfile::Node const &> Enter, std::invocable<TaskProfile::Node const &> Exit>
	void traverse_task_profile_rec(std::span<TaskProfile::Node const> nodes, uint16_t node_index, Enter && enter, Exit && exit)
	{
		enter(nodes[node_index]);

		for (uint16_t child = nodes[node_index].first_child; child != TaskProfile::invalid_node_index; child = nodes[child].next_sibling)
			traverse_task_profile_rec(nodes, child, enter, exit);

		exit(nodes[node_index]);
	}
}

template <std::invocable<TaskProfile::Node const &> Enter, std::invocable<TaskProfile::Node const &> Exit>
void TaskProfile::traverse(Enter && enter, Exit && exit) const
{
	detail::traverse_task_profile_rec(nodes, 0, enter, exit);
}