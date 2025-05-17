#include "Scheduler.h"

Scheduler::Scheduler()
{
    toDoTasks = new sTask[SCH_MAX_TASKS];
    size = 0;
}

Scheduler::~Scheduler()
{
    delete[] toDoTasks;
    size = 0;
}

uint8_t Scheduler::Add_Task(void (*pTasks)(void), uint32_t delay, uint32_t period)
{
    // if full return false
    if (size == SCH_MAX_TASKS)
        return 0;

    // not accept oneshot task
    if (period == 0)
        return 0;

    // index of add task
    uint32_t index = 0;
    while (index < size && delay >= toDoTasks[index].delay)
    {
        delay -= toDoTasks[index].delay;
        index++;
    }

    // move all tasks from index backward
    for (uint32_t i = size - 1; i >= index; i--)
    {
        toDoTasks[i + 1] = toDoTasks[i];
    }
    // insert current task to this position
    toDoTasks[index] = {pTasks, delay, period, 0, index};

    // update the next task
    if (index != size)
        toDoTasks[index + 1].delay -= delay;
    size++;

    // add task successfully
    return 1;
}

uint8_t Scheduler::Delete_Task(uint32_t taskID)
{
    // there is no task --> return false
    if (size == 0)
        return 0;

    for (uint32_t i = taskID; i < size; i++)
    {
        toDoTasks[i] = toDoTasks[i + 1];
    }

    // delete task successfully
    size--;
    return 1;
}

void Scheduler::Update_Task()
{
    if (toDoTasks[0].pTask && !toDoTasks[0].runMe)
    {
        if (toDoTasks[0].delay <= 0)
        {
            toDoTasks[0].runMe = 1;
        }
        else
            toDoTasks[0].delay--;
    }
}

void Scheduler::Dispatch_Task()
{
    if (toDoTasks[0].runMe)
    {
        // run Task
        (*toDoTasks[0].pTask)();

        void (*pTask)(void) = toDoTasks[0].pTask;
        uint32_t period = toDoTasks[0].period;

        // delete task
        Delete_Task(toDoTasks[0].taskID);

        // readd this task
        Add_Task(pTask, period, period);
    }
}