/* Entry Point */
ENTRY(reset_handler)

/* Specify the memory areas */
MEMORY
{
	ROM   (arx) : ORIGIN = 0x00000000,    LENGTH = 0x00100000 /* 1MB */
    RAM   (arw) : ORIGIN = 0x20000000,    LENGTH = 0x00020000 /* 128KB */
}

/* Define output sections */
SECTIONS
{
    . = ORIGIN(ROM);
    .text :
    {
        KEEP(*(.reset_handler))

        *(.text)
        *(.text*)
        *(.rodata*)
        *(.srodata*)
        
        . = ALIGN(4);
        __usbh_class_info_start__ = .;
        KEEP(*(.usbh_class_info))
        __usbh_class_info_end__ = .;
        
        . = ALIGN(4);
    } > ROM

    __data_load__ = LOADADDR(.data);

    .data :
    {
        __data_start__ = .;
	    
        *(.data)
        *(.data*)
        *(.sdata*)

        . = ALIGN(4);
        __data_end__ = .;
    } > RAM AT> ROM

    .bss :
    {
        __bss_start__ = .;

        *(.bss)
        *(.bss*)
        *(.sbss*)
        *(COMMON)

        . = ALIGN(4);
        __bss_end__ = .;
    } > RAM

    .heap :
    {
        _end = .;
        __HeapBase = .;

        *(.heap)
    } > RAM

    /* .stack_dummy section doesn't contains any symbols.
     * It is only used for linker to calculate size of stack sections */
    .stack_dummy :
    {
        *(.stack)
    } > RAM

    __StackTop   = ORIGIN(RAM) + LENGTH(RAM);
    __StackLimit = __StackTop - SIZEOF(.stack_dummy);
    ASSERT(__StackLimit >= __HeapLimit, "region RAM overflowed with stack")
}
