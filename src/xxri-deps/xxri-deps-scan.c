/*
 * xxri-deps-scan - which shared libraries does an unpacked AppImage need
 * that neither it nor the system provides?
 *
 *   xxri-deps-scan APPDIR [--sys DIR]... [--extra DIR]...
 *
 * An AppImage has no dependency list, but every ELF program names the
 * libraries it needs (DT_NEEDED).  This reads those names from every ELF file
 * under APPDIR and removes the ones that exist under APPDIR itself, in the
 * system library directories (--sys; default: the usual ones) or in --extra
 * (the Store's runtime-library directory).  What is left is printed, one per
 * line:
 *
 *   req SONAME    a program (executable) needs it: the app cannot start
 *   opt SONAME    only a bundled library or plugin needs it: the app may
 *                 start without it (optional plugins are common)
 *
 * It reads ELF headers only and never executes or loads anything.  It does
 * not use `ld.so --list`, which stops at the first missing library.
 */
#define _GNU_SOURCE
#include <elf.h>
#include <fcntl.h>
#include <ftw.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

/* ---- a small string set ---- */
typedef struct { char** v; size_t n, cap; } Set;
static void set_add(Set* s, const char* x)
{
    for (size_t i = 0; i < s->n; i++)
        if (!strcmp(s->v[i], x)) return;
    if (s->n == s->cap) { s->cap = s->cap ? s->cap * 2 : 64; s->v = realloc(s->v, s->cap * sizeof *s->v); }
    s->v[s->n++] = strdup(x);
}
static int set_has(const Set* s, const char* x)
{
    for (size_t i = 0; i < s->n; i++)
        if (!strcmp(s->v[i], x)) return 1;
    return 0;
}
static int cmp(const void* a, const void* b) { return strcmp(*(char* const*)a, *(char* const*)b); }

static Set g_have;      /* library file names that exist (app, system, extra) */
static Set g_req, g_opt;

/* ---- ELF: DT_NEEDED of one file --------------------------------------------- */
static void note(const char* name, int is_program)
{
    set_add(is_program ? &g_req : &g_opt, name);
}

#define ELF_NEEDED(ELF, Ehdr, Phdr, Dyn, Off)                                          \
static void needed_##ELF(const unsigned char* m, size_t len, int is_program)           \
{                                                                                      \
    const Ehdr* eh = (const Ehdr*)m;                                                   \
    if (eh->e_phoff == 0 || eh->e_phoff + (size_t)eh->e_phnum * sizeof(Phdr) > len) return; \
    const Phdr* ph = (const Phdr*)(m + eh->e_phoff);                                   \
    const Dyn* dyn = NULL; size_t dn = 0;                                              \
    for (int i = 0; i < eh->e_phnum; i++)                                              \
        if (ph[i].p_type == PT_DYNAMIC && ph[i].p_offset + ph[i].p_filesz <= len) {    \
            dyn = (const Dyn*)(m + ph[i].p_offset); dn = ph[i].p_filesz / sizeof(Dyn); break; \
        }                                                                              \
    if (!dyn) return;                                                                  \
    Off strtab = 0;                                                                    \
    for (size_t i = 0; i < dn && dyn[i].d_tag != DT_NULL; i++)                         \
        if (dyn[i].d_tag == DT_STRTAB) {                                               \
            Off va = dyn[i].d_un.d_ptr;                                                \
            for (int j = 0; j < eh->e_phnum; j++)                                      \
                if (ph[j].p_type == PT_LOAD && va >= ph[j].p_vaddr                     \
                    && va < ph[j].p_vaddr + ph[j].p_filesz) {                          \
                    strtab = va - ph[j].p_vaddr + ph[j].p_offset; break;               \
                }                                                                      \
        }                                                                              \
    if (!strtab || strtab >= len) return;                                              \
    for (size_t i = 0; i < dn && dyn[i].d_tag != DT_NULL; i++)                         \
        if (dyn[i].d_tag == DT_NEEDED) {                                               \
            Off o = strtab + dyn[i].d_un.d_val;                                        \
            if (o >= len) continue;                                                    \
            const char* s = (const char*)m + o;                                        \
            if (memchr(s, 0, len - o)) note(s, is_program);                            \
        }                                                                              \
}
ELF_NEEDED(64, Elf64_Ehdr, Elf64_Phdr, Elf64_Dyn, uint64_t)
ELF_NEEDED(32, Elf32_Ehdr, Elf32_Phdr, Elf32_Dyn, uint32_t)

static void scan_file(const char* path, off_t size, int exec_bit)
{
    if (size < (off_t)sizeof(Elf32_Ehdr)) return;
    int fd = open(path, O_RDONLY);
    if (fd < 0) return;
    unsigned char* m = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    if (m == MAP_FAILED) return;
    if (!memcmp(m, ELFMAG, SELFMAG) && (m[EI_DATA] == ELFDATA2LSB)) {
        const char* base = strrchr(path, '/'); base = base ? base + 1 : path;
        /* a program: executable bit and not named like a library */
        int is_program = exec_bit && !strstr(base, ".so");
        if (m[EI_CLASS] == ELFCLASS64 && (size_t)size >= sizeof(Elf64_Ehdr)) needed_64(m, size, is_program);
        else if (m[EI_CLASS] == ELFCLASS32) needed_32(m, size, is_program);
    }
    munmap(m, size);
}

static int g_collect_only;      /* 1: only record names (system/extra dirs) */
static int walk(const char* path, const struct stat* sb, int flag, struct FTW* ftw)
{
    (void)ftw;
    if (flag != FTW_F && flag != FTW_SL) return 0;
    const char* base = strrchr(path, '/'); base = base ? base + 1 : path;
    if (strstr(base, ".so")) set_add(&g_have, base);
    if (!g_collect_only && flag == FTW_F && S_ISREG(sb->st_mode))
        scan_file(path, sb->st_size, (sb->st_mode & S_IXUSR) != 0);
    return 0;
}

int main(int argc, char** argv)
{
    if (argc < 2) { fprintf(stderr, "usage: %s APPDIR [--sys DIR]... [--extra DIR]...\n", argv[0]); return 2; }
    const char* app = argv[1];
    const char* sys[16]; int ns = 0;
    const char* extra[8]; int ne = 0;
    for (int i = 2; i + 1 < argc; i += 2) {
        if (!strcmp(argv[i], "--sys") && ns < 16) sys[ns++] = argv[i + 1];
        else if (!strcmp(argv[i], "--extra") && ne < 8) extra[ne++] = argv[i + 1];
    }
    if (!ns) {
        static const char* d[] = { "/lib", "/lib64", "/usr/lib", "/usr/lib64", "/usr/local/lib" };
        for (size_t i = 0; i < sizeof d / sizeof *d; i++) sys[ns++] = d[i];
    }
    struct stat st;
    if (stat(app, &st) || !S_ISDIR(st.st_mode)) { fprintf(stderr, "%s: not a directory\n", app); return 2; }

    g_collect_only = 1;
    for (int i = 0; i < ns; i++) nftw(sys[i], walk, 32, FTW_PHYS);
    for (int i = 0; i < ne; i++) nftw(extra[i], walk, 32, FTW_PHYS);
    g_collect_only = 0;
    nftw(app, walk, 64, FTW_PHYS);

    qsort(g_req.v, g_req.n, sizeof *g_req.v, cmp);
    qsort(g_opt.v, g_opt.n, sizeof *g_opt.v, cmp);
    for (size_t i = 0; i < g_req.n; i++)
        if (!set_has(&g_have, g_req.v[i])) printf("req %s\n", g_req.v[i]);
    for (size_t i = 0; i < g_opt.n; i++)
        if (!set_has(&g_have, g_opt.v[i]) && !set_has(&g_req, g_opt.v[i])) printf("opt %s\n", g_opt.v[i]);
    return 0;
}
