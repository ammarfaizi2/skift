#include <libsystem/RingBuffer.h>
#include <libsystem/assert.h>
#include <libsystem/atomic.h>

#include "interrupts/Dispatcher.h"
#include "sheduling/TaskBlocker.h"
#include "tasking.h"

static RingBuffer *_interupts_to_dispatch = NULL;
static DispatcherInteruptHandler _interupts_to_handlers[255] = {};

void dispatcher_initialize(void)
{
    _interupts_to_dispatch = ringbuffer_create(1024);

    Task *interrupts_dispatcher_task = task_spawn(NULL, "InterruptsDispatcher", dispatcher_service, NULL, false);
    task_go(interrupts_dispatcher_task);
}

void dispatcher_dispatch(int interrupt)
{
    ringbuffer_putc(_interupts_to_dispatch, interrupt);
}

bool dispatcher_can_unblock(TaskBlocker *blocker, struct Task *task)
{
    __unused(blocker);
    __unused(task);

    return !ringbuffer_is_empty(_interupts_to_dispatch);
}

void dispatcher_service(void)
{
    while (true)
    {
        atomic_begin();

        bool should_block = ringbuffer_is_empty(_interupts_to_dispatch);

        atomic_end();

        if (should_block)
        {
            TaskBlocker *blocker = __create(TaskBlocker);
            blocker->can_unblock = (TaskBlockerCanUnblock)dispatcher_can_unblock;
            task_block(sheduler_running(), blocker);
        }

        atomic_begin();

        int interrupt = ringbuffer_getc(_interupts_to_dispatch);

        atomic_end();

        if (_interupts_to_handlers[interrupt])
        {
            _interupts_to_handlers[interrupt]();
        }
        else
        {
            logger_warn("No handler for interrupt %d!", interrupt);
        }
    }
}

void dispatcher_register_handler(int interrupt, DispatcherInteruptHandler handler)
{
    assert(!_interupts_to_handlers[interrupt]);
    _interupts_to_handlers[interrupt] = handler;
}

void dispatcher_unregister_handler(DispatcherInteruptHandler handler)
{
    for (int i = 0; i < 255; i++)
    {
        if (_interupts_to_handlers[i] == handler)
        {
            _interupts_to_handlers[i] = NULL;
        }
    }
}