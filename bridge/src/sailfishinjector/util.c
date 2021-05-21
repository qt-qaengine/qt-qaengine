#include <stdio.h>
#include <stdarg.h>
#include "injector_internal.h"

char injector__errmsg[512];
char injector__errmsg_is_set;

void injector__set_errmsg(const char *format, ...)
{
    va_list ap;
    int rv;

    /* prevent the error message from being overwritten. */
    if (injector__errmsg_is_set) {
        return;
    }
    injector__errmsg_is_set = 1;

    va_start(ap, format);
    rv = vsnprintf(injector__errmsg, sizeof(injector__errmsg), format, ap);
    va_end(ap);
    if (rv == -1 || rv >= sizeof(injector__errmsg)) {
        injector__errmsg[sizeof(injector__errmsg) - 1] = '\0';
    }
}

const char *injector__arch2name(arch_t arch)
{
    switch (arch) {
    case ARCH_X86_64:
        return "x86_64";
    case ARCH_X86_64_X32:
        return "x86_64 x32-ABI";
    case ARCH_I386:
        return "i386";
    case ARCH_ARM64:
        return "ARM64";
    case ARCH_ARM_EABI_THUMB:
        return "ARM EABI thumb";
    case ARCH_ARM_EABI:
        return "ARM EABI";
    }
    return "?";
}
