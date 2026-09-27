/*
 * isa-sentinel.c - QEMU TCG plugin: report every EXECUTED guest instruction
 * that the emulated CPU model does not have.
 *
 * QEMU's TCG does not enforce every CPUID feature: -cpu pentium3 runs SSE2
 * instructions and no model traps the multi-byte NOP (0f 1f).  A boot on
 * such a model therefore proves nothing about SSE2 or NOPL by itself; this
 * plugin closes the gap.  Classes (cumulative, like the CPUs):
 *
 *   cpu=pentium   flags p6 (cmov, fcmov, fcomi, nopl), mmx, sse, sse2, sse3+
 *   cpu=pentium2  flags sse, sse2, sse3+           (P6 + MMX)
 *   cpu=pentium3  flags sse2, sse3+                (P6 + MMX + SSE)
 *   cpu=sse2      flags sse3+                      (Pentium 4 / Pentium M)
 *
 * Build:  gcc -shared -fPIC -O2 -o isa-sentinel.so isa-sentinel.c \
 *             $(pkg-config --cflags glib-2.0)
 * Use:    qemu-system-i386 -accel tcg -cpu pentium3 \
 *             -plugin ./isa-sentinel.so,cpu=pentium3,out=hits.tsv ...
 *
 * Output (TSV, written at exit and every 30 s): class, first-exec order,
 * exec count, guest vaddr, kernel/user, instruction bytes, the 32 bytes that
 * start at the instruction (for mapping the hit back to an ELF file: the
 * same bytes sit at the same page offset in the file), disassembly.
 * Classification uses QEMU's own disassembly (AT&T syntax) with the same
 * mnemonic sets as tools/qa/isa-scan.py.
 */
#include <glib.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <qemu-plugin.h>

QEMU_PLUGIN_EXPORT int qemu_plugin_version = QEMU_PLUGIN_VERSION;

enum { C_P6, C_MMX, C_SSE, C_SSE2, C_SSE3P, C_N };
static const char *cname[C_N] = { "p6", "mmx", "sse", "sse2", "sse3+" };
static int flag[C_N];

typedef struct {
    int cls;
    uint64_t vaddr, count, order;
    char bytes[64], window[100], disas[96];
} Hit;

static GMutex lock;
static GHashTable *hits;          /* &vaddr -> Hit* (one per guest address) */
static uint64_t next_order;
static char *outpath;
static time_t last_dump;
static regex_t re_p6, re_sse2, re_sse, re_sse3p;

static int classify(const char *d)
{
    /* strip an address/prefix column if present: QEMU gives "mnemonic ops" */
    while (*d == ' ' || *d == '\t')
        d++;
    int xmm = strstr(d, "%xmm") != NULL, mm = strstr(d, "%mm") != NULL;
    if (!regexec(&re_sse3p, d, 0, NULL, 0) &&
        (xmm || mm || !strncmp(d, "fisttp", 6) || !strncmp(d, "crc32", 5) || !strncmp(d, "popcnt", 6)))
        return C_SSE3P;
    if (xmm && !regexec(&re_sse2, d, 0, NULL, 0))
        return C_SSE2;
    if (!strncmp(d, "lfence", 6) || !strncmp(d, "mfence", 6) || !strncmp(d, "clflush", 7) ||
        !strncmp(d, "movnti", 6))
        return C_SSE2;
    if (!regexec(&re_sse, d, 0, NULL, 0) &&
        (xmm || mm || !strncmp(d, "ldmxcsr", 7) || !strncmp(d, "stmxcsr", 7) ||
         !strncmp(d, "sfence", 6) || !strncmp(d, "prefetch", 8)))
        return C_SSE;
    if (xmm) {
        /* integer ops on xmm registers are SSE2 */
        if (d[0] == 'p' || !strncmp(d, "movq", 4) || !strncmp(d, "movd", 4))
            return C_SSE2;
        return C_SSE;
    }
    if (mm)
        return C_MMX;
    if (!regexec(&re_p6, d, 0, NULL, 0))
        return C_P6;
    return -1;
}

static void dump(void)
{
    FILE *f = fopen(outpath, "w");
    if (!f)
        return;
    fprintf(f, "class\torder\tcount\tvaddr\tspace\tbytes\twindow\tdisas\n");
    GHashTableIter it;
    gpointer k, v;
    g_hash_table_iter_init(&it, hits);
    while (g_hash_table_iter_next(&it, &k, &v)) {
        Hit *h = v;
        if (!h->count)
            continue;
        fprintf(f, "%s\t%" PRIu64 "\t%" PRIu64 "\t%08" PRIx64 "\t%s\t%s\t%s\t%s\n", cname[h->cls],
                h->order, h->count, h->vaddr, h->vaddr >= 0xC0000000u ? "kernel" : "user",
                h->bytes, h->window, h->disas);
    }
    fclose(f);
}

static void on_exec(unsigned int vcpu, void *udata)
{
    Hit *h = udata;
    g_mutex_lock(&lock);
    if (h->count++ == 0)
        h->order = ++next_order;
    time_t now = time(NULL);
    if (now - last_dump >= 30) {
        last_dump = now;
        dump();
    }
    g_mutex_unlock(&lock);
}

static void hex(char *out, size_t outlen, const uint8_t *b, size_t n)
{
    size_t o = 0;
    for (size_t i = 0; i < n && o + 3 <= outlen; i++)
        o += snprintf(out + o, outlen - o, "%02x", b[i]);
}

static void tb_trans(struct qemu_plugin_tb *tb, void *udata)
{
    size_t n = qemu_plugin_tb_n_insns(tb);
    for (size_t i = 0; i < n; i++) {
        struct qemu_plugin_insn *insn = qemu_plugin_tb_get_insn(tb, i);
        char *d = qemu_plugin_insn_disas(insn);
        int c = d ? classify(d) : -1;
        if (c >= 0 && flag[c]) {
            /* a block can be translated again (flushes, page changes): one
             * record per guest address, shared by every translation */
            uint64_t va = qemu_plugin_insn_vaddr(insn);
            g_mutex_lock(&lock);
            Hit *h = g_hash_table_lookup(hits, &va);
            if (h) {
                g_mutex_unlock(&lock);
                qemu_plugin_register_vcpu_insn_exec_cb(insn, on_exec, QEMU_PLUGIN_CB_NO_REGS, h);
                g_free(d);
                continue;
            }
            h = g_new0(Hit, 1);
            h->cls = c;
            h->vaddr = va;
            g_hash_table_insert(hits, &h->vaddr, h);
            g_mutex_unlock(&lock);
            uint8_t b[16];
            size_t len = qemu_plugin_insn_data(insn, b, sizeof(b));
            hex(h->bytes, sizeof(h->bytes), b, len);
            /* the following instructions of this block, as far as 32 bytes */
            uint8_t w[32];
            size_t wl = 0;
            for (size_t j = i; j < n && wl < sizeof(w); j++)
                wl += qemu_plugin_insn_data(qemu_plugin_tb_get_insn(tb, j), w + wl, sizeof(w) - wl);
            hex(h->window, sizeof(h->window), w, wl);
            g_strlcpy(h->disas, d, sizeof(h->disas));
            qemu_plugin_register_vcpu_insn_exec_cb(insn, on_exec, QEMU_PLUGIN_CB_NO_REGS, h);
        }
        g_free(d);
    }
}

static void at_exit(void *p)
{
    g_mutex_lock(&lock);
    dump();
    g_mutex_unlock(&lock);
}

QEMU_PLUGIN_EXPORT int qemu_plugin_install(qemu_plugin_id_t id, const qemu_info_t *info,
                                           int argc, char **argv)
{
    const char *cpu = "pentium3";
    outpath = g_strdup("isa-sentinel.tsv");
    for (int i = 0; i < argc; i++) {
        if (g_str_has_prefix(argv[i], "cpu="))
            cpu = argv[i] + 4;
        else if (g_str_has_prefix(argv[i], "out="))
            outpath = g_strdup(argv[i] + 4);
    }
    if (!strcmp(cpu, "pentium"))
        flag[C_P6] = flag[C_MMX] = flag[C_SSE] = flag[C_SSE2] = flag[C_SSE3P] = 1;
    else if (!strcmp(cpu, "pentium2"))
        flag[C_SSE] = flag[C_SSE2] = flag[C_SSE3P] = 1;
    else if (!strcmp(cpu, "pentium3"))
        flag[C_SSE2] = flag[C_SSE3P] = 1;
    else if (!strcmp(cpu, "sse2"))
        flag[C_SSE3P] = 1;
    else {
        fprintf(stderr, "isa-sentinel: unknown cpu=%s\n", cpu);
        return -1;
    }
    regcomp(&re_p6, "^(cmov[a-z]*|fcmov[a-z]*|f?u?comip?|nop[wl])( |$)", REG_EXTENDED | REG_NOSUB);
    regcomp(&re_sse2, "^([a-z0-9]+pd|[a-z0-9]+sd|movdq[au]|pshuf[dlh]w?|punpck[a-z]+q?dq|cvt[a-z0-9]*2(sd|pd|dq)|"
            "cvt(sd|pd|dq)2[a-z0-9]+|maskmovdqu|movnti|movntpd|movntdq|pmuludq|paddq|psubq|pslldq|psrldq)( |$)",
            REG_EXTENDED | REG_NOSUB);
    regcomp(&re_sse, "^([a-z0-9]+ps|[a-z0-9]+ss|ldmxcsr|stmxcsr|sfence|prefetch[a-z0-9]*|movntq|pavg[bw]|pextrw|"
            "pinsrw|pmaxsw|pmaxub|pminsw|pminub|pmovmskb|pmulhuw|psadbw|pshufw|maskmovq)( |$)",
            REG_EXTENDED | REG_NOSUB);
    regcomp(&re_sse3p, "^(addsub[a-z]+|hadd[a-z]+|hsub[a-z]+|lddqu|movddup|movs[hl]dup|fisttp[a-z]*|pshufb|palignr|"
            "pabs[a-z]|psign[a-z]|phadd[a-z]+|phsub[a-z]+|pmaddubsw|pmulhrsw|pmov[sz]x[a-z]+|ptest|round[ps][sd]|"
            "blendv?[a-z]+|pextr[bdq]|pinsr[bdq]|pcmpeqq|pcmpgtq|pmin[su][dw]|pmax[su][dw]|crc32[a-z]*|popcnt|v[a-z0-9]+)( |$)",
            REG_EXTENDED | REG_NOSUB);
    hits = g_hash_table_new(g_int64_hash, g_int64_equal);
    last_dump = time(NULL);
    qemu_plugin_register_vcpu_tb_trans_cb(id, tb_trans, NULL);
    qemu_plugin_register_atexit_cb(id, at_exit, NULL);
    return 0;
}
