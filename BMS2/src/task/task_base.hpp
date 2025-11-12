#ifndef TASK_BASE_HPP
#define TASK_BASE_HPP

#include <cstdint>


namespace task_base {

class TaskBase {
	public:
		TaskBase(uint32_t period);

		// Virtual destructor for proper cleanup of derived classes
		virtual ~TaskBase() = default;

		// Pure virtual function to be implemented by derived classes
		virtual void task() = 0;

		// Function to start the task
		static void taskWrapper(void* self_ptr);

	private:
		uint32_t period_ms;
};





} // namespace task_base

#endif // TASK_BASE_HPP