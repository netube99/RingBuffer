/**
 * \file ring_buffer_chapter.c
 * \brief 简易分段环形缓冲的实现
 * \author netube_99\netube@163.com
 * \date 2022.09.25
 * \version v0.4.0
*/

#include <stdint.h>
#include "ring_buffer_chapter.h"

/**
 * \brief 初始化带分段功能的环形缓冲区
 * \param[out] rbc_handle: 待初始化的缓冲区结构体句柄
 * \param[in] base_buffer_addr: 数据环缓冲区数组基地址
 * \param[in] base_buffer_size: 数据环缓冲区数组空间大小
 * \param[in] chapter_buffer_addr: 分段环缓冲区数组基地址
 * \param[in] chapter_buffer_size: 分段环缓冲区数组空间大小
 * \return 返回缓冲区初始化的结果
 *      \arg RING_BUFFER_CHAPTER_SUCCESS: 初始化成功
 *      \arg RING_BUFFER_CHAPTER_ERROR: 初始化失败
*/
uint8_t RBC_Init(ring_buffer_chapter *rbc_handle,\
                uint8_t *base_buffer_addr, uint32_t base_buffer_size,\
                uint32_t *chapter_buffer_addr, uint32_t chapter_buffer_size)
{
    if(!RB_Init(&(rbc_handle->base_handle), base_buffer_addr, base_buffer_size))
        return RING_BUFFER_CHAPTER_ERROR ;
    if(!RB_Init(&(rbc_handle->chapter_handle), (uint8_t *)chapter_buffer_addr, chapter_buffer_size))
        return RING_BUFFER_CHAPTER_ERROR ;
    rbc_handle->head_chapter_length = 0 ;
    rbc_handle->tail_chapter_length = 0 ;
    rbc_handle->init_flag = 1 ;
    return RING_BUFFER_CHAPTER_SUCCESS ;
}

/**
 * \brief 向当前尾分段写入一个字节
 * \param[out] rbc_handle: 分段版环形缓冲区结构体句柄
 * \param[in] data: 待写入的数据
 * \return 返回写入结果
 *      \arg RING_BUFFER_CHAPTER_SUCCESS: 写入成功
 *      \arg RING_BUFFER_CHAPTER_ERROR: 写入失败
*/
uint8_t RBC_Write_Byte(ring_buffer_chapter *rbc_handle, uint8_t data)
{
    if(!RBC_Get_Chapter_Free_Size(rbc_handle)) //检查分段环剩余空间是否允许新增一条分段记录
        return RING_BUFFER_CHAPTER_ERROR ;
    if(!RB_Write_Byte(&(rbc_handle->base_handle), data)) //向数据环尾指针写入一个字节
        return RING_BUFFER_CHAPTER_ERROR ;
    rbc_handle->tail_chapter_length ++ ; //尾分段暂存字节数加1
    return RING_BUFFER_CHAPTER_SUCCESS ;
}

/**
 * \brief 向当前尾分段写入指定长度数据
 * \param[out] rbc_handle: 分段版环形缓冲区结构体句柄
 * \param[in] input_addr: 待写入数据的基地址
 * \param[in] write_Length: 要写入的字节数
 * \return 返回写入结果
 *      \arg RING_BUFFER_CHAPTER_SUCCESS: 写入成功
 *      \arg RING_BUFFER_CHAPTER_ERROR: 写入失败
*/
uint8_t RBC_Write_String(ring_buffer_chapter *rbc_handle, uint8_t *input_addr, uint32_t write_Length)
{
    if(!RBC_Get_Chapter_Free_Size(rbc_handle)) //检查分段环剩余空间是否允许新增一条分段记录
        return RING_BUFFER_CHAPTER_ERROR ;
    if(!RB_Write_String(&(rbc_handle->base_handle), input_addr, write_Length)) //向数据环尾指针写入指定长度数据
        return RING_BUFFER_CHAPTER_ERROR;               
    rbc_handle->tail_chapter_length += write_Length ; //累加新增的尾分段暂存字节数
    return RING_BUFFER_CHAPTER_SUCCESS ;
}

/**
 * \brief 分段结尾，将暂存的字节计数保存为一条分段数据
 * \param[out] rbc_handle: 分段版环形缓冲区结构体句柄
 * \return 返回保存结果
 *      \arg RING_BUFFER_CHAPTER_SUCCESS: 保存成功
 *      \arg RING_BUFFER_CHAPTER_ERROR: 保存失败
*/
uint8_t RBC_Ending_Chapter(ring_buffer_chapter *rbc_handle)
{
    //如果尾分段有暂存但未结尾的数据
    if(rbc_handle->tail_chapter_length)
    {
        //将当前尾分段暂存字节计数存入分段环中
        RB_Write_String(&(rbc_handle->chapter_handle), (uint8_t *)&rbc_handle->tail_chapter_length, 4);
        //如果当前储存的是分段环的首条分段记录，则当前头分段可读字节数等于当前尾分段暂存字节计数
        if(rbc_handle->init_flag)
        {
            RB_Read_String(&(rbc_handle->chapter_handle), (uint8_t *)&rbc_handle->head_chapter_length, 4);
            rbc_handle->init_flag = 0 ;
        }
        rbc_handle->tail_chapter_length = 0 ;//当前尾分段暂存字节计数归零
        return RING_BUFFER_CHAPTER_SUCCESS ;
    }
    return RING_BUFFER_CHAPTER_ERROR ;
}

/**
 * \brief 从头分段读取一个字节
 * \param[out] rbc_handle: 分段版环形缓冲区结构体句柄
 * \param[in] output_addr: 读取的字节保存地址
 * \return 返回读取结果
 *      \arg RING_BUFFER_CHAPTER_SUCCESS: 读取成功
 *      \arg RING_BUFFER_CHAPTER_ERROR: 读取失败
*/
uint8_t RBC_Read_Byte(ring_buffer_chapter *rbc_handle, uint8_t *output_addr)
{
    if(rbc_handle->head_chapter_length)
    {
        RB_Read_Byte(&(rbc_handle->base_handle), output_addr);  //读取一个字节
        rbc_handle->head_chapter_length -- ;
        //如果当前头分段可读字节数为空
        if(!rbc_handle->head_chapter_length)
        {
            //如果还有储存的分段记录,则读取到可读字节数变量中
            if(RBC_Get_Chapter_Number(rbc_handle))
                RB_Read_String(&(rbc_handle->chapter_handle), (uint8_t *)&rbc_handle->head_chapter_length, 4);
            //如果所有分段记录都已读取，重新置位初始化完成标志位(恢复到初始化后的状态)
            else rbc_handle->init_flag = 1 ;
        }
        return RING_BUFFER_CHAPTER_SUCCESS ;
    }
    return RING_BUFFER_CHAPTER_ERROR ;
}

/**
 * \brief 读取完整的头分段数据
 * \param[out] rbc_handle: 分段版环形缓冲区结构体句柄
 * \param[in] output_addr: 读取的分段数据保存地址
 * \param[out] output_Length: 读取的分段数据长度保存地址，可为NULL
 * \return 返回读取结果
 *      \arg RING_BUFFER_CHAPTER_SUCCESS: 读取成功
 *      \arg RING_BUFFER_CHAPTER_ERROR: 读取失败
*/
uint8_t RBC_Read_Chapter(ring_buffer_chapter *rbc_handle, uint8_t *output_addr, uint32_t *output_Length)
{
    if(rbc_handle->head_chapter_length)
    {
        RB_Read_String(&(rbc_handle->base_handle), output_addr, rbc_handle->head_chapter_length);  //读取整个头分段的数据
        if(output_Length != NULL)
            *output_Length = rbc_handle->head_chapter_length ;
        rbc_handle->head_chapter_length = 0 ;
        //如果还有储存的分段记录,则读取到可读字节数变量中
        if(RBC_Get_Chapter_Number(rbc_handle))
            RB_Read_String(&(rbc_handle->chapter_handle), (uint8_t *)&rbc_handle->head_chapter_length, 4);
        //如果所有分段记录都已读取，重新置位初始化完成标志位(恢复到初始化后的状态)
        else rbc_handle->init_flag = 1 ;
        return RING_BUFFER_CHAPTER_SUCCESS ;
    }
    return RING_BUFFER_CHAPTER_ERROR ;
}

/**
 * \brief 从头分段开始删除指定数量的分段
 * \param[out] rbc_handle: 分段版环形缓冲区结构体句柄
 * \param[in] chapter_number: 需要删除的分段数量
 * \return 返回删除结果
 *      \arg RING_BUFFER_CHAPTER_SUCCESS: 删除成功
 *      \arg RING_BUFFER_CHAPTER_ERROR: 删除失败
*/
uint8_t RBC_Delete(ring_buffer_chapter *rbc_handle, uint32_t chapter_number)
{
    //检查已存分段数量是否满足参数
    if(RBC_Get_Chapter_Number(rbc_handle) >= chapter_number && chapter_number)
    {
        uint32_t num = rbc_handle->head_chapter_length;
        //从分段环读取分段的长度进行累加
        for(uint32_t i=0; i<chapter_number - 1; i++)
        {
            uint32_t buffer32 = 0 ;
            //从分段环中获取分段的长度进行累加，同时实现了分段环的空间释放
            RB_Read_String(&(rbc_handle->chapter_handle), (uint8_t *)&buffer32, 4);
            num += buffer32 ;
        }
        //从数据环删除指定长度的数据
        RB_Delete(&(rbc_handle->base_handle), num);
        rbc_handle->head_chapter_length = 0 ;
        //如果还有储存的分段记录,则读取到头分段可读字节数变量中
        if(RBC_Get_Chapter_Number(rbc_handle))
            RB_Read_String(&(rbc_handle->chapter_handle), (uint8_t *)&rbc_handle->head_chapter_length, 4);
        //如果所有分段记录都已读取，重新置位初始化完成标志位(恢复到初始化后的状态)
        else
            rbc_handle->init_flag = 1 ;
        return RING_BUFFER_CHAPTER_SUCCESS ;
    }
    else return RING_BUFFER_CHAPTER_ERROR ;
}

/**
 * \brief 获取当前头分段的可读长度
 * \param[in] rbc_handle: 分段版环形缓冲区结构体句柄
 * \return 返回当前头分段的可读长度
*/
uint32_t RBC_Get_head_Chapter_length(ring_buffer_chapter *rbc_handle)
{
    return rbc_handle->head_chapter_length ;
}

/**
 * \brief 获取当前储存的分段数量
 * \param[in] rbc_handle: 分段版环形缓冲区结构体句柄
 * \return 返回当前储存的分段数量
*/
uint32_t RBC_Get_Chapter_Number(ring_buffer_chapter *rbc_handle)
{
    uint32_t number = RB_Get_Length(&(rbc_handle->chapter_handle))/4 ;
    if(rbc_handle->head_chapter_length)
        if(!number) return 1 ;
        else return number + 1 ;
    return number ;
}

/**
 * \brief 获取数据环剩余可用空间
 * \param[in] rbc_handle: 分段版环形缓冲区结构体句柄
 * \return 返回数据环剩余可用空间
*/
uint32_t RBC_Get_Base_Free_Size(ring_buffer_chapter *rbc_handle)
{
    return RB_Get_FreeSize(&(rbc_handle->base_handle)) ;
}

/**
 * \brief 获取剩余可记录的分段数量
 * \param[in] rbc_handle: 分段版环形缓冲区结构体句柄
 * \return 返回剩余可记录的分段数量
*/
uint32_t RBC_Get_Chapter_Free_Size(ring_buffer_chapter *rbc_handle)
{
    return RB_Get_FreeSize(&(rbc_handle->chapter_handle))/4 ;
}