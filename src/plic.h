

void plic_interrupt_enable(u32 id)
{
    u32* enable_reg = (u32*)0x0c002000;
    u32 actual_id = ((u32)1) << id;

    *enable_reg = *enable_reg | actual_id;
}

void plic_interrupt_set_priority(u32 id, u8 priority)
{
    u32* priority_reg = (u32*)0x0c000000;
    priority_reg += id;

    *priority_reg = priority;
}

void plic_interrupt_set_threshold(u8 threshold)
{
    u32* reg = (u32*)0x0c200000;
    u32 actual_threshold = threshold & 7;

    *reg = actual_threshold;
}

/*
    You can pass a null pointer to simply poll for the existance of interrupts.
*/
u64 plic_interrupt_next(u32* interrupt)
{
    u32* claim_reg = (u32*)0x0c200004;
    u32 c = *claim_reg;

    if(c == 0)
    { return 0; }
    else
    {
        if(interrupt != 0) { *interrupt = c; }
        return 1;
    }
}

void plic_interrupt_complete(u32 interrupt)
{
    u32* complete_reg = (u32*)0x0c200004;
    *complete_reg = interrupt;
}
