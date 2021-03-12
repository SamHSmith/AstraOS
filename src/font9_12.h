
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
        } else if(c == '\'') {
            bitmap[2*c+0] = 0x6060301800000;
            bitmap[2*c+1] = 0x0;
        } else if(c == '"') {
            bitmap[2*c+0] = 0x6C361B0D800;
            bitmap[2*c+1] = 0x0;
        } else if(c == ',') {
            bitmap[2*c+0] = 0x0;
            bitmap[2*c+1] = 0x2020301800;
        } else if(c == '.') {
            bitmap[2*c+0] = 0x0;
            bitmap[2*c+1] = 0x301800;
        } else if(c == '<') {
            bitmap[2*c+0] = 0x100808080800000;
            bitmap[2*c+1] = 0x401004;
        } else if(c == '>') {
            bitmap[2*c+0] = 0x1008020080200000;
            bitmap[2*c+1] = 0x101010;
        } else if(c == '/') {
            bitmap[2*c+0] = 0x202020101010000;
            bitmap[2*c+1] = 0x8080404;
        } else if(c == '?') {
            bitmap[2*c+0] = 0x404040201107000;
            bitmap[2*c+1] = 0x40701000;
        } else if(c == '{') {
            bitmap[2*c+0] = 0x30181008040C000;
            bitmap[2*c+1] = 0x180201008;
        } else if(c == '}') {
            bitmap[2*c+0] = 0x180C010080401800;
            bitmap[2*c+1] = 0x30201008;
        } else if(c == '\\') {
            bitmap[2*c+0] = 0x802008040100400;
            bitmap[2*c+1] = 0x200804010;
        } else if(c == '|') {
            bitmap[2*c+0] = 0x402010080402000;
            bitmap[2*c+1] = 0x40201008;
        } else if(c == ' ') {
            bitmap[2*c+0] = 0x0;
            bitmap[2*c+1] = 0x0;
        } else if(c == '-') {
            bitmap[2*c+0] = 0x3F80000000000000;
            bitmap[2*c+1] = 0x0;
        } else if(c == '_') {
            bitmap[2*c+0] = 0x0;
            bitmap[2*c+1] = 0x3F8000000;
        } else if(c == ';') {
            bitmap[2*c+0] = 0x180C0000000;
            bitmap[2*c+1] = 0x6060300000;
        } else if(c == ':') {
            bitmap[2*c+0] = 0x180C0000000;
            bitmap[2*c+1] = 0x60300000;
        }

          else if(c == 'a') {
            bitmap[2*c+0] = 0x1E00000000000000;
            bitmap[2*c+1] = 0x2E0884422;
        } else if(c == 'b') {
            bitmap[2*c+0] = 0x260D008040201000;
            bitmap[2*c+1] = 0x1A1308844;
        } else if(c == 'c') {
            bitmap[2*c+0] = 0x107000000000000;
            bitmap[2*c+1] = 0xE0080402;
        } else if(c == 'd') {
            bitmap[2*c+0] = 0xF04020100804000;
            bitmap[2*c+1] = 0x230A42211;
        } else if(c == 'e') {
            bitmap[2*c+0] = 0x1107000000000000;
            bitmap[2*c+1] = 0xE0083C22;
        } else if(c == 'f') {
            bitmap[2*c+0] = 0x103C0404020E000;
            bitmap[2*c+1] = 0x10080402;
        } else if(c == 'g') {
            bitmap[2*c+0] = 0x904838000000000;
            bitmap[2*c+1] = 0x1C11070040C;
        } else if(c == 'h') {
            bitmap[2*c+0] = 0xD00804020100800;
            bitmap[2*c+1] = 0x110884426;
        } else if(c == 'i') {
            bitmap[2*c+0] = 0x403000000402000;
            bitmap[2*c+1] = 0x40201008;
        } else if(c == 'j') {
            bitmap[2*c+0] = 0x403000000402000;
            bitmap[2*c+1] = 0x6048201008;
        } else if(c == 'k') {
            bitmap[2*c+0] = 0x908804020100000;
            bitmap[2*c+1] = 0x210882C0A;
        } else if(c == 'l') {
            bitmap[2*c+0] = 0x804020100C00000;
            bitmap[2*c+1] = 0x100402010;
        } else if(c == 'm') {
            bitmap[2*c+0] = 0x1B00000000000000;
            bitmap[2*c+1] = 0x249249249;
        } else if(c == 'n') {
            bitmap[2*c+0] = 0x500000000000000;
            bitmap[2*c+1] = 0x90482416;
        } else if(c == 'o') {
            bitmap[2*c+0] = 0xE00000000000000;
            bitmap[2*c+1] = 0xE0884422;
        } else if(c == 'p') {
            bitmap[2*c+0] = 0xF0884C1A0000000;
            bitmap[2*c+1] = 0x10080402;
        } else if(c == 'q') {
            bitmap[2*c+0] = 0x1E08844221E00000;
            bitmap[2*c+1] = 0x100C0C020;
        } else if(c == 'r') {
            bitmap[2*c+0] = 0x306800000000000;
            bitmap[2*c+1] = 0x10080402;
        } else if(c == 's') {
            bitmap[2*c+0] = 0x1E00000000000000;
            bitmap[2*c+1] = 0xF0803802;
        } else if(c == 't') {
            bitmap[2*c+0] = 0x40F010080000000;
            bitmap[2*c+1] = 0x180201008;
        } else if(c == 'u') {
            bitmap[2*c+0] = 0x1100000000000000;
            bitmap[2*c+1] = 0x2E0884422;
        } else if(c == 'v') {
            bitmap[2*c+0] = 0x1110400000000000;
            bitmap[2*c+1] = 0x40502822;
        } else if(c == 'w') {
            bitmap[2*c+0] = 0x2080000000000000;
            bitmap[2*c+1] = 0x11154AA49;
        } else if(c == 'x') {
            bitmap[2*c+0] = 0x1100000000000000;
            bitmap[2*c+1] = 0x110501014;
        } else if(c == 'y') {
            bitmap[2*c+0] = 0xA05044220000000;
            bitmap[2*c+1] = 0x2020101008;
        } else if(c == 'z') {
            bitmap[2*c+0] = 0x1F00000000000000;
            bitmap[2*c+1] = 0x1F0101010;
        }
    }
}
