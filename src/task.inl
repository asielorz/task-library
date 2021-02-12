template <typename F, typename ... Args> requires std::invocable<F, Args...>
auto task(F && f, Args && ... args)
{
	return PackagedTask([f_ = std::forward<F>(f), ...args_ = std::forward<Args>(args)]() mutable
	{
		return std::invoke(std::move(f_), std::move(args_)...);
	});
}

template <typename F, is_task_executor TaskExecutor, typename ... Args>
auto continuation(F f, TaskExecutor & executor, Args && ... args)
{
	using first_param = first_parameter_type_t<F>;
	using result = std::invoke_result_t<F, first_param, Args...>;

	auto bound = [f_ = std::move(f), ...args_ = std::forward<Args>(args)](first_param const & x) mutable
	{
		return std::invoke(std::move(f_), x, std::move(args_)...);
	};
	using bound_t = decltype(bound);
	
	return ScheduledContinuation<TaskExecutor, first_param, result, bound_t>(std::addressof(executor), std::move(bound));
}

template <typename A, typename B>
auto operator >> (A && a, B && b) noexcept(noexcept(std::forward<A>(a).then(std::forward<B>(b))))
	-> decltype(std::forward<A>(a).then(std::forward<B>(b)))
{
	return std::forward<A>(a).then(std::forward<B>(b));
}
