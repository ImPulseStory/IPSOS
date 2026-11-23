#include "tcc_port.h"
#include "tinycc/libtcc.h"
#include "../include/stdlib.h"
#include "../include/string.h"
#include "../include/stdio.h"
#include "../include/ctype.h"
#include "../include/errno.h"
#include "../include/unistd.h"
#include "../include/fcntl.h"
#include "../include/time.h"
#include "../include/sys/time.h"
#include "../include/sys/mman.h"
#include "../include/math.h"
#include "../include/setjmp.h"
#include "../include/ipsfs.h"
#include "../include/desktop.h"
#include "../include/window.h"
#include "../api/api.h"
#include "../apps/terminal/terminal.h"
#include "../include/task.h"
#include "../include/task_internal.h"
#include <stdint.h>

extern int errno;
extern const unsigned char _binary_build_tcc_headers_tar_start[];
extern const unsigned char _binary_build_tcc_headers_tar_end[];
extern long long __divdi3(long long, long long);
extern long long __moddi3(long long, long long);
extern unsigned long long __udivdi3(unsigned long long, unsigned long long);
extern unsigned long long __umoddi3(unsigned long long, unsigned long long);
extern long long __ashrdi3(long long, int);
extern unsigned long long __lshrdi3(unsigned long long, int);
extern long long __ashldi3(long long, int);
extern float __floatundisf(unsigned long long);
extern double __floatundidf(unsigned long long);
extern long double __floatundixf(unsigned long long);
extern unsigned long long __fixunssfdi(float);
extern long long __fixsfdi(float);
extern unsigned long long __fixunsdfdi(double);
extern long long __fixdfdi(double);
extern unsigned long long __fixunsxfdi(long double);
extern long long __fixxfdi(long double);
extern void *alloca(unsigned int size);

static int tcc_headers_ready;

static unsigned long tcc_tar_octal(const unsigned char *field, size_t length)
{
    unsigned long value = 0;
    size_t i = 0;
    while (i < length && (field[i] == ' ' || field[i] == '\0'))
        ++i;
    for (; i < length && field[i] >= '0' && field[i] <= '7'; ++i)
        value = (value << 3) + (unsigned long)(field[i] - '0');
    return value;
}

static int tcc_install_headers(void)
{
    const unsigned char *archive = _binary_build_tcc_headers_tar_start;
    const unsigned char *archive_end = _binary_build_tcc_headers_tar_end;
    size_t archive_size = (size_t)(archive_end - archive);
    size_t offset = 0;

    if (tcc_headers_ready)
        return 0;

    ips_mkdir("/include");
    ips_mkdir("/include/sys");
    ips_mkdir("/include/tcc");

    while (offset + 512u <= archive_size) {
        const unsigned char *header = archive + offset;
        const unsigned char *data = header + 512u;
        unsigned long file_size = tcc_tar_octal(header + 124u, 12u);
        char name[256];
        char target[256];
        size_t name_length = strnlen((const char *)header, 100u);
        size_t prefix_length = strnlen((const char *)header + 345u, 155u);
        size_t target_length;

        if (!name_length || header[0] == 0)
            break;
        if (name_length >= sizeof(name))
            return -1;

        memcpy(name, header, name_length);
        name[name_length] = '\0';

        if (prefix_length) {
            if (prefix_length + name_length + 2u >= sizeof(name))
                return -1;
            memmove(name + prefix_length + 1u, name, name_length + 1u);
            memcpy(name, header + 345u, prefix_length);
            name[prefix_length] = '/';
            name_length += prefix_length + 1u;
        }

        if (name_length >= 2u && name[0] == '.' && name[1] == '/') {
            memmove(name, name + 2u, name_length - 1u);
            name_length -= 2u;
        }

        if ((header[156] == '\0' || header[156] == '0') &&
            name_length && name[name_length - 1u] != '/') {
            target_length = 9u + name_length;
            if (target_length + 1u > sizeof(target))
                return -1;
            memcpy(target, "/include/", 9u);
            memcpy(target + 9u, name, name_length + 1u);

            if (ips_resolve_path(target) < 0)
                ips_create_file(target);
            if (ips_write_file(target, data, (uint32_t)file_size) < 0)
                return -1;
        }

        offset += 512u + ((file_size + 511u) / 512u) * 512u;
        if (offset > archive_size)
            return -1;
    }

    tcc_headers_ready = 1;
    return 0;
}

static void *ips_tcc_reallocator(void *ptr, unsigned long size)
{
    if (!size) {
        free(ptr);
        return NULL;
    }
    return realloc(ptr, (size_t)size);
}

static void tcc_error(void *opaque, const char *message)
{
    (void)opaque;
    term_print(message);
    term_print("\n");
}

static void tcc_add_api_symbol(TCCState *state, const char *name, const void *address)
{
    tcc_add_symbol(state, name, address);
}

static void tcc_register_runtime_symbols(TCCState *state)
{
    extern void ips_tcc_runtime_symbol_0(void) __asm__("__builtin_clrsb");
    extern void ips_tcc_runtime_symbol_1(void) __asm__("__builtin_clrsbl");
    extern void ips_tcc_runtime_symbol_2(void) __asm__("__builtin_clrsbll");
    extern void ips_tcc_runtime_symbol_3(void) __asm__("__builtin_clz");
    extern void ips_tcc_runtime_symbol_4(void) __asm__("__builtin_clzl");
    extern void ips_tcc_runtime_symbol_5(void) __asm__("__builtin_clzll");
    extern void ips_tcc_runtime_symbol_6(void) __asm__("__builtin_ctz");
    extern void ips_tcc_runtime_symbol_7(void) __asm__("__builtin_ctzl");
    extern void ips_tcc_runtime_symbol_8(void) __asm__("__builtin_ctzll");
    extern void ips_tcc_runtime_symbol_9(void) __asm__("__builtin_ffs");
    extern void ips_tcc_runtime_symbol_10(void) __asm__("__builtin_ffsl");
    extern void ips_tcc_runtime_symbol_11(void) __asm__("__builtin_ffsll");
    extern void ips_tcc_runtime_symbol_12(void) __asm__("__builtin_parity");
    extern void ips_tcc_runtime_symbol_13(void) __asm__("__builtin_parityl");
    extern void ips_tcc_runtime_symbol_14(void) __asm__("__builtin_parityll");
    extern void ips_tcc_runtime_symbol_15(void) __asm__("__builtin_popcount");
    extern void ips_tcc_runtime_symbol_16(void) __asm__("__builtin_popcountl");
    extern void ips_tcc_runtime_symbol_17(void) __asm__("__builtin_popcountll");
    extern void ips_tcc_runtime_symbol_18(void) __asm__("__tcc_builtin_clrsb");
    extern void ips_tcc_runtime_symbol_19(void) __asm__("__tcc_builtin_clrsbl");
    extern void ips_tcc_runtime_symbol_20(void) __asm__("__tcc_builtin_clrsbll");
    extern void ips_tcc_runtime_symbol_21(void) __asm__("__tcc_builtin_clz");
    extern void ips_tcc_runtime_symbol_22(void) __asm__("__tcc_builtin_clzl");
    extern void ips_tcc_runtime_symbol_23(void) __asm__("__tcc_builtin_clzll");
    extern void ips_tcc_runtime_symbol_24(void) __asm__("__tcc_builtin_ctz");
    extern void ips_tcc_runtime_symbol_25(void) __asm__("__tcc_builtin_ctzl");
    extern void ips_tcc_runtime_symbol_26(void) __asm__("__tcc_builtin_ctzll");
    extern void ips_tcc_runtime_symbol_27(void) __asm__("__tcc_builtin_ffs");
    extern void ips_tcc_runtime_symbol_28(void) __asm__("__tcc_builtin_ffsl");
    extern void ips_tcc_runtime_symbol_29(void) __asm__("__tcc_builtin_ffsll");
    extern void ips_tcc_runtime_symbol_30(void) __asm__("__tcc_builtin_parity");
    extern void ips_tcc_runtime_symbol_31(void) __asm__("__tcc_builtin_parityl");
    extern void ips_tcc_runtime_symbol_32(void) __asm__("__tcc_builtin_parityll");
    extern void ips_tcc_runtime_symbol_33(void) __asm__("__tcc_builtin_popcount");
    extern void ips_tcc_runtime_symbol_34(void) __asm__("__tcc_builtin_popcountl");
    extern void ips_tcc_runtime_symbol_35(void) __asm__("__tcc_builtin_popcountll");
    extern void ips_tcc_runtime_symbol_36(void) __asm__("__atomic_add_fetch_1");
    extern void ips_tcc_runtime_symbol_37(void) __asm__("__atomic_add_fetch_2");
    extern void ips_tcc_runtime_symbol_38(void) __asm__("__atomic_add_fetch_4");
    extern void ips_tcc_runtime_symbol_39(void) __asm__("__atomic_add_fetch_8");
    extern void ips_tcc_runtime_symbol_40(void) __asm__("__atomic_and_fetch_1");
    extern void ips_tcc_runtime_symbol_41(void) __asm__("__atomic_and_fetch_2");
    extern void ips_tcc_runtime_symbol_42(void) __asm__("__atomic_and_fetch_4");
    extern void ips_tcc_runtime_symbol_43(void) __asm__("__atomic_and_fetch_8");
    extern void ips_tcc_runtime_symbol_44(void) __asm__("__atomic_exchange_1");
    extern void ips_tcc_runtime_symbol_45(void) __asm__("__atomic_exchange_2");
    extern void ips_tcc_runtime_symbol_46(void) __asm__("__atomic_exchange_4");
    extern void ips_tcc_runtime_symbol_47(void) __asm__("__atomic_exchange_8");
    extern void ips_tcc_runtime_symbol_48(void) __asm__("__atomic_fetch_add_1");
    extern void ips_tcc_runtime_symbol_49(void) __asm__("__atomic_fetch_add_2");
    extern void ips_tcc_runtime_symbol_50(void) __asm__("__atomic_fetch_add_4");
    extern void ips_tcc_runtime_symbol_51(void) __asm__("__atomic_fetch_add_8");
    extern void ips_tcc_runtime_symbol_52(void) __asm__("__atomic_fetch_and_1");
    extern void ips_tcc_runtime_symbol_53(void) __asm__("__atomic_fetch_and_2");
    extern void ips_tcc_runtime_symbol_54(void) __asm__("__atomic_fetch_and_4");
    extern void ips_tcc_runtime_symbol_55(void) __asm__("__atomic_fetch_and_8");
    extern void ips_tcc_runtime_symbol_56(void) __asm__("__atomic_fetch_nand_1");
    extern void ips_tcc_runtime_symbol_57(void) __asm__("__atomic_fetch_nand_2");
    extern void ips_tcc_runtime_symbol_58(void) __asm__("__atomic_fetch_nand_4");
    extern void ips_tcc_runtime_symbol_59(void) __asm__("__atomic_fetch_nand_8");
    extern void ips_tcc_runtime_symbol_60(void) __asm__("__atomic_fetch_or_1");
    extern void ips_tcc_runtime_symbol_61(void) __asm__("__atomic_fetch_or_2");
    extern void ips_tcc_runtime_symbol_62(void) __asm__("__atomic_fetch_or_4");
    extern void ips_tcc_runtime_symbol_63(void) __asm__("__atomic_fetch_or_8");
    extern void ips_tcc_runtime_symbol_64(void) __asm__("__atomic_fetch_sub_1");
    extern void ips_tcc_runtime_symbol_65(void) __asm__("__atomic_fetch_sub_2");
    extern void ips_tcc_runtime_symbol_66(void) __asm__("__atomic_fetch_sub_4");
    extern void ips_tcc_runtime_symbol_67(void) __asm__("__atomic_fetch_sub_8");
    extern void ips_tcc_runtime_symbol_68(void) __asm__("__atomic_fetch_xor_1");
    extern void ips_tcc_runtime_symbol_69(void) __asm__("__atomic_fetch_xor_2");
    extern void ips_tcc_runtime_symbol_70(void) __asm__("__atomic_fetch_xor_4");
    extern void ips_tcc_runtime_symbol_71(void) __asm__("__atomic_fetch_xor_8");
    extern void ips_tcc_runtime_symbol_72(void) __asm__("__atomic_is_lock_free");
    extern void ips_tcc_runtime_symbol_73(void) __asm__("__atomic_nand_fetch_1");
    extern void ips_tcc_runtime_symbol_74(void) __asm__("__atomic_nand_fetch_2");
    extern void ips_tcc_runtime_symbol_75(void) __asm__("__atomic_nand_fetch_4");
    extern void ips_tcc_runtime_symbol_76(void) __asm__("__atomic_nand_fetch_8");
    extern void ips_tcc_runtime_symbol_77(void) __asm__("__atomic_or_fetch_1");
    extern void ips_tcc_runtime_symbol_78(void) __asm__("__atomic_or_fetch_2");
    extern void ips_tcc_runtime_symbol_79(void) __asm__("__atomic_or_fetch_4");
    extern void ips_tcc_runtime_symbol_80(void) __asm__("__atomic_or_fetch_8");
    extern void ips_tcc_runtime_symbol_81(void) __asm__("__atomic_sub_fetch_1");
    extern void ips_tcc_runtime_symbol_82(void) __asm__("__atomic_sub_fetch_2");
    extern void ips_tcc_runtime_symbol_83(void) __asm__("__atomic_sub_fetch_4");
    extern void ips_tcc_runtime_symbol_84(void) __asm__("__atomic_sub_fetch_8");
    extern void ips_tcc_runtime_symbol_85(void) __asm__("__atomic_xor_fetch_1");
    extern void ips_tcc_runtime_symbol_86(void) __asm__("__atomic_xor_fetch_2");
    extern void ips_tcc_runtime_symbol_87(void) __asm__("__atomic_xor_fetch_4");
    extern void ips_tcc_runtime_symbol_88(void) __asm__("__atomic_xor_fetch_8");
    extern void ips_tcc_runtime_symbol_89(void) __asm__("__tcc_atomic_is_lock_free");
    extern void ips_tcc_runtime_symbol_90(void) __asm__("__atomic_compare_exchange_1");
    extern void ips_tcc_runtime_symbol_91(void) __asm__("__atomic_compare_exchange_2");
    extern void ips_tcc_runtime_symbol_92(void) __asm__("__atomic_compare_exchange_4");
    extern void ips_tcc_runtime_symbol_93(void) __asm__("__atomic_compare_exchange_8");
    extern void ips_tcc_runtime_symbol_94(void) __asm__("__atomic_load_1");
    extern void ips_tcc_runtime_symbol_95(void) __asm__("__atomic_load_2");
    extern void ips_tcc_runtime_symbol_96(void) __asm__("__atomic_load_4");
    extern void ips_tcc_runtime_symbol_97(void) __asm__("__atomic_load_8");
    extern void ips_tcc_runtime_symbol_98(void) __asm__("__atomic_store_1");
    extern void ips_tcc_runtime_symbol_99(void) __asm__("__atomic_store_2");
    extern void ips_tcc_runtime_symbol_100(void) __asm__("__atomic_store_4");
    extern void ips_tcc_runtime_symbol_101(void) __asm__("__atomic_store_8");
    extern void ips_tcc_runtime_symbol_102(void) __asm__("__atomic_test_and_set_1");
    extern void ips_tcc_runtime_symbol_103(void) __asm__("__atomic_test_and_set_2");
    extern void ips_tcc_runtime_symbol_104(void) __asm__("__atomic_test_and_set_4");
    extern void ips_tcc_runtime_symbol_105(void) __asm__("__atomic_test_and_set_8");
    extern void ips_tcc_runtime_symbol_106(void) __asm__("atomic_flag_clear");
    extern void ips_tcc_runtime_symbol_107(void) __asm__("atomic_flag_clear_explicit");
    extern void ips_tcc_runtime_symbol_108(void) __asm__("atomic_flag_test_and_set");
    extern void ips_tcc_runtime_symbol_109(void) __asm__("atomic_flag_test_and_set_explicit");
    extern void ips_tcc_runtime_symbol_110(void) __asm__("atomic_signal_fence");
    extern void ips_tcc_runtime_symbol_111(void) __asm__("atomic_thread_fence");
    extern void ips_tcc_runtime_symbol_112(void) __asm__("__x86.get_pc_thunk.ax");
    extern void ips_tcc_runtime_symbol_113(void) __asm__("__x86.get_pc_thunk.bx");
    extern void ips_tcc_runtime_symbol_114(void) __asm__("__x86.get_pc_thunk.cx");
    extern void ips_tcc_runtime_symbol_115(void) __asm__("__x86.get_pc_thunk.dx");
    tcc_add_api_symbol(state, "__builtin_clrsb", (const void *)(uintptr_t)ips_tcc_runtime_symbol_0);
    tcc_add_api_symbol(state, "__builtin_clrsbl", (const void *)(uintptr_t)ips_tcc_runtime_symbol_1);
    tcc_add_api_symbol(state, "__builtin_clrsbll", (const void *)(uintptr_t)ips_tcc_runtime_symbol_2);
    tcc_add_api_symbol(state, "__builtin_clz", (const void *)(uintptr_t)ips_tcc_runtime_symbol_3);
    tcc_add_api_symbol(state, "__builtin_clzl", (const void *)(uintptr_t)ips_tcc_runtime_symbol_4);
    tcc_add_api_symbol(state, "__builtin_clzll", (const void *)(uintptr_t)ips_tcc_runtime_symbol_5);
    tcc_add_api_symbol(state, "__builtin_ctz", (const void *)(uintptr_t)ips_tcc_runtime_symbol_6);
    tcc_add_api_symbol(state, "__builtin_ctzl", (const void *)(uintptr_t)ips_tcc_runtime_symbol_7);
    tcc_add_api_symbol(state, "__builtin_ctzll", (const void *)(uintptr_t)ips_tcc_runtime_symbol_8);
    tcc_add_api_symbol(state, "__builtin_ffs", (const void *)(uintptr_t)ips_tcc_runtime_symbol_9);
    tcc_add_api_symbol(state, "__builtin_ffsl", (const void *)(uintptr_t)ips_tcc_runtime_symbol_10);
    tcc_add_api_symbol(state, "__builtin_ffsll", (const void *)(uintptr_t)ips_tcc_runtime_symbol_11);
    tcc_add_api_symbol(state, "__builtin_parity", (const void *)(uintptr_t)ips_tcc_runtime_symbol_12);
    tcc_add_api_symbol(state, "__builtin_parityl", (const void *)(uintptr_t)ips_tcc_runtime_symbol_13);
    tcc_add_api_symbol(state, "__builtin_parityll", (const void *)(uintptr_t)ips_tcc_runtime_symbol_14);
    tcc_add_api_symbol(state, "__builtin_popcount", (const void *)(uintptr_t)ips_tcc_runtime_symbol_15);
    tcc_add_api_symbol(state, "__builtin_popcountl", (const void *)(uintptr_t)ips_tcc_runtime_symbol_16);
    tcc_add_api_symbol(state, "__builtin_popcountll", (const void *)(uintptr_t)ips_tcc_runtime_symbol_17);
    tcc_add_api_symbol(state, "__tcc_builtin_clrsb", (const void *)(uintptr_t)ips_tcc_runtime_symbol_18);
    tcc_add_api_symbol(state, "__tcc_builtin_clrsbl", (const void *)(uintptr_t)ips_tcc_runtime_symbol_19);
    tcc_add_api_symbol(state, "__tcc_builtin_clrsbll", (const void *)(uintptr_t)ips_tcc_runtime_symbol_20);
    tcc_add_api_symbol(state, "__tcc_builtin_clz", (const void *)(uintptr_t)ips_tcc_runtime_symbol_21);
    tcc_add_api_symbol(state, "__tcc_builtin_clzl", (const void *)(uintptr_t)ips_tcc_runtime_symbol_22);
    tcc_add_api_symbol(state, "__tcc_builtin_clzll", (const void *)(uintptr_t)ips_tcc_runtime_symbol_23);
    tcc_add_api_symbol(state, "__tcc_builtin_ctz", (const void *)(uintptr_t)ips_tcc_runtime_symbol_24);
    tcc_add_api_symbol(state, "__tcc_builtin_ctzl", (const void *)(uintptr_t)ips_tcc_runtime_symbol_25);
    tcc_add_api_symbol(state, "__tcc_builtin_ctzll", (const void *)(uintptr_t)ips_tcc_runtime_symbol_26);
    tcc_add_api_symbol(state, "__tcc_builtin_ffs", (const void *)(uintptr_t)ips_tcc_runtime_symbol_27);
    tcc_add_api_symbol(state, "__tcc_builtin_ffsl", (const void *)(uintptr_t)ips_tcc_runtime_symbol_28);
    tcc_add_api_symbol(state, "__tcc_builtin_ffsll", (const void *)(uintptr_t)ips_tcc_runtime_symbol_29);
    tcc_add_api_symbol(state, "__tcc_builtin_parity", (const void *)(uintptr_t)ips_tcc_runtime_symbol_30);
    tcc_add_api_symbol(state, "__tcc_builtin_parityl", (const void *)(uintptr_t)ips_tcc_runtime_symbol_31);
    tcc_add_api_symbol(state, "__tcc_builtin_parityll", (const void *)(uintptr_t)ips_tcc_runtime_symbol_32);
    tcc_add_api_symbol(state, "__tcc_builtin_popcount", (const void *)(uintptr_t)ips_tcc_runtime_symbol_33);
    tcc_add_api_symbol(state, "__tcc_builtin_popcountl", (const void *)(uintptr_t)ips_tcc_runtime_symbol_34);
    tcc_add_api_symbol(state, "__tcc_builtin_popcountll", (const void *)(uintptr_t)ips_tcc_runtime_symbol_35);
    tcc_add_api_symbol(state, "__atomic_add_fetch_1", (const void *)(uintptr_t)ips_tcc_runtime_symbol_36);
    tcc_add_api_symbol(state, "__atomic_add_fetch_2", (const void *)(uintptr_t)ips_tcc_runtime_symbol_37);
    tcc_add_api_symbol(state, "__atomic_add_fetch_4", (const void *)(uintptr_t)ips_tcc_runtime_symbol_38);
    tcc_add_api_symbol(state, "__atomic_add_fetch_8", (const void *)(uintptr_t)ips_tcc_runtime_symbol_39);
    tcc_add_api_symbol(state, "__atomic_and_fetch_1", (const void *)(uintptr_t)ips_tcc_runtime_symbol_40);
    tcc_add_api_symbol(state, "__atomic_and_fetch_2", (const void *)(uintptr_t)ips_tcc_runtime_symbol_41);
    tcc_add_api_symbol(state, "__atomic_and_fetch_4", (const void *)(uintptr_t)ips_tcc_runtime_symbol_42);
    tcc_add_api_symbol(state, "__atomic_and_fetch_8", (const void *)(uintptr_t)ips_tcc_runtime_symbol_43);
    tcc_add_api_symbol(state, "__atomic_exchange_1", (const void *)(uintptr_t)ips_tcc_runtime_symbol_44);
    tcc_add_api_symbol(state, "__atomic_exchange_2", (const void *)(uintptr_t)ips_tcc_runtime_symbol_45);
    tcc_add_api_symbol(state, "__atomic_exchange_4", (const void *)(uintptr_t)ips_tcc_runtime_symbol_46);
    tcc_add_api_symbol(state, "__atomic_exchange_8", (const void *)(uintptr_t)ips_tcc_runtime_symbol_47);
    tcc_add_api_symbol(state, "__atomic_fetch_add_1", (const void *)(uintptr_t)ips_tcc_runtime_symbol_48);
    tcc_add_api_symbol(state, "__atomic_fetch_add_2", (const void *)(uintptr_t)ips_tcc_runtime_symbol_49);
    tcc_add_api_symbol(state, "__atomic_fetch_add_4", (const void *)(uintptr_t)ips_tcc_runtime_symbol_50);
    tcc_add_api_symbol(state, "__atomic_fetch_add_8", (const void *)(uintptr_t)ips_tcc_runtime_symbol_51);
    tcc_add_api_symbol(state, "__atomic_fetch_and_1", (const void *)(uintptr_t)ips_tcc_runtime_symbol_52);
    tcc_add_api_symbol(state, "__atomic_fetch_and_2", (const void *)(uintptr_t)ips_tcc_runtime_symbol_53);
    tcc_add_api_symbol(state, "__atomic_fetch_and_4", (const void *)(uintptr_t)ips_tcc_runtime_symbol_54);
    tcc_add_api_symbol(state, "__atomic_fetch_and_8", (const void *)(uintptr_t)ips_tcc_runtime_symbol_55);
    tcc_add_api_symbol(state, "__atomic_fetch_nand_1", (const void *)(uintptr_t)ips_tcc_runtime_symbol_56);
    tcc_add_api_symbol(state, "__atomic_fetch_nand_2", (const void *)(uintptr_t)ips_tcc_runtime_symbol_57);
    tcc_add_api_symbol(state, "__atomic_fetch_nand_4", (const void *)(uintptr_t)ips_tcc_runtime_symbol_58);
    tcc_add_api_symbol(state, "__atomic_fetch_nand_8", (const void *)(uintptr_t)ips_tcc_runtime_symbol_59);
    tcc_add_api_symbol(state, "__atomic_fetch_or_1", (const void *)(uintptr_t)ips_tcc_runtime_symbol_60);
    tcc_add_api_symbol(state, "__atomic_fetch_or_2", (const void *)(uintptr_t)ips_tcc_runtime_symbol_61);
    tcc_add_api_symbol(state, "__atomic_fetch_or_4", (const void *)(uintptr_t)ips_tcc_runtime_symbol_62);
    tcc_add_api_symbol(state, "__atomic_fetch_or_8", (const void *)(uintptr_t)ips_tcc_runtime_symbol_63);
    tcc_add_api_symbol(state, "__atomic_fetch_sub_1", (const void *)(uintptr_t)ips_tcc_runtime_symbol_64);
    tcc_add_api_symbol(state, "__atomic_fetch_sub_2", (const void *)(uintptr_t)ips_tcc_runtime_symbol_65);
    tcc_add_api_symbol(state, "__atomic_fetch_sub_4", (const void *)(uintptr_t)ips_tcc_runtime_symbol_66);
    tcc_add_api_symbol(state, "__atomic_fetch_sub_8", (const void *)(uintptr_t)ips_tcc_runtime_symbol_67);
    tcc_add_api_symbol(state, "__atomic_fetch_xor_1", (const void *)(uintptr_t)ips_tcc_runtime_symbol_68);
    tcc_add_api_symbol(state, "__atomic_fetch_xor_2", (const void *)(uintptr_t)ips_tcc_runtime_symbol_69);
    tcc_add_api_symbol(state, "__atomic_fetch_xor_4", (const void *)(uintptr_t)ips_tcc_runtime_symbol_70);
    tcc_add_api_symbol(state, "__atomic_fetch_xor_8", (const void *)(uintptr_t)ips_tcc_runtime_symbol_71);
    tcc_add_api_symbol(state, "__atomic_is_lock_free", (const void *)(uintptr_t)ips_tcc_runtime_symbol_72);
    tcc_add_api_symbol(state, "__atomic_nand_fetch_1", (const void *)(uintptr_t)ips_tcc_runtime_symbol_73);
    tcc_add_api_symbol(state, "__atomic_nand_fetch_2", (const void *)(uintptr_t)ips_tcc_runtime_symbol_74);
    tcc_add_api_symbol(state, "__atomic_nand_fetch_4", (const void *)(uintptr_t)ips_tcc_runtime_symbol_75);
    tcc_add_api_symbol(state, "__atomic_nand_fetch_8", (const void *)(uintptr_t)ips_tcc_runtime_symbol_76);
    tcc_add_api_symbol(state, "__atomic_or_fetch_1", (const void *)(uintptr_t)ips_tcc_runtime_symbol_77);
    tcc_add_api_symbol(state, "__atomic_or_fetch_2", (const void *)(uintptr_t)ips_tcc_runtime_symbol_78);
    tcc_add_api_symbol(state, "__atomic_or_fetch_4", (const void *)(uintptr_t)ips_tcc_runtime_symbol_79);
    tcc_add_api_symbol(state, "__atomic_or_fetch_8", (const void *)(uintptr_t)ips_tcc_runtime_symbol_80);
    tcc_add_api_symbol(state, "__atomic_sub_fetch_1", (const void *)(uintptr_t)ips_tcc_runtime_symbol_81);
    tcc_add_api_symbol(state, "__atomic_sub_fetch_2", (const void *)(uintptr_t)ips_tcc_runtime_symbol_82);
    tcc_add_api_symbol(state, "__atomic_sub_fetch_4", (const void *)(uintptr_t)ips_tcc_runtime_symbol_83);
    tcc_add_api_symbol(state, "__atomic_sub_fetch_8", (const void *)(uintptr_t)ips_tcc_runtime_symbol_84);
    tcc_add_api_symbol(state, "__atomic_xor_fetch_1", (const void *)(uintptr_t)ips_tcc_runtime_symbol_85);
    tcc_add_api_symbol(state, "__atomic_xor_fetch_2", (const void *)(uintptr_t)ips_tcc_runtime_symbol_86);
    tcc_add_api_symbol(state, "__atomic_xor_fetch_4", (const void *)(uintptr_t)ips_tcc_runtime_symbol_87);
    tcc_add_api_symbol(state, "__atomic_xor_fetch_8", (const void *)(uintptr_t)ips_tcc_runtime_symbol_88);
    tcc_add_api_symbol(state, "__tcc_atomic_is_lock_free", (const void *)(uintptr_t)ips_tcc_runtime_symbol_89);
    tcc_add_api_symbol(state, "__atomic_compare_exchange_1", (const void *)(uintptr_t)ips_tcc_runtime_symbol_90);
    tcc_add_api_symbol(state, "__atomic_compare_exchange_2", (const void *)(uintptr_t)ips_tcc_runtime_symbol_91);
    tcc_add_api_symbol(state, "__atomic_compare_exchange_4", (const void *)(uintptr_t)ips_tcc_runtime_symbol_92);
    tcc_add_api_symbol(state, "__atomic_compare_exchange_8", (const void *)(uintptr_t)ips_tcc_runtime_symbol_93);
    tcc_add_api_symbol(state, "__atomic_load_1", (const void *)(uintptr_t)ips_tcc_runtime_symbol_94);
    tcc_add_api_symbol(state, "__atomic_load_2", (const void *)(uintptr_t)ips_tcc_runtime_symbol_95);
    tcc_add_api_symbol(state, "__atomic_load_4", (const void *)(uintptr_t)ips_tcc_runtime_symbol_96);
    tcc_add_api_symbol(state, "__atomic_load_8", (const void *)(uintptr_t)ips_tcc_runtime_symbol_97);
    tcc_add_api_symbol(state, "__atomic_store_1", (const void *)(uintptr_t)ips_tcc_runtime_symbol_98);
    tcc_add_api_symbol(state, "__atomic_store_2", (const void *)(uintptr_t)ips_tcc_runtime_symbol_99);
    tcc_add_api_symbol(state, "__atomic_store_4", (const void *)(uintptr_t)ips_tcc_runtime_symbol_100);
    tcc_add_api_symbol(state, "__atomic_store_8", (const void *)(uintptr_t)ips_tcc_runtime_symbol_101);
    tcc_add_api_symbol(state, "__atomic_test_and_set_1", (const void *)(uintptr_t)ips_tcc_runtime_symbol_102);
    tcc_add_api_symbol(state, "__atomic_test_and_set_2", (const void *)(uintptr_t)ips_tcc_runtime_symbol_103);
    tcc_add_api_symbol(state, "__atomic_test_and_set_4", (const void *)(uintptr_t)ips_tcc_runtime_symbol_104);
    tcc_add_api_symbol(state, "__atomic_test_and_set_8", (const void *)(uintptr_t)ips_tcc_runtime_symbol_105);
    tcc_add_api_symbol(state, "atomic_flag_clear", (const void *)(uintptr_t)ips_tcc_runtime_symbol_106);
    tcc_add_api_symbol(state, "atomic_flag_clear_explicit", (const void *)(uintptr_t)ips_tcc_runtime_symbol_107);
    tcc_add_api_symbol(state, "atomic_flag_test_and_set", (const void *)(uintptr_t)ips_tcc_runtime_symbol_108);
    tcc_add_api_symbol(state, "atomic_flag_test_and_set_explicit", (const void *)(uintptr_t)ips_tcc_runtime_symbol_109);
    tcc_add_api_symbol(state, "atomic_signal_fence", (const void *)(uintptr_t)ips_tcc_runtime_symbol_110);
    tcc_add_api_symbol(state, "atomic_thread_fence", (const void *)(uintptr_t)ips_tcc_runtime_symbol_111);
    tcc_add_api_symbol(state, "__x86.get_pc_thunk.ax", (const void *)(uintptr_t)ips_tcc_runtime_symbol_112);
    tcc_add_api_symbol(state, "__x86.get_pc_thunk.bx", (const void *)(uintptr_t)ips_tcc_runtime_symbol_113);
    tcc_add_api_symbol(state, "__x86.get_pc_thunk.cx", (const void *)(uintptr_t)ips_tcc_runtime_symbol_114);
    tcc_add_api_symbol(state, "__x86.get_pc_thunk.dx", (const void *)(uintptr_t)ips_tcc_runtime_symbol_115);
}

static void tcc_register_symbols(TCCState *state)
{
#define TCC_SYMBOL(n) tcc_add_api_symbol(state, #n, (const void *)(uintptr_t)n)
    TCC_SYMBOL(set_up);
    TCC_SYMBOL(desktop_init);
    TCC_SYMBOL(desktop_update);
    TCC_SYMBOL(desktop_run);
    TCC_SYMBOL(desktop_invalidate);
    TCC_SYMBOL(desktop_invalidate_rect);
    TCC_SYMBOL(wm_invalidate_all);
    TCC_SYMBOL(wm_invalidate_rect);
    TCC_SYMBOL(wm_draw);
    TCC_SYMBOL(wm_draw_all);
    TCC_SYMBOL(wm_handle_mouse);
    TCC_SYMBOL(wm_set_renderer);
    TCC_SYMBOL(wm_focus);
    TCC_SYMBOL(wm_close);
    TCC_SYMBOL(wm_hide);
    TCC_SYMBOL(wm_create);
    TCC_SYMBOL(wm_resize);
    TCC_SYMBOL(wm_set_pos);
    TCC_SYMBOL(wm_unvisible);
    TCC_SYMBOL(window_create);
    TCC_SYMBOL(window_destroy);
    TCC_SYMBOL(window_show);
    TCC_SYMBOL(window_hide);
    TCC_SYMBOL(window_is_open);
    TCC_SYMBOL(window_set_title);
    TCC_SYMBOL(window_set_position);
    TCC_SYMBOL(window_set_size);
    TCC_SYMBOL(window_get_client_size);
    TCC_SYMBOL(window_set_render_callback);
    TCC_SYMBOL(window_set_gl_render_callback);
    TCC_SYMBOL(window_invalidate);
    TCC_SYMBOL(window_draw_clear);
    TCC_SYMBOL(window_draw_pixel);
    TCC_SYMBOL(window_draw_line);
    TCC_SYMBOL(window_draw_rect);
    TCC_SYMBOL(window_draw_rect_outline);
    TCC_SYMBOL(window_draw_circle);
    TCC_SYMBOL(window_draw_text);
    TCC_SYMBOL(term_print_char_cl);
    TCC_SYMBOL(term_print_cl);
    TCC_SYMBOL(term_print);
    TCC_SYMBOL(term_print_char);
    TCC_SYMBOL(term_print_int);
    TCC_SYMBOL(term_clear);
    TCC_SYMBOL(term_run);
    TCC_SYMBOL(gfx_init);
    TCC_SYMBOL(gfx_ready);
    TCC_SYMBOL(gfx_present);
    TCC_SYMBOL(gfx_cursor_hardware_ready);
    TCC_SYMBOL(gfx_cursor_move);
    TCC_SYMBOL(gfx_driver_error);
    TCC_SYMBOL(gfx_width);
    TCC_SYMBOL(gfx_height);
    TCC_SYMBOL(gfx_clear);
    TCC_SYMBOL(gfx_draw_pixel);
    TCC_SYMBOL(gfx_get_pixel);
    TCC_SYMBOL(gfx_draw_hline);
    TCC_SYMBOL(gfx_draw_vline);
    TCC_SYMBOL(gfx_draw_line);
    TCC_SYMBOL(gfx_draw_rect);
    TCC_SYMBOL(gfx_draw_square);
    TCC_SYMBOL(gfx_draw_rect_outline);
    TCC_SYMBOL(gfx_draw_circle);
    TCC_SYMBOL(gfx_fill_circle);
    TCC_SYMBOL(gfx_draw_char);
    TCC_SYMBOL(gfx_draw_char_bg);
    TCC_SYMBOL(gfx_draw_text);
    TCC_SYMBOL(gfx_draw_image);
    TCC_SYMBOL(gfx_blit);
    TCC_SYMBOL(gfx_set_clip);
    TCC_SYMBOL(gfx_reset_clip);
    TCC_SYMBOL(gfx_get_clip);
    TCC_SYMBOL(gfx_clear_screen);
    TCC_SYMBOL(gfx_print_char);
    TCC_SYMBOL(gfx_print_gr_cl);
    TCC_SYMBOL(gfx_print_char_gr_cl);
    TCC_SYMBOL(virgl_init);
    TCC_SYMBOL(virgl_ready);
    TCC_SYMBOL(virgl_get_caps);
    TCC_SYMBOL(virgl_get_capset);
    TCC_SYMBOL(virgl_context_create);
    TCC_SYMBOL(virgl_context_destroy);
    TCC_SYMBOL(virgl_resource_create_3d);
    TCC_SYMBOL(virgl_resource_attach);
    TCC_SYMBOL(virgl_resource_inline_write);
    TCC_SYMBOL(virgl_submit);
    TCC_SYMBOL(virgl_last_error);
    TCC_SYMBOL(glInit);
    TCC_SYMBOL(glIsReady);
    TCC_SYMBOL(glClearColor);
    TCC_SYMBOL(glClear);
    TCC_SYMBOL(glClearDepth);
    TCC_SYMBOL(glEnable);
    TCC_SYMBOL(glDisable);
    TCC_SYMBOL(glDepthFunc);
    TCC_SYMBOL(glDepthMask);
    TCC_SYMBOL(glViewport);
    TCC_SYMBOL(glFlush);
    TCC_SYMBOL(glCreateContext);
    TCC_SYMBOL(glDestroyContext);
    TCC_SYMBOL(glCreateResource3D);
    TCC_SYMBOL(glAttachResource);
    TCC_SYMBOL(glSubmitCommands);
    TCC_SYMBOL(glGetError);
    TCC_SYMBOL(glGenBuffers);
    TCC_SYMBOL(glBindBuffer);
    TCC_SYMBOL(glBufferData);
    TCC_SYMBOL(glBufferSubData);
    TCC_SYMBOL(glEnableVertexAttribArray);
    TCC_SYMBOL(glDisableVertexAttribArray);
    TCC_SYMBOL(glVertexAttribPointer);
    TCC_SYMBOL(glCreateShader);
    TCC_SYMBOL(glShaderSourceTGSI);
    TCC_SYMBOL(glCompileShader);
    TCC_SYMBOL(glCreateProgram);
    TCC_SYMBOL(glAttachShader);
    TCC_SYMBOL(glLinkProgram);
    TCC_SYMBOL(glUseProgram);
    TCC_SYMBOL(glDrawArrays);
    TCC_SYMBOL(glDrawElements);
    TCC_SYMBOL(input);
    TCC_SYMBOL(readKey);
    TCC_SYMBOL(mouse_x);
    TCC_SYMBOL(mouse_y);
    TCC_SYMBOL(mouse_moved);
    TCC_SYMBOL(mouse_buttons);
    TCC_SYMBOL(get_mouse_x);
    TCC_SYMBOL(get_mouse_y);
    TCC_SYMBOL(get_mouse_moved);
    TCC_SYMBOL(get_mouse_buttons);
    TCC_SYMBOL(create_file);
    TCC_SYMBOL(deleteEx);
    TCC_SYMBOL(read_file);
    TCC_SYMBOL(write_to_file);
    TCC_SYMBOL(list);
    TCC_SYMBOL(list_ex);
    TCC_SYMBOL(mkdir);
    TCC_SYMBOL(malloc);
    TCC_SYMBOL(free);
    TCC_SYMBOL(calloc);
    TCC_SYMBOL(realloc);
    TCC_SYMBOL(memcpy);
    TCC_SYMBOL(memmove);
    TCC_SYMBOL(memset);
    TCC_SYMBOL(memcmp);
    TCC_SYMBOL(memchr);
    TCC_SYMBOL(strlen);
    TCC_SYMBOL(strnlen);
    TCC_SYMBOL(strcpy);
    TCC_SYMBOL(strncpy);
    TCC_SYMBOL(strcat);
    TCC_SYMBOL(strncat);
    TCC_SYMBOL(strcmp);
    TCC_SYMBOL(strncmp);
    TCC_SYMBOL(strcasecmp);
    TCC_SYMBOL(strncasecmp);
    TCC_SYMBOL(strchr);
    TCC_SYMBOL(strrchr);
    TCC_SYMBOL(strstr);
    TCC_SYMBOL(strdup);
    TCC_SYMBOL(strndup);
    TCC_SYMBOL(strspn);
    TCC_SYMBOL(strcspn);
    TCC_SYMBOL(strpbrk);
    TCC_SYMBOL(strtok);
    TCC_SYMBOL(isalnum);
    TCC_SYMBOL(isalpha);
    TCC_SYMBOL(isascii);
    TCC_SYMBOL(isblank);
    TCC_SYMBOL(iscntrl);
    TCC_SYMBOL(isdigit);
    TCC_SYMBOL(isgraph);
    TCC_SYMBOL(islower);
    TCC_SYMBOL(isprint);
    TCC_SYMBOL(ispunct);
    TCC_SYMBOL(isspace);
    TCC_SYMBOL(isupper);
    TCC_SYMBOL(isxdigit);
    TCC_SYMBOL(tolower);
    TCC_SYMBOL(toupper);
    TCC_SYMBOL(strtol);
    TCC_SYMBOL(strtoul);
    TCC_SYMBOL(strtoll);
    TCC_SYMBOL(getenv);
    TCC_SYMBOL(strtoull);
    TCC_SYMBOL(strtof);
    TCC_SYMBOL(strtold);
    TCC_SYMBOL(strtod);
    TCC_SYMBOL(ldexpl);
    TCC_SYMBOL(atoi);
    TCC_SYMBOL(atol);
    TCC_SYMBOL(abs);
    TCC_SYMBOL(labs);
    TCC_SYMBOL(qsort);
    TCC_SYMBOL(bsearch);
    TCC_SYMBOL(rand);
    TCC_SYMBOL(srand);
    TCC_SYMBOL(printf);
    TCC_SYMBOL(fprintf);
    TCC_SYMBOL(sprintf);
    TCC_SYMBOL(snprintf);
    TCC_SYMBOL(vsnprintf);
    TCC_SYMBOL(vsprintf);
    TCC_SYMBOL(vfprintf);
    TCC_SYMBOL(vprintf);
    TCC_SYMBOL(puts);
    TCC_SYMBOL(putchar);
    TCC_SYMBOL(getchar);
    TCC_SYMBOL(fopen);
    TCC_SYMBOL(fdopen);
    TCC_SYMBOL(fclose);
    TCC_SYMBOL(fflush);
    TCC_SYMBOL(fread);
    TCC_SYMBOL(fwrite);
    TCC_SYMBOL(fseek);
    TCC_SYMBOL(ftell);
    TCC_SYMBOL(rewind);
    TCC_SYMBOL(feof);
    TCC_SYMBOL(ferror);
    TCC_SYMBOL(clearerr);
    TCC_SYMBOL(fgetc);
    TCC_SYMBOL(getc);
    TCC_SYMBOL(ungetc);
    TCC_SYMBOL(fputc);
    TCC_SYMBOL(putc);
    TCC_SYMBOL(fgets);
    TCC_SYMBOL(fputs);
    TCC_SYMBOL(remove);
    TCC_SYMBOL(rename);
    TCC_SYMBOL(fileno);
    TCC_SYMBOL(perror);
    TCC_SYMBOL(strerror);
    TCC_SYMBOL(setvbuf);
    TCC_SYMBOL(setbuf);
    TCC_SYMBOL(open);
    TCC_SYMBOL(read);
    TCC_SYMBOL(write);
    TCC_SYMBOL(close);
    TCC_SYMBOL(lseek);
    TCC_SYMBOL(unlink);
    TCC_SYMBOL(access);
    TCC_SYMBOL(isatty);
    TCC_SYMBOL(getcwd);
    TCC_SYMBOL(chdir);
    TCC_SYMBOL(usleep);
    TCC_SYMBOL(sleep);
    TCC_SYMBOL(realpath);
    TCC_SYMBOL(time);
    TCC_SYMBOL(localtime);
    TCC_SYMBOL(gmtime);
    TCC_SYMBOL(clock);
    TCC_SYMBOL(gettimeofday);
    TCC_SYMBOL(mprotect);
    TCC_SYMBOL(setjmp);
    TCC_SYMBOL(longjmp);
    TCC_SYMBOL(isinf);
    TCC_SYMBOL(isnan);
    TCC_SYMBOL(__divdi3);
    TCC_SYMBOL(__moddi3);
    TCC_SYMBOL(__udivdi3);
    TCC_SYMBOL(__umoddi3);
    TCC_SYMBOL(__ashrdi3);
    TCC_SYMBOL(__lshrdi3);
    TCC_SYMBOL(__ashldi3);
    TCC_SYMBOL(__floatundisf);
    TCC_SYMBOL(__floatundidf);
    TCC_SYMBOL(__floatundixf);
    TCC_SYMBOL(__fixunssfdi);
    TCC_SYMBOL(__fixsfdi);
    TCC_SYMBOL(__fixunsdfdi);
    TCC_SYMBOL(__fixdfdi);
    TCC_SYMBOL(__fixunsxfdi);
    TCC_SYMBOL(__fixxfdi);
    TCC_SYMBOL(alloca);
    TCC_SYMBOL(update_terminal);
    TCC_SYMBOL(task_create);
    TCC_SYMBOL(task_yield);
    TCC_SYMBOL(task_sleep);
    TCC_SYMBOL(task_exit);
    TCC_SYMBOL(task_get_id);
    tcc_add_api_symbol(state, "exit", (const void *)(uintptr_t)ips_process_exit);
    tcc_add_api_symbol(state, "_Exit", (const void *)(uintptr_t)ips_process_exit);
    tcc_add_api_symbol(state, "abort", (const void *)(uintptr_t)ips_process_abort);
    tcc_add_api_symbol(state, "errno", &errno);
    tcc_add_api_symbol(state, "stdin", &stdin);
    tcc_add_api_symbol(state, "stdout", &stdout);
    tcc_add_api_symbol(state, "stderr", &stderr);
    tcc_register_runtime_symbols(state);
#undef TCC_SYMBOL
}

#define TCC_COMMAND_MAX_TOKENS 24
#define TCC_COMMAND_MAX_FILES 16

typedef struct {
    char storage[256];
    char *tokens[TCC_COMMAND_MAX_TOKENS];
    char source_paths[TCC_COMMAND_MAX_FILES][256];
    char include_paths[TCC_COMMAND_MAX_FILES][256];
    char library_paths[TCC_COMMAND_MAX_FILES][256];
    char *libraries[TCC_COMMAND_MAX_FILES];
    char output_path[256];
    char options[512];
} tcc_command_workspace_t;

static int tcc_split_command(const char *input, char *storage, char **tokens, int capacity)
{
    int count = 0;
    int quoted = 0;
    char *out = storage;

    if (!input)
        return -1;

    while (*input) {
        while (*input == ' ' || *input == '\t')
            ++input;
        if (!*input)
            break;
        if (count >= capacity)
            return -1;

        tokens[count++] = out;
        quoted = 0;

        while (*input) {
            if (*input == '"') {
                quoted = !quoted;
                ++input;
                continue;
            }
            if (!quoted && (*input == ' ' || *input == '\t'))
                break;
            *out++ = *input++;
        }
        if (quoted)
            return -1;
        *out++ = '\0';
    }

    return count;
}

static int tcc_append_option(char *buffer, size_t capacity, const char *option)
{
    size_t used = strlen(buffer);
    size_t length = strlen(option);

    if (used + length + 2u > capacity)
        return -1;
    if (used)
        buffer[used++] = ' ';
    memcpy(buffer + used, option, length + 1u);
    return 0;
}

int ips_tcc_compile(const char *arguments)
{
    tcc_command_workspace_t *workspace;
    int token_count;
    int source_count = 0;
    int include_count = 0;
    int library_path_count = 0;
    int library_count = 0;
    int have_output = 0;
    TCCState *state = NULL;
    int result = -1;

    if (!arguments || !*arguments) {
        term_print("Usage: tcc input.c [-o output] [other.c] [-I path] [-L path] [-lname]\n");
        return -1;
    }

    workspace = (tcc_command_workspace_t *)calloc(1, sizeof(*workspace));
    if (!workspace) {
        term_print("TCC: out of memory\n");
        return -1;
    }

    token_count = tcc_split_command(arguments, workspace->storage,
                                    workspace->tokens, TCC_COMMAND_MAX_TOKENS);
    if (token_count <= 0) {
        term_print("TCC: invalid arguments\n");
        goto cleanup;
    }

    for (int i = 0; i < token_count; ++i) {
        char path[256];
        char *token = workspace->tokens[i];

        if (strcmp(token, "-o") == 0) {
            if (++i >= token_count || have_output)
                goto invalid_arguments;
            build_path(workspace->output_path, workspace->tokens[i]);
            have_output = 1;
            continue;
        }

        if (strcmp(token, "-I") == 0 || strcmp(token, "-L") == 0) {
            int is_include = token[1] == 'I';
            if (++i >= token_count)
                goto invalid_arguments;
            build_path(path, workspace->tokens[i]);
            if (is_include) {
                if (include_count >= TCC_COMMAND_MAX_FILES)
                    goto invalid_arguments;
                strcpy(workspace->include_paths[include_count++], path);
            } else {
                if (library_path_count >= TCC_COMMAND_MAX_FILES)
                    goto invalid_arguments;
                strcpy(workspace->library_paths[library_path_count++], path);
            }
            continue;
        }

        if (token[0] == '-' && token[1] == 'I' && token[2]) {
            build_path(path, token + 2);
            if (include_count >= TCC_COMMAND_MAX_FILES)
                goto invalid_arguments;
            strcpy(workspace->include_paths[include_count++], path);
            continue;
        }

        if (token[0] == '-' && token[1] == 'L' && token[2]) {
            build_path(path, token + 2);
            if (library_path_count >= TCC_COMMAND_MAX_FILES)
                goto invalid_arguments;
            strcpy(workspace->library_paths[library_path_count++], path);
            continue;
        }

        if (strcmp(token, "-l") == 0) {
            if (++i >= token_count || library_count >= TCC_COMMAND_MAX_FILES)
                goto invalid_arguments;
            workspace->libraries[library_count++] = workspace->tokens[i];
            continue;
        }

        if (token[0] == '-' && token[1] == 'l' && token[2]) {
            if (library_count >= TCC_COMMAND_MAX_FILES)
                goto invalid_arguments;
            workspace->libraries[library_count++] = token + 2;
            continue;
        }

        if (strcmp(token, "-D") == 0 || strcmp(token, "-U") == 0 ||
            strcmp(token, "-include") == 0 || strcmp(token, "-isystem") == 0 ||
            strcmp(token, "-x") == 0) {
            if (++i >= token_count ||
                tcc_append_option(workspace->options, sizeof(workspace->options), token) != 0 ||
                tcc_append_option(workspace->options, sizeof(workspace->options), workspace->tokens[i]) != 0)
                goto invalid_arguments;
            continue;
        }

        if (token[0] == '-') {
            if (tcc_append_option(workspace->options, sizeof(workspace->options), token) != 0)
                goto invalid_arguments;
            continue;
        }

        if (source_count >= TCC_COMMAND_MAX_FILES)
            goto invalid_arguments;
        build_path(path, token);
        strcpy(workspace->source_paths[source_count++], path);
    }

    if (!have_output || !source_count)
        goto invalid_arguments;

    if (tcc_install_headers() < 0) {
        term_print("TCC: cannot install headers\n");
        goto cleanup;
    }

    if (tcc_append_option(workspace->options, sizeof(workspace->options), "-nostdinc") != 0 ||
        tcc_append_option(workspace->options, sizeof(workspace->options), "-nostdlib") != 0 ||
        tcc_append_option(workspace->options, sizeof(workspace->options), "-static") != 0 ||
        tcc_append_option(workspace->options, sizeof(workspace->options), "-Wl,-nostdlib") != 0 ||
        tcc_append_option(workspace->options, sizeof(workspace->options), "-Wl,-e,main") != 0 ||
        tcc_append_option(workspace->options, sizeof(workspace->options), "-Wl,-Ttext=00c00000") != 0) {
        term_print("TCC: too many options\n");
        goto cleanup;
    }

    tcc_set_realloc(ips_tcc_reallocator);
    state = tcc_new();
    if (!state) {
        term_print("TCC: cannot create state\n");
        goto cleanup;
    }

    tcc_set_error_func(state, NULL, tcc_error);
    tcc_set_lib_path(state, "/tcc");

    if (tcc_set_options(state, workspace->options) < 0 ||
        tcc_set_output_type(state, TCC_OUTPUT_EXE) < 0)
        goto cleanup;

    tcc_add_include_path(state, "/include");
    tcc_add_include_path(state, "/include/tcc");
    tcc_add_include_path(state, "/sys");
    tcc_add_library_path(state, "/sys");
    tcc_add_library_path(state, "/lib");

    for (int i = 0; i < include_count; ++i)
        tcc_add_include_path(state, workspace->include_paths[i]);
    for (int i = 0; i < library_path_count; ++i)
        tcc_add_library_path(state, workspace->library_paths[i]);

    tcc_register_symbols(state);

    for (int i = 0; i < source_count; ++i) {
        if (tcc_add_file(state, workspace->source_paths[i]) < 0)
            goto cleanup;
    }

    for (int i = 0; i < library_count; ++i) {
        if (tcc_add_library(state, workspace->libraries[i]) < 0)
            goto cleanup;
    }

    if (tcc_output_file(state, workspace->output_path) < 0) {
        term_print("TCC: cannot write executable\n");
        goto cleanup;
    }

    term_print("TCC: compilation successful\nOutput: ");
    term_print(workspace->output_path);
    term_print("\n");
    result = 0;
    goto cleanup;

invalid_arguments:
    term_print("Usage: tcc input.c [-o output] [other.c] [-I path] [-L path] [-lname]\n");

cleanup:
    if (state)
        tcc_delete(state);
    free(workspace);
    return result;
}

