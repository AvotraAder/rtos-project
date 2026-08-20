# RTOS x86 — Real-Time Operating System from Scratch

<div align="center">

**A hobby x86 (i686) kernel built from the ground up in C and Assembly**

![Architecture](https://img.shields.io/badge/arch-i686-blue)
![Language](https://img.shields.io/badge/language-C%20%2F%20NASM-orange)
![License](https://img.shields.io/badge/license-MIT-green)
![Status](https://img.shields.io/badge/status-active--development-brightgreen)

</div>

---

## 📖 Overview

**RTOS x86** is a freestanding 32-bit operating system kernel written from scratch, targeting the `i686-elf` architecture. It boots via GRUB (Multiboot 1 spec) and implements core OS primitives: preemptive multitasking, virtual memory paging, privilege rings, signals, IPC, and an interactive shell — all rendered on a custom VGA text-mode terminal with scrollback support.

This project is built for learning and experimentation with low-level systems programming: interrupt handling, memory management, process scheduling, and CPU privilege separation on real x86 hardware semantics (via QEMU).

---

## ✨ Features

### 🧠 Core Kernel
- **Multiboot-compliant** bootloader entry (GRUB-compatible)
- **GDT** (Global Descriptor Table) with Ring 0 / Ring 3 segments + TSS
- **IDT** (Interrupt Descriptor Table) — full 32 CPU exception handlers + IRQ remapping
- **Kernel panic screen** with register dump on unhandled faults

### 🧵 Multitasking & Processes
- Preemptive **task scheduler** (priority-based, timer-driven via PIT)
- Full **process management** (PCB pool, PID/PPID, states: ready/running/sleeping/blocked/zombie)
- Cooperative `task_sleep()` / `task_yield()` primitives

### 🗺️ Memory Management
- **Heap allocator** (`kmalloc`/`kfree`) with block splitting & coalescing
- **Paging** (x86 2-level page tables) with identity-mapped kernel region
- Physical page bitmap allocator (`alloc_page`, `alloc_pages`, `free_page`)
- Virtual memory map inspection (`vmmap`)

### 🔐 Privilege & Protection
- **Ring 0 / Ring 3** switching via TSS + `iret`
- **Signals** subsystem (SIGKILL, SIGTERM, SIGUSR1/2, custom handlers, `kill()`/`raise()`)
- Syscall interface via `int 0x80` (write, malloc, free, kill, getpid, fork-style exit, etc.)

### 🔄 IPC & Synchronization
- **Mutexes** and **semaphores** (blocking + non-blocking `trywait`)
- **Pipes** (producer/consumer, ring buffer, blocking read/write)
- Message **queues** for inter-task communication

### 📜 Logging & Diagnostics
- Leveled logging system (`DEBUG` → `CRITICAL`) with circular buffer + serial (COM1) mirroring
- Kernel panic handler with exception name + register state
- Error code system (`error_t`) with human-readable strings

### 💻 Interactive Shell
- Custom VGA scrollback terminal (200-line history, Page Up/Down navigation)
- Command **history** (↑ / ↓ arrow navigation)
- 30+ built-in commands — see [Shell Commands](#-shell-commands)
- Black & white monochrome display theme

### 🗃️ Virtual File System
- In-memory VFS (create, read, write, list, remove)
- Preloaded demo files on boot

---

## 🏗️ Architecture

```
┌─────────────────────────────────────────────────────────┐
│                        Shell (shell.c)                   │
│         30+ commands · history · scrollback nav           │
├─────────────────────────────────────────────────────────┤
│  VFS   │  Heap   │  Logging  │  Signals  │  Pipes  │ IPC │
├─────────────────────────────────────────────────────────┤
│         Process Manager    │    Task Scheduler            │
├─────────────────────────────────────────────────────────┤
│   Paging (MMU)   │   Rings (TSS/GDT)   │   Syscalls       │
├─────────────────────────────────────────────────────────┤
│         IDT (Interrupts / Exceptions / IRQs)              │
├─────────────────────────────────────────────────────────┤
│   GDT   │   PIT Timer   │   PS/2 Keyboard   │   Serial    │
├─────────────────────────────────────────────────────────┤
│              Multiboot Entry Point (boot.s)                │
└─────────────────────────────────────────────────────────┘
```

---

## 📂 Project Structure

```
.
├── boot.s              # Multiboot header + kernel entry point
├── linker.ld            # Memory layout (loads at 1MB)
├── Makefile              # Build system
│
├── kernel.c              # Kernel main, VGA terminal, header/taskbar UI
├── gdt.c / gdt.h / gdt_flush.s   # Global Descriptor Table
├── idt.c / idt.h / interrupts.s  # Interrupt Descriptor Table + ISR/IRQ stubs
│
├── task.c / task.h       # Preemptive task scheduler
├── process.c / process.h # Process control blocks (PCB)
├── ring.c / ring.h       # Ring 0/3 privilege switching + TSS
├── signal.c / signal.h   # POSIX-style signal handling
├── syscall.c / syscall.h # int 0x80 syscall interface
│
├── paging.c / paging.h   # Virtual memory / MMU
├── heap.c / heap.h       # Kernel heap allocator
│
├── mutex.c / mutex.h     # Mutex locks
├── semaphore.c / semaphore.h  # Counting semaphores
├── sema.c / sema.h       # Blocking semaphore variant
├── pipe.c / pipe.h       # Producer/consumer pipes
├── queue.c / queue.h     # Fixed-size message queue
│
├── vfs.c / vfs.h         # In-memory virtual file system
├── shell.c / shell.h     # Interactive command-line shell
├── keyboard.c / keyboard.h    # PS/2 keyboard driver
├── timer.c / timer.h     # PIT (Programmable Interval Timer)
│
├── logging.c / logging.h # Leveled logging system
├── klog.c / klog.h       # Lightweight boot-time log buffer
├── serial.c / serial.h   # COM1 serial output driver
│
├── panic.c / panic.h     # Kernel panic handler
├── error.c / error.h     # Error code definitions
│
└── build-cross-compiler.sh   # i686-elf cross-compiler build script
```

---

## 🚀 Getting Started

### Prerequisites

You'll need an `i686-elf` cross-compiler toolchain, plus `nasm`, `qemu`, and (optionally) `grub-mkrescue` for ISO builds.

```bash
# Debian/Ubuntu
sudo apt install nasm qemu-system-x86 grub-pc-bin xorriso build-essential

# Build the cross-compiler (binutils + gcc, targets i686-elf)
chmod +x build-cross-compiler.sh
./build-cross-compiler.sh

# Add it to your PATH
export PATH="$HOME/opt/cross/bin:$PATH"
```

### Build & Run

```bash
# Compile the kernel
make

# Run directly in QEMU (kernel loaded via -kernel)
make run

# Run with GDB debugging support (listens on :1234)
make run-debug

# Build a bootable GRUB ISO
make iso

# Build and run the ISO in QEMU
make run-iso

# Clean build artifacts
make clean
```

---

## 🖥️ Shell Commands

Once booted, you land in the `RTOS>` shell. Type `help` for the full list.

| Category | Commands |
|---|---|
| **System** | `help`, `clear`, `uptime`, `ticks`, `reboot`, `shutdown`, `sysinfo` |
| **Logging** | `logs`, `loglevel <0-4>`, `logclear` |
| **Paging** | `pages`, `vmmap` |
| **Signals & Rings** | `ring`, `siglist`, `testsig <pid>` |
| **Tasks & Processes** | `tasks`, `ps`, `spawn <name>` |
| **Memory** | `mem`, `alloc [bytes]`, `free`, `hexdump <addr>` |
| **VFS Files** | `ls`, `cat <file>`, `touch <file>`, `write <f> <text>`, `rm <file>` |
| **Misc** | `echo`, `calc <n> <op> <m>`, `sem`, `sys`, `history` |
| **Navigation** | `Page Up/Down` (scroll), `↑ / ↓` (command history) |

### Example Session

```
RTOS> sysinfo
=== RTOS System Info ===
Version: RTOS v3.1
Tasks active: 3
Processes: 5
Free pages: 1046528

RTOS> spawn worker
Process 'worker' created, PID=2

RTOS> ps
 PID  PPID  PRI  CPU    STATE     NAME
 ---  ----  ---  -----  --------  --------
 0    0     0    120    READY     idle
 2    0     1    3      READY     worker

RTOS> calc 42 * 10
Result: 420
```

---

## 🧩 Design Notes

- **Boot flow safety**: panic handlers (`init_panic_handlers`) are installed *before* paging or ring switching, so any CPU fault during MMU/TSS setup produces a diagnosable panic screen instead of a silent freeze.
- **Identity-mapped paging**: the first 64 MB of physical memory are identity-mapped before `CR0.PG` is set, guaranteeing the kernel, its stacks, heap, and VGA buffer remain accessible immediately after paging is enabled.
- **Monochrome UI**: the VGA terminal uses a single black/white color scheme (`0x0F`) throughout — no reliance on color for readability.
- **Safe VFS copies**: string copies into fixed-size VFS buffers reject (rather than overflow) oversized input.

---

## 🗺️ Roadmap

- [ ] Watchdog timer / deadlock detection
- [ ] ELF loader for user-space binaries
- [ ] Persistent storage driver (ATA/IDE)
- [ ] Networking stack (basic Ethernet/IP)
- [ ] SMP (multi-core) support

---

## 📜 License

MIT — free to use, modify, and learn from.

---

<div align="center">

*Built one interrupt at a time.* 🛠️

</div>
