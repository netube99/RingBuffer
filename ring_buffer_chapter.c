#include "ring_buffer.h"
#include <stdint.h>

/**
 * \brief 结束记录活动分段
 * \param[in] ring_buffer_handle: 缓冲区结构体句柄
 * \return 分段记录结果
 *      \arg RING_BUFFER_SUCCESS: 分段成功
 *      \arg RING_BUFFER_ERROR: 分段失败(分段记录数量已达到上限)
*/
uint8_t Ring_Buffer_Chapter_Set_Active_End(ring_buffer *ring_buffer_handle)
{
    if(ring_buffer_handle->chapter_number < RING_BUFFER_MAX_CHAPTER_NUMBER)
    {
        ring_buffer_handle->chapter_number ++ ;
        ring_buffer_handle->chapter_active ++ ;
        if(ring_buffer_handle->chapter_active >= RING_BUFFER_MAX_CHAPTER_NUMBER)
            ring_buffer_handle->chapter_active = 0 ;
        return RING_BUFFER_SUCCESS ;
    }
    return RING_BUFFER_ERROR ;
}

/**
 * \brief 获取当前已储存的分段数量
 * \param[in] ring_buffer_handle: 缓冲区结构体句柄
 * \return 已储存的分段数量
*/
uint32_t Ring_Buffer_Chapter_Get_Count(ring_buffer *ring_buffer_handle, uint8_t *output_addr)
{
    return ring_buffer_handle->chapter_number ;
}

/**
 * \brief 获取待取出的分段内容长度
 * \param[in] ring_buffer_handle: 缓冲区结构体句柄
 * \return 待取出的分段内容长度
*/
uint32_t Ring_Buffer_Chapter_Get_Standby_Length(ring_buffer *ring_buffer_handle)
{
    return ring_buffer_handle->chapter_buffer[ring_buffer_handle->chapter_standby];
}

/**
 * \brief 向缓冲区头部读字节，保存到指定的地址
 * \param[in] ring_buffer_handle: 缓冲区结构体句柄
 * \param[out] output_addr: 读取的数据保存地址
 * \param[in] read_Length: 要读取的字节数
 * \return 返回缓冲区头部读分段长度字节的结果
 *      \arg RING_BUFFER_SUCCESS: 读取成功
 *      \arg RING_BUFFER_ERROR: 读取失败(没有可读取的分段)
*/
uint8_t Ring_Buffer_Chapter_Read_String(ring_buffer *ring_buffer_handle, uint8_t *output_addr)
{
    if(Ring_Buffer_Get_Chapter_Number(ring_buffer_handle))
    {
        Ring_Buffer_Read_String(ring_buffer_handle, output_addr, \
            ring_buffer_handle->chapter_buffer[ring_buffer_handle->chapter_standby]);
        ring_buffer_handle->chapter_number -- ;     //已记录的分段数量 -1
        ring_buffer_handle->chapter_standby ++ ;    //待读取的分段下标后移
        if(ring_buffer_handle->chapter_standby >= RING_BUFFER_MAX_CHAPTER_NUMBER)
            ring_buffer_handle->chapter_standby = 0 ;
        return RING_BUFFER_SUCCESS ;
    }
    return RING_BUFFER_ERROR ;
}
#endif

/**
 * \brief 向缓冲区头部读分段长度的数据，保存到指定的地址
 * \param[in] ring_buffer_handle: 缓冲区结构体句柄
 * \param[out] output_addr: 读取的数据保存地址
 * \param[in] read_Length: 要读取的字节数
 * \return 返回缓冲区头部读分段长度字节的结果
 *      \arg RING_BUFFER_SUCCESS: 读取成功
 *      \arg RING_BUFFER_ERROR: 读取失败(没有可读取的分段)
*/
uint8_t Ring_Buffer_Chapter_Read_String(ring_buffer *ring_buffer_handle, uint8_t *output_addr)
{
    if(Ring_Buffer_Get_Chapter_Number(ring_buffer_handle))
    {
        Ring_Buffer_Read_String(ring_buffer_handle, output_addr, \
            ring_buffer_handle->chapter_buffer[ring_buffer_handle->chapter_standby]);
        ring_buffer_handle->chapter_number -- ;     //已记录的分段数量 -1
        ring_buffer_handle->chapter_standby ++ ;    //待读取的分段下标后移
        if(ring_buffer_handle->chapter_standby >= RING_BUFFER_MAX_CHAPTER_NUMBER)
            ring_buffer_handle->chapter_standby = 0 ;
        return RING_BUFFER_SUCCESS ;
    }
    return RING_BUFFER_ERROR ;
}