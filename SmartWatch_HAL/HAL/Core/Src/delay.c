/**
  * @file           : delay.c
  * @brief          : 基于DWT周期计数器的微秒延时模块
  * @author         : CHSGY
  * @date           : 2026-06-05
  *
  * @note           : 使用Cortex-M3内核DWT CYCCNT实现周期级精确延时
  *                   与FreeRTOS兼容（不占用SysTick），用完即关，不增加静态功耗
  */

#include "delay.h"

/**
  * @brief  微秒级延时（基于DWT周期计数器）
  * @param  us: 延时微秒数，范围：0~596523（8MHz时钟下）
  * @retval 无
  * @note   每次调用自动开启/关闭DWT CYCCNT，空闲时无额外功耗
  *         使用SystemCoreClock自动适配时钟频率
  */
void delay_us(uint32_t us)
{
    uint32_t start, ticks;

    /* 开启DWT CYCCNT */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    /* 计算需要等待的时钟周期数 */
    ticks = us * (SystemCoreClock / 1000000);

    /* 记录起始值并等待 */
    start = DWT->CYCCNT;
    while ((DWT->CYCCNT - start) < ticks);

    /* 关闭DWT CYCCNT，节省功耗 */
    DWT->CTRL &= ~DWT_CTRL_CYCCNTENA_Msk;
    CoreDebug->DEMCR &= ~CoreDebug_DEMCR_TRCENA_Msk;
}

/**
  * @brief  毫秒级延时
  * @param  ms: 延时毫秒数
  * @retval 无
  */
void delay_ms(uint32_t ms)
{
    while (ms--)
    {
        delay_us(1000);
    }
}
