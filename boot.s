; boot.s - En-tête Multiboot + point d'entrée assembleur
; Compatible GRUB (spec Multiboot 1)

; ==== Constantes Multiboot ====
MBALIGN     equ  1 << 0              ; aligner les modules chargés sur 4KB
MEMINFO     equ  1 << 1              ; fournir la mémoire disponible
MBFLAGS     equ  MBALIGN | MEMINFO   ; combinaison des flags
MAGIC       equ  0x1BADB002          ; nombre magique pour que GRUB nous détecte
CHECKSUM    equ -(MAGIC + MBFLAGS)   ; checksum requis par la spec

; ==== En-tête Multiboot (doit être dans les 8 premiers Ko du fichier) ====
section .multiboot
align 4
    dd MAGIC
    dd MBFLAGS
    dd CHECKSUM

; ==== Pile du noyau (16 Ko) ====
section .bss
align 16
stack_bottom:
    resb 16384
stack_top:

; ==== Point d'entrée ====
section .text
global _start
extern kernel_main

_start:
    ; Configure la pile (le CPU ne nous en donne pas une au démarrage)
    mov esp, stack_top

    ; Appelle le noyau C
    call kernel_main

    ; Si kernel_main retourne jamais (ne devrait pas arriver), boucle infinie
    cli
.hang:
    hlt
    jmp .hang
