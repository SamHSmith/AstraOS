u8 scancode_to_u8(u8 c, AOS_KeyboardState ks)
{
AOS_H_printf("%u\n", c);
    u8 to_add = 0;

if(ks.keys_down[0] & (1llu << 4)) // Altgr
{
    if(ks.keys_down[0] & ((1llu << 1) | (1llu << 2))) //shift
    {
    if(c == 9 ) { to_add = '!'; }
    if(c == 10) { to_add = '\"'; }
    if(c == 11) { to_add = '#'; }
    if(c == 12) { to_add = '$'; }
    if(c == 13) { to_add = '%'; }
    if(c == 14) { to_add = '&'; }
    if(c == 15) { to_add = '/'; }
    if(c == 16) { to_add = '('; }
    if(c == 17) { to_add = ')'; }
    if(c == 18) { to_add = '='; }
    if(c == 19) { to_add = '?'; }
    if(c == 20) { to_add = '\`'; }
 
    if(c == 23) { to_add = 'Q'; }
    if(c == 24) { to_add = 'W'; }
    if(c == 25) { to_add = 'E'; }
    if(c == 26) { to_add = 'R'; }
    if(c == 27) { to_add = 'T'; }
    if(c == 28) { to_add = 'Y'; }
    if(c == 29) { to_add = 'U'; }
    if(c == 30) { to_add = 'I'; }
    if(c == 31) { to_add = 'O'; }
    if(c == 32) { to_add = 'P'; }
    if(c == 33) { to_add = 143; }
    if(c == 34) { to_add = '^'; }
    if(c == 48) { to_add = '*'; }
 
    if(c == 36) { to_add = 'A'; }
    if(c == 37) { to_add = 'S'; }
    if(c == 38) { to_add = 'D'; }
    if(c == 39) { to_add = 'F'; }
    if(c == 40) { to_add = 'G'; }
    if(c == 41) { to_add = 'H'; }
    if(c == 42) { to_add = 'J'; }
    if(c == 43) { to_add = 'K'; }
    if(c == 44) { to_add = 'L'; }
    if(c == 45) { to_add = 153; }
    if(c == 46) { to_add = 142; }

    if(c == 47) { to_add = '*'; }
 
    if(c == 91) { to_add = '>'; }

    if(c == 49) { to_add = 'Z'; }
    if(c == 50) { to_add = 'X'; }
    if(c == 51) { to_add = 'C'; }
    if(c == 52) { to_add = 'V'; }
    if(c == 53) { to_add = 'B'; }
    if(c == 54) { to_add = 'N'; }
    if(c == 55) { to_add = 'M'; }
    if(c == 56) { to_add = ';'; }
    if(c == 57) { to_add = ':'; }
    if(c == 58) { to_add = '_'; }

    }
    else
    {
    if(c == 9 ) { to_add = '1'; }
    if(c == 10) { to_add = '2'; }
    if(c == 11) { to_add = '3'; }
    if(c == 12) { to_add = '4'; }
    if(c == 13) { to_add = '5'; }
    if(c == 14) { to_add = '6'; }
    if(c == 15) { to_add = '7'; }
    if(c == 16) { to_add = '8'; }
    if(c == 17) { to_add = '9'; }
    if(c == 18) { to_add = '0'; }
    if(c == 19) { to_add = '\\'; }
    if(c == 20) { to_add = 2; }

    if(c == 23) { to_add = 'q'; }
    if(c == 24) { to_add = 'w'; }
    if(c == 25) { to_add = 'e'; }
    if(c == 26) { to_add = 'r'; }
    if(c == 27) { to_add = 't'; }
    if(c == 28) { to_add = 'y'; }
    if(c == 29) { to_add = 'u'; }
    if(c == 30) { to_add = 'i'; }
    if(c == 31) { to_add = 'o'; }
    if(c == 32) { to_add = 'p'; }
    if(c == 33) { to_add = 134; }
    if(c == 34) { to_add = 228; }
    if(c == 48) { to_add = '\''; }
 
    if(c == 36) { to_add = 'a'; }
    if(c == 37) { to_add = 's'; }
    if(c == 38) { to_add = 'd'; }
    if(c == 39) { to_add = 'f'; }
    if(c == 40) { to_add = 'g'; }
    if(c == 41) { to_add = 'h'; }
    if(c == 42) { to_add = 'j'; }
    if(c == 43) { to_add = 'k'; }
    if(c == 44) { to_add = 'l'; }
    if(c == 45) { to_add = 148; }
    if(c == 46) { to_add = 132; }

    if(c == 49) { to_add = 'z'; }
    if(c == 50) { to_add = 'x'; }
    if(c == 51) { to_add = 'c'; }
    if(c == 52) { to_add = 'v'; }
    if(c == 53) { to_add = 'b'; }
    if(c == 54) { to_add = 'n'; }
    if(c == 55) { to_add = 'm'; }
    if(c == 56) { to_add = ','; }
    if(c == 57) { to_add = '.'; }
    if(c == 58) { to_add = '-'; }
    }
}
else
{
    if(ks.keys_down[0] & ((1llu << 1) | (1llu << 2))) //shift
    {
    if(c == 9 ) { to_add = '!'; }
    if(c == 10) { to_add = '\"'; }
    if(c == 11) { to_add = '#'; }
    if(c == 12) { to_add = '$'; }
    if(c == 13) { to_add = '%'; }
    if(c == 14) { to_add = '&'; }
    if(c == 15) { to_add = '/'; }
    if(c == 16) { to_add = '('; }
    if(c == 17) { to_add = ')'; }
    if(c == 18) { to_add = '='; }
    if(c == 19) { to_add = '?'; }
    if(c == 20) { to_add = '\`'; }

    if(c == 23) { to_add = 'Q'; }
    if(c == 24) { to_add = 'W'; }
    if(c == 25) { to_add = 'E'; }
    if(c == 26) { to_add = 'R'; }
    if(c == 27) { to_add = 'T'; }
    if(c == 28) { to_add = 'Y'; }
    if(c == 29) { to_add = 'U'; }
    if(c == 30) { to_add = 'I'; }
    if(c == 31) { to_add = 'O'; }
    if(c == 32) { to_add = 'P'; }
    if(c == 33) { to_add = 143; }
    if(c == 34) { to_add = '^'; }
    if(c == 48) { to_add = '*'; }

    if(c == 36) { to_add = 'A'; }
    if(c == 37) { to_add = 'S'; }
    if(c == 38) { to_add = 'D'; }
    if(c == 39) { to_add = 'F'; }
    if(c == 40) { to_add = 'G'; }
    if(c == 41) { to_add = 'H'; }
    if(c == 42) { to_add = 'J'; }
    if(c == 43) { to_add = 'K'; }
    if(c == 44) { to_add = 'L'; }
    if(c == 45) { to_add = 153; }
    if(c == 46) { to_add = 142; }

    if(c == 47) { to_add = '*'; }

    if(c == 91) { to_add = '>'; }

    if(c == 49) { to_add = 'Z'; }
    if(c == 50) { to_add = 'X'; }
    if(c == 51) { to_add = 'C'; }
    if(c == 52) { to_add = 'V'; }
    if(c == 53) { to_add = 'B'; }
    if(c == 54) { to_add = 'N'; }
    if(c == 55) { to_add = 'M'; }
    if(c == 56) { to_add = ';'; }
    if(c == 57) { to_add = ':'; }
    if(c == 58) { to_add = '_'; }

    }
    else
    {
    if(c == 9 ) { to_add = '1'; }
    if(c == 10) { to_add = '2'; }
    if(c == 11) { to_add = '3'; }
    if(c == 12) { to_add = '4'; }
    if(c == 13) { to_add = '5'; }
    if(c == 14) { to_add = '6'; }
    if(c == 15) { to_add = '7'; }
    if(c == 16) { to_add = '8'; }
    if(c == 17) { to_add = '9'; }
    if(c == 18) { to_add = '0'; }
    if(c == 19) { to_add = '+'; }
    if(c == 20) { to_add = 2; }

    if(c == 23) { to_add = 'q'; }
    if(c == 24) { to_add = 'w'; }
    if(c == 25) { to_add = 'e'; }
    if(c == 26) { to_add = 'r'; }
    if(c == 27) { to_add = 't'; }
    if(c == 28) { to_add = 'y'; }
    if(c == 29) { to_add = 'u'; }
    if(c == 30) { to_add = 'i'; }
    if(c == 31) { to_add = 'o'; }
    if(c == 32) { to_add = 'p'; }
    if(c == 33) { to_add = 134; }
    if(c == 34) { to_add = 228; }
    if(c == 48) { to_add = '\''; }

    if(c == 36) { to_add = 'a'; }
    if(c == 37) { to_add = 's'; }
    if(c == 38) { to_add = 'd'; }
    if(c == 39) { to_add = 'f'; }
    if(c == 40) { to_add = 'g'; }
    if(c == 41) { to_add = 'h'; }
    if(c == 42) { to_add = 'j'; }
    if(c == 43) { to_add = 'k'; }
    if(c == 44) { to_add = 'l'; }
    if(c == 45) { to_add = 148; }
    if(c == 46) { to_add = 132; }

    if(c == 49) { to_add = 'z'; }
    if(c == 50) { to_add = 'x'; }
    if(c == 51) { to_add = 'c'; }
    if(c == 52) { to_add = 'v'; }
    if(c == 53) { to_add = 'b'; }
    if(c == 54) { to_add = 'n'; }
    if(c == 55) { to_add = 'm'; }
    if(c == 56) { to_add = ','; }
    if(c == 57) { to_add = '.'; }
    if(c == 58) { to_add = '-'; }
    }
}

    if(c == 60) { to_add = ' '; }

    return to_add;
}
