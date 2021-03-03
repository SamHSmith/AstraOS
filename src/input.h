
typedef struct
{
    f64 x;
    f64 y;
    f64 z;
    u32 pressed;
    u32 released;
    u32 down;
} RawMouse;

RawMouse mouse;

void new_mouse_input_from_serial(s32 mouse_data[3])
{
    mouse.x += (f64)mouse_data[0];
    mouse.y += (f64)mouse_data[1];

    mouse.pressed |= (~mouse.down) & mouse_data[2];
    mouse.released |= (~mouse_data[2]) & mouse.down;
    mouse.down = mouse_data[2];
}
