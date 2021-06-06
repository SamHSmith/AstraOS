
typedef struct
{
    u64 keys_down[4];
} KeyboardState;

typedef struct
{
    u8 event;
    u8 scancode;
    KeyboardState current_state;
} KeyboardEvent;

// Nothing implies there are no events
#define KEYBOARD_EVENT_NOTHING 0
#define KEYBOARD_EVENT_PRESSED 1
#define KEYBOARD_EVENT_RELEASED 2

#define KEYBOARD_EVENT_QUEUE_LEN 2028
typedef struct
{
    KeyboardState current_state;
    u64 count;
    u16 new_events[KEYBOARD_EVENT_QUEUE_LEN];
} KeyboardEventQueue;

void keyboard_put_new_event(KeyboardEventQueue* queue, u8 event, u8 scancode)
{
    if(queue->count + 1 >= KEYBOARD_EVENT_QUEUE_LEN)
    {  return;  }

    u16 val = ((u16)scancode) << 8;
    val |= event;
    queue->new_events[queue->count] = val;
    queue->count += 1;
}

void keyboard_poll_events(KeyboardEventQueue* queue, KeyboardEvent* event)
{
    u16* event16 = (u16*)event;
    if(queue->count > 0)
    {
        *event16 = queue->new_events[0];
        queue->count -= 1;

        for(u64 i = 0; i < queue->count; i++)
        {
            queue->new_events[i] = queue->new_events[i+1];
        }

        u64 change_index = event->scancode >> 6;
        u64 change = ((u64)1) << (event->scancode & 0x3F);
        if(event->event == KEYBOARD_EVENT_PRESSED)
        {
            queue->current_state.keys_down[change_index] |= change;
        }
        else if(event->event == KEYBOARD_EVENT_RELEASED)
        {
            queue->current_state.keys_down[change_index] &= ~change;
        }
    }
    else
    {
        event->event = KEYBOARD_EVENT_NOTHING;
    }
    event->current_state = queue->current_state;
}

typedef struct
{
    f64 delta_x;
    f64 delta_y;
    f64 delta_z;
    u32 buttons_down;
    u8 button;
    u8 pressed;
    u8 released;
} RawMouseEvent;

#define RAWMOUSE_EVENT_QUEUE_LEN 511

typedef struct
{
    u32 buttons_down;
    u32 event_count;
    RawMouseEvent events[RAWMOUSE_EVENT_QUEUE_LEN];
} RawMouseEventQueue;

void new_mouse_input(RawMouseEventQueue* queue, f64 dx, f64 dy, f64 dz, u8 button, u8 pressed, u8 released)
{
    if(queue->event_count + 1 >= RAWMOUSE_EVENT_QUEUE_LEN)
    { return; }
    RawMouseEvent* event = queue->events + queue->event_count;
    queue->event_count++;

    event->delta_x = dx;
    event->delta_y = dy;
    event->delta_z = dz;
    event->buttons_down = queue->buttons_down;
    if(pressed)
    {
        queue->buttons_down |= (u32)1 << button;
    }
    else if(released)
    {
        queue->buttons_down &= ~((u32)1 << button);
    }
    event->button = button;
    event->pressed = pressed;
    event->released = released;
}

