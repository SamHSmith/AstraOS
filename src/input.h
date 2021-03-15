
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
    f64 x;
    f64 y;
    f64 z;
    u32 pressed;
    u32 released;
    u32 down;
} RawMouse;

void new_mouse_input_from_serial(RawMouse* mouse, s32 mouse_data[3])
{
    mouse->x += (f64)mouse_data[0];
    mouse->y += (f64)mouse_data[1];

    mouse->pressed |= (~mouse->down) & mouse_data[2];
    mouse->released |= (~mouse_data[2]) & mouse->down;
    mouse->down = mouse_data[2];
}

RawMouse fetch_mouse_data(RawMouse* ptr)
{
    RawMouse ret = *ptr;
    ptr->x = 0.0;
    ptr->y = 0.0;
    ptr->z = 0.0;
    ptr->pressed = 0;
    ptr->released = 0;
    return ret;
}
