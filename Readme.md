# RingBuffer 轻量级环形缓冲区
[![GitHub repository](https://img.shields.io/badge/github-RingBuffer-blue)](https://github.com/netube99/RingBuffer) [![GitHub license](https://img.shields.io/github/license/netube99/RingBuffer?color=green)](https://github.com/netube99/RingBuffer/blob/main/LICENSE) [![Language](https://img.shields.io/badge/make%20with-C-red)]()

## 简介
RingBuffer 是一个基于C语言开发的轻量级环形缓冲区，适用于各嵌入式平台的串口收发等应用场景；在基本功能的基础上还提供了一个分段记录框架，使得数据管理更加方便；代码在AT32F403A平台上编译运行，经过简单的串口收发测试后暂未发现显性BUG但不保证稳定性，用于产品开发的话请务必谨慎测试，如果发现BUG请及时反馈，谢谢；

## 开始
### 基础功能 RingBuffer Base 的使用方法

```c
//引用相关头文件
#include <stdint.h>
#include <stdio.h>
#include "ring_buffer.h"

//创建一个数组作为数据存储空间
#define BUFFER_SIZE 128
static uint8_t buffer[BUFFER_SIZE];

//创建环形缓冲区句柄
static ring_buffer rb;

int main(void)
{
    //初始化环形缓冲区参数
    RB_Init(&rb, buffer, BUFFER_SIZE);

    //写入向环形缓冲区写入数据
    RB_Write_String(&rb, "hello world", 11);
    RB_Write_Byte(&rb, '!');
    RB_Write_Byte(&rb, 0x00);

    //删除环形缓冲区部分数据
    RB_Delete(&rb, 2);

    //获取已储存的数据长度
    uint32_t num = RB_Get_Length(&rb);

    //读出环形缓冲区中的数据并打印
    uint8_t get[16];
    RB_Read_String(&rb, get, num);
    printf("%s", get);
    
    //控制台输出内容
    //llo world!
    return 0;
}
```
### 分段框架 RingBuffer Chapter 的使用方法

```c
//引用相关头文件
#include <stdint.h>
#include <stdio.h>
#include "ring_buffer_chapter.h"

//创建两个数组，一个作为数据存储空间，一个用于记录分段信息
#define BASE_SIZE 128
static uint8_t buffer_base[BASE_SIZE];
#define CHAPTER_SIZE 16
static uint32_t buffer_chapter[CHAPTER_SIZE];

//创建分段环形缓冲区句柄
static ring_buffer_chapter rbc;

int main(void)
{
    //初始化分段环形缓冲区参数
    RBC_Init(&rbc, buffer_base, BASE_SIZE, buffer_chapter, CHAPTER_SIZE);

    //写入向环形缓冲区写入数据1，并记录分段结尾
    RBC_Write_String(&rbc, "string1", 7);
    RBC_Write_Byte(&rbc, '!');
    RBC_Write_Byte(&rbc, 0x00);
    RBC_Ending_Chapter(&rbc);

    //写入向环形缓冲区写入数据2，并记录分段结尾
    RBC_Write_String(&rbc, "string2", 7);
    RBC_Write_Byte(&rbc, '!');
    RBC_Write_Byte(&rbc, 0x00);
    RBC_Ending_Chapter(&rbc);

    //获取已储存的分段数量
    uint32_t num = RBC_Get_Chapter_Number(&rbc);

    //读出环形缓冲区中的数据并打印
    uint8_t get[16];
    for (uint32_t i = 0; i < num; i++)
    {
        RBC_Read_Chapter(&rbc, get, NULL);
        printf("%s\r\n", get);
    }
    
    //控制台输出内容
    //string1!
    //string2!
    return 0;
}
```

## 更新

2021.01.19 v0.1.0 第一版<br>
2021.01.24 v0.1.0 增加匹配字符查找函数<br>
2021.01.27 v0.2.0 重制匹配字符查找函数，现已支持8位到32位关键词查询<br>
2021.01.28 v0.3.0 复位函数修改为删除函数、增加关键词插入函数（自适应大小端）<br>
2021.01.30 v0.3.1 修复了String读写函数的小概率指针溢出错误<br>
2021.06.29 v0.3.2 修复了 RB_Write_String 的参数类型错误，修复了RB_Write_Byte 无法写数组最后一位的问题<br>
2022.08.20 v0.3.3 删除了关键词功能，使用分段结束记录功能替换；修改函数RB_Read_Byte的输出方式<br>2022.09.25 v0.4.0 删除旧版分段，重新开发分段记录框架功能<br>