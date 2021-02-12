#pragma once

namespace detail
{
	template <typename T> struct first_parameter_type_base {};

	template <typename ClassType, typename Ret, typename First, typename ... Args>
	struct first_parameter_type_base<Ret(ClassType:: *)(First, Args...)>
	{
		using type = First;
	};

	template <typename ClassType, typename Ret, typename First, typename ... Args>
	struct first_parameter_type_base<Ret(ClassType:: *)(First, Args...) const>
	{
		using type = First;
	};
} // namespace detail

template <typename T>
struct first_parameter_type
	: public detail::first_parameter_type_base<decltype(&T::operator())>
{};

template <typename Ret, typename First, typename ... Args>
struct first_parameter_type<Ret(*)(First, Args...)>
{
	using type = First;
};

template <typename T>
using first_parameter_type_t = typename first_parameter_type<T>::type;
