/**
 * ref_model.h - 基础环形缓冲的参照模型（线性数组 + memmove 实现）
 * 语义与库的文档约定一致：
 *  - Write_Byte: 满则 RB_ERR_FULL
 *  - Write_String(n): n 超过剩余空间则 RB_ERR_FULL；n==0 恒 RB_OK
 *  - Read_Byte: 空则 RB_ERR_EMPTY
 *  - Read_String(n): n > Length 则 RB_ERR_EMPTY；n==0 恒 RB_OK
 *  - Delete(n): n > Length 则 RB_ERR_EMPTY；n==0 恒 RB_OK
 * 同时镜像内部 head 指针，用于逐字节布局校验（非破坏性）。
 */
#ifndef REF_MODEL_H_
#define REF_MODEL_H_

#include <stdint.h>
#include <string.h>
#include "ring_buffer.h"

#define REF_CAP 4096u

typedef struct {
    uint8_t buf[REF_CAP];   /* FIFO 内容，buf[0] 为下一个将被读出的字节 */
    uint32_t len;
    uint32_t head;          /* 镜像 rb->head —— 有意为之的白盒校验：
                               用于锁定内部指针推进约定，合法的内部重构
                               若改变指针布局会在此暴露（属预期误报） */
} ref_t;

static void ref_init(ref_t *r) { r->len = 0u; r->head = 0u; }

static rb_status_t ref_write_byte(ref_t *r, uint8_t b, uint32_t cap)
{
    if (r->len == cap) return RB_ERR_FULL;
    r->buf[r->len] = b;
    r->len++;
    return RB_OK;
}

static rb_status_t ref_write_string(ref_t *r, const uint8_t *p, uint32_t n, uint32_t cap)
{
    if ((r->len + n) > cap) return RB_ERR_FULL;
    memcpy(r->buf + r->len, p, (size_t)n);
    r->len += n;
    return RB_OK;
}

static rb_status_t ref_read_byte(ref_t *r, uint8_t *out, uint32_t cap)
{
    if (r->len == 0u) return RB_ERR_EMPTY;
    *out = r->buf[0];
    memmove(r->buf, r->buf + 1, (size_t)(r->len - 1u));
    r->len--;
    r->head = (r->head + 1u) % cap;
    return RB_OK;
}

static rb_status_t ref_read_string(ref_t *r, uint8_t *out, uint32_t n, uint32_t cap)
{
    if (n > r->len) return RB_ERR_EMPTY;
    memcpy(out, r->buf, (size_t)n);
    memmove(r->buf, r->buf + n, (size_t)(r->len - n));
    r->len -= n;
    r->head = (r->head + n) % cap;
    return RB_OK;
}

static rb_status_t ref_delete(ref_t *r, uint32_t n, uint32_t cap)
{
    if (r->len < n) return RB_ERR_EMPTY;
    memmove(r->buf, r->buf + n, (size_t)(r->len - n));
    r->len -= n;
    r->head = (r->head + n) % cap;
    return RB_OK;
}

static rb_status_t ref_peek_byte(const ref_t *r, uint32_t offset, uint8_t *out)
{
    if (offset >= r->len) return RB_ERR_EMPTY;
    *out = r->buf[offset];
    return RB_OK;
}

static rb_status_t ref_peek_string(const ref_t *r, uint8_t *out, uint32_t max_len, uint32_t *copied)
{
    uint32_t n = r->len;
    if (n > max_len) n = max_len;
    memcpy(out, r->buf, (size_t)n);
    *copied = n;
    return RB_OK;
}

#endif
