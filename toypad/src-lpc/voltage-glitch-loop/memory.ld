MEMORY {
	VECTORS (r) : ORIGIN = 0, LENGTH = 4 * 48
	FLASH (rx) : ORIGIN = 1k LENGTH = 32k
	RAM (rw) : ORIGIN = 0x10000000 LENGTH = 8k
}

SECTIONS {

.isr_vector :
{
	KEEP(*(.isr_vector))
} > VECTORS

.text :
{
	. = ALIGN(4);
	KEEP(*(.init))
	. = ALIGN(4);
	KEEP(*(.text.startup))
	. = ALIGN(4);
	*(.text)
	. = ALIGN(4);
	*(.text*)
	. = ALIGN(4);
	KEEP(*(.fini))
	. = ALIGN(4);
} > FLASH

.rodata :
{
	. = ALIGN(4);
	*(.rodata)
	. = ALIGN(4);
	*(.eh_frame)
	. = ALIGN(4);
	*(.ARM.exidx)
	. = ALIGN(4);
} > FLASH

.data :
{
	. = ALIGN(4);
	*(.data)
	. = ALIGN(4);
	*(.data*)
	. = ALIGN(4);
	*(.init_array)
	. = ALIGN(4);
	*(.init_array*)
	. = ALIGN(4);
	*(.fini_array)
	. = ALIGN(4);
	*(.fini_array*)
	. = ALIGN(4);
} > RAM AT > FLASH

.bss (NOLOAD) :
{
	. = ALIGN(4);
	__bss_start__ = .;
	*(.bss)
	. = ALIGN(4);
	*(COMMON)
	. = ALIGN(4);
	__bss_end__ = .;
} > RAM

}
