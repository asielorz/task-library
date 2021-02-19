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
} // namespace detail

template <std::invocable<TaskProfile::Node const &> Enter, std::invocable<TaskProfile::Node const &> Exit>
void TaskProfile::traverse(Enter && enter, Exit && exit) const
{
	detail::traverse_task_profile_rec(nodes, 0, enter, exit);
}

#if ENABLE_GLOBAL_PROFILER

template <typename F, typename ... Args> requires std::invocable<F, Args...>
auto main_task(std::string_view name, F && f, Args && ... args)
{
	return Continuable([name_ = name, f_ = std::forward<F>(f), ...args_ = std::forward<Args>(args)]() mutable
	{
		auto const g = ProfileScopeAsTask(name_);
		return std::invoke(std::move(f_), std::move(args_)...);
	});
}

template <typename F, typename ... Args> requires std::invocable<F, Args...>
auto sub_task(std::string_view name, F && f, Args && ... args)
{
	return Continuable([name_ = name, parent_id = global_profiler::current_task_id(), f_ = std::forward<F>(f), ...args_ = std::forward<Args>(args)]() mutable
	{
		auto const g = ProfileScopeAsTask(name_, parent_id);
		return std::invoke(std::move(f_), std::move(args_)...);
	});
}

#endif // ENABLE_GLOBAL_PROFILER
