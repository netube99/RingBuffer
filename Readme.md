# Ring Buffer 简易环形缓冲区

## 简介
Ring Buffer 是一个基于C语言实现的简易的环形缓冲区库，用户可新建一个普通的 uint8_t 数组作为缓冲区物理内存，通过与Ring Buffer操作句柄绑定，该数组即可转换为环形缓冲区并使用本库提供的函数对该缓冲区进行读写等操作；

Ring Buffer 适用于单片机串口收发等应用场景，与普通的数组缓冲区相比，Ring Buffer 的读写操作不需要记录地址、数据量加减和覆写等复杂的操作，Ring Buffer 仅使用少量的函数即可实现单字节、多字节读写、查询、清空环形缓冲区；Ring Buffer 用简洁的代码实现了核心的功能，小巧的内存占用和较高的运行效率非常适合移植到各单片机应用中；

## 使用方法
1. 复制库文件至工程中，在源代码中引用 Ring Buffer 头文件；
2. 新建一个 uint8_t 数组，数组的大小即是环形缓冲区的大小；
3. 新建 Ring Buffer 操作句柄；
4. 初始化 Ring Buffer 操作句柄，将其与刚才新建的数组绑定，转换成环形缓冲区；
5. 这时候我们就可以使用库提供的各种函数对新建的缓冲区进行读写等操作；

## 示例代码
```c
#include <stdio.h>
#include <ring_buffer.h>

#define Read_BUFFER_SIZE	256

int main()
{
    //新建缓冲区数组与Ring Buff操作句柄
    uint8_t buffer[Read_BUFFER_SIZE] ;
    ring_buffer RB ;

    //初始化Ring Buff操作句柄，绑定缓冲区数组；
    Ring_Buffer_Init(&RB, buffer, Read_BUFFER_SIZE);

    //向环形缓冲区写入一段字节和一个字节
    Ring_Buffer_Write_String(&RB, "hello world", 11);
    Ring_Buffer_Write_Byte(&RB, '!');

    //读出环形缓冲区中的数据并打印
    uint8_t get[16] ;
    Ring_Buffer_Read_String(&RB, get, 12);
    printf("%s", get);
}
```