namespace detail
{

	template <is_task_executor Executor, typename F, typename ... Args> requires std::invocable<F, Args...>
	struct JointContinuation
	{
		explicit JointContinuation(Executor * executor_, F function_) noexcept
			: executor(executor_)
			, function(std::move(function_))
		{}

		template <size_t N>
		void set_value(std::integral_constant<size_t, N>, std::tuple_element_t<N, std::tuple<Args...>> value)
		{
			std::get<N>(args) = std::move(value);
			if (++set_arguments == sizeof...(Args))
			{
				auto const helper = [executor_ = executor]<size_t ... Is>(F & f_, std::tuple<std::optional<Args>...> &args_, std::index_sequence<Is...>)
				{
					executor_->run_task(task(std::move(f_), std::move(*std::get<Is>(args_))...));
				};
				helper(function, args, std::make_index_sequence<sizeof...(Args)>());
			}
		}

	private:
		Executor * executor;
		F function;
		std::tuple<std::optional<Args>...> args;
		std::atomic<int> set_arguments = 0;
	};

	template <typename T>
	struct disable_copy : public T
	{
		constexpr disable_copy(T t) noexcept : T(std::move(t)) {}

		disable_copy(disable_copy const &) = delete;
		disable_copy & operator = (disable_copy const &) = delete;

		constexpr disable_copy(disable_copy &&) noexcept = default;
		constexpr disable_copy & operator = (disable_copy &&) noexcept = default;
	};

	template <size_t N, is_task_executor Executor, typename F, typename ... Args>
	auto set_value_continuation(std::shared_ptr<JointContinuation<Executor, F, Args...>> c)
	{
		return[c_ = disable_copy(std::move(c))](std::tuple_element_t<N, std::tuple<Args...>> value)
		{
			c_->set_value(std::integral_constant<size_t, N>(), std::move(value));
		};
	}

} // namespace detail

template <typename F, is_task_executor Executor, typename ... ArgProducers> requires std::invocable<F, result_type<ArgProducers>...>
auto when_all(F f, Executor & executor, ArgProducers ... arg_producers)
{
	auto joint_continuation = std::make_shared<detail::JointContinuation<Executor, F, result_type<ArgProducers>...>>(std::addressof(executor), std::move(f));

	auto const helper = []<size_t ... Is>(
		std::index_sequence<Is...>,
		std::shared_ptr<detail::JointContinuation<Executor, F, result_type<ArgProducers>...>> c,
		std::tuple<ArgProducers...> p)
	{
		return std::make_tuple(
			std::move(std::get<Is>(p)).then(detail::set_value_continuation<Is>(c))...
		);
	};
	return helper(
		std::make_index_sequence<sizeof...(ArgProducers)>(),
		std::move(joint_continuation),
		std::make_tuple(std::move(arg_producers)...)
	);
}