template <typename T>
auto store_in(std::future<T> & future)
{
	std::promise<T> promise;
	future = promise.get_future();
	return[promise_ = std::move(promise)](T t) mutable
	{
		promise_.set_value(std::move(t));
	};
}

template <is_task_executor TaskExecutor, is_task T>
auto async(TaskExecutor & executor, PackagedTask<T> t) -> std::future<task_result_type<T>>
{
	std::future<task_result_type<T>> future;
	executor.run_task(std::move(t).then(store_in(future)));
	return future;
}

template <typename T>
auto is_ready(std::future<T> const & future) -> bool
{
	return future.valid() && future.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
}

template <typename T>
auto get_if_ready(std::future<T> & future) -> std::optional<T>
{
	if (is_ready(future))
		return future.get();
	else
		return std::nullopt;
}
