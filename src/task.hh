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

template <typename F, typename ... Args>
concept is_continuable = std::move_constructible<F> && std::invocable<F, Args..., return_result_t>;

template <typename T, typename ... Args> requires(is_continuable<T, Args...>)
using continuable_result_type = std::invoke_result_t<T, Args..., return_result_t>;

template <typename F, typename C, typename ... Args>
	requires(is_continuable<F, Args...> && is_continuation<C, continuable_result_type<F, Args...>>)
struct FunctionWithContinuation
{
	explicit FunctionWithContinuation(F f, C c) noexcept : function(std::move(f)), continuation_function(std::move(c)) {}

	using result_type = continuable_result_type<F, Args...>;

	auto operator () (Args ... args) -> void
	{
		auto result = std::invoke(std::move(function), std::forward<Args>(args)..., return_result);
		std::invoke(std::move(continuation_function), std::move(result));
	}

	[[nodiscard]] auto operator () (Args ... args, return_result_t) -> result_type
	{
		auto result = std::invoke(std::move(function), std::forward<Args>(args)..., return_result);
		std::invoke(std::move(continuation_function), result);
		return result;
	}

	template <is_continuation<result_type> C2>
	[[nodiscard]] auto then(C2 c) const & -> FunctionWithContinuation<FunctionWithContinuation<F, C, Args...>, C2, Args...>
	{
		return FunctionWithContinuation<FunctionWithContinuation<F, C, Args...>, C2, Args...>(*this, std::move(c));
	}

	template <is_continuation<result_type> C2>
	[[nodiscard]] auto then(C2 c) && noexcept -> FunctionWithContinuation<FunctionWithContinuation<F, C, Args...>, C2, Args...>
	{
		return FunctionWithContinuation<FunctionWithContinuation<F, C, Args...>, C2, Args...>(std::move(*this), std::move(c));
	}

private:
	F function;
	C continuation_function;
};

template <std::move_constructible F, typename ... Args>
	requires(std::invocable<F, Args...>)
struct Continuable
{
	explicit Continuable(F f) noexcept : function(std::move(f)) {}

	using result_type = std::invoke_result_t<F, Args...>;

	auto operator () (Args ... args) -> result_type { return std::invoke(std::move(function), std::forward<Args>(args)...); }
	[[nodiscard]] auto operator () (Args ... args, return_result_t) -> result_type { return std::invoke(std::move(function), std::forward<Args>(args)...); }

	template <is_continuation<result_type> C>
	[[nodiscard]] auto then(C c) const & -> FunctionWithContinuation<Continuable<F, Args...>, C, Args...>
	{
		return FunctionWithContinuation<Continuable<F, Args...>, C, Args...>(*this, std::move(c));
	}

	template <is_continuation<result_type> C>
	[[nodiscard]] auto then(C c) && noexcept -> FunctionWithContinuation<Continuable<F, Args...>, C, Args...>
	{
		return FunctionWithContinuation<Continuable<F, Args...>, C, Args...>(std::move(*this), std::move(c));
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
		auto with_continuation = function.then(std::move(c));
		using continuation_t = decltype(with_continuation);
		return ScheduledContinuation<TaskExecutor, argument_type, result_type, continuation_t>(
			executor,
			std::move(with_continuation)
		);
	}

	template <is_continuation<result_type> C>
	[[nodiscard]] auto then(C c) && noexcept
	{
		auto with_continuation = std::move(function).then(std::move(c));
		using continuation_t = decltype(with_continuation);
		return ScheduledContinuation<TaskExecutor, argument_type, result_type, continuation_t>(
			executor,
			std::move(with_continuation)
		);
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
