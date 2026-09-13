/**
 * \file ring_buffer.c
 * \brief 简易环形缓冲的实现
 * \author netube_99\netube@163.com
 * \date 2026.09.13
 * \version v0.5.0
*/

#include <stdint.h>
#include <string.h>
#include "ring_buffer.h"

//内部无锁核心，公开接口仅在封装层进出临界区

static rb_status_t rb_init_core(ring_buffer *rb_handle, uint8_t *buffer_addr ,uint32_t buffer_size)
{
    //缓冲区数组空间必须不小于2且小于数据类型最大值
    if(buffer_size < 2 || buffer_size == 0xFFFFFFFF)
        return RB_ERR_PARAM ;
    rb_handle->head = 0 ;
    rb_handle->tail = 0 ;
    rb_handle->Length = 0 ;
    rb_handle->array_addr = buffer_addr ;
    rb_handle->max_Length = buffer_size ;
    return RB_OK ;
}

static rb_status_t rb_clear_core(ring_buffer *rb_handle)
{
    rb_handle->head = 0 ;
    rb_handle->tail = 0 ;
    rb_handle->Length = 0 ;
    return RB_OK ;
}

static rb_status_t rb_write_string_core(ring_buffer *rb_handle, const uint8_t *input_addr, uint32_t write_Length)
{
    //剩余空间不足则整体拒绝（先减后比，防止长度回绕绕过检查）
    if(write_Length > (rb_handle->max_Length - rb_handle->Length))
        return RB_ERR_FULL ;
    uint32_t write_size_a, write_size_b ;
    if((rb_handle->max_Length - rb_handle->tail) < write_Length)
    {
        //写入长度小于顺序可用空间，拆成两段分别写入
        write_size_a = rb_handle->max_Length - rb_handle->tail ;
        write_size_b = write_Length - write_size_a ;
        memcpy(rb_handle->array_addr + rb_handle->tail, input_addr, write_size_a);
        memcpy(rb_handle->array_addr, input_addr + write_size_a, write_size_b);
        rb_handle->tail = write_size_b ;
    }
    else
    {
        write_size_a = write_Length ;
        memcpy(rb_handle->array_addr + rb_handle->tail, input_addr, write_size_a);
        rb_handle->tail += write_size_a ;
        if(rb_handle->tail == rb_handle->max_Length)
            rb_handle->tail = 0 ;//尾指针写到数组尾部，回到开头
    }
    rb_handle->Length += write_Length ;
    return RB_OK ;
}

static rb_status_t rb_read_string_core(ring_buffer *rb_handle, uint8_t *output_addr, uint32_t read_Length)
{
    if(read_Length > rb_handle->Length)
        return RB_ERR_EMPTY ;
    uint32_t Read_size_a, Read_size_b ;
    if(read_Length > (rb_handle->max_Length - rb_handle->head))
    {
        //读取长度小于顺序可用空间，拆成两段分别读取
        Read_size_a = rb_handle->max_Length - rb_handle->head ;
        Read_size_b = read_Length - Read_size_a ;
        memcpy(output_addr, rb_handle->array_addr + rb_handle->head, Read_size_a);
        memcpy(output_addr + Read_size_a, rb_handle->array_addr, Read_size_b);
        rb_handle->head = Read_size_b ;
    }
    else
    {
        Read_size_a = read_Length ;
        memcpy(output_addr, rb_handle->array_addr + rb_handle->head, Read_size_a);
        rb_handle->head += Read_size_a ;
        if(rb_handle->head == rb_handle->max_Length)
            rb_handle->head = 0 ;//头指针读到数组尾部，回到开头
    }
    rb_handle->Length -= read_Length ;
    return RB_OK ;
}

static rb_status_t rb_peek_byte_core(ring_buffer *rb_handle, uint32_t offset, uint8_t *output_addr)
{
    if(offset >= rb_handle->Length)
        return RB_ERR_EMPTY ;
    uint32_t pos ;//减法定位，防止整数回绕
    if(offset >= (rb_handle->max_Length - rb_handle->head))
        pos = offset - (rb_handle->max_Length - rb_handle->head) ;
    else
        pos = rb_handle->head + offset ;
    *output_addr = *(rb_handle->array_addr + pos) ;
    return RB_OK ;
}

static rb_status_t rb_peek_string_core(ring_buffer *rb_handle, uint8_t *output_addr, uint32_t max_len, uint32_t *copied_Length)
{
    uint32_t peek_len = rb_handle->Length ;
    if(peek_len > max_len)
        peek_len = max_len ;
    uint32_t copy_size_a = peek_len ;
    uint32_t copy_size_b = 0 ;
    if(peek_len > (rb_handle->max_Length - rb_handle->head))
    {
        copy_size_a = rb_handle->max_Length - rb_handle->head ;
        copy_size_b = peek_len - copy_size_a ;
    }
    memcpy(output_addr, rb_handle->array_addr + rb_handle->head, copy_size_a);
    memcpy(output_addr + copy_size_a, rb_handle->array_addr, copy_size_b);
    *copied_Length = peek_len ;
    return RB_OK ;
}

static rb_status_t rb_delete_core(ring_buffer *rb_handle, uint32_t Length)
{
    if(rb_handle->Length < Length)
        return RB_ERR_EMPTY ;
    if(Length >= (rb_handle->max_Length - rb_handle->head))//减法判断回绕，防止整数回绕
        rb_handle->head = Length - (rb_handle->max_Length - rb_handle->head);
    else
        rb_handle->head += Length ;
    rb_handle->Length -= Length ;
    return RB_OK ;
}

//公开接口：参数校验 + 临界区 + 委托核心

rb_status_t RB_Init(ring_buffer *rb_handle, uint8_t *buffer_addr ,uint32_t buffer_size)
{
    if(rb_handle == NULL || buffer_addr == NULL)
        return RB_ERR_PARAM ;
    return rb_init_core(rb_handle, buffer_addr, buffer_size) ;
}

rb_status_t RB_Clear(ring_buffer *rb_handle)
{
    uint32_t key ;
    rb_status_t ret ;
    if(rb_handle == NULL)
        return RB_ERR_PARAM ;
    key = RB_CRITICAL_ENTER() ;
    ret = rb_clear_core(rb_handle) ;
    RB_CRITICAL_EXIT(key) ;
    return ret ;
}

rb_status_t RB_Write_Byte(ring_buffer *rb_handle, uint8_t data)
{
    uint32_t key ;
    rb_status_t ret ;
    if(rb_handle == NULL)
        return RB_ERR_PARAM ;
    key = RB_CRITICAL_ENTER() ;
    ret = rb_write_string_core(rb_handle, &data, 1u) ;
    RB_CRITICAL_EXIT(key) ;
    return ret ;
}

rb_status_t RB_Write_String(ring_buffer *rb_handle, const uint8_t *input_addr, uint32_t write_Length)
{
    uint32_t key ;
    rb_status_t ret ;
    if(write_Length == 0u)//0 长度视为无操作，先于一切校验
        return RB_OK ;
    if(rb_handle == NULL || input_addr == NULL)
        return RB_ERR_PARAM ;
    key = RB_CRITICAL_ENTER() ;
    ret = rb_write_string_core(rb_handle, input_addr, write_Length) ;
    RB_CRITICAL_EXIT(key) ;
    return ret ;
}

rb_status_t RB_Read_Byte(ring_buffer *rb_handle, uint8_t *output_addr)
{
    uint32_t key ;
    rb_status_t ret ;
    if(rb_handle == NULL || output_addr == NULL)
        return RB_ERR_PARAM ;
    key = RB_CRITICAL_ENTER() ;
    ret = rb_read_string_core(rb_handle, output_addr, 1u) ;
    RB_CRITICAL_EXIT(key) ;
    return ret ;
}

rb_status_t RB_Read_String(ring_buffer *rb_handle, uint8_t *output_addr, uint32_t read_Length)
{
    uint32_t key ;
    rb_status_t ret ;
    if(read_Length == 0u)//0 长度视为无操作，先于一切校验
        return RB_OK ;
    if(rb_handle == NULL || output_addr == NULL)
        return RB_ERR_PARAM ;
    key = RB_CRITICAL_ENTER() ;
    ret = rb_read_string_core(rb_handle, output_addr, read_Length) ;
    RB_CRITICAL_EXIT(key) ;
    return ret ;
}

rb_status_t RB_Peek_Byte(ring_buffer *rb_handle, uint32_t offset, uint8_t *output_addr)
{
    uint32_t key ;
    rb_status_t ret ;
    if(rb_handle == NULL || output_addr == NULL)
        return RB_ERR_PARAM ;
    key = RB_CRITICAL_ENTER() ;
    ret = rb_peek_byte_core(rb_handle, offset, output_addr) ;
    RB_CRITICAL_EXIT(key) ;
    return ret ;
}

rb_status_t RB_Peek_String(ring_buffer *rb_handle, uint8_t *output_addr, uint32_t max_len, uint32_t *copied_Length)
{
    uint32_t key ;
    rb_status_t ret ;
    if(rb_handle == NULL || output_addr == NULL || copied_Length == NULL)
        return RB_ERR_PARAM ;
    key = RB_CRITICAL_ENTER() ;
    ret = rb_peek_string_core(rb_handle, output_addr, max_len, copied_Length) ;
    RB_CRITICAL_EXIT(key) ;
    return ret ;
}

rb_status_t RB_Delete(ring_buffer *rb_handle, uint32_t Length)
{
    uint32_t key ;
    rb_status_t ret ;
    if(rb_handle == NULL)
        return RB_ERR_PARAM ;
    key = RB_CRITICAL_ENTER() ;
    ret = rb_delete_core(rb_handle, Length) ;
    RB_CRITICAL_EXIT(key) ;
    return ret ;
}

uint32_t RB_Get_Length(ring_buffer *rb_handle)
{
    uint32_t len ;
    if(rb_handle == NULL)
        return 0 ;
    uint32_t key = RB_CRITICAL_ENTER() ;
    len = rb_handle->Length ;
    RB_CRITICAL_EXIT(key) ;
    return len ;
}

uint32_t RB_Get_FreeSize(ring_buffer *rb_handle)
{
    uint32_t len ;
    if(rb_handle == NULL)
        return 0 ;
    uint32_t key = RB_CRITICAL_ENTER() ;
    len = (rb_handle->max_Length - rb_handle->Length) ;
    RB_CRITICAL_EXIT(key) ;
    return len ;
}

uint32_t RB_Get_Capacity(ring_buffer *rb_handle)
{
    uint32_t len ;
    if(rb_handle == NULL)
        return 0 ;
    uint32_t key = RB_CRITICAL_ENTER() ;
    len = rb_handle->max_Length ;
    RB_CRITICAL_EXIT(key) ;
    return len ;
}
