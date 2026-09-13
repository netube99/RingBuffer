/**
 * ref_model_chapter.h - 分段环形缓冲的参照模型（"正确语义"）
 *
 * 模型状态：ch[0..n-1] 为已记录可读的分段队列（ch[0] = 头分段），
 * tail 为正在写入、尚未 Ending 的分段；bhead/rhead 镜像内部基环/分段环 head，
 * 用于逐字节布局校验。
 *
 * 关键不变量（对应库实现）：
 *  - 分段环中保存 ch[1..n-1] 的 4 字节长度记录（头的记录在成为头时已被取出），
 *    n==0 时分段环为空
 *  - 基环内容 = ch[0..n-1] 数据 + tail 暂存数据
 *  - Ending 成功条件（正确语义）：tail.len > 0 且 n*4 <= ring_cap
 */
#ifndef REF_MODEL_CHAPTER_H_
#define REF_MODEL_CHAPTER_H_

#include <stdint.h>
#include <string.h>
#include "ring_buffer_chapter.h"

#define RCH_MAX_CH   64u
#define RCH_MAX_DATA 2048u

typedef struct { uint8_t data[RCH_MAX_DATA]; uint32_t len; } rch_ch_t;

typedef struct {
    rch_ch_t ch[RCH_MAX_CH];
    uint32_t n;
    rch_ch_t tail;
    uint32_t base_cap, ring_cap;
    uint32_t bhead, rhead;   /* 镜像内部基环/分段环 head —— 白盒校验，同 ref_model 注释 */
} rch_ref_t;

static void rch_ref_init(rch_ref_t *r, uint32_t bcap, uint32_t rcap)
{
    memset(r, 0, sizeof(*r));
    r->base_cap = bcap;
    r->ring_cap = rcap;
}

static uint32_t rch_stream_len(const rch_ref_t *r)
{
    uint32_t s = 0u;
    for (uint32_t i = 0u; i < r->n; i++) s += r->ch[i].len;
    return s;
}

static uint32_t rch_ring_records(const rch_ref_t *r) { return (r->n != 0u) ? (r->n - 1u) : 0u; }
static uint32_t rch_base_free(const rch_ref_t *r) { return r->base_cap - rch_stream_len(r) - r->tail.len; }
static uint32_t rch_chapter_free(const rch_ref_t *r) { return (r->ring_cap - rch_ring_records(r) * 4u) / 4u; }

static rb_status_t rch_write_byte(rch_ref_t *r, uint8_t b)
{
    if (rch_chapter_free(r) == 0u) return RB_ERR_FULL;
    if (rch_base_free(r) == 0u) return RB_ERR_FULL;
    r->tail.data[r->tail.len] = b;
    r->tail.len++;
    return RB_OK;
}

static rb_status_t rch_write_string(rch_ref_t *r, const uint8_t *p, uint32_t n)
{
    if (n == 0u) return RB_OK;
    if (rch_chapter_free(r) == 0u) return RB_ERR_FULL;
    if (n > rch_base_free(r)) return RB_ERR_FULL;
    memcpy(r->tail.data + r->tail.len, p, (size_t)n);
    r->tail.len += n;
    return RB_OK;
}

static rb_status_t rch_ending(rch_ref_t *r)
{
    if (r->tail.len == 0u) return RB_ERR_EMPTY;
    if ((r->n * 4u) > r->ring_cap) return RB_ERR_FULL; /* 正确语义：分段环无空位则失败 */
    if (r->n >= RCH_MAX_CH) return RB_ERR_FULL;        /* 模型容量防线，防止越界 */
    if (r->n == 0u) r->rhead = (r->rhead + 4u) % r->ring_cap;  /* 首条记录写入后立即被取回 */
    r->ch[r->n] = r->tail;
    r->n++;
    r->tail.len = 0u;
    return RB_OK;
}

static rb_status_t rch_read_byte(rch_ref_t *r, uint8_t *out)
{
    if (r->n == 0u) return RB_ERR_EMPTY;
    *out = r->ch[0].data[0];
    memmove(r->ch[0].data, r->ch[0].data + 1, (size_t)(r->ch[0].len - 1u));
    r->ch[0].len--;
    r->bhead = (r->bhead + 1u) % r->base_cap;
    if (r->ch[0].len == 0u) {
        memmove(&r->ch[0], &r->ch[1], (size_t)(r->n - 1u) * sizeof(rch_ch_t));
        r->n--;
        if (r->n != 0u) r->rhead = (r->rhead + 4u) % r->ring_cap;
    }
    return RB_OK;
}

static rb_status_t rch_read_chapter(rch_ref_t *r, uint8_t *out, uint32_t *outlen)
{
    if (r->n == 0u) return RB_ERR_EMPTY;
    memcpy(out, r->ch[0].data, (size_t)r->ch[0].len);
    if (outlen != NULL) *outlen = r->ch[0].len;
    r->bhead = (r->bhead + r->ch[0].len) % r->base_cap;
    memmove(&r->ch[0], &r->ch[1], (size_t)(r->n - 1u) * sizeof(rch_ch_t));
    r->n--;
    if (r->n != 0u) r->rhead = (r->rhead + 4u) % r->ring_cap;
    return RB_OK;
}

static rb_status_t rch_read_chapter_bounded(rch_ref_t *r, uint8_t *out, uint32_t max_len, uint32_t *actual)
{
    if (r->n == 0u) return RB_ERR_EMPTY;
    if (r->ch[0].len > max_len) {
        *actual = r->ch[0].len;
        return RB_ERR_TOO_SMALL;
    }
    return rch_read_chapter(r, out, actual);
}

static rb_status_t rch_delete(rch_ref_t *r, uint32_t k)
{
    if (k == 0u) return RB_OK;
    if (k > r->n) return RB_ERR_EMPTY;
    uint32_t consumed = 0u;
    for (uint32_t i = 0u; i < k; i++) consumed += r->ch[i].len;
    r->bhead = (r->bhead + consumed) % r->base_cap;
    memmove(&r->ch[0], &r->ch[k], (size_t)(r->n - k) * sizeof(rch_ch_t));
    r->n -= k;
    if (r->n != 0u) r->rhead = (r->rhead + 4u * k) % r->ring_cap;
    else            r->rhead = (r->rhead + 4u * (k - 1u)) % r->ring_cap;
    return RB_OK;
}

#endif
