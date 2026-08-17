/* linker.ld - Définit comment les sections sont placées en mémoire */

ENTRY(_start)

SECTIONS
{
    /* Le noyau démarre à 1 Mo, zone standard hors des zones réservées basses */
    . = 1M;

    .text BLOCK(4K) : ALIGN(4K)
    {
        *(.multiboot)
        *(.text)
    }

    .rodata BLOCK(4K) : ALIGN(4K)
    {
        *(.rodata)
    }

    .data BLOCK(4K) : ALIGN(4K)
    {
        *(.data)
    }

    .bss BLOCK(4K) : ALIGN(4K)
    {
        *(COMMON)
        *(.bss)
    }
}
