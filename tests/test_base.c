/**
 * test_base.c - ring_buffer.c 全面测试
 * 编译需 -I..（ring_buffer.h 所在目录）
 */
#include "test_framework.h"
#include "ref_model.h"
#include "ring_buffer.h"

TEST_FRAMEWORK_STATE

static ring_buffer g_rb;
static uint8_t *g_buf = NULL;
static ref_t g_ref;
static uint32_t g_cap;

/* ---- 通用工具 ---- */

static void setup(uint32_t cap)
{
    free(g_buf);
    g_buf = (uint8_t *)malloc((size_t)cap);   /* 堆分配以启用 ASan 越界检测 */
    TF_CHECK(g_buf != NULL, "malloc(%u) failed", (unsigned)cap);
    memset(g_buf, 0xCC, (size_t)cap);
    TF_CHECK(RB_Init(&g_rb, g_buf, cap) == RB_OK, "RB_Init(cap=%u) failed", (unsigned)cap);
    ref_init(&g_ref);
    g_cap = cap;
}

/* 状态一致性 + 非破坏性逐字节布局校验 */
static void state_check(const char *ctx)
{
    TF_CHECK(g_rb.max_Length == g_cap, "%s: max_Length %u != %u", ctx, (unsigned)g_rb.max_Length, (unsigned)g_cap);
    TF_CHECK(g_rb.head < g_cap, "%s: head %u >= cap %u", ctx, (unsigned)g_rb.head, (unsigned)g_cap);
    TF_CHECK(g_rb.tail < g_cap, "%s: tail %u >= cap %u", ctx, (unsigned)g_rb.tail, (unsigned)g_cap);
    TF_CHECK(g_rb.Length == g_ref.len, "%s: Length %u != ref %u", ctx, (unsigned)g_rb.Length, (unsigned)g_ref.len);
    TF_CHECK(RB_Get_Length(&g_rb) == g_ref.len, "%s: Get_Length mismatch", ctx);
    TF_CHECK(RB_Get_FreeSize(&g_rb) == g_cap - g_ref.len, "%s: FreeSize %u != %u", ctx,
             (unsigned)RB_Get_FreeSize(&g_rb), (unsigned)(g_cap - g_ref.len));
    TF_CHECK(g_rb.head == g_ref.head, "%s: head %u != ref %u", ctx, (unsigned)g_rb.head, (unsigned)g_ref.head);
    TF_CHECK(g_rb.tail == (g_ref.head + g_ref.len) % g_cap, "%s: tail %u != %u", ctx,
             (unsigned)g_rb.tail, (unsigned)((g_ref.head + g_ref.len) % g_cap));
    for (uint32_t i = 0u; i < g_ref.len; i++) {
        uint32_t pos = (g_ref.head + i) % g_cap;
        TF_CHECK(g_buf[pos] == g_ref.buf[i], "%s: byte %u @pos %u: 0x%02X != 0x%02X", ctx,
                 (unsigned)i, (unsigned)pos, (unsigned)g_buf[pos], (unsigned)g_ref.buf[i]);
    }
}

static void drain_expect(const uint8_t *expect, uint32_t n)
{
    uint8_t out[REF_CAP];
    TF_CHECK(RB_Get_Length(&g_rb) == n, "drain: length %u != %u", (unsigned)RB_Get_Length(&g_rb), (unsigned)n);
    if (n != 0u) {
        TF_CHECK(RB_Read_String(&g_rb, out, n) == RB_OK, "drain: read failed");
        TF_CHECK(memcmp(out, expect, (size_t)n) == 0, "drain: content mismatch");
    }
    TF_CHECK(RB_Get_Length(&g_rb) == 0u, "drain: not empty after reading all");
}

/* ---- 初始化测试 ---- */

static void t_init_sizes(void)
{
    ring_buffer rb;
    static uint8_t b[4];
    TF_CHECK(RB_Init(&rb, b, 0u) == RB_ERR_PARAM, "size 0 accepted");
    TF_CHECK(RB_Init(&rb, b, 1u) == RB_ERR_PARAM, "size 1 accepted");
    TF_CHECK(RB_Init(&rb, b, 2u) == RB_OK, "size 2 rejected");
    TF_CHECK(RB_Init(&rb, b, 0xFFFFFFFFu) == RB_ERR_PARAM, "size 0xFFFFFFFF accepted");
    TF_CHECK(RB_Init(&rb, (uint8_t *)0x1000, 0xFFFFFFFEu) == RB_OK, "size 0xFFFFFFFE rejected");
}

static void t_reinit_resets(void)
{
    setup(8u);
    TF_CHECK(RB_Write_Byte(&g_rb, 0x11) == RB_OK, "w1");
    TF_CHECK(RB_Write_Byte(&g_rb, 0x22) == RB_OK, "w2");
    TF_CHECK(RB_Init(&g_rb, g_buf, 8u) == RB_OK, "reinit");
    TF_CHECK(g_rb.head == 0u && g_rb.tail == 0u && g_rb.Length == 0u, "state not reset");
    TF_CHECK(RB_Get_FreeSize(&g_rb) == 8u, "free after reinit");
}

/* ---- 读写字节 ---- */

static void t_write_full_then_fail(void)
{
    setup(8u);
    for (uint32_t i = 0u; i < 8u; i++)
        TF_CHECK(RB_Write_Byte(&g_rb, (uint8_t)(0xA0u + i)) == RB_OK, "write %u failed", (unsigned)i);
    TF_CHECK(RB_Write_Byte(&g_rb, 0xFF) == RB_ERR_FULL, "write to FULL accepted");
    TF_CHECK(RB_Get_Length(&g_rb) == 8u, "len");
    TF_CHECK(RB_Get_FreeSize(&g_rb) == 0u, "free");
    uint8_t expect[8];
    for (uint32_t i = 0u; i < 8u; i++) expect[i] = (uint8_t)(0xA0u + i);
    drain_expect(expect, 8u);
}

static void t_read_empty(void)
{
    setup(8u);
    uint8_t b = 0;
    static uint8_t s[4];
    TF_CHECK(RB_Read_Byte(&g_rb, &b) == RB_ERR_EMPTY, "read byte from empty accepted");
    TF_CHECK(RB_Read_String(&g_rb, s, 1u) == RB_ERR_EMPTY, "read 1 from empty accepted");
    TF_CHECK(RB_Read_String(&g_rb, s, 0u) == RB_OK, "read 0 from empty rejected");
    TF_CHECK(RB_Delete(&g_rb, 0u) == RB_OK, "delete 0 from empty rejected");
    TF_CHECK(RB_Delete(&g_rb, 1u) == RB_ERR_EMPTY, "delete 1 from empty accepted");
    state_check("read_empty");
}

static void t_fill_drain_rounds(void)
{
    setup(8u);
    for (uint32_t round = 0u; round < 40u; round++) {
        for (uint32_t i = 0u; i < 8u; i++)
            TF_CHECK(RB_Write_Byte(&g_rb, (uint8_t)(round * 8u + i)) == RB_OK,
                     "w round %u pos %u", (unsigned)round, (unsigned)i);
        for (uint32_t i = 0u; i < 8u; i++) {
            uint8_t b;
            TF_CHECK(RB_Read_Byte(&g_rb, &b) == RB_OK, "r");
            TF_CHECK(b == (uint8_t)(round * 8u + i), "FIFO order round %u pos %u: got %u want %u",
                     (unsigned)round, (unsigned)i, (unsigned)b, (unsigned)(uint8_t)(round * 8u + i));
        }
        TF_CHECK(g_rb.head == g_rb.tail, "head/tail diverge when empty: h=%u t=%u",
                 (unsigned)g_rb.head, (unsigned)g_rb.tail);
    }
}

/* ---- Write_String 边界 ---- */

static void t_string_exact_fit_wrap(void)
{
    setup(8u);
    static uint8_t src[8] = {0xF0,0xF1,0xF2,0xF3,0xF4,0xF5,0xF6,0xF7};
    TF_CHECK(RB_Write_String(&g_rb, src, 6u) == RB_OK, "w6");
    TF_CHECK(RB_Delete(&g_rb, 6u) == RB_OK, "d6");
    TF_CHECK(g_rb.head == 6u && g_rb.tail == 6u, "h/t = %u/%u", (unsigned)g_rb.head, (unsigned)g_rb.tail);
    /* 6+2 == max：单次写入后 tail 恰好等于 max，应回绕到 0 */
    TF_CHECK(RB_Write_String(&g_rb, src, 2u) == RB_OK, "exact-fit w2");
    TF_CHECK(g_rb.tail == 0u, "tail %u != 0 (should wrap)", (unsigned)g_rb.tail);
    uint8_t expect[2] = {0xF0, 0xF1};
    drain_expect(expect, 2u);
}

static void t_string_split_wrap(void)
{
    setup(8u);
    static uint8_t src[8] = {0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17};
    TF_CHECK(RB_Write_String(&g_rb, src, 6u) == RB_OK, "w6");
    TF_CHECK(RB_Delete(&g_rb, 6u) == RB_OK, "d6");
    /* tail=6, free=8：写 5 字节需拆成 2+3 */
    TF_CHECK(RB_Write_String(&g_rb, src, 5u) == RB_OK, "split w5");
    TF_CHECK(g_rb.tail == 3u, "tail %u != 3", (unsigned)g_rb.tail);
    drain_expect(src, 5u);
}

static void t_string_split_tail7(void)
{
    setup(8u);
    static uint8_t src[8] = {0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27};
    TF_CHECK(RB_Write_String(&g_rb, src, 7u) == RB_OK, "w7");
    TF_CHECK(RB_Delete(&g_rb, 7u) == RB_OK, "d7");
    TF_CHECK(g_rb.tail == 7u, "tail %u != 7", (unsigned)g_rb.tail);
    /* tail=7：写 4 字节拆成 1+3 */
    TF_CHECK(RB_Write_String(&g_rb, src, 4u) == RB_OK, "split w4");
    TF_CHECK(g_rb.tail == 3u, "tail %u != 3", (unsigned)g_rb.tail);
    drain_expect(src, 4u);
}

static void t_string_split_head_behind(void)
{
    setup(8u);
    static uint8_t w[5] = {0x30,0x31,0x32,0x33,0x34};
    static uint8_t x[4] = {0x40,0x41,0x42,0x43};
    TF_CHECK(RB_Write_String(&g_rb, w, 5u) == RB_OK, "w5");
    TF_CHECK(RB_Delete(&g_rb, 2u) == RB_OK, "d2");
    /* head=2, tail=5, len=3, free=5：写 4 字节需拆分（3+1）且 head 在 tail 后面 */
    TF_CHECK(RB_Write_String(&g_rb, x, 4u) == RB_OK, "split w4 head-behind");
    TF_CHECK(g_rb.tail == 1u, "tail %u != 1", (unsigned)g_rb.tail);
    TF_CHECK(RB_Get_Length(&g_rb) == 7u, "len %u != 7", (unsigned)RB_Get_Length(&g_rb));
    static uint8_t expect[7] = {0x32,0x33,0x34,0x40,0x41,0x42,0x43};
    drain_expect(expect, 7u);
}

static void t_string_overflow_keeps_state(void)
{
    setup(8u);
    static uint8_t src[8] = {1,2,3,4,5,6,7,8};
    TF_CHECK(RB_Write_String(&g_rb, src, 5u) == RB_OK, "w5");
    TF_CHECK(RB_Write_String(&g_rb, src, 4u) == RB_ERR_FULL, "5+4 > 8 accepted");
    TF_CHECK(RB_Get_Length(&g_rb) == 5u, "len changed after failed write");
    TF_CHECK(g_rb.tail == 5u && g_rb.head == 0u, "pointers moved after failed write");
    drain_expect(src, 5u);
}

static void t_string_fill_exact_full(void)
{
    setup(8u);
    static uint8_t src[8] = {1,2,3,4,5,6,7,8};
    TF_CHECK(RB_Write_String(&g_rb, src, 5u) == RB_OK, "w5");
    TF_CHECK(RB_Write_String(&g_rb, src + 5, 3u) == RB_OK, "w3 exact");
    TF_CHECK(RB_Get_Length(&g_rb) == 8u && RB_Get_FreeSize(&g_rb) == 0u, "not full");
    TF_CHECK(RB_Write_Byte(&g_rb, 9) == RB_ERR_FULL, "byte to full");
    TF_CHECK(RB_Write_String(&g_rb, src, 1u) == RB_ERR_FULL, "string to full");
    drain_expect(src, 8u);
}

/* BUG-1 回归测试：超长 write_Length 不得回绕绕过容量检查（ring_buffer.c 容量判断） */
static void t_huge_write_length_rejected(void)
{
    static const uint32_t huge_lens[4] = {0xFFFFFFFFu, 0xFFFFFFFEu, 0x80000000u, 0x7FFFFFFFu};
    /* 空缓冲（Length=0） */
    setup(8u);
    for (uint32_t i = 0u; i < 4u; i++) {
        TF_CHECK(RB_Write_String(&g_rb, g_buf, huge_lens[i]) == RB_ERR_FULL,
                 "empty: huge len 0x%08X accepted", (unsigned)huge_lens[i]);
        TF_CHECK(RB_Get_Length(&g_rb) == 0u, "empty: state changed by failed write");
    }
    /* Length=1（触发原加法回绕的场景） */
    TF_CHECK(RB_Write_Byte(&g_rb, 0x41) == RB_OK, "w1");
    for (uint32_t i = 0u; i < 4u; i++) {
        TF_CHECK(RB_Write_String(&g_rb, g_buf, huge_lens[i]) == RB_ERR_FULL,
                 "len1: huge len 0x%08X accepted", (unsigned)huge_lens[i]);
        TF_CHECK(RB_Get_Length(&g_rb) == 1u, "len1: state changed by failed write");
        TF_CHECK(g_rb.head == 0u && g_rb.tail == 1u, "len1: pointers moved by failed write");
    }
    /* 满缓冲（Length=max） */
    TF_CHECK(RB_Delete(&g_rb, 1u) == RB_OK, "d1 before fill");
    static uint8_t src[8] = {1,2,3,4,5,6,7,8};
    TF_CHECK(RB_Write_String(&g_rb, src, 8u) == RB_OK, "w8");
    for (uint32_t i = 0u; i < 4u; i++) {
        TF_CHECK(RB_Write_String(&g_rb, src, huge_lens[i]) == RB_ERR_FULL,
                 "full: huge len 0x%08X accepted", (unsigned)huge_lens[i]);
        TF_CHECK(RB_Get_Length(&g_rb) == 8u, "full: state changed by failed write");
    }
    drain_expect(src, 8u);
}

static void t_zero_write_full(void)
{
    setup(8u);
    static uint8_t src[8] = {1,2,3,4,5,6,7,8};
    TF_CHECK(RB_Write_String(&g_rb, src, 8u) == RB_OK, "w8");
    TF_CHECK(RB_Write_String(&g_rb, src, 0u) == RB_OK, "write 0 to FULL rejected");
    TF_CHECK(RB_Get_Length(&g_rb) == 8u, "len changed");
    drain_expect(src, 8u);
}

/* ---- Read_String / Delete 边界 ---- */

static void t_read_exact_end_wrap(void)
{
    setup(8u);
    static uint8_t src[8] = {0,1,2,3,4,5,6,7};
    TF_CHECK(RB_Write_String(&g_rb, src, 8u) == RB_OK, "w8");
    TF_CHECK(RB_Delete(&g_rb, 6u) == RB_OK, "d6");
    uint8_t out[8];
    /* head=6：读 2 字节恰好跨过数组末尾，head 应回绕到 0 */
    TF_CHECK(RB_Read_String(&g_rb, out, 2u) == RB_OK, "r2");
    TF_CHECK(out[0] == 6u && out[1] == 7u, "content %u %u", (unsigned)out[0], (unsigned)out[1]);
    TF_CHECK(g_rb.head == 0u, "head %u != 0", (unsigned)g_rb.head);
    TF_CHECK(RB_Get_Length(&g_rb) == 0u, "len");
}

static void t_read_split_wrap(void)
{
    setup(8u);
    static uint8_t x[6] = {0x50,0x51,0x52,0x53,0x54,0x55};
    TF_CHECK(RB_Write_String(&g_rb, x, 6u) == RB_OK, "w6");
    TF_CHECK(RB_Delete(&g_rb, 6u) == RB_OK, "d6");
    TF_CHECK(RB_Write_String(&g_rb, x, 6u) == RB_OK, "w6 wrapped full");
    /* 满：数据 x0..x5 位于 [6],[7],[0],[1],[2],[3]；head=6 */
    TF_CHECK(RB_Get_Length(&g_rb) == 6u, "len %u", (unsigned)RB_Get_Length(&g_rb));
    uint8_t out[8];
    /* 读 4 字节跨末尾：2+2 拆分 */
    TF_CHECK(RB_Read_String(&g_rb, out, 4u) == RB_OK, "split r4");
    TF_CHECK(memcmp(out, x, 4) == 0, "split content");
    TF_CHECK(g_rb.head == 2u, "head %u != 2", (unsigned)g_rb.head);
    TF_CHECK(RB_Get_Length(&g_rb) == 2u, "len");
    TF_CHECK(RB_Read_String(&g_rb, out, 2u) == RB_OK, "r2");
    TF_CHECK(out[0] == x[4] && out[1] == x[5], "tail content");
}

static void t_read_more_than_length(void)
{
    setup(8u);
    static uint8_t src[3] = {7,8,9};
    TF_CHECK(RB_Write_String(&g_rb, src, 3u) == RB_OK, "w3");
    static uint8_t out[8];
    TF_CHECK(RB_Read_String(&g_rb, out, 4u) == RB_ERR_EMPTY, "r4 accepted with len 3");
    TF_CHECK(RB_Get_Length(&g_rb) == 3u, "len changed after failed read");
    TF_CHECK(g_rb.head == 0u, "head moved after failed read");
    drain_expect(src, 3u);
}

static void t_delete_wrap(void)
{
    setup(8u);
    static uint8_t x[8] = {0x60,0x61,0x62,0x63,0x64,0x65,0x66,0x67};
    TF_CHECK(RB_Write_String(&g_rb, x, 6u) == RB_OK, "w6");
    TF_CHECK(RB_Delete(&g_rb, 6u) == RB_OK, "d6");
    TF_CHECK(RB_Write_String(&g_rb, x, 8u) == RB_OK, "w8 full wrapped");
    /* 数据跨末尾：head=6，len=8 */
    TF_CHECK(RB_Delete(&g_rb, 4u) == RB_OK, "d4 wrap");
    TF_CHECK(g_rb.head == 2u, "head %u != 2", (unsigned)g_rb.head);
    TF_CHECK(RB_Get_Length(&g_rb) == 4u, "len");
    drain_expect(x + 4, 4u);
}

static void t_delete_all_full(void)
{
    setup(8u);
    static uint8_t src[8] = {1,2,3,4,5,6,7,8};
    TF_CHECK(RB_Write_String(&g_rb, src, 8u) == RB_OK, "w8");
    TF_CHECK(RB_Delete(&g_rb, 8u) == RB_OK, "d8");
    TF_CHECK(RB_Get_Length(&g_rb) == 0u, "len");
    TF_CHECK(RB_Get_FreeSize(&g_rb) == 8u, "free");
    TF_CHECK(g_rb.head == 0u && g_rb.tail == 0u, "h/t %u/%u", (unsigned)g_rb.head, (unsigned)g_rb.tail);
    TF_CHECK(RB_Write_String(&g_rb, src, 8u) == RB_OK, "rewrite after delete-all");
    drain_expect(src, 8u);
}

static void t_delete_edges(void)
{
    setup(8u);
    static uint8_t src[3] = {1,2,3};
    TF_CHECK(RB_Write_String(&g_rb, src, 3u) == RB_OK, "w3");
    TF_CHECK(RB_Delete(&g_rb, 0u) == RB_OK, "d0");
    TF_CHECK(RB_Get_Length(&g_rb) == 3u, "len after d0");
    TF_CHECK(RB_Delete(&g_rb, 4u) == RB_ERR_EMPTY, "d4 accepted");
    TF_CHECK(RB_Get_Length(&g_rb) == 3u, "len after failed d4");
    TF_CHECK(RB_Delete(&g_rb, 3u) == RB_OK, "d3");
    TF_CHECK(RB_Get_Length(&g_rb) == 0u, "empty after d3");
}

/* BUG-7 修复验证：空指针防护与空写入/空读取语义 */
static void t_null_guard(void)
{
    ring_buffer rb;
    static uint8_t b[8];
    static uint8_t out[8];
    TF_CHECK(RB_Init(NULL, b, 8u) == RB_ERR_PARAM, "init(NULL handle) accepted");
    TF_CHECK(RB_Init(&rb, NULL, 8u) == RB_ERR_PARAM, "init(NULL buffer) accepted");
    TF_CHECK(RB_Write_Byte(NULL, 0x41) == RB_ERR_PARAM, "Write_Byte(NULL) accepted");
    TF_CHECK(RB_Write_String(NULL, out, 1u) == RB_ERR_PARAM, "Write_String(NULL handle) accepted");
    TF_CHECK(RB_Read_Byte(NULL, out) == RB_ERR_PARAM, "Read_Byte(NULL handle) accepted");
    TF_CHECK(RB_Read_String(NULL, out, 1u) == RB_ERR_PARAM, "Read_String(NULL handle) accepted");
    TF_CHECK(RB_Delete(NULL, 1u) == RB_ERR_PARAM, "Delete(NULL) accepted");
    TF_CHECK(RB_Get_Length(NULL) == 0u, "Get_Length(NULL) != 0");
    TF_CHECK(RB_Get_FreeSize(NULL) == 0u, "Get_FreeSize(NULL) != 0");
    /* 空读写：无论指针/状态如何都不应触碰缓冲区，且不构成未定义行为 */
    TF_CHECK(RB_Init(&rb, b, 8u) == RB_OK, "init failed");
    TF_CHECK(RB_Write_String(&rb, NULL, 0u) == RB_OK, "write 0 from NULL rejected");
    TF_CHECK(RB_Read_String(&rb, NULL, 0u) == RB_OK, "read 0 to NULL rejected");
    /* 0 长度调用先于一切校验：句柄无效时同样返回成功 */
    TF_CHECK(RB_Write_String(NULL, out, 0u) == RB_OK, "write 0 on NULL handle rejected");
    TF_CHECK(RB_Read_String(NULL, out, 0u) == RB_OK, "read 0 on NULL handle rejected");
    TF_CHECK(RB_Read_Byte(&rb, NULL) == RB_ERR_PARAM, "Read_Byte(NULL out) accepted (param check first)");
    TF_CHECK(RB_Get_Length(&rb) == 0u, "state changed by null/zero ops");
}

/* 错误码枚举值互异性（防止赋值冲突导致语义塌缩） */
static void t_enum_distinct(void)
{
    static const rb_status_t codes[5] = {RB_OK, RB_ERR_PARAM, RB_ERR_EMPTY, RB_ERR_FULL, RB_ERR_TOO_SMALL};
    for (uint32_t i = 0u; i < 5u; i++)
        for (uint32_t j = i + 1u; j < 5u; j++)
            TF_CHECK(codes[i] != codes[j], "enum collision %u/%u", (unsigned)i, (unsigned)j);
}

/* P1 新 API：错误码分类——满/空/参数错误必须可区分 */
static void t_error_codes(void)
{
    setup(8u);
    for (uint32_t i = 0u; i < 8u; i++)
        TF_CHECK(RB_Write_Byte(&g_rb, (uint8_t)i) == RB_OK, "w %u", (unsigned)i);
    TF_CHECK(RB_Write_Byte(&g_rb, 0xAA) == RB_ERR_FULL, "full not distinguished");
    uint8_t b;
    for (uint32_t i = 0u; i < 8u; i++)
        TF_CHECK(RB_Read_Byte(&g_rb, &b) == RB_OK, "r %u", (unsigned)i);
    TF_CHECK(RB_Read_Byte(&g_rb, &b) == RB_ERR_EMPTY, "empty not distinguished");
    TF_CHECK(RB_Init(NULL, g_buf, 8u) == RB_ERR_PARAM, "param not distinguished");
}

/* P1 新 API：RB_Clear 复位且可继续使用 */
static void t_clear(void)
{
    setup(8u);
    static uint8_t src[8] = {1,2,3,4,5,6,7,8};
    TF_CHECK(RB_Write_String(&g_rb, src, 5u) == RB_OK, "w5");
    TF_CHECK(RB_Clear(&g_rb) == RB_OK, "clear failed");
    TF_CHECK(RB_Get_Length(&g_rb) == 0u && RB_Get_FreeSize(&g_rb) == 8u, "state after clear");
    TF_CHECK(g_rb.head == 0u && g_rb.tail == 0u, "pointers after clear");
    TF_CHECK(RB_Clear(NULL) == RB_ERR_PARAM, "clear(NULL) accepted");
    TF_CHECK(RB_Write_String(&g_rb, src, 8u) == RB_OK, "write after clear");
    drain_expect(src, 8u);
}

/* P1 新 API：RB_Get_Capacity */
static void t_get_capacity(void)
{
    setup(33u);
    TF_CHECK(RB_Get_Capacity(&g_rb) == 33u, "capacity mismatch");
    TF_CHECK(RB_Get_Capacity(NULL) == 0u, "capacity(NULL) != 0");
}

/* P1 新 API：Peek 不消费（单字节 offset + 批量 + 跨末尾布局） */
static void t_peek(void)
{
    setup(8u);
    static uint8_t src[6] = {0x51,0x52,0x53,0x54,0x55,0x56};
    TF_CHECK(RB_Write_String(&g_rb, src, 6u) == RB_OK, "w6");
    uint8_t b;
    TF_CHECK(RB_Peek_Byte(&g_rb, 0u, &b) == RB_OK, "peek head");
    TF_CHECK(b == 0x51, "peek head value %u", (unsigned)b);
    TF_CHECK(RB_Peek_Byte(&g_rb, 3u, &b) == RB_OK, "peek mid");
    TF_CHECK(b == 0x54, "peek mid value %u", (unsigned)b);
    TF_CHECK(RB_Peek_Byte(&g_rb, 5u, &b) == RB_OK, "peek last");
    TF_CHECK(b == 0x56, "peek last value %u", (unsigned)b);
    TF_CHECK(RB_Peek_Byte(&g_rb, 6u, &b) == RB_ERR_EMPTY, "peek at end accepted");
    TF_CHECK(RB_Peek_Byte(&g_rb, 100u, &b) == RB_ERR_EMPTY, "peek overflow accepted");
    TF_CHECK(RB_Peek_Byte(&g_rb, 0u, NULL) == RB_ERR_PARAM, "peek NULL out accepted");
    TF_CHECK(RB_Get_Length(&g_rb) == 6u, "peek consumed data");
    uint32_t copied = 0;
    static uint8_t out[8];
    TF_CHECK(RB_Peek_String(&g_rb, out, 8u, &copied) == RB_OK, "peek string full");
    TF_CHECK(copied == 6u && memcmp(out, src, 6) == 0, "peek string content");
    TF_CHECK(RB_Get_Length(&g_rb) == 6u, "peek string consumed data");
    TF_CHECK(RB_Peek_String(&g_rb, out, 3u, &copied) == RB_OK, "peek string partial");
    TF_CHECK(copied == 3u && memcmp(out, src, 3) == 0, "peek partial content");
    TF_CHECK(RB_Clear(&g_rb) == RB_OK, "clear");
    TF_CHECK(RB_Peek_Byte(&g_rb, 0u, &b) == RB_ERR_EMPTY, "peek byte on empty accepted");
    TF_CHECK(RB_Peek_String(&g_rb, out, 4u, &copied) == RB_OK, "peek empty");
    TF_CHECK(copied == 0u, "peek empty copied != 0");
    TF_CHECK(RB_Write_String(&g_rb, src, 6u) == RB_OK, "rew");
    TF_CHECK(RB_Peek_String(&g_rb, out, 0u, &copied) == RB_OK, "peek 0");
    TF_CHECK(copied == 0u, "peek 0 copied != 0");
    TF_CHECK(RB_Peek_String(&g_rb, out, 8u, NULL) == RB_ERR_PARAM, "peek string NULL copied accepted");
    /* 跨末尾布局下的 peek：数据 x0..x7 位于 [6],[7],[0..5]，head=6 */
    setup(8u);
    static uint8_t x[8] = {0x60,0x61,0x62,0x63,0x64,0x65,0x66,0x67};
    TF_CHECK(RB_Write_String(&g_rb, x, 6u) == RB_OK, "w6");
    TF_CHECK(ref_write_string(&g_ref, x, 6u, 8u) == RB_OK, "w6");
    TF_CHECK(RB_Delete(&g_rb, 6u) == RB_OK, "d6");
    TF_CHECK(ref_delete(&g_ref, 6u, 8u) == RB_OK, "d6");
    TF_CHECK(RB_Write_String(&g_rb, x, 8u) == RB_OK, "w8 wrapped");
    TF_CHECK(ref_write_string(&g_ref, x, 8u, 8u) == RB_OK, "w8 wrapped");
    for (uint32_t i = 0u; i < 8u; i++) {
        uint8_t b2;
        TF_CHECK(RB_Peek_Byte(&g_rb, i, &b2) == RB_OK, "peek wrap %u", (unsigned)i);
        TF_CHECK(b2 == x[i], "peek wrap %u value", (unsigned)i);
    }
    uint32_t copied2 = 0;
    static uint8_t out2[8];
    TF_CHECK(RB_Peek_String(&g_rb, out2, 8u, &copied2) == RB_OK, "peek wrap string");
    TF_CHECK(copied2 == 8u && memcmp(out2, x, 8) == 0, "peek wrap string content");
    state_check("peek");
}

/* ---- 随机属性测试 ---- */

static void property_run(uint32_t cap, uint32_t iters, uint32_t seed)
{
    static uint8_t src[REF_CAP];
    static uint8_t outA[REF_CAP], outB[REF_CAP];
    for (uint32_t round = 0u; round < RB_TEST_SOAK; round++) {
        setup(cap);
        tf_seed(seed + round);/* 每轮独立种子：SOAK 放大的是新序列而非同种子重放 */
        for (uint32_t it = 0u; it < iters; it++) {
            uint32_t op = tf_rand_range(7u);
            if (op == 0u) {
                uint8_t b = (uint8_t)tf_rand();
                rb_status_t er = ref_write_byte(&g_ref, b, cap);
                rb_status_t ar = RB_Write_Byte(&g_rb, b);
                TF_CHECK(ar == er, "it %u: Write_Byte ret %u ref %u", (unsigned)it, (unsigned)ar, (unsigned)er);
            } else if (op == 1u) {
                uint32_t n = tf_rand_range(cap + 2u);
                for (uint32_t i = 0u; i < n; i++) src[i] = (uint8_t)tf_rand();
                rb_status_t er = ref_write_string(&g_ref, src, n, cap);
                rb_status_t ar = RB_Write_String(&g_rb, src, n);
                TF_CHECK(ar == er, "it %u: Write_String(n=%u) ret %u ref %u", (unsigned)it, (unsigned)n, (unsigned)ar, (unsigned)er);
            } else if (op == 2u) {
                uint8_t ea = 0, aa = 0;
                rb_status_t er = ref_read_byte(&g_ref, &ea, cap);
                rb_status_t ar = RB_Read_Byte(&g_rb, &aa);
                TF_CHECK(ar == er, "it %u: Read_Byte ret %u ref %u", (unsigned)it, (unsigned)ar, (unsigned)er);
                if (er == RB_OK)
                    TF_CHECK(aa == ea, "it %u: Read_Byte data %02X != %02X", (unsigned)it, (unsigned)aa, (unsigned)ea);
            } else if (op == 3u) {
                uint32_t n = tf_rand_range(cap + 2u);
                rb_status_t er = ref_read_string(&g_ref, outA, n, cap);
                rb_status_t ar = RB_Read_String(&g_rb, outB, n);
                TF_CHECK(ar == er, "it %u: Read_String(n=%u) ret %u ref %u", (unsigned)it, (unsigned)n, (unsigned)ar, (unsigned)er);
                if (er == RB_OK && n != 0u)
                    TF_CHECK(memcmp(outA, outB, (size_t)n) == 0, "it %u: Read_String data mismatch n=%u", (unsigned)it, (unsigned)n);
            } else if (op == 4u) {
                uint32_t n = tf_rand_range(cap + 2u);
                rb_status_t er = ref_delete(&g_ref, n, cap);
                rb_status_t ar = RB_Delete(&g_rb, n);
                TF_CHECK(ar == er, "it %u: Delete(n=%u) ret %u ref %u", (unsigned)it, (unsigned)n, (unsigned)ar, (unsigned)er);
            } else if (op == 5u) {
                uint32_t off = tf_rand_range(cap + 2u);
                uint8_t ea = 0, aa = 0;
                rb_status_t er = ref_peek_byte(&g_ref, off, &ea);
                rb_status_t ar = RB_Peek_Byte(&g_rb, off, &aa);
                TF_CHECK(ar == er, "it %u: Peek_Byte(off=%u) ret %u ref %u", (unsigned)it, (unsigned)off, (unsigned)ar, (unsigned)er);
                if (er == RB_OK)
                    TF_CHECK(aa == ea, "it %u: Peek_Byte data %02X != %02X", (unsigned)it, (unsigned)aa, (unsigned)ea);
            } else {
                uint32_t n = tf_rand_range(cap + 2u);
                uint32_t ec = 0, ac = 0;
                rb_status_t er = ref_peek_string(&g_ref, outA, n, &ec);
                rb_status_t ar = RB_Peek_String(&g_rb, outB, n, &ac);
                TF_CHECK(ar == er, "it %u: Peek_String(max=%u) ret %u ref %u", (unsigned)it, (unsigned)n, (unsigned)ar, (unsigned)er);
                TF_CHECK(ec == ac, "it %u: Peek_String copied %u != %u", (unsigned)it, (unsigned)ac, (unsigned)ec);
                if (n != 0u && ec != 0u)
                    TF_CHECK(memcmp(outA, outB, (size_t)ec) == 0, "it %u: Peek_String data mismatch n=%u", (unsigned)it, (unsigned)ec);
            }
            state_check("prop");
        }
    }
}

static void t_property(void)
{
    property_run(2u,   20000u, 1u);
    property_run(3u,   20000u, 2u);
    property_run(5u,   20000u, 3u);
    property_run(8u,   30000u, 4u);
    property_run(16u,  30000u, 5u);
    property_run(33u,  20000u, 6u);
    property_run(64u,  20000u, 7u);
    property_run(127u, 20000u, 8u);
}

int main(void)
{
    RUN_TEST(t_init_sizes);
    RUN_TEST(t_reinit_resets);
    RUN_TEST(t_write_full_then_fail);
    RUN_TEST(t_read_empty);
    RUN_TEST(t_fill_drain_rounds);
    RUN_TEST(t_string_exact_fit_wrap);
    RUN_TEST(t_string_split_wrap);
    RUN_TEST(t_string_split_tail7);
    RUN_TEST(t_string_split_head_behind);
    RUN_TEST(t_string_overflow_keeps_state);
    RUN_TEST(t_huge_write_length_rejected);
    RUN_TEST(t_string_fill_exact_full);
    RUN_TEST(t_zero_write_full);
    RUN_TEST(t_read_exact_end_wrap);
    RUN_TEST(t_read_split_wrap);
    RUN_TEST(t_read_more_than_length);
    RUN_TEST(t_delete_wrap);
    RUN_TEST(t_delete_all_full);
    RUN_TEST(t_delete_edges);
    RUN_TEST(t_null_guard);
    RUN_TEST(t_enum_distinct);
    RUN_TEST(t_error_codes);
    RUN_TEST(t_clear);
    RUN_TEST(t_get_capacity);
    RUN_TEST(t_peek);
    RUN_TEST(t_property);
    int rc = tf_summary();
    free(g_buf);
    return rc;
}
