/**
 * repro_overflow.c - 复现 RB_Write_String 的整数溢出越界写入
 *
 * 预期结果：ASan 报告 heap-buffer-overflow 并中止（进程异常退出 => BUG 确认）。
 * 正常运行到结尾输出 "NO CRASH" 说明未复现。
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "ring_buffer.h"

int main(void)
{
    /* Part 1: memcpy(NULL, ., 0) 形式的未定义行为（UBSan 会报告） */
    ring_buffer rb;
    static uint8_t buf[16];
    if (RB_Init(&rb, buf, (uint32_t)sizeof buf) != RB_OK) {
        printf("init failed\n");
        return 2;
    }
    printf("[1] calling RB_Write_String(rb, NULL, 0) ...\n");
    fflush(stdout);
    (void)RB_Write_String(&rb, NULL, 0u);
    printf("[1] calling RB_Read_String(rb, NULL, 0) ...\n");
    fflush(stdout);
    (void)RB_Read_String(&rb, NULL, 0u);

    /* Part 2: 整数溢出绕过容量检查
       Length=1, write_Length=0xFFFFFFFF => 1+0xFFFFFFFF 回绕为 0，
       通过容量检查，随后 memcpy 长度约 4GB => 越界读写崩溃 */
    if (RB_Write_Byte(&rb, 0x41) != RB_OK) {
        printf("write byte failed\n");
        return 2;
    }
    printf("[2] Length=1, calling RB_Write_String(rb, src, 0xFFFFFFFF) ...\n");
    fflush(stdout);
    {
        static uint8_t src[4] = {1, 2, 3, 4};
        (void)RB_Write_String(&rb, src, 0xFFFFFFFFu);
    }
    printf("[2] NO CRASH - overflow NOT reproduced\n");
    return 0;
}
