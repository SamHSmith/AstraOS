
void font9_12_get_bitmap(u64* bitmap)
{
    for(u64 c = 0; c < 256; c++)
    {
        bitmap[2*c+0] = 0xE06068223187000;
        bitmap[2*c+1] = 0x40709222;

        if(c == '!') {
            bitmap[2*c+0] = 0x4070381C0E07010;
            bitmap[2*c+1] = 0x40701000;
        } else if(c == '@') {
            bitmap[2*c+0] = 0x2A954AA59208F800;
            bitmap[2*c+1] = 0xF004D255;
        } else if(c == '#') {
            bitmap[2*c+0] = 0xA1907C251412000;
            bitmap[2*c+1] = 0x49A83417;
        } else if(c == '$') {
            bitmap[2*c+0] = 0x407044200802008220E02000;
            bitmap[2*c+1] = 0x40704420;
        } else if(c == '%') {
            bitmap[2*c+0] = 0x20202C292481800;
            bitmap[2*c+1] = 0x181249432;
        } else if(c == '^') {
            bitmap[2*c+0] = 0x220A02000;
            bitmap[2*c+1] = 0x0;
        } else if(c == '&') {
            bitmap[2*c+0] = 0x301814120A02000;
            bitmap[2*c+1] = 0x270C46229;
        } else if(c == '*') {
            bitmap[2*c+0] = 0x40A8381C1502000;
            bitmap[2*c+1] = 0x0;
        } else if(c == '(') {
            bitmap[2*c+0] = 0x100804020206000;
            bitmap[2*c+1] = 0xC0100402;
        } else if(c == ')') {
            bitmap[2*c+0] = 0x1008040200803000;
            bitmap[2*c+1] = 0x60404020;
        } else if(c == ']') {
            bitmap[2*c+0] = 0x1008040201F00000;
            bitmap[2*c+1] = 0x1F0804020;
        } else if(c == '[') {
            bitmap[2*c+0] = 0x100804021F00000;
            bitmap[2*c+1] = 0x1F0080402;
        } else if(c == '+') {
            bitmap[2*c+0] = 0x3F82010080000000;
            bitmap[2*c+1] = 0x201008;
        } else if(c == '=') {
            bitmap[2*c+0] = 0x7C000000000;
            bitmap[2*c+1] = 0x7C00;
        } else if(c == '!') {
            bitmap[2*c+0] = 0x4070381C0E07010;
            bitmap[2*c+1] = 0x40701000;
        } else if(c == '!') {
            bitmap[2*c+0] = 0x4070381C0E07010;
            bitmap[2*c+1] = 0x40701000;
        } else if(c == '!') {
            bitmap[2*c+0] = 0x4070381C0E07010;
            bitmap[2*c+1] = 0x40701000;
        }
    }
}
