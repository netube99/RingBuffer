/**
 * \file ring_buffer.h
 * \brief 简易环形缓冲相关定义与声明
 * \author netube_99\netube@163.com
 * \date 2021.01.19
 * \version V1.0
*/

#ifndef _RING_BUFFER_H_
#define _RING_BUFFER_H_

#include <stdint.h>
#include <string.h>

#define RING_BUFFER_ARRAY_SUCCESS     0x01
#define RING_BUFFER_ARRAY_ERROR       0x00

//定义一个环形缓冲区结构体
typedef struct
{
    uint32_t head ;//操作指针头
    uint32_t tail ;//操作指针尾
    uint32_t lenght ;//已储存的数据量
    uint8_t *array_addr ;//缓冲区储存数组基地址
    uint32_t max_lenght ;//缓冲区最大可储存数据量
}ring_buffer;

uint8_t Ring_Buffer_Init(ring_buffer *ring_buffer_handle, uint8_t *buffer_addr ,uint32_t buffer_size);//初始化新缓冲区
void Ring_Buffer_Reset(ring_buffer *ring_buffer_handle);//清空缓冲区里的数据
uint8_t Ring_Buffer_Write_Byte(ring_buffer *ring_buffer_handle, uint8_t data);//向缓冲区里写一个字节
uint8_t Ring_Buffer_Read_Byte(ring_buffer *ring_buffer_handle);//从缓冲区读取一个字节
uint8_t Ring_Buffer_Write_String(ring_buffer *ring_buffer_handle, void *input_addr, uint32_t write_lenght);//向缓冲区里写一段数据
uint8_t Ring_Buffer_Read_String(ring_buffer *ring_buffer_handle, uint8_t *output_addr, uint32_t read_lenght);//从缓冲区读取一段数据
uint32_t Ring_Buffer_Get_Lenght(ring_buffer *ring_buffer_handle);//获取缓冲区里已储存的数据长度
uint32_t Ring_Buffer_Get_Free_Size(ring_buffer *ring_buffer_handle);//获取缓冲区可用储存空间

#endif