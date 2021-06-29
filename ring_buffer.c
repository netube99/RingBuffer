/**
 * \file ring_buffer.c
 * \brief 简易环形缓冲的实现
 * \details 可作为单片机串口的环形缓冲/软FIFO，提升串口数据传输能力；
 * 可以向环形缓冲区快速的储存/读取数据，并且无需手动记录数据接收量即可准确的读出所有数据；
 * 无需手动清空数据缓存区，只要将上次接收的数据读取出来，缓冲区即可准备好接收下一段数据；
 * 节省了手动清空普通缓存区的时间，能够提升串口程序的运行效率；
 * \author netube_99\netube@163.com
 * \date 2021.06.29
 * \version v1.3.2
 * 
 * 2021.01.19 v1.0.0 发布第一版本
 * 2021.01.24 v1.1.0 增加匹配字符查找函数
 * 2021.01.27 v1.2.0 重制匹配字符查找功能，现已支持8位到32位关键词查询
 * 2021.01.28 v1.3.0 复位函数修改为删除函数、增加关键词插入函数（自适应大小端）
 * 2021.01.30 v1.3.1 修复了String读写函数的小概率指针溢出错误
 * 2021.06.29 v1.3.2 修复了 Ring_Buffer_Write_String 的参数类型错误
                    修复了 Ring_Buffer_Write_Byte 无法写数组最后一位的问题
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
 * \brief 从头指针开始删除指定长度的数据
 * \param[out] ring_buffer_handle: 缓冲区结构体句柄
 * \param[in] lenght: 要删除的长度
 * \return 返回删除指定长度数据结果
 *      \arg RING_BUFFER_SUCCESS: 删除成功
 *      \arg RING_BUFFER_ERROR: 删除失败
*/
uint8_t Ring_Buffer_Delete(ring_buffer *ring_buffer_handle, uint8_t lenght)
{
    if(ring_buffer_handle->lenght < lenght)
        return RING_BUFFER_ERROR ;//已储存的数据量小于需删除的数据量
    else
    {
        if((ring_buffer_handle->head + lenght) >= ring_buffer_handle->max_lenght)
            ring_buffer_handle->head = lenght - (ring_buffer_handle->max_lenght - ring_buffer_handle->head);
        else
            ring_buffer_handle->head += lenght ;//头指针向前推进，抛弃数据
        ring_buffer_handle->lenght -= lenght ;//重新记录有效数据长度
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
    if(ring_buffer_handle->lenght == (ring_buffer_handle->max_lenght))
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
 * \brief 从缓冲区头指针读取一个字节
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
 * \param[out] input_addr: 待写入数据的基地址
 * \param[in] write_lenght: 要写入的字节数
 * \return 返回缓冲区尾部写指定长度字节的结果
 *      \arg RING_BUFFER_SUCCESS: 写入成功
 *      \arg RING_BUFFER_ERROR: 写入失败
*/
uint8_t Ring_Buffer_Write_String(ring_buffer *ring_buffer_handle, uint8_t *input_addr, uint32_t write_lenght)
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
            write_size_b = write_lenght - write_size_a ;//从储存数组开头写数据
        }
        else//如果顺序可用长度大于或等于需写入的长度，则只需要写入一次
        {
            write_size_a = write_lenght ;//从尾指针开始写到储存数组末尾
            write_size_b = 0 ;//无需从储存数组开头写数据
        }
        //开始写入数据
        if(write_size_b != 0)//需要写入两次
        {
            //分别拷贝a、b段数据到储存数组中
            memcpy(ring_buffer_handle->array_addr + ring_buffer_handle->tail, input_addr, write_size_a);
            memcpy(ring_buffer_handle->array_addr, input_addr + write_size_a, write_size_b);
            ring_buffer_handle->lenght += write_lenght ;//记录新存储了多少数据量
            ring_buffer_handle->tail = write_size_b ;//重新定位尾指针位置
        }
        else//只需写入一次
        {
            memcpy(ring_buffer_handle->array_addr + ring_buffer_handle->tail, input_addr, write_size_a);
            ring_buffer_handle->lenght += write_lenght ;//记录新存储了多少数据量
            ring_buffer_handle->tail += write_size_a ;//重新定位尾指针位置
            if(ring_buffer_handle->tail == ring_buffer_handle->max_lenght)
                ring_buffer_handle->tail = 0 ;//如果写入数据后尾指针刚好写到数组尾部，则回到开头，防止越位
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
            Read_size_b = read_lenght - Read_size_a ;
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
            if(ring_buffer_handle->head == ring_buffer_handle->max_lenght)
                ring_buffer_handle->head = 0 ;//如果读取数据后头指针刚好写到数组尾部，则回到开头，防止越位
        }
        return RING_BUFFER_SUCCESS ;
    }
}

/**
 * \brief 环形缓冲区插入关键词
 * \param[in] ring_buffer_handle: 缓冲区结构体句柄
 * \param[in] keyword: 要插入的关键词
 * \param[in] keyword_lenght:关键词字节数，最大4字节（32位）
 * \return 返回插入关键词的结果
 *      \arg RING_BUFFER_SUCCESS: 插入成功
 *      \arg RING_BUFFER_ERROR: 插入失败
*/
uint8_t Ring_Buffer_Insert_Keyword(ring_buffer *ring_buffer_handle, uint32_t keyword, uint8_t keyword_lenght)
{
    uint8_t *keyword_addr = (uint8_t *)&keyword;
    uint8_t keyword_byte[4];
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    //小端模式字节序排列
    keyword_byte[0] = *(keyword_addr + 3) ;
    keyword_byte[1] = *(keyword_addr + 2) ;
    keyword_byte[2] = *(keyword_addr + 1) ;
    keyword_byte[3] = *(keyword_addr + 0) ;
    //将关键词填入环形缓冲区
    return Ring_Buffer_Write_String(ring_buffer_handle, keyword_byte, keyword_lenght);
#else
    //大端模式字节序排列
    keyword_byte[0] = *(keyword_addr + 0) ;
    keyword_byte[1] = *(keyword_addr + 1) ;
    keyword_byte[2] = *(keyword_addr + 2) ;
    keyword_byte[3] = *(keyword_addr + 3) ;
    //将关键词填入环形缓冲区
    return Ring_Buffer_Write_String(ring_buffer_handle, keyword_byte + (keyword_lenght - 1), keyword_lenght);
#endif
}

/**
 * \brief 从头指针开始查找最近的匹配关键词
 * \param[in] ring_buffer_handle: 缓冲区结构体句柄
 * \param[in] keyword: 要查找的关键词
 * \param[in] keyword_lenght:关键词字节数，最大4字节（32位）
 * \return 返回获取匹配字符最高位需要读取的字节数，返回 0/RING_BUFFER_ERROR: 则查找失败
*/
uint32_t Ring_Buffer_Find_Keyword(ring_buffer *ring_buffer_handle, uint32_t keyword, uint8_t keyword_lenght)
{
    uint32_t max_find_lenght = ring_buffer_handle->lenght - keyword_lenght + 1 ;//计算需要搜索的最大长度
    uint8_t trigger_word = keyword >> ((keyword_lenght - 1) * 8) ;//计算触发关键词检查的字节（最高位）
    uint32_t distance = 1 , find_head = ring_buffer_handle->head;//记录关键词距离头指针的长度/临时头指针获取原头指针初始值
    while(distance <= max_find_lenght)//在设定范围内搜索关键词（防止指针越位错误）
    {
        if(*(ring_buffer_handle->array_addr + find_head) == trigger_word)//如果高位字节匹配则开始向低位检查
            if(Ring_Buffer_Get_Word(ring_buffer_handle, find_head, keyword_lenght) == keyword)//满足关键词匹配
                return distance ;//返回长度，使用 Ring_Buffer_Read_String 可提取数据
        find_head ++ ;//当前字符不匹配，临时头指针后移，检查下一个
        distance ++ ;//长度也要随着搜索进度后移
        if(find_head > (ring_buffer_handle->max_lenght - 1))
            find_head = 0 ;//如果到了数组尾部，则返回数组开头（环形缓冲的特性）
    }
    return RING_BUFFER_ERROR ;//咩都某搵到
}

/**
 * \brief 从指定头指针地址获取完整长度的关键词（私有函数，无指针越位保护）
 * \param[in] ring_buffer_handle: 缓冲区结构体句柄
 * \param[in] head: 头指针偏移量（匹配字符所在地址）
 * \param[in] read_lenght:关键词字节数，最大4字节（32位）
 * \return 返回完整的关键词
*/
static uint32_t Ring_Buffer_Get_Word(ring_buffer *ring_buffer_handle, uint32_t head, uint32_t read_lenght)
{
    uint32_t data = 0, i ;
    for(i=1; i<=read_lenght; i++)//按照关键词的长度（字符数）向后提取数据
    {
        //从最高位到最低位，整合成一个32位数据
        data |= *(ring_buffer_handle->array_addr + head) << (8*(read_lenght - i)) ;
        head ++ ;
        if(head > (ring_buffer_handle->max_lenght - 1))
            head = 0 ;//如果到了数组尾部，则返回数组开头（环形缓冲的特性）
    }
    return data ;//返回移位整合后的32位数据
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
uint32_t Ring_Buffer_Get_FreeSize(ring_buffer *ring_buffer_handle)
{
    return (ring_buffer_handle->max_lenght - ring_buffer_handle->lenght) ;
}