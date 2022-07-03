
#ifndef __BITFIELD_ALLOCATOR
#define __BITFIELD_ALLOCATOR

#include "types.h"

typedef struct
{
    u64 field_count;
    u64 field_data[];
} BitfieldAllocator;

typedef struct
{
    u64 start_element;
    u64 element_count;
} BitfieldAllocation;


u64 _bitfield_allocator_size_of_fields(u64 field_count)
{
    u64 add = 1;
    u64 acc = 0;
    for(u64 i = 0; i < field_count; i++)
    {
        acc += add;
        add *= 64;
    }
    return acc;
}

u64 _bitfield_allocator_fields_required(u64 number_of_elements_managed)
{
    u64 binary_tree_depth = NEXT_POWER_OF_2_POWER(number_of_elements_managed);
    return (binary_tree_depth + 6 - 1) / 6;
}

u64 bitfield_allocator_size(u64 number_of_elements_managed)
{
    if(number_of_elements_managed < 64)
    {
        u8* ptr = 0;
        *ptr = 1; // crash
    }

    u64 field_count = _bitfield_allocator_fields_required(number_of_elements_managed);

    // first field is 64^0 u64's
    // second field is 64^1 u64's
    // third field is 64^2 u64's
    // et cetera

    u64 total_byte_size = _bitfield_allocator_size_of_fields(field_count);
    return sizeof(BitfieldAllocator) + total_byte_size;
}

u64 create_bitfield_allocator(BitfieldAllocator* allocator, u64 space_for_allocator_in_bytes, u64 number_of_elements_managed)
{
    if(number_of_elements_managed < 64)
    { return 0; }

    u64 required_size = bitfield_allocator_size(number_of_elements_managed);
    if(space_for_allocator_in_bytes < required_size)
    { return 0; }

    allocator->field_count = _bitfield_allocator_fields_required(number_of_elements_managed);
    u64* field_ptr = allocator->field_data;
    u64 current_size = 1;
    for(u64 i = 0; i < allocator->field_count; i++)
    {
        for(u64 j = 0; j < current_size; j++)
        { *(field_ptr++) = 0; }
        current_size *= 64;
    }

    // TODO mark end
}

// set_to is either 0 or U64_MAX
void _bitfield_allocator_propagate_bit_change_down(BitfieldAllocator* allocator, u64 field_index, u64 local_index, u64 set_to)
{
    u64* field;
    if(field_index == 0)
    {
        field = allocator->field_data;
    }
    else
    {
        field = allocator->field_data + _bitfield_allocator_size_of_fields(field_index-1);
    }

    field[local_index] = set_to;

    if(field_index + 1 < allocator->field_count)
    {
        for(u64 i = 0; i < 64; i++)
        {
            _bitfield_allocator_propagate_bit_change_down(allocator, field_index + 1, local_index*64 + i, set_to);
        }
    }
}

void _bitfield_allocator_propagate_bit_change_up(BitfieldAllocator* allocator, u64 field_index, u64 bit_index, u64 set_to)
{
    u64* field;
    if(field_index == 0)
    {
        field = allocator->field_data;
    }
    else
    {
        field = allocator->field_data + _bitfield_allocator_size_of_fields(field_index-1);
    }

    u64 local_index = bit_index / 64;
    u64 local_bit = bit_index % 64;

    u64 value_before = !!field[local_index]; // !! turns it into a proper boolean

    if(set_to)
    {
        field[local_index] = field[local_index] | (1llu << local_bit);
    }
    else
    {
        field[local_index] = field[local_index] & (~(1llu << local_bit));
    }

    u64 value_after = !!field[local_index];

    if(field_index > 0 && value_before != value_after)
    {
        _bitfield_allocator_propagate_bit_change_up(allocator, field_index - 1, local_index, set_to);
    }
}

BitfieldAllocation bitfield_allocate(BitfieldAllocator* allocator, u64 number_of_elements)
{
    BitfieldAllocation null_allocation;
    null_allocation.start_element = 0;
    null_allocation.element_count = 0;

    if(allocator->field_count < _bitfield_allocator_fields_required(number_of_elements + 1))
    { return null_allocation; }

    u64 field_index = allocator->field_count - _bitfield_allocator_fields_required(number_of_elements + 1);

    u64 field_local_bit_size = 1llu << ((_bitfield_allocator_fields_required(number_of_elements + 1) - 1) * 6);
    u64 looking_for_bit_count = (number_of_elements + field_local_bit_size - 1) / field_local_bit_size;
    u64 looking_for_mask = 0;
    for(u64 i = 0; i < looking_for_bit_count; i++)
    {
        looking_for_mask = looking_for_mask | (1llu << i);
    }

    u64* field;
    if(field_index == 0)
    {
        field = allocator->field_data;
    }
    else
    {
        field = allocator->field_data + _bitfield_allocator_size_of_fields(field_index-1);
    }
    u64 field_subfield_count = 1llu << (field_index * 6);

    for(u64 i = 0; i < field_subfield_count; i++)
    {
        for(u64 j = 0; j < 64 - looking_for_bit_count; j++)
        {
            u64 test_mask = looking_for_mask << j;
            if(test_mask & field[i] == 0) // empty space
            {

                field[i] = field[i] | test_mask;
                if(field_index + 1 < allocator->field_count)
                {
                    for(u64 k = 0; k < looking_for_bit_count; k++)
                    {
                        _bitfield_allocator_propagate_bit_change_down(allocator, field_index + 1, i*64 + j, U64_MAX);
                    }
                }
                if(field_index > 0)
                {
                    _bitfield_allocator_propagate_bit_change_up(allocator, field_index - 1, i, 1);
                }

                BitfieldAllocation alloc;
                alloc.start_element = (i * 64 + j) * field_local_bit_size;
                alloc.element_count = looking_for_bit_count * field_local_bit_size;
                return alloc;
            }
        }
    }

    return null_allocation;
}


#endif
