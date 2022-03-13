#ifndef _SE_DEBUG_H_
#define _SE_DEBUG_H_

#ifdef SE_DEBUG

#include <stdio.h>

#define se_assert(cond) (!!(cond) || (se_assert_impl(#cond, __FILE__, __LINE__, ""), false))
#define se_assert_msg(cond, msg) (!!(cond) || (se_assert_impl(#cond, __FILE__, __LINE__, msg), false))

void se_assert_impl(const char* cond, const char* file, int line, const char* message)
{
    printf("Assertion failed.\nCondition : %s\nFile : %s\nLine : %d\nMessage : %s\n", cond, file, line, message);
    int* a = 0;
    *a = 0;
}

#else
#   define se_assert(cond)
#   define se_assert_msg(cond, msg)
#endif

#endif
