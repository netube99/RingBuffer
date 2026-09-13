/**
 * \file ring_buffer.h
 * \brief 简易环形缓冲相关定义与声明
 * \author netube_99\netube@163.com
 * \date 2026.09.13
 * \version v0.5.0
*/
#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <stdint.h>

//返回值定义
typedef enum
{
    RB_OK = 0,          //成功
    RB_ERR_PARAM,       //参数错误：句柄/指针无效、长度或配置非法
    RB_ERR_EMPTY,       //数据不足：读取/删除/窥视的数量超过已存数据量；分段结尾时无暂存数据
    RB_ERR_FULL,        //空间不足：待写入数据量超过剩余空间
    RB_ERR_TOO_SMALL,   //（分段有界读）输出缓冲区容纳不下整个分段，未消费任何数据
} rb_status_t;

//并发契约：同一句柄的所有调用（读、写、查询）必须互斥串行化
//默认空钩子适用于单上下文；跨上下文（如中断写+主循环读）须注入真实实现，
//锁存值须支持嵌套恢复（分段接口内部会再调用基础接口），注入示例见 Readme
#ifndef RB_CRITICAL_ENTER
#define RB_CRITICAL_ENTER()   ((uint32_t)0)
#endif
#ifndef RB_CRITICAL_EXIT
#define RB_CRITICAL_EXIT(key) ((void)(key))
#endif

//环形缓冲区结构体（字段仅限库内部使用，句柄不得按值复制）
typedef struct
{
    uint32_t head ;             //操作头指针
    uint32_t tail ;             //操作尾指针
    uint32_t Length ;           //已储存的数据量
    uint8_t *array_addr ;       //缓冲区储存数组基地址
    uint32_t max_Length ;       //缓冲区最大可储存数据量
}ring_buffer;

//基础环形缓冲接口（除 Init 外每个接口内部自带临界区）
rb_status_t RB_Init(ring_buffer *rb_handle, uint8_t *buffer_addr ,uint32_t buffer_size);                   //初始化基础环形缓冲区
rb_status_t RB_Clear(ring_buffer *rb_handle);                                                             //清空缓冲区，丢弃全部已存数据
rb_status_t RB_Write_Byte(ring_buffer *rb_handle, uint8_t data);                                          //向缓冲区尾指针写一个字节
rb_status_t RB_Write_String(ring_buffer *rb_handle, const uint8_t *input_addr, uint32_t write_Length);     //向缓冲区尾指针写指定长度数据（全有全无）
rb_status_t RB_Read_Byte(ring_buffer *rb_handle, uint8_t *output_addr);                                   //从缓冲区头指针读一个字节
rb_status_t RB_Read_String(ring_buffer *rb_handle, uint8_t *output_addr, uint32_t read_Length);            //从缓冲区头指针读指定长度数据（全有全无）
rb_status_t RB_Peek_Byte(ring_buffer *rb_handle, uint32_t offset, uint8_t *output_addr);                   //窥视缓冲区第 offset 字节，不消费
rb_status_t RB_Peek_String(ring_buffer *rb_handle, uint8_t *output_addr, uint32_t max_len, uint32_t *copied_Length); //窥视最多 max_len 字节，不消费
rb_status_t RB_Delete(ring_buffer *rb_handle, uint32_t Length);                                            //从头指针开始删除指定长度的数据
uint32_t RB_Get_Length(ring_buffer *rb_handle);                                                            //获取缓冲区里已储存的数据长度（句柄无效返回 0）
uint32_t RB_Get_FreeSize(ring_buffer *rb_handle);                                                          //获取缓冲区可用储存空间（句柄无效返回 0）
uint32_t RB_Get_Capacity(ring_buffer *rb_handle);                                                          //获取缓冲区总容量（句柄无效返回 0）

#endif/*RING_BUFFER_H*/
