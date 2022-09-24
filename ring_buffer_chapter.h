/**
 * \file ring_buffer_chapter.h
 * \brief 简易分段环形缓冲相关定义与声明
 * \author netube_99\netube@163.com
 * \date 2022.09.25
 * \version v0.1
*/

#ifndef _RING_BUFFER_CHAPTER_H_
#define _RING_BUFFER_CHAPTER_H_

#include "ring_buffer.h"

//返回值定义
#define RING_BUFFER_CHAPTER_SUCCESS     0x01
#define RING_BUFFER_CHAPTER_ERROR       0x00

//环形缓冲分段结构体
typedef struct
{
    ring_buffer base_handle ;       //数据储存环形缓冲区句柄
    ring_buffer chapter_handle ;    //分段记录环形缓冲区句柄
    uint32_t head_chapter_length;   //当前头分段可读字节数
    uint32_t tail_chapter_length;   //当前尾分段暂存字节计数
    uint8_t init_flag;              //初始化完成标志位
}ring_buffer_chapter;

uint8_t RBC_Init(ring_buffer_chapter *rbc_handle,\
                uint8_t *base_buffer_addr, uint32_t base_buffer_size,\
                uint8_t *chapter_buffer_addr, uint32_t chapter_buffer_size);                                //初始化带分段功能的环形缓冲区
uint8_t RBC_Write_Byte(ring_buffer_chapter *rbc_handle, uint8_t data);                                      //向尾分段里写一个字节
uint8_t RBC_Write_String(ring_buffer_chapter *rbc_handle, uint8_t *input_addr, uint32_t write_Length);      //向尾分段里写指定长度数据
uint8_t RBC_Ending_Chapter(ring_buffer_chapter *rbc_handle);                                                //分段结尾，完成一次分段记录
uint8_t RBC_Read_Byte(ring_buffer_chapter *rbc_handle, uint8_t *output_addr);                               //从头分段读取一个字节
uint8_t RBC_Read_Chapter(ring_buffer_chapter *rbc_handle, uint8_t *output_addr, uint32_t *output_Length);   //读取整个头分段
uint8_t RBC_Delete(ring_buffer_chapter *rbc_handle, uint32_t Chapter_Number);                               //从头分段开始删除指定数量的分段
uint32_t RBC_Get_head_Chapter_length(ring_buffer_chapter *rbc_handle);                                      //获取当前头分段的长度
uint32_t RBC_Get_Chapter_Number(ring_buffer_chapter *rbc_handle);                                           //获取当前已记录的分段数量
uint32_t RBC_Get_Base_Free_Size(ring_buffer_chapter *rbc_handle);                                           //获取数据环剩余可用空间
uint32_t RBC_Get_Chapter_Free_Size(ring_buffer_chapter *rbc_handle);                                        //获取剩余可记录的分段数量

#endif