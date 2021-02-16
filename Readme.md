# RingBuffer 轻量级环形缓冲区
[![GitHub license](https://img.shields.io/github/license/netube99/RingBuffer?color=green)](https://github.com/netube99/RingBuffer/blob/main/LICENSE)[![GitHub license](https://img.shields.io/badge/MADE%20WITH-C-red)](https://github.com/netube99/RingBuffer/blob/main/LICENSE)[![GitHub license](https://img.shields.io/github/stars/netube99/RingBuffer?color=blue)](https://github.com/netube99/RingBuffer/blob/main/LICENSE)[![GitHub license](	https://img.shields.io/github/forks/netube99/RingBuffer?color=blue)](https://github.com/netube99/RingBuffer/blob/main/LICENSE)

## 简介
RingBuffer 是一个基于C语言开发的轻量级环形缓冲区，适用于各嵌入式平台的串口收发等应用场景；

相较于普通的收发缓冲数组，环形缓冲区的操作更加简单，性能更加强大；使用 RingBuffer 提供的函数，可以轻松实现数据的读写、查询、删除等操作，不需要编写复杂的地址记录、读写计数等代码，减少了数据指针溢出、数据被覆盖的风险；同时 RingBuffer 还提供了可自定义分隔关键字的插入、查找等功能，运用在嵌入式的串口收发中可以实现多段数据的储存，段落之间分隔独立，读写缓冲区不再受到硬件时序的阻塞，降低串口对芯片实时响应的要求并提升了数据传输的可靠性；

代码已在GD32F130C8T6平台上通过编译，已进行过大量数据的串口收发压力测试，暂未发现显性BUG；

本项目开源地址：[Github](https://github.com/netube99/RingBuffer)

## 开始
1. 复制库文件至工程中，在源代码中引用 RingBuffer 头文件；
2. 新建一个 uint8_t 数组，数组的大小即是环形缓冲区的大小；
3. 新建 RingBuffer 操作句柄；
4. 初始化 RingBuffer 操作句柄，将其与刚才新建的数组绑定，转换成环形缓冲区；
5. 这时候我们就可以使用库提供的各种函数对新建的缓冲区进行读写等操作；
6. 函数详细内容请在编程的过程中参考 ring_buffer.c 内的注释；

## 示例
在这里我展示了如何新建并初始化 RingBuffer、读写环形缓冲、查询数据长度等基础操作；您可以了解到 RingBuffer 的函数命名与参数的风格和基本的使用方法

```c
#include <stdio.h>
#include <ring_buffer.h>

#define Read_BUFFER_SIZE	256

int main()
{
    //新建缓冲区数组与RingBuffer操作句柄
    uint8_t buffer[Read_BUFFER_SIZE] ;
    ring_buffer RB ;
    
    //初始化RingBuffer操作句柄，绑定缓冲区数组；
    Ring_Buffer_Init(&RB, buffer, Read_BUFFER_SIZE);
    
    //向环形缓冲区写入一段字节和一个字节
    Ring_Buffer_Write_String(&RB, "hello world", 11);
    Ring_Buffer_Write_Byte(&RB, '!');
    
    //获取已储存的数据长度，读出环形缓冲区中的数据并打印
    uint32_t num = Ring_Buffer_Get_Lenght(&RB);
    uint8_t get[16] ;
    Ring_Buffer_Read_String(&RB, get, num);
    printf("%s", get);
    
    return 0 ;
}
```
除了基本的读写操作之外，为了更好的利用环形这一特点，我加入了分隔关键词、查询关键词、删除数据等功能，基于这些功能您可以在串口收发中实现多段数据的缓存与准确读取；降低了实时性响应的要求、提升了串口收发的性能

```c
#include <stdio.h>
#include <ring_buffer.h>

#define Read_BUFFER_SIZE	256

//设定一个分隔关键词和关键词的长度（字节）
#define SEPARATE_SIGN       0xCCFB22AA
#define SEPARATE_SIGN_SIZE  4

int main()
{
    //新建缓冲区数组与RingBuffer操作句柄
    uint8_t buffer[Read_BUFFER_SIZE] ;
    ring_buffer RB ;

    //初始化RingBuffer操作句柄，绑定缓冲区数组；
    Ring_Buffer_Init(&RB, buffer, Read_BUFFER_SIZE);

    //记录段落数量
    uint8_t String_Count = 0 ;

    //向环形缓冲区写入三段数据，每段之间插入一个分隔关键词
    Ring_Buffer_Write_String(&RB, "ABCDEFGHIJK\r\n", 13);//写入一段数据
    Ring_Buffer_Insert_Keyword(&RB, SEPARATE_SIGN, SEPARATE_SIGN_SIZE);//插入一个分隔关键词
    String_Count ++ ;//记录段落数量 +1

    Ring_Buffer_Write_String(&RB, "abcdefg\r\n", 9);
    Ring_Buffer_Insert_Keyword(&RB, SEPARATE_SIGN, SEPARATE_SIGN_SIZE);
    String_Count ++ ;

    Ring_Buffer_Write_String(&RB, "1234\r\n", 6);
    Ring_Buffer_Insert_Keyword(&RB, SEPARATE_SIGN, SEPARATE_SIGN_SIZE);
    String_Count ++ ;

    while(String_Count != 0)
    {
        uint8_t get[16] ;
        //获得头指针到关键词高位的距离，距离-1得到第一段数据的长度
        uint8_t lenght = Ring_Buffer_Find_Keyword(&RB, SEPARATE_SIGN, SEPARATE_SIGN_SIZE) - 1 ;
        Ring_Buffer_Read_String(&RB, get, lenght);//读取一段数据，保存到get数组
        printf("%s", get);//打印数据
        Ring_Buffer_Delete(&RB, SEPARATE_SIGN_SIZE);//删除分隔关键词的长度的数据，即删除关键词
        String_Count -- ;//记录段落数量 -1
    }

    return 0 ;
}
```
## 更新
2021.01.19 v1.0.0 发布第一版本  
2021.01.24 v1.1.0 增加匹配字符查找函数  
2021.01.27 v1.2.0 重制匹配字符查找函数，现已支持8位到32位关键词查询  
2021.01.28 v1.3.0 复位函数修改为删除函数、增加关键词插入函数（自适应大小端）  
2021.01.30 v1.3.1 修复了String读写函数的小概率指针溢出错误