/**
 * test_chapter.c - ring_buffer_chapter.c 全面测试
 * 所有操作同时打到真实现与参照模型上，逐一比对返回值/数据/内部状态。
 */
#include "test_framework.h"
#include "ref_model_chapter.h"
#include "ring_buffer_chapter.h"

TEST_FRAMEWORK_STATE

static ring_buffer_chapter g_rbc;
static uint8_t *g_base = NULL;
static uint8_t *g_ring = NULL;      /* 原始字节，传给 API 时转成 uint32_t* */
static rch_ref_t g_ref;
static uint32_t g_bcap, g_rcap;

static void csetup(uint32_t bcap, uint32_t rcap)
{
    free(g_base); free(g_ring);
    g_base = (uint8_t *)malloc((size_t)bcap);
    g_ring = (uint8_t *)malloc((size_t)rcap);
    TF_CHECK(g_base != NULL && g_ring != NULL, "malloc(%u,%u) failed", (unsigned)bcap, (unsigned)rcap);
    memset(g_base, 0xCC, (size_t)bcap);
    memset(g_ring, 0xCC, (size_t)rcap);
    TF_CHECK(RBC_Init(&g_rbc, g_base, bcap, (uint32_t *)(void *)g_ring, rcap) == RB_OK,
             "RBC_Init(base=%u,ring=%u) failed", (unsigned)bcap, (unsigned)rcap);
    rch_ref_init(&g_ref, bcap, rcap);
    g_bcap = bcap; g_rcap = rcap;
}

/* 深度状态校验：计数、内部指针、基环/分段环逐字节布局（非破坏性） */
static void cstate_check(const char *ctx)
{
    uint32_t stream = rch_stream_len(&g_ref);
    uint32_t total = stream + g_ref.tail.len;
    uint32_t records = rch_ring_records(&g_ref);

    TF_CHECK(RBC_Get_Chapter_Number(&g_rbc) == g_ref.n, "%s: Number %u != ref %u",
             ctx, (unsigned)RBC_Get_Chapter_Number(&g_rbc), (unsigned)g_ref.n);
    uint32_t exp_head_len = (g_ref.n != 0u) ? g_ref.ch[0].len : 0u;
    TF_CHECK(RBC_Get_Head_Chapter_Length(&g_rbc) == exp_head_len, "%s: head_len %u != %u",
             ctx, (unsigned)RBC_Get_Head_Chapter_Length(&g_rbc), (unsigned)exp_head_len);
    TF_CHECK(RBC_Get_Base_Free_Size(&g_rbc) == g_bcap - total, "%s: base free %u != %u",
             ctx, (unsigned)RBC_Get_Base_Free_Size(&g_rbc), (unsigned)(g_bcap - total));
    TF_CHECK(RBC_Get_Chapter_Free_Size(&g_rbc) == rch_chapter_free(&g_ref), "%s: chapter free mismatch", ctx);
    TF_CHECK(g_rbc.tail_chapter_length == g_ref.tail.len, "%s: tail_chapter_length %u != %u",
             ctx, (unsigned)g_rbc.tail_chapter_length, (unsigned)g_ref.tail.len);
    TF_CHECK(g_rbc.init_flag == (g_ref.n == 0u ? 1u : 0u), "%s: init_flag %u unexpected (n=%u)",
             ctx, (unsigned)g_rbc.init_flag, (unsigned)g_ref.n);

    /* 基环内部状态与布局 */
    ring_buffer *bh = &g_rbc.base_handle;
    TF_CHECK(bh->head < g_bcap && bh->tail < g_bcap, "%s: base head/tail out of range", ctx);
    TF_CHECK(bh->head == g_ref.bhead, "%s: base head %u != %u", ctx, (unsigned)bh->head, (unsigned)g_ref.bhead);
    TF_CHECK(bh->Length == total, "%s: base Length %u != %u", ctx, (unsigned)bh->Length, (unsigned)total);
    TF_CHECK(bh->tail == (g_ref.bhead + total) % g_bcap, "%s: base tail %u != %u",
             ctx, (unsigned)bh->tail, (unsigned)((g_ref.bhead + total) % g_bcap));
    static uint8_t expect[RCH_MAX_DATA * 2u];
    uint32_t pos = 0u;
    for (uint32_t c = 0u; c < g_ref.n; c++) {
        memcpy(expect + pos, g_ref.ch[c].data, (size_t)g_ref.ch[c].len);
        pos += g_ref.ch[c].len;
    }
    memcpy(expect + pos, g_ref.tail.data, (size_t)g_ref.tail.len);
    pos += g_ref.tail.len;
    TF_CHECK(pos == total, "%s: model total mismatch", ctx);
    for (uint32_t i = 0u; i < pos; i++) {
        uint32_t p = (g_ref.bhead + i) % g_bcap;
        TF_CHECK(g_base[p] == expect[i], "%s: base byte %u @pos %u: 0x%02X != 0x%02X",
                 ctx, (unsigned)i, (unsigned)p, (unsigned)g_base[p], (unsigned)expect[i]);
    }

    /* 分段环内部状态与记录布局（4 字节小端长度记录） */
    ring_buffer *rh = &g_rbc.chapter_handle;
    TF_CHECK(rh->head < g_rcap && rh->tail < g_rcap, "%s: ring head/tail out of range", ctx);
    TF_CHECK(rh->head == g_ref.rhead, "%s: ring head %u != %u", ctx, (unsigned)rh->head, (unsigned)g_ref.rhead);
    TF_CHECK(rh->Length == records * 4u, "%s: ring Length %u != %u", ctx, (unsigned)rh->Length, (unsigned)(records * 4u));
    TF_CHECK(rh->tail == (g_ref.rhead + records * 4u) % g_rcap, "%s: ring tail", ctx);
    for (uint32_t j = 0u; j < records; j++) {
        uint32_t val = g_ref.ch[j + 1u].len;
        uint8_t rec[4];
        memcpy(rec, &val, 4u);
        for (uint32_t t = 0u; t < 4u; t++) {
            uint32_t p = (g_ref.rhead + j * 4u + t) % g_rcap;
            TF_CHECK(g_ring[p] == rec[t], "%s: ring rec %u byte %u @pos %u: 0x%02X != 0x%02X",
                     ctx, (unsigned)j, (unsigned)t, (unsigned)p, (unsigned)g_ring[p], (unsigned)rec[t]);
        }
    }
}

/* ---- 镜像操作：同时驱动真实现与模型并比对 ---- */

static uint8_t mb(uint8_t b)
{
    rb_status_t er = rch_write_byte(&g_ref, b);
    rb_status_t ar = RBC_Write_Byte(&g_rbc, b);
    TF_CHECK(ar == er, "Write_Byte(0x%02X): real %u model %u", (unsigned)b, (unsigned)ar, (unsigned)er);
    return ar;
}

static uint8_t ms(const uint8_t *p, uint32_t n)
{
    rb_status_t er = rch_write_string(&g_ref, p, n);
    rb_status_t ar = RBC_Write_String(&g_rbc, (uint8_t *)(uintptr_t)p, n);
    TF_CHECK(ar == er, "Write_String(n=%u): real %u model %u", (unsigned)n, (unsigned)ar, (unsigned)er);
    return ar;
}

static uint8_t mend(void)
{
    rb_status_t er = rch_ending(&g_ref);
    rb_status_t ar = RBC_Ending_Chapter(&g_rbc);
    TF_CHECK(ar == er, "Ending_Chapter: real %u model %u (n=%u tail=%u)",
             (unsigned)ar, (unsigned)er, (unsigned)g_ref.n, (unsigned)g_ref.tail.len);
    return ar;
}

static uint8_t mrb(uint8_t *out)
{
    uint8_t em = 0, ea = 0;
    rb_status_t er = rch_read_byte(&g_ref, &em);
    rb_status_t ar = RBC_Read_Byte(&g_rbc, &ea);
    TF_CHECK(ar == er, "Read_Byte: real %u model %u", (unsigned)ar, (unsigned)er);
    if (er == RB_OK) {
        TF_CHECK(ea == em, "Read_Byte data: 0x%02X != 0x%02X", (unsigned)ea, (unsigned)em);
        if (out != NULL) *out = ea;
    }
    return ar;
}

static uint8_t mrc(uint8_t *out, uint32_t *olen)
{
    static uint8_t em[RCH_MAX_DATA];
    static uint8_t ea[RCH_MAX_DATA];
    uint32_t lm = 0u, la = 0u;
    rb_status_t er = rch_read_chapter(&g_ref, em, &lm);
    rb_status_t ar = RBC_Read_Chapter(&g_rbc, ea, (olen != NULL) ? &la : NULL);
    TF_CHECK(ar == er, "Read_Chapter: real %u model %u", (unsigned)ar, (unsigned)er);
    if (er == RB_OK) {
        TF_CHECK(memcmp(ea, em, (size_t)lm) == 0, "Read_Chapter data mismatch (len=%u)", (unsigned)lm);
        if (olen != NULL) {
            TF_CHECK(la == lm, "Read_Chapter olen %u != %u", (unsigned)la, (unsigned)lm);
            *olen = la;
        }
        if (out != NULL) memcpy(out, ea, (size_t)lm);
    }
    return ar;
}

static uint8_t mdel(uint32_t k)
{
    rb_status_t er = rch_delete(&g_ref, k);
    rb_status_t ar = RBC_Delete(&g_rbc, k);
    TF_CHECK(ar == er, "Delete(%u): real %u model %u", (unsigned)k, (unsigned)ar, (unsigned)er);
    return ar;
}

/* ---- 定向测试 ---- */

static void t_c_readme_lifecycle(void)
{
    csetup(128u, 16u);
    static const uint8_t s1[9] = {'s','t','r','i','n','g','1','!',0x00};
    static const uint8_t s2[9] = {'s','t','r','i','n','g','2','!',0x00};
    TF_CHECK(ms(s1, 8u) == RB_OK, "w s1");
    TF_CHECK(mb(0x00) == RB_OK, "w nul1");
    TF_CHECK(mend() == RB_OK, "end1");
    TF_CHECK(ms(s2, 8u) == RB_OK, "w s2");
    TF_CHECK(mb(0x00) == RB_OK, "w nul2");
    TF_CHECK(mend() == RB_OK, "end2");
    TF_CHECK(RBC_Get_Chapter_Number(&g_rbc) == 2u, "number %u != 2", (unsigned)RBC_Get_Chapter_Number(&g_rbc));
    uint8_t out[32]; uint32_t olen = 0;
    TF_CHECK(mrc(out, &olen) == RB_OK, "read ch1");
    TF_CHECK(olen == 9u, "olen %u != 9", (unsigned)olen);
    TF_CHECK(memcmp(out, s1, 9) == 0, "ch1 content");
    TF_CHECK(mrc(out, NULL) == RB_OK, "read ch2 (NULL olen)");
    TF_CHECK(memcmp(out, s2, 9) == 0, "ch2 content");
    TF_CHECK(RBC_Get_Chapter_Number(&g_rbc) == 0u, "number != 0 after all reads");
    TF_CHECK(RBC_Read_Chapter(&g_rbc, out, NULL) == RB_ERR_EMPTY, "read empty chapter accepted");
    TF_CHECK(RBC_Read_Byte(&g_rbc, out) == RB_ERR_EMPTY, "read empty byte accepted");
    cstate_check("readme");
}

static void t_c_counts_and_head_len(void)
{
    csetup(64u, 16u);
    static const uint8_t a[5] = {1,2,3,4,5};
    static const uint8_t b[3] = {6,7,8};
    TF_CHECK(ms(a, 5u) == RB_OK, "w a");
    TF_CHECK(mend() == RB_OK, "end a");
    TF_CHECK(RBC_Get_Chapter_Number(&g_rbc) == 1u, "n1 %u", (unsigned)RBC_Get_Chapter_Number(&g_rbc));
    TF_CHECK(RBC_Get_Head_Chapter_Length(&g_rbc) == 5u, "head len != 5");
    TF_CHECK(ms(b, 3u) == RB_OK, "w b");
    TF_CHECK(mend() == RB_OK, "end b");
    TF_CHECK(RBC_Get_Chapter_Number(&g_rbc) == 2u, "n2 %u", (unsigned)RBC_Get_Chapter_Number(&g_rbc));
    TF_CHECK(RBC_Get_Head_Chapter_Length(&g_rbc) == 5u, "head len changed");
    uint8_t out[16]; uint32_t olen;
    TF_CHECK(mrc(out, &olen) == RB_OK, "read ch1");
    TF_CHECK(olen == 5u, "olen %u != 5", (unsigned)olen);
    TF_CHECK(memcmp(out, a, 5) == 0, "ch1 data");
    TF_CHECK(RBC_Get_Head_Chapter_Length(&g_rbc) == 3u, "head len != 3 after read");
    TF_CHECK(mrc(out, &olen) == RB_OK, "read ch2");
    TF_CHECK(olen == 3u, "olen %u != 3", (unsigned)olen);
    TF_CHECK(memcmp(out, b, 3) == 0, "ch2 data");
    TF_CHECK(RBC_Get_Chapter_Number(&g_rbc) == 0u, "n != 0");
    TF_CHECK(RBC_Get_Head_Chapter_Length(&g_rbc) == 0u, "head len != 0");
    cstate_check("counts");
}

static void t_c_read_byte_across_boundary(void)
{
    csetup(64u, 16u);
    static const uint8_t a[3] = {0xA1,0xA2,0xA3};
    static const uint8_t b[5] = {0xB1,0xB2,0xB3,0xB4,0xB5};
    TF_CHECK(ms(a, 3u) == RB_OK, "w a");
    TF_CHECK(mend() == RB_OK, "end a");
    TF_CHECK(ms(b, 5u) == RB_OK, "w b");
    TF_CHECK(mend() == RB_OK, "end b");
    static const uint8_t expect[8] = {0xA1,0xA2,0xA3,0xB1,0xB2,0xB3,0xB4,0xB5};
    uint8_t nonconst_sink;
    for (uint32_t i = 0u; i < 8u; i++) {
        uint8_t v;
        TF_CHECK(mrb(&v) == RB_OK, "read %u failed", (unsigned)i);
        TF_CHECK(v == expect[i], "byte %u: 0x%02X != 0x%02X", (unsigned)i, (unsigned)v, (unsigned)expect[i]);
    }
    TF_CHECK(mrb(&nonconst_sink) == RB_ERR_EMPTY, "read past end accepted");
    TF_CHECK(RBC_Get_Chapter_Number(&g_rbc) == 0u, "n != 0 after reading all");
    cstate_check("boundary");
}

static void t_c_empty_ops(void)
{
    csetup(32u, 8u);
    uint8_t out[8]; uint32_t olen;
    TF_CHECK(mend() == RB_ERR_EMPTY, "ending with empty tail accepted");
    TF_CHECK(mend() == RB_ERR_EMPTY, "second ending with empty tail accepted");
    TF_CHECK(mrb(out) == RB_ERR_EMPTY, "read byte on empty accepted");
    TF_CHECK(mrc(out, &olen) == RB_ERR_EMPTY, "read chapter on empty accepted");
    TF_CHECK(mdel(1u) == RB_ERR_EMPTY, "delete on empty accepted");
    TF_CHECK(mdel(0u) == RB_OK, "delete 0 accepted");
    TF_CHECK(RBC_Get_Chapter_Number(&g_rbc) == 0u, "n != 0");
    TF_CHECK(RBC_Get_Base_Free_Size(&g_rbc) == 32u, "base free != full");
    cstate_check("empty");
}

static void t_c_delete_semantics(void)
{
    csetup(64u, 16u);
    static const uint8_t a[4] = {1,2,3,4};
    static const uint8_t b[4] = {5,6,7,8};
    static const uint8_t c[4] = {9,10,11,12};
    ms(a, 4u); TF_CHECK(mend() == RB_OK, "end a");
    ms(b, 4u); TF_CHECK(mend() == RB_OK, "end b");
    ms(c, 4u); TF_CHECK(mend() == RB_OK, "end c");
    TF_CHECK(RBC_Get_Chapter_Number(&g_rbc) == 3u, "n3 %u", (unsigned)RBC_Get_Chapter_Number(&g_rbc));
    TF_CHECK(mdel(4u) == RB_ERR_EMPTY, "delete 4 > 3 accepted");
    TF_CHECK(RBC_Get_Chapter_Number(&g_rbc) == 3u, "n changed after failed delete");
    /* 头分段被部分读取后再删除：应只删除剩余部分 */
    uint8_t v;
    TF_CHECK(mrb(&v) == RB_OK, "rb1");
    TF_CHECK(mrb(&v) == RB_OK, "rb2");
    TF_CHECK(RBC_Get_Head_Chapter_Length(&g_rbc) == 2u, "head len != 2 after 2 reads");
    TF_CHECK(mdel(1u) == RB_OK, "delete head chapter");
    TF_CHECK(RBC_Get_Chapter_Number(&g_rbc) == 2u, "n2 %u", (unsigned)RBC_Get_Chapter_Number(&g_rbc));
    TF_CHECK(RBC_Get_Head_Chapter_Length(&g_rbc) == 4u, "new head len != 4");
    uint8_t out[16]; uint32_t olen;
    TF_CHECK(mrc(out, &olen) == RB_OK, "read ch2");
    TF_CHECK(olen == 4u, "olen");
    TF_CHECK(memcmp(out, b, 4) == 0, "ch2 data after delete");
    TF_CHECK(mdel(1u) == RB_OK, "delete last chapter");
    TF_CHECK(RBC_Get_Chapter_Number(&g_rbc) == 0u, "n != 0");
    TF_CHECK(RBC_Get_Base_Free_Size(&g_rbc) == 64u, "base not fully freed");
    cstate_check("delete");
}

static void t_c_base_full(void)
{
    csetup(4u, 8u);
    TF_CHECK(mb(0x11) == RB_OK, "w1");
    TF_CHECK(mb(0x22) == RB_OK, "w2");
    TF_CHECK(mb(0x33) == RB_OK, "w3");
    TF_CHECK(mb(0x44) == RB_OK, "w4");
    TF_CHECK(mb(0x55) == RB_ERR_FULL, "w5 to full base accepted");
    TF_CHECK(RBC_Get_Base_Free_Size(&g_rbc) == 0u, "base free != 0");
    TF_CHECK(mend() == RB_OK, "end");
    uint8_t out[8]; uint32_t olen;
    TF_CHECK(mrc(out, &olen) == RB_OK, "read");
    TF_CHECK(olen == 4u, "olen");
    static const uint8_t expect[4] = {0x11,0x22,0x33,0x44};
    TF_CHECK(memcmp(out, expect, 4) == 0, "data");
    cstate_check("basefull");
}

static void t_c_ring_one_slot(void)
{
    csetup(32u, 4u);
    static const uint8_t a[3] = {1,2,3};
    static const uint8_t b[2] = {4,5};
    TF_CHECK(ms(a, 3u) == RB_OK, "w a");
    TF_CHECK(mend() == RB_OK, "end a");
    TF_CHECK(ms(b, 2u) == RB_OK, "w b");
    TF_CHECK(mend() == RB_OK, "end b");
    TF_CHECK(RBC_Get_Chapter_Number(&g_rbc) == 2u, "n2 %u", (unsigned)RBC_Get_Chapter_Number(&g_rbc));
    TF_CHECK(RBC_Get_Chapter_Free_Size(&g_rbc) == 0u, "ring free != 0");
    TF_CHECK(mb(0x99) == RB_ERR_FULL, "write accepted when ring full");
    static const uint8_t b2[1] = {0x77};
    TF_CHECK(ms(b2, 1u) == RB_ERR_FULL, "write_string accepted when ring full");
    uint8_t out[8]; uint32_t olen;
    TF_CHECK(mrc(out, &olen) == RB_OK, "read ch1");
    TF_CHECK(olen == 3u, "olen");
    TF_CHECK(memcmp(out, a, 3) == 0, "ch1 data");
    TF_CHECK(RBC_Get_Chapter_Free_Size(&g_rbc) == 1u, "slot not freed after reading head");
    TF_CHECK(mb(0x99) == RB_OK, "write refused after head read");
    TF_CHECK(mrc(out, &olen) == RB_OK, "read ch2");
    TF_CHECK(olen == 2u, "olen");
    TF_CHECK(memcmp(out, b, 2) == 0, "ch2 data");
    TF_CHECK(RBC_Get_Chapter_Number(&g_rbc) == 0u, "n != 0 (0x99 tail not ended yet)");
    TF_CHECK(RBC_Get_Base_Free_Size(&g_rbc) == 31u, "base free %u != 31 (0x99 staged)",
             (unsigned)RBC_Get_Base_Free_Size(&g_rbc));
    TF_CHECK(mend() == RB_OK, "end staged 0x99");
    TF_CHECK(RBC_Get_Chapter_Number(&g_rbc) == 1u, "n != 1 after end");
    cstate_check("oneslot");
}

static void t_c_write_string_zero_len(void)
{
    csetup(16u, 8u);
    static const uint8_t dummy[1] = {0};
    TF_CHECK(ms(dummy, 0u) == RB_OK, "write 0 rejected");
    TF_CHECK(g_rbc.tail_chapter_length == 0u, "tail length changed");
    TF_CHECK(mend() == RB_ERR_EMPTY, "ending with tail=0 accepted");
    /* 正常写后 + 空写，tail 长度不变 */
    static const uint8_t a[3] = {1,2,3};
    TF_CHECK(ms(a, 3u) == RB_OK, "w3");
    TF_CHECK(ms(dummy, 0u) == RB_OK, "write 0 after data");
    TF_CHECK(g_rbc.tail_chapter_length == 3u, "tail length != 3");
    cstate_check("zerolen");
}

/* P1 新 API：RBC_Clear 清空并恢复到初始化状态 */
static void t_c_clear(void)
{
    csetup(32u, 8u);
    static const uint8_t a[4] = {1,2,3,4};
    TF_CHECK(ms(a, 4u) == RB_OK, "w");
    TF_CHECK(mend() == RB_OK, "end");
    TF_CHECK(RBC_Get_Chapter_Number(&g_rbc) == 1u, "n1 %u", (unsigned)RBC_Get_Chapter_Number(&g_rbc));
    TF_CHECK(RBC_Clear(&g_rbc) == RB_OK, "clear failed");
    rch_ref_init(&g_ref, 32u, 8u);
    cstate_check("clear");
    TF_CHECK(RBC_Get_Chapter_Number(&g_rbc) == 0u, "n after clear");
    TF_CHECK(RBC_Get_Base_Free_Size(&g_rbc) == 32u, "free after clear");
    TF_CHECK(RBC_Get_Head_Chapter_Length(&g_rbc) == 0u, "head len after clear");
    TF_CHECK(RBC_Clear(NULL) == RB_ERR_PARAM, "clear(NULL) accepted");
    TF_CHECK(ms(a, 4u) == RB_OK, "write after clear");
    TF_CHECK(mend() == RB_OK, "end after clear");
}

/* P1 新 API：RBC_Read_Chapter_Bounded——放不下则 TOO_SMALL 且不消费 */
static void t_c_read_chapter_bounded(void)
{
    csetup(32u, 8u);
    static const uint8_t a[5] = {0xA1,0xA2,0xA3,0xA4,0xA5};
    static const uint8_t b[2] = {0xB1,0xB2};
    TF_CHECK(ms(a, 5u) == RB_OK, "w a");
    TF_CHECK(mend() == RB_OK, "end a");
    TF_CHECK(ms(b, 2u) == RB_OK, "w b");
    TF_CHECK(mend() == RB_OK, "end b");
    uint8_t out[8];
    uint32_t actual = 0;
    /* 放不下：TOO_SMALL 且不消费（模型状态不变，仅比对长度口径） */
    TF_CHECK(RBC_Read_Chapter_Bounded(&g_rbc, out, 3u, &actual) == RB_ERR_TOO_SMALL, "TOO_SMALL expected");
    TF_CHECK(actual == g_ref.ch[0].len, "actual %u != model %u", (unsigned)actual, (unsigned)g_ref.ch[0].len);
    TF_CHECK(RBC_Get_Chapter_Number(&g_rbc) == g_ref.n, "bounded read consumed data");
    TF_CHECK(RBC_Get_Head_Chapter_Length(&g_rbc) == g_ref.ch[0].len, "head len changed after TOO_SMALL");
    /* max_len 为 0：必然 TOO_SMALL 且报完整长度 */
    TF_CHECK(RBC_Read_Chapter_Bounded(&g_rbc, out, 0u, &actual) == RB_ERR_TOO_SMALL, "max_len=0 TOO_SMALL expected");
    TF_CHECK(actual == 5u, "max_len=0 actual %u != 5", (unsigned)actual);
    /* 放得下：完整读取（镜像消费） */
    {
        static uint8_t em[RCH_MAX_DATA];
        uint32_t lm = 0;
        TF_CHECK(rch_read_chapter(&g_ref, em, &lm) == RB_OK, "model read ch1");
        TF_CHECK(RBC_Read_Chapter_Bounded(&g_rbc, out, 5u, &actual) == RB_OK, "exact fit");
        TF_CHECK(actual == lm, "actual %u != %u", (unsigned)actual, (unsigned)lm);
        TF_CHECK(memcmp(out, em, lm) == 0, "bounded content");
    }
    {
        static uint8_t em[RCH_MAX_DATA];
        uint32_t lm = 0;
        TF_CHECK(rch_read_chapter(&g_ref, em, &lm) == RB_OK, "model read ch2");
        TF_CHECK(RBC_Read_Chapter_Bounded(&g_rbc, out, 5u, &actual) == RB_OK, "read ch2");
        TF_CHECK(actual == lm, "actual %u != %u", (unsigned)actual, (unsigned)lm);
        TF_CHECK(memcmp(out, em, lm) == 0, "ch2 content");
    }
    TF_CHECK(RBC_Read_Chapter_Bounded(&g_rbc, out, 5u, &actual) == RB_ERR_EMPTY, "empty after all read");
    TF_CHECK(RBC_Read_Chapter_Bounded(&g_rbc, NULL, 5u, &actual) == RB_ERR_PARAM, "bounded NULL out accepted");
    TF_CHECK(RBC_Read_Chapter_Bounded(&g_rbc, out, 5u, NULL) == RB_ERR_PARAM, "bounded NULL actual accepted");
    cstate_check("bounded");
}

/* P0/BUG-2/BUG-3 修复验证：RBC_Init 参数校验全集（合并原三个初始化测试） */
static void t_c_init_validation(void)
{
    static uint8_t b[16];
    static uint32_t r[4];
    ring_buffer_chapter h;
    /* 非法 base 大小 */
    TF_CHECK(RBC_Init(&h, b, 0u, r, 16u) == RB_ERR_PARAM, "base size 0 accepted");
    TF_CHECK(RBC_Init(&h, b, 1u, r, 16u) == RB_ERR_PARAM, "base size 1 accepted");
    /* 非法 ring 大小：过小或非 4 倍数（非倍数会静默损失容量） */
    TF_CHECK(RBC_Init(&h, b, 8u, r, 0u) == RB_ERR_PARAM, "ring size 0 accepted");
    TF_CHECK(RBC_Init(&h, b, 8u, r, 1u) == RB_ERR_PARAM, "ring size 1 accepted");
    TF_CHECK(RBC_Init(&h, b, 16u, r, 2u) == RB_ERR_PARAM, "ring 2B accepted");
    TF_CHECK(RBC_Init(&h, b, 16u, r, 3u) == RB_ERR_PARAM, "ring 3B accepted");
    TF_CHECK(RBC_Init(&h, b, 16u, r, 5u) == RB_ERR_PARAM, "ring 5B accepted");
    TF_CHECK(RBC_Init(&h, b, 16u, r, 6u) == RB_ERR_PARAM, "ring 6B accepted");
    TF_CHECK(RBC_Init(&h, b, 16u, r, 7u) == RB_ERR_PARAM, "ring 7B accepted");
    /* 合法边界 */
    TF_CHECK(RBC_Init(&h, b, 16u, r, 4u) == RB_OK, "ring 4B rejected");
    TF_CHECK(RBC_Init(&h, b, 16u, r, 8u) == RB_OK, "ring 8B rejected");
    TF_CHECK(RBC_Init(&h, b, 16u, r, 12u) == RB_OK, "ring 12B rejected");
    /* BUG-3：失败时必须完全不触碰句柄（参数预检查） */
    memset(&h, 0xAA, sizeof h);
    TF_CHECK(RBC_Init(&h, b, 8u, r, 1u) == RB_ERR_PARAM, "bad ring accepted");
    TF_CHECK(h.base_handle.max_Length == 0xAAAAAAAAu, "base handle touched by failed init");
    TF_CHECK(h.init_flag == 0xAAu, "init_flag touched by failed init");
    /* 失败后用合法参数重新初始化应成功 */
    TF_CHECK(RBC_Init(&h, b, 8u, r, 16u) == RB_OK, "re-init after failure failed");
    TF_CHECK(h.init_flag == 1u, "init_flag not set after re-init");
    TF_CHECK(h.base_handle.max_Length == 8u, "base handle not set after re-init");
}

/* P0 防御分支验证（白盒）：内部状态失步时读取失败且分段状态冻结，
 * 随后 RBC_Clear 可恢复——这是头文件"错误后恢复"契约的代码级验证 */
static void t_c_defensive_freeze(void)
{
    csetup(32u, 8u);
    static const uint8_t a[4] = {1,2,3,4};
    TF_CHECK(ms(a, 4u) == RB_OK, "w");
    TF_CHECK(mend() == RB_OK, "end");
    TF_CHECK(RBC_Get_Head_Chapter_Length(&g_rbc) == 4u, "head len 4");
    /* 白盒注入：破坏基环一致性，模拟失步 */
    g_rbc.base_handle.Length = 0u;
    uint8_t out[8];
    uint32_t actual = 0;
    TF_CHECK(RBC_Read_Byte(&g_rbc, out) == RB_ERR_EMPTY, "defensive read byte");
    TF_CHECK(RBC_Get_Head_Chapter_Length(&g_rbc) == 4u, "head len frozen after failed read");
    TF_CHECK(RBC_Read_Chapter_Bounded(&g_rbc, out, 8u, &actual) == RB_ERR_EMPTY, "defensive bounded");
    TF_CHECK(RBC_Get_Head_Chapter_Length(&g_rbc) == 4u, "head len frozen after failed bounded");
    /* RBC_Clear 恢复 */
    TF_CHECK(RBC_Clear(&g_rbc) == RB_OK, "clear after defensive");
    TF_CHECK(RBC_Get_Head_Chapter_Length(&g_rbc) == 0u, "recovered");
    TF_CHECK(RBC_Get_Chapter_Number(&g_rbc) == 0u, "chapters after recover");
    TF_CHECK(RBC_Get_Base_Free_Size(&g_rbc) == 32u, "base free after recover");
    TF_CHECK(ms(a, 4u) == RB_OK, "write after recover");
    TF_CHECK(mend() == RB_OK, "end after recover");
}

static void t_c_reinit(void)
{
    csetup(32u, 8u);
    static const uint8_t a[4] = {1,2,3,4};
    TF_CHECK(ms(a, 4u) == RB_OK, "w");
    TF_CHECK(mend() == RB_OK, "end");
    TF_CHECK(RBC_Get_Chapter_Number(&g_rbc) == 1u, "n1");
    TF_CHECK(RBC_Init(&g_rbc, g_base, 32u, (uint32_t *)(void *)g_ring, 8u) == RB_OK, "reinit");
    rch_ref_init(&g_ref, 32u, 8u);
    TF_CHECK(RBC_Get_Chapter_Number(&g_rbc) == 0u, "n != 0 after reinit");
    TF_CHECK(RBC_Get_Base_Free_Size(&g_rbc) == 32u, "free after reinit");
    TF_CHECK(RBC_Get_Head_Chapter_Length(&g_rbc) == 0u, "head len after reinit");
    cstate_check("reinit");
}

/* ---- 随机属性测试 ---- */

static void cproperty_run(uint32_t bcap, uint32_t rcap, uint32_t iters, uint32_t seed)
{
    static uint8_t src[RCH_MAX_DATA];
    static uint8_t out[RCH_MAX_DATA];
    for (uint32_t round = 0u; round < RB_TEST_SOAK; round++) {
        csetup(bcap, rcap);
        tf_seed(seed + round);/* 每轮独立种子：SOAK 放大的是新序列而非同种子重放 */
        for (uint32_t it = 0u; it < iters; it++) {
            uint32_t op = tf_rand_range(100u);
            if (op < 24u) {
                (void)mb((uint8_t)tf_rand());
            } else if (op < 48u) {
                uint32_t n = tf_rand_range(bcap + 2u);
                for (uint32_t i = 0u; i < n; i++) src[i] = (uint8_t)tf_rand();
                (void)ms(src, n);
            } else if (op < 60u) {
                (void)mend();
            } else if (op < 72u) {
                (void)mrb(NULL);
            } else if (op < 82u) {
                (void)mrc(out, NULL);
            } else if (op < 90u) {
                uint32_t k = tf_rand_range(g_ref.n + 2u);
                (void)mdel(k);
            } else if (op < 96u) {
                /* 有界读取：镜像模型，TOO_SMALL 不消费 */
                uint32_t max_len = tf_rand_range(bcap + 2u);
                uint32_t actual = 0, expected = 0;
                rb_status_t er = rch_read_chapter_bounded(&g_ref, out, max_len, &expected);
                rb_status_t ar = RBC_Read_Chapter_Bounded(&g_rbc, out, max_len, &actual);
                TF_CHECK(ar == er, "it %u: Bounded(max=%u) ret %u ref %u", (unsigned)it, (unsigned)max_len, (unsigned)ar, (unsigned)er);
                if (er != RB_ERR_EMPTY)
                    TF_CHECK(actual == expected, "it %u: Bounded actual %u != %u", (unsigned)it, (unsigned)actual, (unsigned)expected);
                if (er == RB_OK)
                    cstate_check("prop-bounded");
            } else {
                /* 清空：恢复初始化状态 */
                TF_CHECK(RBC_Clear(&g_rbc) == RB_OK, "it %u: Clear failed", (unsigned)it);
                rch_ref_init(&g_ref, bcap, rcap);
            }
            cstate_check("prop");
        }
    }
}

static void t_c_property(void)
{
    cproperty_run(16u, 8u,  20000u, 11u);
    cproperty_run(64u, 16u, 20000u, 12u);
    cproperty_run(8u,  4u,  20000u, 13u);
    cproperty_run(33u, 12u, 20000u, 14u);
    cproperty_run(7u,  8u,  20000u, 15u);
}

/* BUG-3 修复验证：RBC_Init 失败时必须完全不触碰句柄（参数预检查） */
static void t_c_partial_init_state(void)
{
    ring_buffer_chapter h;
    memset(&h, 0xAA, sizeof h);   /* 模拟复用/未清理的旧句柄 */
    static uint8_t b[8];
    static uint32_t r[4];
    /* base 合法、ring 非法：预检查失败，句柄不应被触碰 */
    TF_CHECK(RBC_Init(&h, b, 8u, r, 1u) == RB_ERR_PARAM, "bad ring size accepted");
    TF_CHECK(h.base_handle.max_Length == 0xAAAAAAAAu, "base handle touched by failed init");
    TF_CHECK(h.init_flag == 0xAAu, "init_flag touched by failed init");
    /* 失败后用合法参数重新初始化应成功 */
    TF_CHECK(RBC_Init(&h, b, 8u, r, 16u) == RB_OK, "re-init after failure failed");
    TF_CHECK(h.init_flag == 1u, "init_flag not set after re-init");
    TF_CHECK(h.base_handle.max_Length == 8u, "base handle not set after re-init");
}

/* BUG-7 修复验证：空指针防护 */
static void t_c_null_guard(void)
{
    static uint8_t b[8];
    static uint32_t r[4];
    static uint8_t out[8];
    uint32_t olen;
    TF_CHECK(RBC_Init(NULL, b, 8u, r, 16u) == RB_ERR_PARAM, "init(NULL handle) accepted");
    TF_CHECK(RBC_Init(&g_rbc, NULL, 8u, r, 16u) == RB_ERR_PARAM, "init(NULL base) accepted");
    TF_CHECK(RBC_Init(&g_rbc, b, 8u, NULL, 16u) == RB_ERR_PARAM, "init(NULL ring) accepted");
    TF_CHECK(RBC_Write_Byte(NULL, 0x41) == RB_ERR_PARAM, "Write_Byte(NULL) accepted");
    TF_CHECK(RBC_Write_String(NULL, out, 1u) == RB_ERR_PARAM, "Write_String(NULL) accepted");
    TF_CHECK(RBC_Ending_Chapter(NULL) == RB_ERR_PARAM, "Ending(NULL) accepted");
    TF_CHECK(RBC_Read_Byte(NULL, out) == RB_ERR_PARAM, "Read_Byte(NULL) accepted");
    TF_CHECK(RBC_Read_Chapter(NULL, out, &olen) == RB_ERR_PARAM, "Read_Chapter(NULL) accepted");
    TF_CHECK(RBC_Delete(NULL, 1u) == RB_ERR_PARAM, "Delete(NULL) accepted");
    TF_CHECK(RBC_Get_Chapter_Number(NULL) == 0u, "Chapter_Number(NULL) != 0");
    TF_CHECK(RBC_Get_Head_Chapter_Length(NULL) == 0u, "head_len(NULL) != 0");
    TF_CHECK(RBC_Get_Base_Free_Size(NULL) == 0u, "Base_Free_Size(NULL) != 0");
    TF_CHECK(RBC_Get_Chapter_Free_Size(NULL) == 0u, "Chapter_Free_Size(NULL) != 0");
}

int main(void)
{
    RUN_TEST(t_c_init_validation);
    RUN_TEST(t_c_partial_init_state);
    RUN_TEST(t_c_null_guard);
    RUN_TEST(t_c_readme_lifecycle);
    RUN_TEST(t_c_counts_and_head_len);
    RUN_TEST(t_c_read_byte_across_boundary);
    RUN_TEST(t_c_empty_ops);
    RUN_TEST(t_c_delete_semantics);
    RUN_TEST(t_c_base_full);
    RUN_TEST(t_c_ring_one_slot);
    RUN_TEST(t_c_write_string_zero_len);
    RUN_TEST(t_c_clear);
    RUN_TEST(t_c_read_chapter_bounded);
    RUN_TEST(t_c_defensive_freeze);
    RUN_TEST(t_c_reinit);
    RUN_TEST(t_c_property);
    int rc = tf_summary();
    free(g_base);
    free(g_ring);
    return rc;
}
