#include "polymorphic_task.hh"
#include <utility>
#include <cassert>

PolymorphicTask::PolymorphicTask(PolymorphicTask && other) noexcept
{
	move_from_other(other);
}

PolymorphicTask & PolymorphicTask::operator = (PolymorphicTask && other) noexcept
{
	// If this was to be called more in practical situations I would bother to write a version that
	// tries to reuse the buffer when possible but I don't really care. Move assignment is a pretty
	// infrequent operation on tasks in real scenarios.
	destroy();
	virtual_table = nullptr;
	is_small = true;
	move_from_other(other);
	return *this;
}

PolymorphicTask::~PolymorphicTask()
{
	destroy();
}

void PolymorphicTask::operator () ()
{
	assert(virtual_table);
	virtual_table->operator_function_call(buffer());
}

auto PolymorphicTask::buffer() noexcept -> void *
{
	if (is_small)
		return &small_buffer;
	else
		return big_buffer.memory;
}

auto PolymorphicTask::destroy() noexcept -> void
{
	if (virtual_table)
		virtual_table->destructor(buffer());

	if (!is_small)
		::operator delete(big_buffer.memory, big_buffer.used_size, std::align_val_t(big_buffer.object_alignment));
}

auto PolymorphicTask::move_from_other(PolymorphicTask & other) noexcept -> void
{
	if (other.virtual_table)
	{
		if (other.is_small)
		{
			is_small = true;
			virtual_table = other.virtual_table;
			virtual_table->move_constructor(buffer(), other.buffer());
			virtual_table->destructor(other.buffer());
			other.virtual_table = nullptr;
		}
		else // other is big. Steal the memory
		{
			is_small = false;
			virtual_table = other.virtual_table;
			big_buffer = other.big_buffer;
			other.virtual_table = nullptr;
			other.is_small = true;
		}
	}
}
