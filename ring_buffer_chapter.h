#ifndef _RING_BUFFER_CHAPTER_H_
#define _RING_BUFFER_CHAPTER_H_

#include <stdint.h>

//最大分段记录量，根据具体使用情况修改
#define RING_BUFFER_MAX_CHAPTER_NUMBER      16

typedef struct
{
    uint32_t chapter_buffer[RING_BUFFER_MAX_CHAPTER_NUMBER]; //分段长度记录
    uint32_t chapter_standby;   //待机分段（最先输出的分段记录）的缓存下标
    uint32_t chapter_active;    //活动分段（最后输入且未结束的分段记录）的缓存下标
    uint32_t chapter_number;    //已记录的分段数量
}ring_buffer_chapter;

uint8_t Ring_Buffer_Chapter_Set_Active_End(ring_buffer *ring_buffer_handle);                                        //结束保存一个分段记录
uint32_t Ring_Buffer_Chapter_Get_Count(ring_buffer *ring_buffer_handle);                                            //获取已记录的分段数量
uint32_t Ring_Buffer_Chapter_Get_Standby_Length(ring_buffer *ring_buffer_handle);                                   //获取待取出的分段内容长度
uint8_t Ring_Buffer_Chapter_Read_String(ring_buffer *ring_buffer_handle, uint8_t *output_addr);                     //读取一段完整的分段内容

#endif