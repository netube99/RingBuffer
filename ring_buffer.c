/**
 * \file ring_buffer.c
 * \brief 简易环形缓冲的实现
 * \details 可作为单片机串口的环形缓冲/软FIFO，提升串口数据传输能力；
 * 可以向环形缓冲区快速的储存/读取数据，并且无需手动记录数据接收量即可准确的读出所有数据；
 * 无需手动清空数据缓存区，只要将上次接收的数据读取出来，缓冲区即可准备好接收下一段数据；
 * 节省了手动清空普通缓存区的时间，能够提升串口程序的运行效率；
 * \author netube_99\netube@163.com
 * \date 2021.01.23
 * \version V1.1
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
    ring_buffer_handle->head = 0 ;//复位头指针
    ring_buffer_handle->tail = 0 ;//复位尾指针
    ring_buffer_handle->lenght = 0 ;//复位已存储数据长度
    ring_buffer_handle->array_addr = buffer_addr ;//缓冲区储存数组基地址
    ring_buffer_handle->max_lenght = buffer_size ;//缓冲区最大可储存数据量
    if(ring_buffer_handle->max_lenght < 2)//缓冲区数组必须有两个元素以上
        return RING_BUFFER_ERROR ;//缓冲区数组过小，队列初始化失败
    else
        return RING_BUFFER_SUCCESS ;//缓冲区初始化成功
}

/**
 * \brief 清空缓冲区里的数据
 * \param[out] ring_buffer_handle: 缓冲区结构体句柄
 * \return 无
*/
void Ring_Buffer_Reset(ring_buffer *ring_buffer_handle)
{
    ring_buffer_handle->head = 0 ;//复位头指针
    ring_buffer_handle->tail = 0 ;//复位尾指针
    ring_buffer_handle->lenght = 0 ;//复位已存储数据长度
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
    if(ring_buffer_handle->lenght == (ring_buffer_handle->max_lenght - 1))
        return RING_BUFFER_ERROR ;
    else
    {
        *(ring_buffer_handle->array_addr + ring_buffer_handle->tail) = data;//基地址+偏移量，存放数据
        ring_buffer_handle->lenght ++ ;//数据量计数+1
        ring_buffer_handle->tail ++ ;//尾指针后移
    }
    //如果尾指针超越了数组末尾，尾指针指向缓冲区数组开头，形成闭环
    if(ring_buffer_handle->tail > (ring_buffer_handle->max_lenght - 1))
        ring_buffer_handle->tail = 0 ;
	return RING_BUFFER_SUCCESS ;
}

/**
 * \brief 从缓冲区头部读取一个字节
 * \param[in] ring_buffer_handle: 缓冲区结构体句柄
 * \return 返回读取的字节
*/
uint8_t Ring_Buffer_Read_Byte(ring_buffer *ring_buffer_handle)
{
    uint8_t data ;
    if (ring_buffer_handle->lenght != 0)//有数据未读出
    {
        data = *(ring_buffer_handle->array_addr + ring_buffer_handle->head);//读取数据
        ring_buffer_handle->head ++ ;
        ring_buffer_handle->lenght -- ;//数据量计数-1
        //如果头指针超越了数组末尾，头指针指向数组开头，形成闭环
        if(ring_buffer_handle->head > (ring_buffer_handle->max_lenght - 1))
            ring_buffer_handle->head = 0 ;
    }
    return data ;
}

/**
 * \brief 向缓冲区尾部写指定长度的数据
 * \param[out] ring_buffer_handle: 缓冲区结构体句柄
 * \param[out] input_addr: 待写入数组基地址
 * \param[in] write_lenght: 要写入的字节数
 * \return 返回缓冲区尾部写指定长度字节的结果
 *      \arg RING_BUFFER_SUCCESS: 写入成功
 *      \arg RING_BUFFER_ERROR: 写入失败
*/
uint8_t Ring_Buffer_Write_String(ring_buffer *ring_buffer_handle, void *input_addr, uint32_t write_lenght)
{
    //如果不够存储空间存放新数据,返回错误
    if((ring_buffer_handle->lenght + write_lenght) > (ring_buffer_handle->max_lenght))
        return RING_BUFFER_ERROR ;
    else
    {
        //设置两次写入长度
        uint32_t write_size_a, write_size_b ;
        //如果顺序可用长度小于需写入的长度，需要将数据拆成两次分别写入
        if((ring_buffer_handle->max_lenght - ring_buffer_handle->tail) < write_lenght)
        {
            write_size_a = ring_buffer_handle->max_lenght - ring_buffer_handle->tail ;//从尾指针开始写到储存数组末尾
            write_size_b = write_lenght - (ring_buffer_handle->max_lenght - ring_buffer_handle->tail) ;//从储存数组开头写数据
        }
        else//如果顺序可用长度大于需写入的长度，则只需要写入一次
        {
            write_size_a = write_lenght ;//从尾指针开始写到储存数组末尾
            write_size_b = 0 ;//无需从储存数组开头写数据
        }
        //开始写入数据
        if(write_size_b != 0)//需要写入两次
        {
            //分别拷贝a、b段数据到储存数组中
            memcpy(ring_buffer_handle->array_addr + ring_buffer_handle->tail, input_addr, write_size_a);
            memcpy(ring_buffer_handle->array_addr, input_addr + write_size_a , write_size_b);
            ring_buffer_handle->lenght += write_lenght ;//记录新存储了多少数据量
            ring_buffer_handle->tail = write_size_b ;//重新定位尾指针位置
        }
        else//只需写入一次
        {
            memcpy(ring_buffer_handle->array_addr + ring_buffer_handle->tail, input_addr, write_size_a);
            ring_buffer_handle->lenght += write_lenght ;//记录新存储了多少数据量
            ring_buffer_handle->tail += write_size_a ;//重新定位尾指针位置
        }
        return RING_BUFFER_SUCCESS ;
    }
}

/**
 * \brief 向缓冲区头部读指定长度的数据，保存到指定的地址
 * \param[in] ring_buffer_handle: 缓冲区结构体句柄
 * \param[out] output_addr: 读取的数据保存地址
 * \param[in] read_lenght: 要读取的字节数
 * \return 返回缓冲区头部读指定长度字节的结果
 *      \arg RING_BUFFER_SUCCESS: 读取成功
 *      \arg RING_BUFFER_ERROR: 读取失败
*/
uint8_t Ring_Buffer_Read_String(ring_buffer *ring_buffer_handle, uint8_t *output_addr, uint32_t read_lenght)
{
    if(read_lenght > ring_buffer_handle->lenght)
        return RING_BUFFER_ERROR ;
    else
    {
        uint32_t Read_size_a, Read_size_b ;
        if(read_lenght > (ring_buffer_handle->max_lenght - ring_buffer_handle->head))
        {
            Read_size_a = ring_buffer_handle->max_lenght - ring_buffer_handle->head ;
            Read_size_b = read_lenght - (ring_buffer_handle->max_lenght - ring_buffer_handle->head) ;
        }
        else
        {
            Read_size_a = read_lenght ;
            Read_size_b = 0 ;
        }
        if(Read_size_b != 0)//需要读取两次
        {
            memcpy(output_addr, ring_buffer_handle->array_addr + ring_buffer_handle->head, Read_size_a);
            memcpy(output_addr + Read_size_a, ring_buffer_handle->array_addr, Read_size_b);
            ring_buffer_handle->lenght -= read_lenght ;//记录剩余数据量
            ring_buffer_handle->head = Read_size_b ;//重新定位头指针位置
        }
        else
        {
            memcpy(output_addr, ring_buffer_handle->array_addr + ring_buffer_handle->head, Read_size_a);
            ring_buffer_handle->lenght -= read_lenght ;//记录剩余数据量
            ring_buffer_handle->head += Read_size_a ;//重新定位头指针位置
        }
        return RING_BUFFER_SUCCESS ;
    }
}

/**
 * \brief 从头指针开始查找最近的匹配字符
 * \param[in] ring_buffer_handle: 缓冲区结构体句柄
 * \param[in] find_byte: 要查找的字符
 * \return 返回字符查找的结果
 *      \arg >0: 若要获取到匹配字符则需要读取的数据量
 *      \arg RING_BUFFER_ERROR: 查找失败
*/
uint32_t Ring_Buffer_Find_Byte(ring_buffer *ring_buffer_handle, uint8_t find_byte)
{
    uint32_t distance = 0 ;
    uint32_t lenght_a, lenght_b ;
    //如果有效数据已在数组中头尾相连
    if((ring_buffer_handle->lenght) > (ring_buffer_handle->max_lenght - ring_buffer_handle->head))
    {
        lenght_a = ring_buffer_handle->max_lenght - ring_buffer_handle->head ;
        lenght_b = ring_buffer_handle->lenght - lenght_a ;
    }
    else
    {
        lenght_a = ring_buffer_handle->lenght ;
        lenght_b = 0 ;
    }
    if(lenght_b != 0)//需要查找两个区域
    {
        while(distance < lenght_a)
        {
            //在区域1查找匹配的字符
            if(*(ring_buffer_handle->array_addr + ring_buffer_handle->head + distance) != find_byte)
                distance ++ ;
            else return distance + 1 ;
        }
        while(distance < ring_buffer_handle->lenght)
        {
            //在区域2查找匹配的字符
            if(*(ring_buffer_handle->array_addr + distance - lenght_a ) != find_byte)
                distance ++ ;
            else return distance + 1 ;
        }
        return RING_BUFFER_ERROR ;//查找失败，返回 RING_BUFFER_ERROR
    }
    else
    {
        while(distance < lenght_a)
        {
            //在区域1查找匹配的字符
            if(*(ring_buffer_handle->array_addr + ring_buffer_handle->head + distance) != find_byte)
                distance ++ ;
            else return distance + 1 ;
        }
        return RING_BUFFER_ERROR ;
    }
}

/**
 * \brief 获取缓冲区里已储存的数据长度
 * \param[in] ring_buffer_handle: 缓冲区结构体句柄
 * \return 返回缓冲区里已储存的数据长度
*/
uint32_t Ring_Buffer_Get_Lenght(ring_buffer *ring_buffer_handle)
{
    return ring_buffer_handle->lenght ;
}

/**
 * \brief 获取缓冲区可用储存空间
 * \param[in] ring_buffer_handle: 缓冲区结构体句柄
 * \return 返回缓冲区可用储存空间
*/
uint32_t Ring_Buffer_Get_Free_Size(ring_buffer *ring_buffer_handle)
{
    return (ring_buffer_handle->max_lenght - ring_buffer_handle->lenght) ;
}