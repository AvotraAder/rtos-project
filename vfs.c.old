#include "vfs.h"

static vfs_file_t files[MAX_FILES];

static int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

static void strcpy(char* dest, const char* src) {
    while ((*dest++ = *src++));
}

static size_t strlen(const char* str) {
    size_t len = 0;
    while (str[len]) len++;
    return len;
}

void vfs_init(void) {
    for (int i = 0; i < MAX_FILES; i++) {
        files[i].used = 0;
    }

    vfs_create("readme.txt", "Bienvenue sur le RTOS x86 (v1.0) !");
    vfs_create("kernel.info", "Architecture : i686 (x86 32-bit)\nMultitasking : Preemptif + Priorite");
    vfs_create("author.txt", "Concu par un Developpeur OS passionne.");
}

int vfs_create(const char* name, const char* content) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (!files[i].used) {
            strcpy(files[i].name, name);
            strcpy(files[i].content, content);
            files[i].size = strlen(content);
            files[i].used = 1;
            return 0;
        }
    }
    return -1;
}

vfs_file_t* vfs_open(const char* name) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (files[i].used && strcmp(files[i].name, name) == 0) {
            return &files[i];
        }
    }
    return 0;
}

void vfs_list(void (*print_fn)(const char* str)) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (files[i].used) {
            print_fn("  - ");
            print_fn(files[i].name);
            print_fn("\n");
        }
    }
}

int vfs_touch(const char* name) {
    if (vfs_open(name) != 0) return 0; /* Existe déjà */
    return vfs_create(name, "");
}

int vfs_write(const char* name, const char* content) {
    vfs_file_t* file = vfs_open(name);
    if (!file) {
        if (vfs_create(name, content) < 0) return -1;
        return 0;
    }
    strcpy(file->content, content);
    file->size = strlen(content);
    return 0;
}

int vfs_remove(const char* name) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (files[i].used && strcmp(files[i].name, name) == 0) {
            files[i].used = 0;
            return 0;
        }
    }
    return -1;
}
