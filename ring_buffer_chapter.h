/**
 * \file ring_buffer_chapter.h
 * \brief 简易分段环形缓冲相关定义与声明
 * \author netube_99\netube@163.com
 * \date 2026.09.13
 * \version v0.5.0
*/

#ifndef RING_BUFFER_CHAPTER_H
#define RING_BUFFER_CHAPTER_H

#include "ring_buffer.h"

//分段环形缓冲区结构体（字段仅限库内部使用，句柄不得按值复制）
typedef struct
{
    ring_buffer base_handle ;       //数据储存环形缓冲区句柄
    ring_buffer chapter_handle ;    //分段记录环形缓冲区句柄
    uint32_t head_chapter_length;   //当前头分段可读字节数
    uint32_t tail_chapter_length;   //当前尾分段暂存字节计数
    uint8_t init_flag;              //初始化完成标志位
}ring_buffer_chapter;

//分段环形缓冲接口
//常规的满/空/参数拒绝不破坏内部状态；出现无法解释的错误时，
//唯一安全的恢复动作是 RBC_Clear 或重新 RBC_Init（仅限已初始化的句柄）
rb_status_t RBC_Init(ring_buffer_chapter *rbc_handle,\
                uint8_t *base_buffer_addr, uint32_t base_buffer_size,\
                uint32_t *chapter_buffer_addr, uint32_t chapter_buffer_size);                               //初始化，分段环大小单位为字节，须为 4 的倍数且不小于 4
rb_status_t RBC_Clear(ring_buffer_chapter *rbc_handle);                                                     //清空全部数据与分段记录，恢复到初始化后的状态
rb_status_t RBC_Write_Byte(ring_buffer_chapter *rbc_handle, uint8_t data);                                  //向尾分段里写一个字节
rb_status_t RBC_Write_String(ring_buffer_chapter *rbc_handle, const uint8_t *input_addr, uint32_t write_Length); //向尾分段里写指定长度数据（全有全无）
rb_status_t RBC_Ending_Chapter(ring_buffer_chapter *rbc_handle);                                            //分段结尾，完成一次分段记录
rb_status_t RBC_Read_Byte(ring_buffer_chapter *rbc_handle, uint8_t *output_addr);                           //从头分段读取一个字节
rb_status_t RBC_Read_Chapter(ring_buffer_chapter *rbc_handle, uint8_t *output_addr, uint32_t *output_Length);   //读取整个头分段（缓冲区须足够容纳，可先查 RBC_Get_Head_Chapter_Length；output_Length 可为 NULL）
rb_status_t RBC_Read_Chapter_Bounded(ring_buffer_chapter *rbc_handle, uint8_t *output_addr, uint32_t max_len, uint32_t *actual_Length); //有界读取：放不下返回 RB_ERR_TOO_SMALL 且不消费（*actual_Length 报完整分段长度）
rb_status_t RBC_Delete(ring_buffer_chapter *rbc_handle, uint32_t Chapter_Number);                            //从头分段开始删除指定数量的分段
uint32_t RBC_Get_Head_Chapter_Length(ring_buffer_chapter *rbc_handle);                                       //获取当前头分段的可读长度
uint32_t RBC_Get_Chapter_Number(ring_buffer_chapter *rbc_handle);                                            //获取当前已记录的分段数量
uint32_t RBC_Get_Base_Free_Size(ring_buffer_chapter *rbc_handle);                                            //获取数据环剩余可用空间
uint32_t RBC_Get_Chapter_Free_Size(ring_buffer_chapter *rbc_handle);                                         //获取分段环剩余记录槽位

#endif/*RING_BUFFER_CHAPTER_H*/
