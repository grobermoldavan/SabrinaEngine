#ifndef _SE_DEBUG_H_
#define _SE_DEBUG_H_

void se_assert_impl(const char* cond);

#ifdef SE_DEBUG
#   define se_assert(cond) (!!(cond) || (se_assert_impl(#cond), false))
#else
#   define se_assert(cond)
#endif

#endif