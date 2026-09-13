# RingBuffer 环形缓冲区

## 简介

RingBuffer 是一个基于 C99 开发的环形缓冲区，无动态内存分配，面向嵌入式平台的串口收发等字节流场景，在基础功能之上提供分段管理扩展框架

## 快速开始

### 基础功能 RingBuffer Base

```c
#include <stdint.h>
#include <stdio.h>
#include "ring_buffer.h"

#define BUFFER_SIZE 128
static uint8_t buffer[BUFFER_SIZE];
static ring_buffer rb;

int main(void)
{
    RB_Init(&rb, buffer, BUFFER_SIZE);

    RB_Write_String(&rb, (const uint8_t *)"hello world", 11);
    RB_Write_Byte(&rb, '!');
    RB_Write_Byte(&rb, 0x00);
    RB_Delete(&rb, 2);

    uint32_t num = RB_Get_Length(&rb);

    uint8_t get[16];
    RB_Read_String(&rb, get, num);
    printf("%s", get);
    //控制台输出内容
    //llo world!
    return 0;
}
```

### 分段框架 RingBuffer Chapter

```c
#include <stdint.h>
#include <stdio.h>
#include "ring_buffer_chapter.h"
//注意：分段版依赖基础版实现，编译时须同时编译 ring_buffer.c 与 ring_buffer_chapter.c

#define BASE_SIZE 128
static uint8_t buffer_base[BASE_SIZE];
#define CHAPTER_SIZE 16
static uint32_t buffer_chapter[CHAPTER_SIZE];

static ring_buffer_chapter rbc;

int main(void)
{
    //初始化分段环形缓冲区参数（分段环大小单位为字节，须为 4 的倍数且不小于 4）
    RBC_Init(&rbc, buffer_base, BASE_SIZE, buffer_chapter, sizeof(buffer_chapter));

    //写入数据1，并记录分段结尾
    RBC_Write_String(&rbc, (const uint8_t *)"string1!", 8);
    RBC_Write_Byte(&rbc, 0x00);
    RBC_Ending_Chapter(&rbc);

    //写入数据2，并记录分段结尾
    RBC_Write_String(&rbc, (const uint8_t *)"string2!", 8);
    RBC_Write_Byte(&rbc, 0x00);
    RBC_Ending_Chapter(&rbc);

    uint32_t num = RBC_Get_Chapter_Number(&rbc);

    //逐段读出（输出缓冲区须足够容纳分段，可先查 RBC_Get_Head_Chapter_Length，
    //或使用 RBC_Read_Chapter_Bounded 做有界读取）
    uint8_t get[16];
    for (uint32_t i = 0; i < num; i++)
    {
        uint32_t len = 0;
        RBC_Read_Chapter(&rbc, get, &len);
        printf("%s\r\n", get);
    }
    //控制台输出内容
    //string1!
    //string2!
    return 0;
}
```

## 并发

**默认配置非线程安全**，跨上下文使用必须完成中断、多线程的保护适配（使用前须先 `RB_Init / RBC_Init`）
* 未注入适配就跨上下文使用会导致数据静默错乱
* `rb_lock` 的返回值必须支持嵌套恢复
* 句柄内含指针，不得按值复制
* 注入方法：宏须在包含库头文件之前定义（或经 `-D` 编译命令行传入），且必须对 `ring_buffer.c` 自身的编译同样生效（同传 `-D`，或用 `-include` 强制包含配置头）——漏掉后者时库内仍是空实现，等于没保护；注入姿势参考 `tests/build.sh` 中 test_hooks 的编译命令

### 裸机（ARM Cortex-M）

```c
uint32_t rb_lock(void)         { uint32_t pm = __get_PRIMASK(); __disable_irq(); return pm; }
void     rb_unlock(uint32_t k) { __set_PRIMASK(k); }

#define RB_CRITICAL_ENTER()   rb_lock()
#define RB_CRITICAL_EXIT(k)   rb_unlock(k)
```

### RTOS（FreeRTOS）

```c
uint32_t rb_lock(void)         { return portSET_INTERRUPT_MASK_FROM_ISR(); }
void     rb_unlock(uint32_t k) { portCLEAR_INTERRUPT_MASK_FROM_ISR(k); }

#define RB_CRITICAL_ENTER()   rb_lock()
#define RB_CRITICAL_EXIT(k)   rb_unlock(k)
```

仅两个线程之间共享，不涉及中断时可改用**递归**互斥量（必须递归——分段接口内部会再次调用基础接口，非递归互斥量会死锁；FreeRTOS 须开启 `configUSE_RECURSIVE_MUTEXES`）：

```c
static SemaphoreHandle_t rb_mutex;   //须由 xSemaphoreCreateRecursiveMutex() 创建

uint32_t rb_lock(void)         { xSemaphoreTakeRecursive(rb_mutex, portMAX_DELAY); return 0; }
void     rb_unlock(uint32_t k) { (void)k; xSemaphoreGiveRecursive(rb_mutex); }
```

### Linux

`PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP` 需 glibc 且定义 `_GNU_SOURCE`；musl/macOS 无此宏，可改用属性初始化（见注释）：

```c
#include <pthread.h>

static pthread_mutex_t rb_mutex;

uint32_t rb_lock(void)         { pthread_mutex_lock(&rb_mutex);   return 0; }
void     rb_unlock(uint32_t k) { (void)k; pthread_mutex_unlock(&rb_mutex); }

#define RB_CRITICAL_ENTER()   rb_lock()
#define RB_CRITICAL_EXIT(k)   rb_unlock(k)

//初始化（放在使用前，可移植写法）：
//pthread_mutexattr_t a; pthread_mutexattr_init(&a);
//pthread_mutexattr_settype(&a, PTHREAD_MUTEX_RECURSIVE);
//pthread_mutex_init(&rb_mutex, &a);
```

## API

| 基础功能 | 说明 |
|---|---|
| `RB_Init(rb, buf, size)` | 初始化；`size` 须不小于 2 且小于 0xFFFFFFFF |
| `RB_Clear(rb)` | 清空，丢弃全部数据 |
| `RB_Write_Byte(rb, data)` | 写 1 字节；满返回 `RB_ERR_FULL` |
| `RB_Write_String(rb, buf, len)` | 写 `len` 字节（全有全无）；空间不足返回 `RB_ERR_FULL`；0 长度视为无操作；`buf` 不得与环缓冲区重叠 |
| `RB_Read_Byte(rb, *out)` | 读 1 字节；空返回 `RB_ERR_EMPTY` |
| `RB_Read_String(rb, buf, len)` | 读 `len` 字节（全有全无）；不足返回 `RB_ERR_EMPTY`；0 长度视为无操作 |
| `RB_Peek_Byte(rb, offset, *out)` | 窥视第 `offset` 字节（从 0 起），不消费；越界返回 `RB_ERR_EMPTY` |
| `RB_Peek_String(rb, buf, max, *copied)` | 窥视最多 `max` 字节（实际数写入 `*copied`，不足时按实拷贝），不消费 |
| `RB_Delete(rb, len)` | 删除 `len` 字节（不拷贝）；删 0 条视为无操作 |
| `RB_Get_Length / Get_FreeSize / Get_Capacity` | 查询：已存 / 剩余 / 容量 |

| 分段功能 | 说明 |
|---|---|
| `RBC_Init(rbc, base, base_size, ring, ring_size)` | 初始化；`ring_size` 单位为字节，须为 4 的倍数且不小于 4 |
| `RBC_Clear(rbc)` | 清空全部数据与分段记录，恢复初始化状态 |
| `RBC_Write_Byte / RBC_Write_String` | 向当前尾分段写数据（0 长度视为无操作） |
| `RBC_Ending_Chapter(rbc)` | 结尾当前尾分段，形成一条分段记录；无暂存数据返回 `RB_ERR_EMPTY` |
| `RBC_Read_Byte(rbc, *out)` | 从头分段读 1 字节（自动跨段推进） |
| `RBC_Read_Chapter(rbc, buf, *len)` | 读整个头分段（输出缓冲区须足够容纳，先查 `RBC_Get_Head_Chapter_Length`；`*len` 可为 NULL） |
| `RBC_Read_Chapter_Bounded(rbc, buf, max, *actual)` | 有界读取；放不下返回 `RB_ERR_TOO_SMALL` 且不消费（此时 `*actual` 报完整分段长度），可扩容重试 |
| `RBC_Delete(rbc, n)` | 删除前 n 条分段；删 0 条视为无操作 |
| `RBC_Get_Head_Chapter_Length / Get_Chapter_Number / Get_Base_Free_Size / Get_Chapter_Free_Size` | 查询（分段环容量 = `ring_size/4 + 1` 条，`Get_Chapter_Free_Size` 仅计分段环槽位，头分段自身不占槽） |

## 错误码

统一返回 `rb_status_t`，判断失败一律使用 `ret != RB_OK`：

> 注意：与旧版本（0.4 及更早）的 0x01/0x00 约定相反，现在 `RB_OK` 为 0，旧代码的真值判断（如 `if(ret)`）必须改为 `ret != RB_OK`。

| 码 | 含义 |
|---|---|
| `RB_OK` (0) | 成功 |
| `RB_ERR_PARAM` | 参数错误：句柄/指针无效、长度或配置非法 |
| `RB_ERR_EMPTY` | 数据不足：读取/删除的数量超过已存数据量、单字节窥视越界、分段结尾无暂存数据 |
| `RB_ERR_FULL` | 空间不足：待写入数据量超过剩余空间 |
| `RB_ERR_TOO_SMALL` | （分段有界读）输出缓冲区容纳不下整个分段，未消费任何数据，`*actual_Length` 报完整分段长度 |

## License

MIT License，详见 [LICENSE](LICENSE)
