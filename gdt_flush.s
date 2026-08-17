global gdt_flush

section .text
gdt_flush:
    mov eax, [esp+4]  ; Récupère le pointeur de la GDT passé en paramètre
    lgdt [eax]        ; Charge la nouvelle GDT

    ; Met à jour les registres de données avec l'offset 0x10
    mov ax, 0x10      
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Fait un "far jump" pour mettre à jour le registre de code (CS)
    jmp 0x08:.flush   
.flush:
    ret
