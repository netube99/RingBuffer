# RingBuffer 轻量级环形缓冲区
[![GitHub repository](https://img.shields.io/badge/github-RingBuffer-blue)](https://github.com/netube99/RingBuffer) [![GitHub license](https://img.shields.io/github/license/netube99/RingBuffer?color=green)](https://github.com/netube99/RingBuffer/blob/main/LICENSE) [![Language](https://img.shields.io/badge/make%20with-C-red)]()

## 简介
RingBuffer 是一个基于C语言开发的轻量级环形缓冲区，适用于各嵌入式平台的串口收发等应用场景；库文件提供了基础的环形数据读写功能，为了方便多组数据同时储存，RingBuffer提供了一个可选的分段记录功能；代码在GD32F130C8T6平台上编译运行，已进行过大量数据的串口收发压力测试，暂未发现显性BUG但不保证稳定性，用于产品开发的话请务必谨慎测试，如果发现BUG请及时反馈，谢谢；

## 开始
1. 复制库文件至工程中，在源代码中引用 RingBuffer 头文件；
2. 新建一个 uint8_t 数组，数组的大小即是环形缓冲区的大小；
3. 新建 RingBuffer 操作句柄；
4. 初始化 RingBuffer 操作句柄，将其与刚才新建的数组空间绑定，转换成环形缓冲区；
5. 使用库提供的各种函数对新建的缓冲区进行读写等操作；

## 示例
RingBuffer 的基本的使用方法
```c
#include <stdio.h>
#include <ring_buffer.h>

#define READ_BUFFER_SIZE	256

int main()
{
    //新建缓冲区数组与RingBuffer操作句柄
    uint8_t buffer[READ_BUFFER_SIZE] ;
    ring_buffer RB ;
    
    //初始化RingBuffer操作句柄，绑定缓冲区数组；
    Ring_Buffer_Init(&RB, buffer, READ_BUFFER_SIZE);
    
    //向环形缓冲区写入一段字节和一个字节
    Ring_Buffer_Write_String(&RB, "hello world", 11);
    Ring_Buffer_Write_Byte(&RB, '!');
    
    //获取已储存的数据长度，读出环形缓冲区中的数据并打印
    uint32_t num = Ring_Buffer_Get_Length(&RB);
    uint8_t get[16] ;
    Ring_Buffer_Read_String(&RB, get, num);
    printf("%s", get);
    
    return 0 ;
}
```
## 更新
2021.01.19 v1.0.0 发布第一版本<br>
2021.01.24 v1.1.0 增加匹配字符查找函数<br>
2021.01.27 v1.2.0 重制匹配字符查找函数，现已支持8位到32位关键词查询<br>
2021.01.28 v1.3.0 复位函数修改为删除函数、增加关键词插入函数（自适应大小端）<br>
2021.01.30 v1.3.1 修复了String读写函数的小概率指针溢出错误<br>
2021.06.29 v1.3.2 修复了 Ring_Buffer_Write_String 的参数类型错误，修复了Ring_Buffer_Write_Byte 无法写数组最后一位的问题<br>
2022.05.31 v1.4.0 删除了关键词插入/查询功能，并使用分段结束记录功能替换；修改函数Ring_Buffer_Read_Byte的输出方式<br>