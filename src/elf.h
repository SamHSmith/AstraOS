

typedef struct
{
    u32 magic;
    u8 bitsize;
    u8 endian;
    u8 ident_abi_version;
    u8 target_platform;
    u8 abi_version;
    u8 padding[7];
    u16 obj_type;
    u16 machine;
    u32 version;
    u64 entry_addr;
    u64 phoff;
    u64 shoff;
    u32 flags;
    u16 ehsize;
    u16 phentsize;
    u16 phnum;
    u16 shentsize;
    u16 shnum;
    u16 shtrndx;
} ELF_Header;

#define ELF_MAGIC 0x464c457f
#define ELF_TYPE_EXEC 2
#define ELF_MACHINE_RISCV 0xf3

#define ELF_PH_SEG_TYPE_LOAD 1

#define ELF_PROG_READ 4
#define ELF_PROG_WRITE 2
#define ELF_PROG_EXECUTE 1


typedef struct
{
    u32 seg_type;
    u32 flags;
    u64 off;
    u64 vaddr;
    u64 paddr;
    u64 filesz;
    u64 memsz;
    u64 align;
} ELF_ProgramHeader;
