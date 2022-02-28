

// expressions point into the original string that was parsed

typedef struct
{
    u8* text;
    u64 text_len;
} Expression;


// returns expression count
u64 parse_string_into_expressions(u8* source_text, u64 source_text_len, Expression* exp_array, u64 exp_array_size)
{
    u8* final_char = source_text + source_text_len;
    u8* current_char = source_text;
    u8* dest_char = current_char;

    Expression* final_expression = exp_array + exp_array_size;
    Expression* current_expression = exp_array - 1;
    u64 expression_count = 0;

    u8 new_expression = 1;
    u8 skip_if_blankspace = 1;

    u8 was_backslash = 0;

    u8 is_in_citation = 0;

    while(current_char != final_char)
    {
        if(skip_if_blankspace && *current_char == ' ')
        { goto next_loop; }
        if(new_expression)
        {
            expression_count++;
            current_expression++;
            if(current_expression == final_expression)
            { break; }
            current_expression->text = dest_char;
            current_expression->text_len = 0;
            new_expression = 0;
        }
        skip_if_blankspace = 0;

        if(was_backslash)
        {
            if(*current_char == ' ')
            {
                was_backslash = 0;
                goto write_char;
            }
            was_backslash = 0;
        }
        else
        {
            if(*current_char == '\\')
            { was_backslash = 1; goto next_loop; }
            else if(*current_char == '"')
            { is_in_citation = !is_in_citation; goto next_loop; }
        }

        if(!is_in_citation && *current_char == ' ')
        { skip_if_blankspace = 1; new_expression = 1; goto next_loop; }

write_char:
        *(dest_char++) = *current_char;
        current_expression->text_len++;

next_loop:
        current_char++;
    }
    return expression_count;
}
