#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdint.h>

#define SCH_MAX_TASKS 20

typedef struct
{
    // function pointer
    void (*pTask)(void);
    uint32_t delay;
    uint32_t period;
    uint8_t runMe;
    uint32_t taskID;
} sTask;

class Scheduler
{
private:
    sTask *toDoTasks;
    uint32_t size;

public:
    Scheduler();
    ~Scheduler();
    uint8_t Add_Task(void (*pTasks)(void), uint32_t delay, uint32_t period);
    uint8_t Delete_Task(uint32_t taskID);
    void Update_Task();
    void Dispatch_Task();
};

#endif
