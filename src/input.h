
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

RawMouse fetch_mouse_data(RawMouse* ptr)
{
    RawMouse ret = *ptr;
printf("Mouse raw: x: %lf, y:%lf, p:%x, r:%x, h:%x\n",ret.x, ret.y, ret.pressed, ret.released, ret.down);
    ptr->x = 0.0;
    ptr->y = 0.0;
    ptr->z = 0.0;
    ptr->pressed = 0;
    ptr->released = 0;
    return ret;
}
