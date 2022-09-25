/**
 * \file ring_buffer.h
 * \brief 简易环形缓冲相关定义与声明
 * \author netube_99\netube@163.com
 * \date 2022.08.20
 * \version v0.4.0
*/
#ifndef _RING_BUFFER_H_
#define _RING_BUFFER_H_

//返回值定义
#define RING_BUFFER_SUCCESS     0x01
#define RING_BUFFER_ERROR       0x00

//环形缓冲区结构体
typedef struct
{
    uint32_t head ;             //操作头指针
    uint32_t tail ;             //操作尾指针
    uint32_t Length ;           //已储存的数据量
    uint8_t *array_addr ;       //缓冲区储存数组基地址
    uint32_t max_Length ;       //缓冲区最大可储存数据量
}ring_buffer;

uint8_t RB_Init(ring_buffer *rb_handle, uint8_t *buffer_addr ,uint32_t buffer_size);               //初始化基础环形缓冲区
uint8_t RB_Delete(ring_buffer *rb_handle, uint32_t Length);                                        //从头指针开始删除指定长度的数据
uint8_t RB_Write_Byte(ring_buffer *rb_handle, uint8_t data);                                       //向缓冲区尾指针写一个字节
uint8_t RB_Write_String(ring_buffer *rb_handle, uint8_t *input_addr, uint32_t write_Length);       //向缓冲区尾指针写指定长度数据
uint8_t RB_Read_Byte(ring_buffer *rb_handle, uint8_t *output_addr);                                //从缓冲区头指针读一个字节
uint8_t RB_Read_String(ring_buffer *rb_handle, uint8_t *output_addr, uint32_t read_Length);        //从缓冲区头指针读指定长度数据
uint32_t RB_Get_Length(ring_buffer *rb_handle);                                                    //获取缓冲区里已储存的数据长度
uint32_t RB_Get_FreeSize(ring_buffer *rb_handle);                                                  //获取缓冲区可用储存空间

#endif//#ifndef _RING_BUFFER_H_