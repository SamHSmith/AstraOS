void append_scancode_to_string(u8 c, KeyboardState ks, char* s)
{
    char to_add = 0;
    if(ks.keys_down[3] & ( (((u64)1) << 33) | (((u64)1) << 37) )) //shift
    {

    if(c == 30) { to_add = '!'; }
    if(c == 31) { to_add = '@'; }
    if(c == 32) { to_add = '#'; }
    if(c == 33) { to_add = '$'; }
    if(c == 34) { to_add = '%'; }
    if(c == 35) { to_add = '^'; }
    if(c == 36) { to_add = '&'; }
    if(c == 37) { to_add = '*'; }
    if(c == 38) { to_add = '('; }
    if(c == 39) { to_add = ')'; }
    if(c == 45) { to_add = ']'; }
    if(c == 46) { to_add = '+'; }
 
    if(c == 20) { to_add = '"'; }
    if(c == 26) { to_add = '<'; }
    if(c == 8 ) { to_add = '>'; }
    if(c == 21) { to_add = 'P'; }
    if(c == 23) { to_add = 'Y'; }
    if(c == 28) { to_add = 'F'; }
    if(c == 24) { to_add = 'G'; }
    if(c == 12) { to_add = 'C'; }
    if(c == 18) { to_add = 'R'; }
    if(c == 19) { to_add = 'L'; }
    if(c == 47) { to_add = '?'; }
    if(c == 48) { to_add = '}'; }
    if(c == 49) { to_add = '|'; }
 
    if(c == 4 ) { to_add = 'A'; }
    if(c == 22) { to_add = 'O'; }
    if(c == 7 ) { to_add = 'E'; }
    if(c == 9 ) { to_add = 'U'; }
    if(c == 10) { to_add = 'I'; }
    if(c == 11) { to_add = 'D'; }
    if(c == 13) { to_add = 'H'; }
    if(c == 14) { to_add = 'T'; }
    if(c == 15) { to_add = 'N'; }
    if(c == 51) { to_add = 'S'; }
    if(c == 52) { to_add = '_'; }
 
    if(c == 29) { to_add = 'Q'; }
    if(c == 27) { to_add = 'J'; }
    if(c == 6 ) { to_add = 'K'; }
    if(c == 25) { to_add = 'X'; }
    if(c == 5 ) { to_add = 'B'; }
    if(c == 17) { to_add = 'M'; }
    if(c == 16) { to_add = 'W'; }
    if(c == 54) { to_add = 'V'; }
    if(c == 55) { to_add = 'Z'; }
    if(c == 56) { to_add = ':'; }

    }
    else
    {

    if(c == 30) { to_add = '1'; }
    if(c == 31) { to_add = '2'; }
    if(c == 32) { to_add = '3'; }
    if(c == 33) { to_add = '4'; }
    if(c == 34) { to_add = '5'; }
    if(c == 35) { to_add = '6'; }
    if(c == 36) { to_add = '7'; }
    if(c == 37) { to_add = '8'; }
    if(c == 38) { to_add = '9'; }
    if(c == 39) { to_add = '0'; }
    if(c == 45) { to_add = '['; }
    if(c == 46) { to_add = '='; }

    if(c == 20) { to_add = '\''; }
    if(c == 26) { to_add = ','; }
    if(c == 8) { to_add = '.'; }
    if(c == 21) { to_add = 'p'; }
    if(c == 23) { to_add = 'y'; }
    if(c == 28) { to_add = 'f'; }
    if(c == 24) { to_add = 'g'; }
    if(c == 12) { to_add = 'c'; }
    if(c == 18) { to_add = 'r'; }
    if(c == 19) { to_add = 'l'; }
    if(c == 47) { to_add = '/'; }
    if(c == 48) { to_add = '{'; }
    if(c == 49) { to_add = '\\'; }
 
    if(c == 4 ) { to_add = 'a'; }
    if(c == 22) { to_add = 'o'; }
    if(c == 7 ) { to_add = 'e'; }
    if(c == 9 ) { to_add = 'u'; }
    if(c == 10) { to_add = 'i'; }
    if(c == 11) { to_add = 'd'; }
    if(c == 13) { to_add = 'h'; }
    if(c == 14) { to_add = 't'; }
    if(c == 15) { to_add = 'n'; }
    if(c == 51) { to_add = 's'; }
    if(c == 52) { to_add = '-'; }
 
    if(c == 29) { to_add = 'q'; }
    if(c == 27) { to_add = 'j'; }
    if(c == 6 ) { to_add = 'k'; }
    if(c == 25) { to_add = 'x'; }
    if(c == 5 ) { to_add = 'b'; }
    if(c == 17) { to_add = 'm'; }
    if(c == 16) { to_add = 'w'; }
    if(c == 54) { to_add = 'v'; }
    if(c == 55) { to_add = 'z'; }
    if(c == 56) { to_add = ';'; }
    }

    if(c == 44) { to_add = ' '; }

    s[strlen(s)+1] = 0;
    s[strlen(s)] = to_add;
}
