/**
 * test_hooks.c - 并发临界区钩子注入与配对完整性验证
 *
 * 编译时注入：-DRB_CRITICAL_ENTER=rb_hook_lock -DRB_CRITICAL_EXIT=rb_hook_unlock
 * 并通过 -include rb_hook_decl.h 使 ring_buffer.c 可见钩子函数声明。
 *
 * 验证目标（对应并发契约）：
 *  1. 每个公开接口调用都恰好发生一次 ENTER/EXIT 配对；
 *  2. 分段接口内部再调用基础接口时，钩子嵌套配对正确（LIFO）；
 *  3. 混合操作序列结束后锁计数归零。
 */
#include "test_framework.h"
#include "rb_hook_decl.h"
#include "ring_buffer.h"
#include "ring_buffer_chapter.h"

static int g_lock_cnt = 0;
static int g_unlock_cnt = 0;
static int g_depth = 0;        /* 模拟锁深度：ENTER 加一、EXIT 减一 */
static int g_pair_err = 0;     /* 配对错误计数（EXIT 时深度不符/越界） */

uint32_t rb_hook_lock(void)
{
    g_lock_cnt++;
    g_depth++;
    return (uint32_t)g_depth;   /* 锁存值 = 当前深度，用于 LIFO 校验 */
}

void rb_hook_unlock(uint32_t key)
{
    g_unlock_cnt++;
    if (key != (uint32_t)g_depth || g_depth == 0)
        g_pair_err++;
    g_depth--;
}

static void hook_stats_check(const char *ctx)
{
    TF_CHECK(g_pair_err == 0, "%s: hook pair error %d", ctx, g_pair_err);
    TF_CHECK(g_depth == 0, "%s: lock depth %d != 0 after ops", ctx, g_depth);
    TF_CHECK(g_lock_cnt == g_unlock_cnt, "%s: lock %d != unlock %d", ctx, g_lock_cnt, g_unlock_cnt);
}

static void reset_stats(void)
{
    g_lock_cnt = 0;
    g_unlock_cnt = 0;
    g_depth = 0;
    g_pair_err = 0;
}

static void t_hooks_base_ops(void)
{
    static uint8_t buf[16];
    ring_buffer rb;
    reset_stats();
    TF_CHECK(RB_Init(&rb, buf, 16u) == RB_OK, "init");
    for (uint32_t i = 0u; i < 16u; i++)
        TF_CHECK(RB_Write_Byte(&rb, (uint8_t)i) == RB_OK, "w %u", (unsigned)i);
    uint8_t b;
    TF_CHECK(RB_Read_Byte(&rb, &b) == RB_OK, "r");
    TF_CHECK(RB_Peek_Byte(&rb, 2u, &b) == RB_OK, "peek");
    uint32_t copied = 0;
    static uint8_t out[16];
    TF_CHECK(RB_Peek_String(&rb, out, 16u, &copied) == RB_OK, "peek string");
    TF_CHECK(RB_Read_String(&rb, out, 8u) == RB_OK, "read string");
    TF_CHECK(RB_Delete(&rb, 4u) == RB_OK, "delete");
    TF_CHECK(RB_Clear(&rb) == RB_OK, "clear");
    TF_CHECK(RB_Get_Length(&rb) == 0u, "length");
    TF_CHECK(RB_Get_FreeSize(&rb) == 16u, "free");
    TF_CHECK(RB_Get_Capacity(&rb) == 16u, "capacity");
    /* 失败路径同样应正确配对 */
    TF_CHECK(RB_Write_Byte(&rb, 0x41) == RB_OK, "w after clear");
    TF_CHECK(RB_Read_Byte(&rb, NULL) == RB_ERR_PARAM, "param fail path clean");
    /* 0 长度调用早退于临界区之外：不产生任何锁活动 */
    uint32_t before = (uint32_t)g_lock_cnt;
    TF_CHECK(RB_Write_String(&rb, NULL, 0u) == RB_OK, "zero-length write");
    TF_CHECK(RB_Read_String(&rb, NULL, 0u) == RB_OK, "zero-length read");
    TF_CHECK((uint32_t)g_lock_cnt == before, "zero-length ops entered critical section");
    /* Init 不进入临界区（必须在多上下文启用前完成） */
    before = (uint32_t)g_lock_cnt;
    TF_CHECK(RB_Init(&rb, buf, 16u) == RB_OK, "reinit");
    TF_CHECK((uint32_t)g_lock_cnt == before, "Init entered critical section");
    hook_stats_check("base");
    TF_CHECK(g_lock_cnt > 0, "no hook activity observed");
}

static void t_hooks_chapter_ops(void)
{
    static uint8_t base_buf[32];
    static uint32_t ring_buf[4];
    ring_buffer_chapter rbc;
    reset_stats();
    TF_CHECK(RBC_Init(&rbc, base_buf, 32u, ring_buf, 16u) == RB_OK, "init");
    /* 写 + 结尾：RBC 封装层进临界区，内部再调 RB_*（嵌套） */
    static const uint8_t ch1[4] = {'c','h','1','!'};
    static const uint8_t ch2[4] = {'c','h','2','!'};
    TF_CHECK(RBC_Write_String(&rbc, ch1, 3u) == RB_OK, "w ch1");
    TF_CHECK(RBC_Write_Byte(&rbc, '!') == RB_OK, "w byte");
    TF_CHECK(RBC_Ending_Chapter(&rbc) == RB_OK, "end");
    TF_CHECK(RBC_Write_String(&rbc, ch2, 3u) == RB_OK, "w ch2");
    TF_CHECK(RBC_Ending_Chapter(&rbc) == RB_OK, "end2");
    TF_CHECK(RBC_Get_Chapter_Number(&rbc) == 2u, "number");
    TF_CHECK(RBC_Get_Head_Chapter_Length(&rbc) == 4u, "head len");
    uint8_t out[8];
    uint32_t actual = 0;
    TF_CHECK(RBC_Read_Chapter_Bounded(&rbc, out, 8u, &actual) == RB_OK, "bounded read");
    TF_CHECK(actual == 4u, "actual");
    TF_CHECK(RBC_Read_Byte(&rbc, out) == RB_OK, "read byte");
    TF_CHECK(RBC_Delete(&rbc, 1u) == RB_OK, "delete");
    TF_CHECK(RBC_Clear(&rbc) == RB_OK, "clear");
    /* 失败路径同样配对 */
    TF_CHECK(RBC_Ending_Chapter(&rbc) == RB_ERR_EMPTY, "ending empty also locked");
    TF_CHECK(RBC_Write_Byte(&rbc, 0x41) == RB_OK, "write after clear");
    hook_stats_check("chapter");
    TF_CHECK(g_lock_cnt > 0, "no hook activity observed");
}

int main(void)
{
    RUN_TEST(t_hooks_base_ops);
    RUN_TEST(t_hooks_chapter_ops);
    int rc = tf_summary();
    return rc;
}
