/**
 * \file ring_buffer_chapter.c
 * \brief 简易分段环形缓冲的实现
 * \author netube_99\netube@163.com
 * \date 2026.09.13
 * \version v0.5.0
*/

#include <stdint.h>
#include <stddef.h>
#include "ring_buffer_chapter.h"

//内部无锁核心，核心函数内部调用基础版公开接口（依赖临界区钩子支持嵌套）

static rb_status_t rbc_init_core(ring_buffer_chapter *rbc_handle,\
                uint8_t *base_buffer_addr, uint32_t base_buffer_size,\
                uint32_t *chapter_buffer_addr, uint32_t chapter_buffer_size)
{
    //分段环空间必须为 4 的倍数且不小于 4（每条分段记录占 4 字节）
    if((chapter_buffer_size < 4) || ((chapter_buffer_size % 4) != 0))
        return RB_ERR_PARAM ;
    if(RB_Init(&(rbc_handle->base_handle), base_buffer_addr, base_buffer_size) != RB_OK)
        return RB_ERR_PARAM ;
    if(RB_Init(&(rbc_handle->chapter_handle), (uint8_t *)chapter_buffer_addr, chapter_buffer_size) != RB_OK)
        return RB_ERR_PARAM ;//防御：分段环参数已预检，此处不可达
    rbc_handle->head_chapter_length = 0 ;
    rbc_handle->tail_chapter_length = 0 ;
    rbc_handle->init_flag = 1 ;
    return RB_OK ;
}

static rb_status_t rbc_clear_core(ring_buffer_chapter *rbc_handle)
{
    rb_status_t ret ;
    ret = RB_Clear(&(rbc_handle->base_handle)) ;
    if(ret != RB_OK)
        return ret ;
    ret = RB_Clear(&(rbc_handle->chapter_handle)) ;
    if(ret != RB_OK)
        return ret ;
    rbc_handle->head_chapter_length = 0 ;
    rbc_handle->tail_chapter_length = 0 ;
    rbc_handle->init_flag = 1 ;
    return RB_OK ;
}

static rb_status_t rbc_write_string_core(ring_buffer_chapter *rbc_handle, const uint8_t *input_addr, uint32_t write_Length)
{
    if(RBC_Get_Chapter_Free_Size(rbc_handle) == 0)//分段环无空位则拒绝，防止产生无法回读的孤儿数据
        return RB_ERR_FULL ;
    if(RB_Write_String(&(rbc_handle->base_handle), input_addr, write_Length) != RB_OK)
        return RB_ERR_FULL ;
    rbc_handle->tail_chapter_length += write_Length ;
    return RB_OK ;
}

static rb_status_t rbc_ending_core(ring_buffer_chapter *rbc_handle)
{
    if(rbc_handle->tail_chapter_length == 0)
        return RB_ERR_EMPTY ;
    //将尾分段暂存字节计数作为一条 4 字节记录存入分段环
    if(RB_Write_String(&(rbc_handle->chapter_handle), (uint8_t *)&rbc_handle->tail_chapter_length, 4) != RB_OK)
        return RB_ERR_FULL ;//防御：写入数据时已预留记录空间，此处不可达
    //首条记录直接取回为头分段可读字节数
    if(rbc_handle->init_flag)
    {
        if(RB_Read_String(&(rbc_handle->chapter_handle), (uint8_t *)&rbc_handle->head_chapter_length, 4) != RB_OK)
            return RB_ERR_EMPTY ;//防御：刚写入必能读出，此处不可达
        rbc_handle->init_flag = 0 ;
    }
    rbc_handle->tail_chapter_length = 0 ;
    return RB_OK ;
}

/* 头分段读取完毕后的推进：取回下一条分段的长度记录，无记录则复位到初始化状态 */
static rb_status_t rbc_advance_head(ring_buffer_chapter *rbc_handle)
{
    rbc_handle->head_chapter_length = 0 ;
    if(RBC_Get_Chapter_Number(rbc_handle) == 0)
    {
        rbc_handle->init_flag = 1 ;
        return RB_OK ;
    }
    if(RB_Read_String(&(rbc_handle->chapter_handle), (uint8_t *)&rbc_handle->head_chapter_length, 4) != RB_OK)
        return RB_ERR_EMPTY ;//防御：读取失败，保持状态不推进
    return RB_OK ;
}

static rb_status_t rbc_read_byte_core(ring_buffer_chapter *rbc_handle, uint8_t *output_addr)
{
    if(rbc_handle->head_chapter_length == 0)
        return RB_ERR_EMPTY ;
    if(RB_Read_Byte(&(rbc_handle->base_handle), output_addr) != RB_OK)
        return RB_ERR_EMPTY ;//防御：读取失败，保持状态不推进
    rbc_handle->head_chapter_length -- ;
    if(rbc_handle->head_chapter_length == 0)
        return rbc_advance_head(rbc_handle) ;
    return RB_OK ;
}

static rb_status_t rbc_read_chapter_bounded_core(ring_buffer_chapter *rbc_handle, uint8_t *output_addr, uint32_t max_len, uint32_t *actual_Length)
{
    if(rbc_handle->head_chapter_length == 0)
        return RB_ERR_EMPTY ;
    if(rbc_handle->head_chapter_length > max_len)
    {
        if(actual_Length != NULL)
            *actual_Length = rbc_handle->head_chapter_length ;
        return RB_ERR_TOO_SMALL ;//不消费，可扩大缓冲区后重试
    }
    if(RB_Read_String(&(rbc_handle->base_handle), output_addr, rbc_handle->head_chapter_length) != RB_OK)
        return RB_ERR_EMPTY ;//防御：读取失败，保持状态不推进
    if(actual_Length != NULL)
        *actual_Length = rbc_handle->head_chapter_length ;
    return rbc_advance_head(rbc_handle) ;
}

static rb_status_t rbc_delete_core(ring_buffer_chapter *rbc_handle, uint32_t chapter_number)
{
    if(chapter_number == 0)
        return RB_OK ;//删 0 条视为无操作，与基础版口径一致
    if(RBC_Get_Chapter_Number(rbc_handle) < chapter_number)
        return RB_ERR_EMPTY ;
    uint32_t num = rbc_handle->head_chapter_length;
    //累加待删分段的长度，同时释放分段环记录
    for(uint32_t i=0; i<chapter_number - 1; i++)
    {
        uint32_t buffer32 = 0 ;
        if(RB_Read_String(&(rbc_handle->chapter_handle), (uint8_t *)&buffer32, 4) != RB_OK)
            return RB_ERR_EMPTY ;//防御：读取失败，中止删除
        num += buffer32 ;
    }
    if(RB_Delete(&(rbc_handle->base_handle), num) != RB_OK)
        return RB_ERR_EMPTY ;//防御：删除失败，中止
    return rbc_advance_head(rbc_handle) ;
}

//公开接口：参数校验 + 临界区 + 委托核心

rb_status_t RBC_Init(ring_buffer_chapter *rbc_handle,\
                uint8_t *base_buffer_addr, uint32_t base_buffer_size,\
                uint32_t *chapter_buffer_addr, uint32_t chapter_buffer_size)
{
    if(rbc_handle == NULL || base_buffer_addr == NULL || chapter_buffer_addr == NULL)
        return RB_ERR_PARAM ;
    return rbc_init_core(rbc_handle, base_buffer_addr, base_buffer_size, chapter_buffer_addr, chapter_buffer_size) ;
}

rb_status_t RBC_Clear(ring_buffer_chapter *rbc_handle)
{
    uint32_t key ;
    rb_status_t ret ;
    if(rbc_handle == NULL)
        return RB_ERR_PARAM ;
    key = RB_CRITICAL_ENTER() ;
    ret = rbc_clear_core(rbc_handle) ;
    RB_CRITICAL_EXIT(key) ;
    return ret ;
}

rb_status_t RBC_Write_Byte(ring_buffer_chapter *rbc_handle, uint8_t data)
{
    uint32_t key ;
    rb_status_t ret ;
    if(rbc_handle == NULL)
        return RB_ERR_PARAM ;
    key = RB_CRITICAL_ENTER() ;
    ret = rbc_write_string_core(rbc_handle, &data, 1u) ;
    RB_CRITICAL_EXIT(key) ;
    return ret ;
}

rb_status_t RBC_Write_String(ring_buffer_chapter *rbc_handle, const uint8_t *input_addr, uint32_t write_Length)
{
    uint32_t key ;
    rb_status_t ret ;
    if(write_Length == 0u)//0 长度视为无操作，先于一切校验
        return RB_OK ;
    if(rbc_handle == NULL || input_addr == NULL)
        return RB_ERR_PARAM ;
    key = RB_CRITICAL_ENTER() ;
    ret = rbc_write_string_core(rbc_handle, input_addr, write_Length) ;
    RB_CRITICAL_EXIT(key) ;
    return ret ;
}

rb_status_t RBC_Ending_Chapter(ring_buffer_chapter *rbc_handle)
{
    uint32_t key ;
    rb_status_t ret ;
    if(rbc_handle == NULL)
        return RB_ERR_PARAM ;
    key = RB_CRITICAL_ENTER() ;
    ret = rbc_ending_core(rbc_handle) ;
    RB_CRITICAL_EXIT(key) ;
    return ret ;
}

rb_status_t RBC_Read_Byte(ring_buffer_chapter *rbc_handle, uint8_t *output_addr)
{
    uint32_t key ;
    rb_status_t ret ;
    if(rbc_handle == NULL || output_addr == NULL)
        return RB_ERR_PARAM ;
    key = RB_CRITICAL_ENTER() ;
    ret = rbc_read_byte_core(rbc_handle, output_addr) ;
    RB_CRITICAL_EXIT(key) ;
    return ret ;
}

rb_status_t RBC_Read_Chapter(ring_buffer_chapter *rbc_handle, uint8_t *output_addr, uint32_t *output_Length)
{
    uint32_t key ;
    rb_status_t ret ;
    if(rbc_handle == NULL || output_addr == NULL)
        return RB_ERR_PARAM ;
    key = RB_CRITICAL_ENTER() ;
    ret = rbc_read_chapter_bounded_core(rbc_handle, output_addr, 0xFFFFFFFFu, output_Length) ;
    RB_CRITICAL_EXIT(key) ;
    return ret ;
}

rb_status_t RBC_Read_Chapter_Bounded(ring_buffer_chapter *rbc_handle, uint8_t *output_addr, uint32_t max_len, uint32_t *actual_Length)
{
    uint32_t key ;
    rb_status_t ret ;
    if(rbc_handle == NULL || output_addr == NULL || actual_Length == NULL)
        return RB_ERR_PARAM ;
    key = RB_CRITICAL_ENTER() ;
    ret = rbc_read_chapter_bounded_core(rbc_handle, output_addr, max_len, actual_Length) ;
    RB_CRITICAL_EXIT(key) ;
    return ret ;
}

rb_status_t RBC_Delete(ring_buffer_chapter *rbc_handle, uint32_t Chapter_Number)
{
    uint32_t key ;
    rb_status_t ret ;
    if(rbc_handle == NULL)
        return RB_ERR_PARAM ;
    key = RB_CRITICAL_ENTER() ;
    ret = rbc_delete_core(rbc_handle, Chapter_Number) ;
    RB_CRITICAL_EXIT(key) ;
    return ret ;
}

uint32_t RBC_Get_Head_Chapter_Length(ring_buffer_chapter *rbc_handle)
{
    uint32_t len ;
    if(rbc_handle == NULL)
        return 0 ;
    uint32_t key = RB_CRITICAL_ENTER() ;
    len = rbc_handle->head_chapter_length ;
    RB_CRITICAL_EXIT(key) ;
    return len ;
}

uint32_t RBC_Get_Chapter_Number(ring_buffer_chapter *rbc_handle)
{
    uint32_t number ;
    if(rbc_handle == NULL)
        return 0 ;
    uint32_t key = RB_CRITICAL_ENTER() ;
    number = RB_Get_Length(&(rbc_handle->chapter_handle))/4 + (rbc_handle->head_chapter_length != 0) ;
    RB_CRITICAL_EXIT(key) ;
    return number ;
}

uint32_t RBC_Get_Base_Free_Size(ring_buffer_chapter *rbc_handle)
{
    if(rbc_handle == NULL)
        return 0 ;
    return RB_Get_FreeSize(&(rbc_handle->base_handle)) ;
}

uint32_t RBC_Get_Chapter_Free_Size(ring_buffer_chapter *rbc_handle)
{
    if(rbc_handle == NULL)
        return 0 ;
    return RB_Get_FreeSize(&(rbc_handle->chapter_handle))/4 ;
}
