global isr_common_stub
global irq_common_stub
global isr128

extern isr_handler
extern irq_handler
extern syscall_handler

%macro ISR_NOERRCODE 1
global isr%1
isr%1:
 cli
 push dword 0
 push dword %1
 jmp isr_common_stub
%endmacro

%macro ISR_ERRCODE 1
global isr%1
isr%1:
 cli
 push dword %1
 jmp isr_common_stub
%endmacro

%macro IRQ 2
global irq%1
irq%1:
 cli
 push dword 0
 push dword %2
 jmp irq_common_stub
%endmacro

ISR_NOERRCODE 0
ISR_NOERRCODE 1
ISR_NOERRCODE 2
ISR_NOERRCODE 3
ISR_NOERRCODE 4
ISR_NOERRCODE 5
ISR_NOERRCODE 6
ISR_NOERRCODE 7
ISR_ERRCODE 8
ISR_NOERRCODE 9
ISR_ERRCODE 10
ISR_ERRCODE 11
ISR_ERRCODE 12
ISR_ERRCODE 13
ISR_ERRCODE 14
ISR_NOERRCODE 15
ISR_NOERRCODE 16
ISR_NOERRCODE 17
ISR_NOERRCODE 18
ISR_NOERRCODE 19
ISR_NOERRCODE 20
ISR_NOERRCODE 21
ISR_NOERRCODE 22
ISR_NOERRCODE 23
ISR_NOERRCODE 24
ISR_NOERRCODE 25
ISR_NOERRCODE 26
ISR_NOERRCODE 27
ISR_NOERRCODE 28
ISR_NOERRCODE 29
ISR_NOERRCODE 30
ISR_NOERRCODE 31

IRQ 0, 32
IRQ 1, 33

section .text

isr_common_stub:
 pusha
 mov ax, ds
 push eax
 mov ax, 0x10
 mov ds, ax
 mov es, ax
 mov fs, ax
 mov gs, ax
 call isr_handler
 pop eax
 mov ds, ax
 mov es, ax
 mov fs, ax
 mov gs, ax
 popa
 add esp, 8
 sti
 iret

irq_common_stub:
 pusha
 mov ax, ds
 push eax
 mov ax, 0x10
 mov ds, ax
 mov es, ax
 mov fs, ax
 mov gs, ax
 push esp
 call irq_handler
 mov esp, eax
 pop eax
 mov ds, ax
 mov es, ax
 mov fs, ax
 mov gs, ax
 popa
 add esp, 8
 sti
 iret

isr128:
 cli
 push edx ; arg3
 push ecx ; arg2
 push ebx ; arg1
 push eax ; sys_num
 call syscall_handler
 add esp, 16
 sti
 iret
