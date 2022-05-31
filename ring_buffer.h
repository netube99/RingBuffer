/**
 * \file ring_buffer.h
 * \brief 简易环形缓冲相关定义与声明
 * \author netube_99\netube@163.com
 * \date 2021.06.29
 * \version v1.3.2
*/

#ifndef _RING_BUFFER_H_
#define _RING_BUFFER_H_

#include <stdint.h>
#include <string.h>

#define RING_BUFFER_SUCCESS     0x01
#define RING_BUFFER_ERROR       0x00

//使用环形缓冲区分段功能
#define RING_BUFFER_USE_CHAPTER             1

//最大分段记录量，根据具体使用情况修改
#define RING_BUFFER_MAX_CHAPTER_NUMBER      16

//环形缓冲区结构体
typedef struct
{
    //基础功能
    uint32_t head ;             //操作头指针
    uint32_t tail ;             //操作尾指针
    uint32_t Length ;           //已储存的数据量
    uint8_t *array_addr ;       //缓冲区储存数组基地址
    uint32_t max_Length ;       //缓冲区最大可储存数据量
    //分段记录功能
    #if(RING_BUFFER_USE_CHAPTER)
    uint32_t chapter_buffer[RING_BUFFER_MAX_CHAPTER_NUMBER]; //分段长度记录
    uint32_t chapter_standby;   //待机分段（最先输出的分段记录）的缓存下标
    uint32_t chapter_active;    //活动分段（最后输入且未结束的分段记录）的缓存下标
    uint32_t chapter_number;    //已记录的分段数量
    #endif
}ring_buffer;

//基础功能
uint8_t Ring_Buffer_Init(ring_buffer *ring_buffer_handle, uint8_t *buffer_addr ,uint32_t buffer_size);              //初始化新缓冲区
uint8_t Ring_Buffer_Delete(ring_buffer *ring_buffer_handle, uint32_t Length);                                       //从头指针开始删除指定长度的数据
uint8_t Ring_Buffer_Write_Byte(ring_buffer *ring_buffer_handle, uint8_t data);                                      //向缓冲区里写一个字节
uint8_t Ring_Buffer_Read_Byte(ring_buffer *ring_buffer_handle, uint8_t *output_addr);                               //从缓冲区读取一个字节
uint8_t Ring_Buffer_Write_String(ring_buffer *ring_buffer_handle, uint8_t *input_addr, uint32_t write_Length);      //向缓冲区里写指定长度数据
uint8_t Ring_Buffer_Read_String(ring_buffer *ring_buffer_handle, uint8_t *output_addr, uint32_t read_Length);       //从缓冲区读取指定长度数据
uint32_t Ring_Buffer_Get_Length(ring_buffer *ring_buffer_handle);                                                   //获取缓冲区里已储存的数据长度
uint32_t Ring_Buffer_Get_FreeSize(ring_buffer *ring_buffer_handle);                                                 //获取缓冲区可用储存空间
//分段记录功能
#if(RING_BUFFER_USE_CHAPTER)
uint8_t Ring_Buffer_Chapter_Set_Active_End(ring_buffer *ring_buffer_handle);                                        //结束保存一个分段记录
uint32_t Ring_Buffer_Chapter_Get_Count(ring_buffer *ring_buffer_handle);                                            //获取已记录的分段数量
uint32_t Ring_Buffer_Chapter_Get_Standby_Length(ring_buffer *ring_buffer_handle);                                   //获取待取出的分段内容长度
uint8_t Ring_Buffer_Chapter_Read_String(ring_buffer *ring_buffer_handle, uint8_t *output_addr);                     //读取一段完整的分段内容
#endif//#if(RING_BUFFER_USE_CHAPTER)

#endif//#ifndef _RING_BUFFER_H_