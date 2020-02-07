#include <libsystem/assert.h>
#include <libsystem/eventloop/EventLoop.h>
#include <libsystem/eventloop/Notifier.h>
#include <libsystem/list.h>
#include <libsystem/logger.h>

static List *_eventloop_notifiers = NULL;

static size_t _eventloop_handles_count;
static Handle *_eventloop_handles[PROCESS_HANDLE_COUNT];
static SelectEvent _eventloop_events[PROCESS_HANDLE_COUNT];

static bool _eventloop_is_running = false;
static bool _eventloop_is_initialize = false;
static int _eventloop_exit_value = 0;

void eventloop_initilize(void)
{
    assert(!_eventloop_is_initialize);

    _eventloop_notifiers = list_create();

    _eventloop_is_initialize = true;
}

void eventloop_uninitialize(void)
{
    assert(_eventloop_is_initialize);
    assert(!_eventloop_is_running);

    list_destroy(_eventloop_notifiers, LIST_KEEP_VALUES);

    _eventloop_is_initialize = false;
}

int eventloop_run(void)
{
    assert(_eventloop_is_initialize);
    assert(!_eventloop_is_running);

    _eventloop_is_running = true;

    while (_eventloop_is_running)
    {
        eventloop_pump();
    }

    eventloop_uninitialize();

    return _eventloop_exit_value;
}

void eventloop_pump(void)
{
    assert(_eventloop_is_initialize);

    Handle *selected = NULL;
    SelectEvent selected_events = 0;

    error_t result = handle_select(&_eventloop_handles[0], &_eventloop_events[0], _eventloop_handles_count, &selected, &selected_events);

    if (result != ERR_SUCCESS)
    {
        logger_error("Failled to select : %s", error_to_string(result));
        return;
    }

    list_foreach(Notifier, notifier, _eventloop_notifiers)
    {
        if (notifier->handle == selected)
        {
            if ((selected_events & SELECT_READ) &&
                (notifier->on_ready_to_read != NULL))
            {
                notifier->on_ready_to_read(notifier, notifier->handle);
            }

            if ((selected_events & SELECT_WRITE) &&
                (notifier->on_ready_to_write != NULL))
            {
                notifier->on_ready_to_write(notifier, notifier->handle);
            }

            if ((selected_events & SELECT_SEND) &&
                (notifier->on_ready_to_send != NULL))
            {
                notifier->on_ready_to_send(notifier, notifier->handle);
            }

            if ((selected_events & SELECT_RECEIVE) &&
                (notifier->on_ready_to_receive != NULL))
            {
                notifier->on_ready_to_receive(notifier, notifier->handle);
            }

            if ((selected_events & SELECT_CONNECT) &&
                (notifier->on_ready_to_connect != NULL))
            {
                notifier->on_ready_to_connect(notifier, notifier->handle);
            }

            if ((selected_events & SELECT_ACCEPT) &&
                (notifier->on_ready_to_accept != NULL))
            {
                notifier->on_ready_to_accept(notifier, notifier->handle);
            }
        }
    }
}

void eventloop_exit(int exit_value)
{
    assert(_eventloop_is_initialize);
    assert(_eventloop_is_running);

    _eventloop_is_running = false;
    _eventloop_exit_value = exit_value;
}

void eventloop_update_notifier(void)
{
    _eventloop_handles_count = 0;

    list_foreach(Notifier, notifier, _eventloop_notifiers)
    {
        _eventloop_handles[_eventloop_handles_count] = notifier->handle;
        _eventloop_events[_eventloop_handles_count] = notifier->events;

        _eventloop_handles_count++;
    }
}
void eventloop_register_notifier(Notifier *notifier)
{
    assert(_eventloop_is_initialize);

    list_pushback(_eventloop_notifiers, notifier);

    eventloop_update_notifier();
}

void eventloop_unregister_notifier(Notifier *notifier)
{
    assert(_eventloop_is_initialize);

    list_remove(_eventloop_notifiers, notifier);

    eventloop_update_notifier();
}