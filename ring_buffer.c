/**
 * \file ring_buffer.c
 * \brief 简易环形缓冲的实现
 * \author netube_99\netube@163.com
 * \date 2021.06.29
 * \version v1.3.2
*/

#include "ring_buffer.h"

/**
 * \brief 初始化新缓冲区
 * \param[out] ring_buffer_handle: 待初始化的缓冲区结构体句柄
 * \param[in] buffer_addr: 外部定义的缓冲区数组，类型必须为 uint8_t
 * \param[in] buffer_size: 外部定义的缓冲区数组空间
 * \return 返回缓冲区初始化的结果
 *      \arg RING_BUFFER_SUCCESS: 初始化成功
 *      \arg RING_BUFFER_ERROR: 初始化失败
*/
uint8_t Ring_Buffer_Init(ring_buffer *ring_buffer_handle, uint8_t *buffer_addr ,uint32_t buffer_size)
{
    ring_buffer_handle->head = 0 ;                  //复位头指针
    ring_buffer_handle->tail = 0 ;                  //复位尾指针
    ring_buffer_handle->Length = 0 ;                //复位已存储数据长度
    ring_buffer_handle->array_addr = buffer_addr ;  //缓冲区储存数组基地址
    ring_buffer_handle->max_Length = buffer_size ;  //缓冲区最大可储存数据量
    #if(RING_BUFFER_USE_CHAPTER)
    ring_buffer_handle->chapter_number = 0 ;        //分段记录数量为0
    ring_buffer_handle->chapter_standby = 0 ;       //分段长度记录数组待机下标
    ring_buffer_handle->chapter_active = 0 ;        //分段长度记录数组活动下标
    #endif
    if(ring_buffer_handle->max_Length < 2)          //缓冲区数组必须有两个元素以上
        return RING_BUFFER_ERROR ;                  //缓冲区数组过小，队列初始化失败
    else
        return RING_BUFFER_SUCCESS ;                //缓冲区初始化成功
}

/**
 * \brief 从头指针开始删除指定长度的数据
 * \param[out] ring_buffer_handle: 缓冲区结构体句柄
 * \param[in] Length: 要删除的长度
 * \return 返回删除指定长度数据结果
 *      \arg RING_BUFFER_SUCCESS: 删除成功
 *      \arg RING_BUFFER_ERROR: 删除失败
*/
uint8_t Ring_Buffer_Delete(ring_buffer *ring_buffer_handle, uint32_t Length)
{
    if(ring_buffer_handle->Length < Length)
        return RING_BUFFER_ERROR ;//已储存的数据量小于需删除的数据量
    else
    {
        if((ring_buffer_handle->head + Length) >= ring_buffer_handle->max_Length)
            ring_buffer_handle->head = Length - (ring_buffer_handle->max_Length - ring_buffer_handle->head);
        else
            ring_buffer_handle->head += Length ;//头指针向前推进，抛弃数据
        ring_buffer_handle->Length -= Length ;//重新记录有效数据长度
        #if(RING_BUFFER_USE_CHAPTER)
        uint32_t len = Length ;
        while(len)
        {
            
        }
        #endif
        return RING_BUFFER_SUCCESS ;//已储存的数据量小于需删除的数据量
    }
}

/**
 * \brief 向缓冲区尾部写一个字节
 * \param[out] ring_buffer_handle: 缓冲区结构体句柄
 * \param[in] data: 要写入的字节
 * \return 返回缓冲区写字节的结果
 *      \arg RING_BUFFER_SUCCESS: 写入成功
 *      \arg RING_BUFFER_ERROR: 写入失败
*/
uint8_t Ring_Buffer_Write_Byte(ring_buffer *ring_buffer_handle, uint8_t data)
{
    //缓冲区数组已满，产生覆盖错误
    if(ring_buffer_handle->Length == (ring_buffer_handle->max_Length))
        return RING_BUFFER_ERROR ;
    else
    {
        *(ring_buffer_handle->array_addr + ring_buffer_handle->tail) = data;//基地址+偏移量，存放数据
        ring_buffer_handle->Length ++ ;//数据量计数+1
        ring_buffer_handle->tail ++ ;//尾指针后移
    }
    //如果尾指针超越了数组末尾，尾指针指向缓冲区数组开头，形成闭环
    if(ring_buffer_handle->tail > (ring_buffer_handle->max_Length - 1))
        ring_buffer_handle->tail = 0 ;
	return RING_BUFFER_SUCCESS ;
}

/**
 * \brief 从缓冲区头指针读取一个字节
 * \param[in] ring_buffer_handle: 缓冲区结构体句柄
 * \param[out] output_addr: 读取的字节保存地址
 * \return 返回读取状态
 *      \arg RING_BUFFER_SUCCESS: 读取成功
 *      \arg RING_BUFFER_ERROR: 读取失败
*/
uint8_t Ring_Buffer_Read_Byte(ring_buffer *ring_buffer_handle, uint8_t *output_addr)
{
    if (ring_buffer_handle->Length != 0)//有数据未读出
    {
        *output_addr = *(ring_buffer_handle->array_addr + ring_buffer_handle->head);//读取数据
        ring_buffer_handle->head ++ ;
        ring_buffer_handle->Length -- ;//数据量计数-1
        //如果头指针超越了数组末尾，头指针指向数组开头，形成闭环
        if(ring_buffer_handle->head > (ring_buffer_handle->max_Length - 1))
            ring_buffer_handle->head = 0 ;
        return RING_BUFFER_SUCCESS ;
    }
    return RING_BUFFER_ERROR ;
}

/**
 * \brief 向缓冲区尾部写指定长度的数据
 * \param[out] ring_buffer_handle: 缓冲区结构体句柄
 * \param[out] input_addr: 待写入数据的基地址
 * \param[in] write_Length: 要写入的字节数
 * \return 返回缓冲区尾部写指定长度字节的结果
 *      \arg RING_BUFFER_SUCCESS: 写入成功
 *      \arg RING_BUFFER_ERROR: 写入失败
*/
uint8_t Ring_Buffer_Write_String(ring_buffer *ring_buffer_handle, uint8_t *input_addr, uint32_t write_Length)
{
    //如果不够存储空间存放新数据,返回错误
    if((ring_buffer_handle->Length + write_Length) > (ring_buffer_handle->max_Length))
        return RING_BUFFER_ERROR ;
    else
    {
        //设置两次写入长度
        uint32_t write_size_a, write_size_b ;
        //如果顺序可用长度小于需写入的长度，需要将数据拆成两次分别写入
        if((ring_buffer_handle->max_Length - ring_buffer_handle->tail) < write_Length)
        {
            write_size_a = ring_buffer_handle->max_Length - ring_buffer_handle->tail ;//从尾指针开始写到储存数组末尾
            write_size_b = write_Length - write_size_a ;//从储存数组开头写数据
        }
        else//如果顺序可用长度大于或等于需写入的长度，则只需要写入一次
        {
            write_size_a = write_Length ;//从尾指针开始写到储存数组末尾
            write_size_b = 0 ;//无需从储存数组开头写数据
        }
        //开始写入数据
        if(write_size_b != 0)//需要写入两次
        {
            //分别拷贝a、b段数据到储存数组中
            memcpy(ring_buffer_handle->array_addr + ring_buffer_handle->tail, input_addr, write_size_a);
            memcpy(ring_buffer_handle->array_addr, input_addr + write_size_a, write_size_b);
            ring_buffer_handle->Length += write_Length ;//记录新存储了多少数据量
            ring_buffer_handle->tail = write_size_b ;//重新定位尾指针位置
        }
        else//只需写入一次
        {
            memcpy(ring_buffer_handle->array_addr + ring_buffer_handle->tail, input_addr, write_size_a);
            ring_buffer_handle->Length += write_Length ;//记录新存储了多少数据量
            ring_buffer_handle->tail += write_size_a ;//重新定位尾指针位置
            if(ring_buffer_handle->tail == ring_buffer_handle->max_Length)
                ring_buffer_handle->tail = 0 ;//如果写入数据后尾指针刚好写到数组尾部，则回到开头，防止越位
        }
        return RING_BUFFER_SUCCESS ;
    }
}

/**
 * \brief 向缓冲区头部读指定长度的数据，保存到指定的地址
 * \param[in] ring_buffer_handle: 缓冲区结构体句柄
 * \param[out] output_addr: 读取的数据保存地址
 * \param[in] read_Length: 要读取的字节数
 * \return 返回缓冲区头部读指定长度字节的结果
 *      \arg RING_BUFFER_SUCCESS: 读取成功
 *      \arg RING_BUFFER_ERROR: 读取失败
*/
uint8_t Ring_Buffer_Read_String(ring_buffer *ring_buffer_handle, uint8_t *output_addr, uint32_t read_Length)
{
    if(read_Length > ring_buffer_handle->Length)
        return RING_BUFFER_ERROR ;
    else
    {
        uint32_t Read_size_a, Read_size_b ;
        if(read_Length > (ring_buffer_handle->max_Length - ring_buffer_handle->head))
        {
            Read_size_a = ring_buffer_handle->max_Length - ring_buffer_handle->head ;
            Read_size_b = read_Length - Read_size_a ;
        }
        else
        {
            Read_size_a = read_Length ;
            Read_size_b = 0 ;
        }
        if(Read_size_b != 0)//需要读取两次
        {
            memcpy(output_addr, ring_buffer_handle->array_addr + ring_buffer_handle->head, Read_size_a);
            memcpy(output_addr + Read_size_a, ring_buffer_handle->array_addr, Read_size_b);
            ring_buffer_handle->Length -= read_Length ;//记录剩余数据量
            ring_buffer_handle->head = Read_size_b ;//重新定位头指针位置
        }
        else
        {
            memcpy(output_addr, ring_buffer_handle->array_addr + ring_buffer_handle->head, Read_size_a);
            ring_buffer_handle->Length -= read_Length ;//记录剩余数据量
            ring_buffer_handle->head += Read_size_a ;//重新定位头指针位置
            if(ring_buffer_handle->head == ring_buffer_handle->max_Length)
                ring_buffer_handle->head = 0 ;//如果读取数据后头指针刚好写到数组尾部，则回到开头，防止越位
        }
        return RING_BUFFER_SUCCESS ;
    }
}

/**
 * \brief 获取缓冲区里已储存的数据长度
 * \param[in] ring_buffer_handle: 缓冲区结构体句柄
 * \return 返回缓冲区里已储存的数据长度
*/
uint32_t Ring_Buffer_Get_Length(ring_buffer *ring_buffer_handle)
{
    return ring_buffer_handle->Length ;
}

/**
 * \brief 获取缓冲区可用储存空间
 * \param[in] ring_buffer_handle: 缓冲区结构体句柄
 * \return 返回缓冲区可用储存空间
*/
uint32_t Ring_Buffer_Get_FreeSize(ring_buffer *ring_buffer_handle)
{
    return (ring_buffer_handle->max_Length - ring_buffer_handle->Length) ;
}

#if(RING_BUFFER_USE_CHAPTER)
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
#endif