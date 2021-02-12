#pragma once

#include "polymorphic_task.hh"
#include "function_traits.hh"
#include <utility>

template <typename T>
using task_result_type = std::invoke_result_t<T>;

// Bind arguments in a callable in order to make it a task.
template <typename F, typename ... Args> requires std::invocable<F, Args...>
auto task(F && f, Args && ... args);

// Minimalistic executor concept. Can run tasks.
template <typename E>
concept is_task_executor = requires(E & executor, PolymorphicTask f) { executor.run_task(std::move(f)); };

template <typename T>
using result_type = typename T::result_type;

template <typename F, typename T>
concept is_continuation = std::invocable<F, T>;

template <typename F, typename Prev>
concept is_continuation_for = is_continuation<F, result_type<Prev>>;

template <is_task F>
struct PackagedTask
{
	explicit PackagedTask(F f) noexcept : function(std::move(f)) {}

	using result_type = std::invoke_result_t<F>;

	auto operator () () -> result_type { return std::invoke(std::move(function)); }

	template <is_continuation<result_type> C>
	[[nodiscard]] auto then(C c) const &
	{
		auto with_continuation = [f_ = function, c_ = std::move(c)]() mutable
		{
			auto result = std::invoke(std::move(f_));
			std::invoke(std::move(c_), result);
			return result;
		};
		return PackagedTask<decltype(with_continuation)>(std::move(with_continuation));
	}

	template <is_continuation<result_type> C>
	[[nodiscard]] auto then(C c) &&
	{
		auto with_continuation = [f_ = std::move(function), c_ = std::move(c)]() mutable
		{
			auto result = std::invoke(std::move(f_));
			std::invoke(std::move(c_), result);
			return result;
		};
		return PackagedTask<decltype(with_continuation)>(std::move(with_continuation));
	}

private:
	F function;
};

template <is_task_executor TaskExecutor, typename Arg, typename Result, typename F> 
struct ScheduledContinuation
{
	explicit ScheduledContinuation(TaskExecutor * exec, F f) noexcept
		: executor(exec)
		, function(std::move(f))
	{}

	using argument_type = Arg;
	using result_type = Result;

	auto operator () (Arg const & arg) -> void { executor->run_task(task(std::move(function), arg)); }

	template <is_continuation<result_type> C>
	[[nodiscard]] auto then(C c) const &
	{
		auto with_continuation = [f_ = function, c_ = std::move(c)](argument_type const & arg) mutable
		{
			auto result = std::invoke(std::move(f_), arg);
			std::invoke(std::move(c_), result);
			return result;
		};
		using new_function_t = decltype(with_continuation);

		return ScheduledContinuation<TaskExecutor, argument_type, result_type, new_function_t>(executor, std::move(with_continuation));
	}

	template <is_continuation<result_type> C>
	[[nodiscard]] auto then(C c) &&
	{
		auto with_continuation = [f_ = std::move(function), c_ = std::move(c)](argument_type const & arg) mutable
		{
			auto result = std::invoke(std::move(f_), arg);
			std::invoke(std::move(c_), result);
			return result;
		};
		using new_function_t = decltype(with_continuation);

		return ScheduledContinuation<TaskExecutor, argument_type, result_type, new_function_t>(executor, std::move(with_continuation));
	}

private:
	TaskExecutor * executor;
	F function;
};

template <typename F, is_task_executor TaskExecutor, typename ... Args>
auto continuation(F f, TaskExecutor & executor, Args && ... args);

template <typename A, typename B>
auto operator >> (A && a, B && b) noexcept(noexcept(std::forward<A>(a).then(std::forward<B>(b))))
	-> decltype(std::forward<A>(a).then(std::forward<B>(b)));

#include "task.inl"
