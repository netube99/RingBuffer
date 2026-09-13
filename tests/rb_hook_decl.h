/**
 * rb_hook_decl.h - 临界区钩子测试用：钩子函数声明
 * 编译 test_hooks 时通过 -include 注入给 ring_buffer.c，
 * 使钩子宏指向的函数在各翻译单元内均有声明可见。
 */
#ifndef RB_HOOK_DECL_H
#define RB_HOOK_DECL_H

#include <stdint.h>

uint32_t rb_hook_lock(void);
void rb_hook_unlock(uint32_t key);

#endif
