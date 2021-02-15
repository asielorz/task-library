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

struct return_result_t {};
constexpr return_result_t return_result;

template <typename F>
concept is_continuable = std::move_constructible<F> && std::invocable<F, return_result_t>;

template <is_continuable T>
using continuable_result_type = std::invoke_result_t<T, return_result_t>;

template <is_continuable F, is_continuation<continuable_result_type<F>> C>
struct PackagedTaskWithContinuation
{
	explicit PackagedTaskWithContinuation(F f, C c) noexcept : function(std::move(f)), continuation_function(std::move(c)) {}

	using result_type = continuable_result_type<F>;

	auto operator () () -> void
	{
		auto result = std::invoke(std::move(function), return_result);
		std::invoke(std::move(continuation_function), std::move(result));
	}

	auto operator () (return_result_t) -> result_type
	{
		auto result = std::invoke(std::move(function), return_result);
		std::invoke(std::move(continuation_function), result);
		return result;
	}

	template <is_continuation<result_type> C2>
	[[nodiscard]] auto then(C2 c) const & -> PackagedTaskWithContinuation<PackagedTaskWithContinuation<F, C>, C2>
	{
		return PackagedTaskWithContinuation<PackagedTaskWithContinuation<F, C>, C2>(*this, std::move(c));
	}

	template <is_continuation<result_type> C2>
	[[nodiscard]] auto then(C2 c) && -> PackagedTaskWithContinuation<PackagedTaskWithContinuation<F, C>, C2>
	{
		return PackagedTaskWithContinuation<PackagedTaskWithContinuation<F, C>, C2>(std::move(*this), std::move(c));
	}

private:
	F function;
	C continuation_function;
};

template <is_task F>
struct PackagedTask
{
	explicit PackagedTask(F f) noexcept : function(std::move(f)) {}

	using result_type = task_result_type<F>;

	auto operator () () -> result_type { return std::invoke(std::move(function)); }
	auto operator () (return_result_t) -> result_type { return std::invoke(std::move(function)); }

	template <is_continuation<result_type> C>
	[[nodiscard]] auto then(C c) const &
	{
		return PackagedTaskWithContinuation<PackagedTask<F>, C>(*this, std::move(c));
	}

	template <is_continuation<result_type> C>
	[[nodiscard]] auto then(C c) &&
	{
		return PackagedTaskWithContinuation<PackagedTask<F>, C>(std::move(*this), std::move(c));
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

	auto operator () (Arg arg) -> void { executor->run_task(task(std::move(function), std::move(arg))); }

	template <is_continuation<result_type> C>
	[[nodiscard]] auto then(C c) const &
	{
		auto with_continuation = [f_ = function, c_ = std::move(c)](argument_type arg) mutable
		{
			auto result = std::invoke(std::move(f_), std::move(arg));
			std::invoke(std::move(c_), result);
			return result;
		};
		using new_function_t = decltype(with_continuation);

		return ScheduledContinuation<TaskExecutor, argument_type, result_type, new_function_t>(executor, std::move(with_continuation));
	}

	template <is_continuation<result_type> C>
	[[nodiscard]] auto then(C c) &&
	{
		auto with_continuation = [f_ = std::move(function), c_ = std::move(c)](argument_type arg) mutable
		{
			auto result = std::invoke(std::move(f_), std::move(arg));
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
