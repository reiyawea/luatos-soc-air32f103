#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "delay.h"
#include "air32f10x.h"
#include "air_rcc.h"
#include "air32f10x_tim.h"
#include "air32f10x_rcc.h"
#include "misc.h"

/**
* @brief Air32F103 coremark测试Demo
* 初始化系统时钟为8*32=256MHz
* 使用测试串口1 (Tx=PA9, Rx=PA10)输出信息
*/

//如果需要修改系统主频，修改以下两个宏
#define RCC_PLL 32
#define RCC_PLL_NUM RCC_PLLMul_32    



#define PRINTF_LOG printf

USART_TypeDef *USART_TEST = USART1;

void UART_Configuration(uint32_t bound);
void RCC_ClkConfiguration(void);
void TIM2_Init(void);

// coremark里定义了自己的main函数，此处的main函数执行系统初始化工作，在core_portme.c里的portable_init里调用即可
void main_original(void)
{
    RCC_ClocksTypeDef clocks;

    RCC_ClkConfiguration();		//配置时钟
    Delay_Init();				//延时初始化
    UART_Configuration(115200); //串口初始化

    PRINTF_LOG("AIR32F103 RCC Clock Config.\n");
    RCC_GetClocksFreq(&clocks); //获取时钟频率

    PRINTF_LOG("\n");
    PRINTF_LOG("SYSCLK: %3.1fMhz, \nHCLK: %3.1fMhz, \nPCLK1: %3.1fMhz, \nPCLK2: %3.1fMhz, \nADCCLK: %3.1fMhz\n",
               (float)clocks.SYSCLK_Frequency / 1000000, (float)clocks.HCLK_Frequency / 1000000,
               (float)clocks.PCLK1_Frequency / 1000000, (float)clocks.PCLK2_Frequency / 1000000, (float)clocks.ADCCLK_Frequency / 1000000);

    TIM2_Init();
}

void TIM2_Init(void)
{
// 初始化TIM2为更新中断，每毫秒中断一次，用于coremark tick更新
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2,ENABLE);
    TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStruct;
    TIM_TimeBaseInitStruct.TIM_ClockDivision=TIM_CKD_DIV1;
    TIM_TimeBaseInitStruct.TIM_CounterMode=TIM_CounterMode_Up;
    TIM_TimeBaseInitStruct.TIM_Period=999;
    TIM_TimeBaseInitStruct.TIM_Prescaler=8*RCC_PLL-1;
    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseInitStruct);

    TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
    NVIC_InitTypeDef NVIC_InitStruct;
    NVIC_InitStruct.NVIC_IRQChannel=TIM2_IRQn;
    NVIC_InitStruct.NVIC_IRQChannelCmd=ENABLE;
    NVIC_InitStruct.NVIC_IRQChannelPreemptionPriority=1;
    NVIC_InitStruct.NVIC_IRQChannelSubPriority=1;
    NVIC_Init(&NVIC_InitStruct);
    
}



void RCC_ClkConfiguration(void)
{
    RCC_DeInit(); //复位RCC寄存器

    RCC_HSEConfig(RCC_HSE_ON); //使能HSE
    while (RCC_GetFlagStatus(RCC_FLAG_HSERDY) == RESET)
        ; //等待HSE就绪

    RCC_PLLCmd(DISABLE);										 //关闭PLL
    AIR_RCC_PLLConfig(RCC_PLLSource_HSE_Div1, RCC_PLL_NUM, 1); //配置PLL,8*32=256MHz

    RCC_PLLCmd(ENABLE); //使能PLL
    while (RCC_GetFlagStatus(RCC_FLAG_PLLRDY) == RESET)
        ; //等待PLL就绪

    RCC_SYSCLKConfig(RCC_SYSCLKSource_PLLCLK); //选择PLL作为系统时钟

    RCC_HCLKConfig(RCC_SYSCLK_Div1); //配置AHB时钟
    RCC_PCLK1Config(RCC_HCLK_Div2);	 //配置APB1时钟
    RCC_PCLK2Config(RCC_HCLK_Div1);	 //配置APB2时钟

    RCC_LSICmd(ENABLE); //使能内部低速时钟
    while (RCC_GetFlagStatus(RCC_FLAG_LSIRDY) == RESET)
        ;				//等待LSI就绪
    RCC_HSICmd(ENABLE); //使能内部高速时钟
    while (RCC_GetFlagStatus(RCC_FLAG_HSIRDY) == RESET)
        ; //等待HSI就绪
}

void UART_Configuration(uint32_t bound)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    USART_InitStructure.USART_BaudRate = bound;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;

    USART_Init(USART_TEST, &USART_InitStructure);
    USART_Cmd(USART_TEST, ENABLE);
}

int SER_PutChar(int ch)
{
    while (!USART_GetFlagStatus(USART_TEST, USART_FLAG_TC))
        ;
    USART_SendData(USART_TEST, (uint8_t)ch);

    return ch;
}

int fputc(int c, FILE *f)
{
    /* Place your implementation of fputc here */
    /* e.g. write a character to the USART */
    if (c == '\n')
    {
        SER_PutChar('\r');
    }
    return (SER_PutChar(c));
}
