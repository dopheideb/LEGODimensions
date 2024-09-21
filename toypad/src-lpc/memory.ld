MEMORY {
	VECTORS (r) : ORIGIN = 0, LENGTH = 8
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
	*(.text)
} > FLASH

.rodata :
{
	. = ALIGN(4);
	*(.rodata)
} > FLASH

/*.data (NOLOAD) :
{
	. = ALIGN(4);
	*(.data)
	*(.data*)
} > RAM*/

.bss (NOLOAD) :
{
	. = ALIGN(4);
	__bss_start__ = .;
	*(.bss)
	*(COMMON)
	. = ALIGN(4);
	__bss_end__ = .;
} > RAM

}
