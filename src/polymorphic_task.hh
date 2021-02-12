#pragma once

#include <type_traits>
#include <concepts>

template <typename T>
using function_ptr = T *;

template <typename F>
concept is_task = std::move_constructible<F> && std::invocable<F>;

struct PolymorphicTask
{
	struct VirtualTable
	{
		function_ptr<void(void *) noexcept> destructor;
		function_ptr<void(void *, void *) noexcept> move_constructor;
		function_ptr<void(void *)> operator_function_call;
	};
	template <typename T>
	static constexpr auto virtual_table_for() noexcept -> VirtualTable;

	constexpr PolymorphicTask() noexcept = default;

	template <is_task F>
	PolymorphicTask(F f);

	PolymorphicTask(PolymorphicTask const &) = delete;
	PolymorphicTask & operator = (PolymorphicTask const &) = delete;

	PolymorphicTask(PolymorphicTask && other) noexcept;
	PolymorphicTask & operator = (PolymorphicTask && other) noexcept;

	~PolymorphicTask();

	constexpr explicit operator bool() const noexcept { return virtual_table != nullptr; }

	void operator () ();

private:
	auto buffer() noexcept -> void *;
	auto destroy() noexcept -> void;
	auto move_from_other(PolymorphicTask & other) noexcept -> void;

	static constexpr size_t small_buffer_size = 32;
	static_assert(small_buffer_size >= 4 * sizeof(void *));
	static constexpr size_t small_buffer_alignment = 8;
	static_assert(small_buffer_alignment >= alignof(void *));

	union
	{
		std::aligned_storage_t<small_buffer_size, small_buffer_alignment> small_buffer = {0};
		struct
		{
			void * memory;
			size_t used_size;
			size_t capacity;
			size_t object_alignment;
		} big_buffer;
	};
	VirtualTable const * virtual_table = nullptr;
	bool is_small = true;
};

template <typename T>
constexpr auto PolymorphicTask::virtual_table_for() noexcept -> VirtualTable
{
	return PolymorphicTask::VirtualTable{
		[](void * obj) noexcept { static_cast<T *>(obj)->~T(); },
		[](void * to, void * from) noexcept { ::new(to) T(std::move(*static_cast<T *>(from))); },
		[](void * obj) noexcept { std::invoke(std::move(*static_cast<T *>(obj))); },
	};
}

template <typename T>
constexpr PolymorphicTask::VirtualTable polymorphic_task_virtual_table_for = PolymorphicTask::virtual_table_for<T>();

template <is_task F>
PolymorphicTask::PolymorphicTask(F f)
	: virtual_table(&polymorphic_task_virtual_table_for<F>)
{
	if constexpr (sizeof(F) <= small_buffer_size && alignof(F) <= small_buffer_alignment)
	{
		is_small = true;
		::new(buffer()) F(std::move(f));
	}
	else
	{
		is_small = false;
		big_buffer.memory = ::operator new(sizeof(F), std::align_val_t(alignof(F)));
		big_buffer.used_size = sizeof(F);
		big_buffer.capacity = sizeof(F);
		big_buffer.object_alignment = alignof(F);
		::new(big_buffer.memory) F(std::move(f));
	}
}
