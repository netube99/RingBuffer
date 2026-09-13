/**
 * test_framework.h - 极简单元测试框架 + 确定性随机数
 */
#ifndef TEST_FRAMEWORK_H_
#define TEST_FRAMEWORK_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <stdint.h>

/* 框架状态（每个测试二进制为单翻译单元，直接用内部链接变量） */
static jmp_buf tf_jmp;
static const char *tf_current_test = "";
static int tf_total = 0;
static int tf_failed = 0;

/* 随机属性测试迭代倍率：CI 深跑（soak）时通过 -DRB_TEST_SOAK=N 放大 */
#ifndef RB_TEST_SOAK
#define RB_TEST_SOAK 1
#endif

#define TEST_FRAMEWORK_STATE

#define TF_FAIL(...) do { \
    printf("      [CHECK-FAIL] %s:%d: ", __FILE__, __LINE__); \
    printf(__VA_ARGS__); \
    printf("\n"); \
    longjmp(tf_jmp, 1); \
} while (0)

#define TF_CHECK(cond, ...) do { if (!(cond)) { TF_FAIL(__VA_ARGS__); } } while (0)

#define RUN_TEST(fn) do { \
    tf_current_test = #fn; \
    tf_total++; \
    if (setjmp(tf_jmp) == 0) { \
        fn(); \
        printf("[PASS] %s\n", tf_current_test); \
    } else { \
        printf("[FAIL] %s\n", tf_current_test); \
        tf_failed++; \
    } \
} while (0)

static int tf_summary(void)
{
    printf("\n========== %d test(s), %d failed ==========\n", tf_total, tf_failed);
    return (tf_failed != 0) ? 1 : 0;
}

/* xorshift32 确定性随机数（部分测试二进制不使用，标注 unused 避免 -Wall 噪音） */
#if defined(__GNUC__)
#define TF_UNUSED __attribute__((unused))
#else
#define TF_UNUSED
#endif

static TF_UNUSED uint32_t tf_rng_state = 0x12345678u;
static TF_UNUSED void tf_seed(uint32_t s) { tf_rng_state = (s != 0u) ? s : 0xA5A5A5A5u; }
static TF_UNUSED uint32_t tf_rand(void)
{
    uint32_t x = tf_rng_state;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    tf_rng_state = x;
    return x;
}
static TF_UNUSED uint32_t tf_rand_range(uint32_t n) { return (n == 0u) ? 0u : (tf_rand() % n); }

#endif
