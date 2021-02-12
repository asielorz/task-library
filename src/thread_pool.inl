inline auto as_work_source(TaskQueue & queue, int preferred_queue_index)
{
	// Avoid out of bounds indices.
	int const actual_index = preferred_queue_index % queue.number_of_queues();

	return [&queue, actual_index]()
	{
		return queue.pop_task(actual_index);
	};
}
