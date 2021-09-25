OUTPUT_ARCH( "riscv" )

ENTRY( _start )

MEMORY
{
    ram   (wxa!ri) : ORIGIN = 0x80000000, LENGTH = 512M
}

PHDRS
{
    text PT_LOAD;
    data PT_LOAD;
    bss PT_LOAD;
}

SECTIONS
{
    .text : {
    . = ALIGN(4096);
    PROVIDE(_text_start = .);
    *(.text.init) *(.text .text.*)
    } >ram AT>ram :text
    . = ALIGN(4096);
    PROVIDE(_text_end = .);

    PROVIDE(_global_pointer = .);
    .rodata : {
    . = ALIGN(4096);
    PROVIDE(_rodata_start = .);
    *(.rodata .rodata.*)
    } >ram AT>ram :text
    . = ALIGN(4096);
    PROVIDE(_rodata_end = .);

    .data : {
    . = ALIGN(4096);
    PROVIDE(_data_start = .);
    *(.sdata .sdata.*) *(.data .data.*)
    } >ram AT>ram :data
    . = ALIGN(4096);
    PROVIDE(_data_end = .);

    .bss :{
    . = ALIGN(4096);
    PROVIDE(_bss_start = .);
    *(.sbss .sbss.*) *(.bss .bss.*)
    } >ram AT>ram :bss
    . = ALIGN(4096);
    PROVIDE(_bss_end = .);

    PROVIDE(_memory_start = ORIGIN(ram));
    PROVIDE(_stack_start = .);
    PROVIDE(_stack_end = . + 0x80000);
    PROVIDE(_memory_end = ORIGIN(ram) + LENGTH(ram));
    PROVIDE(_heap_start = _stack_end);
    PROVIDE(_heap_size = _memory_end - _heap_start);
}
