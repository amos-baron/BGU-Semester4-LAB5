#include <elf.h>


#define SYS_exit 1
#define SYS_write 4
#define SYS_open 5
#define SYS_lseek 19
#define SYS_mmap 90
#define MAP_ANONYMOUS 0x20
#define O_RDONLY 0
#define SEEK_SET 0
#define SEEK_END 2
#define PROT_READ 0x1
#define PROT_WRITE 0x2
#define PROT_EXEC 0x4
#define MAP_PRIVATE 0x02
#define MAP_FIXED 0x10
#define MAP_FAILED ((void *)-1)

extern int system_call(int syscall_num, ...);
extern int startup(int argc, char **argv, void (*start)());

int my_strlen(const char *str) {
    int i = 0;
    while (str[i]) i++;
    return i;
}

void my_print(const char *str) {
    system_call(SYS_write, 1, str, my_strlen(str));
}

void my_print_hex(unsigned int value, int width) {
    char buffer[8];
    for (int i = width - 1; i >= 0; i--) {
        int hex_val = value & 0xF;
        buffer[i] = (hex_val < 10) ? ('0' + hex_val) : ('a' + (hex_val - 10));
        value >>= 4;
    }
    system_call(SYS_write, 1, buffer, width);
}

int my_open(const char *pathname, int flags) {
    return system_call(SYS_open, pathname, flags, 0);
}

int my_lseek(int fd, int offset, int whence) {
    return system_call(SYS_lseek, fd, offset, whence);
}

void *my_mmap(void *addr, int length, int prot, int flags, int fd, int offset) {
    unsigned int mmap_args[6];
    mmap_args[0] = (unsigned int)addr;
    mmap_args[1] = (unsigned int)length;
    mmap_args[2] = (unsigned int)prot;
    mmap_args[3] = (unsigned int)flags;
    mmap_args[4] = (unsigned int)fd;
    mmap_args[5] = (unsigned int)offset;
    return (void *)system_call(SYS_mmap, mmap_args);
}

void my_exit(int status) {
    system_call(SYS_exit, status, 0, 0);
}

int foreach_phdr(void *map_start, void (*func)(Elf32_Phdr *, int), int arg) {
    Elf32_Ehdr *ehdr = (Elf32_Ehdr *)map_start;
    Elf32_Phdr *phdr = (Elf32_Phdr *)((unsigned char *)map_start + ehdr->e_phoff);
    for (int i = 0; i < ehdr->e_phnum; i++) {
        func(&phdr[i], arg);
    }
    return 0;
}

void print_phdr_info(Elf32_Phdr *phdr, int arg) {
    char *type;
    switch (phdr->p_type) {
        case PT_NULL: type = "NULL   "; break;
        case PT_LOAD: type = "LOAD   "; break;
        case PT_DYNAMIC: type = "DYNAMIC"; break;
        case PT_INTERP: type = "INTERP "; break;
        case PT_NOTE: type = "NOTE   "; break;
        case PT_SHLIB: type = "SHLIB  "; break;
        case PT_PHDR: type = "PHDR   "; break;
        default: type = "UNKNOWN"; break;
    }

    int prot = 0;
    char flg[4] = "   ";
    if (phdr->p_flags & PF_R) { prot |= PROT_READ; flg[0] = 'R'; }
    if (phdr->p_flags & PF_W) { prot |= PROT_WRITE; flg[1] = 'W'; }
    if (phdr->p_flags & PF_X) { prot |= PROT_EXEC; flg[2] = 'E'; }

    int map_flags = MAP_PRIVATE | MAP_FIXED;

    my_print(type); my_print(" 0x");
    my_print_hex(phdr->p_offset, 6); my_print(" 0x");
    my_print_hex(phdr->p_vaddr, 8); my_print(" 0x");
    my_print_hex(phdr->p_paddr, 8); my_print(" 0x");
    my_print_hex(phdr->p_filesz, 5); my_print(" 0x");
    my_print_hex(phdr->p_memsz, 5); my_print(" ");
    my_print(flg); my_print(" 0x");
    my_print_hex(phdr->p_align, 4); my_print("\n");

    my_print("Prot Flags: 0x"); my_print_hex(prot, 1);
    my_print(" | Map Flags: 0x"); my_print_hex(map_flags, 2); my_print("\n");
}

void load_phdr(Elf32_Phdr *phdr, int fd) {
    if (phdr->p_type != PT_LOAD) {
        return;
    }

    print_phdr_info(phdr, 0);

    int prot = 0;
    if (phdr->p_flags & PF_R) prot |= PROT_READ;
    if (phdr->p_flags & PF_W) prot |= PROT_WRITE;
    if (phdr->p_flags & PF_X) prot |= PROT_EXEC;

    int flags = MAP_PRIVATE | MAP_FIXED;

    unsigned int vaddr = phdr->p_vaddr & 0xfffff000;
    unsigned int offset = phdr->p_offset & 0xfffff000;
    unsigned int padding = phdr->p_vaddr & 0xfff;

    if (phdr->p_memsz > phdr->p_filesz) {
        void *bss_res = my_mmap((void *)vaddr, phdr->p_memsz + padding, prot, flags | MAP_ANONYMOUS, -1, 0);
        if (bss_res == MAP_FAILED) {
            my_exit(1);
        }
    }

    void *map_res = my_mmap((void *)vaddr, phdr->p_filesz + padding, prot, flags, fd, offset);
    if (map_res == MAP_FAILED) {
        my_exit(1);
    }
}

int main(int argc, char **argv) {
    if (argc < 2) {
        my_exit(1);
    }

    int fd = my_open(argv[1], O_RDONLY);
    if (fd < 0) {
        my_exit(1);
    }

    int file_size = my_lseek(fd, 0, SEEK_END);
    my_lseek(fd, 0, SEEK_SET);

    void *map_start = my_mmap(0, file_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (map_start == MAP_FAILED) {
        my_exit(1);
    }

    Elf32_Ehdr *ehdr = (Elf32_Ehdr *)map_start;

    if (ehdr->e_ident[EI_MAG0] != ELFMAG0 || ehdr->e_ident[EI_MAG1] != ELFMAG1 || 
        ehdr->e_ident[EI_MAG2] != ELFMAG2 || ehdr->e_ident[EI_MAG3] != ELFMAG3) {
        my_exit(1);
    }

    my_print("Type    Offset   VirtAddr   PhysAddr   FileSiz MemSiz Flg Align\n");
    foreach_phdr(map_start, print_phdr_info, 0);

    foreach_phdr(map_start, load_phdr, fd);

    void (*start_routine)() = (void (*)())(ehdr->e_entry);
    startup(argc - 1, argv + 1, start_routine);

    return 0;
}
