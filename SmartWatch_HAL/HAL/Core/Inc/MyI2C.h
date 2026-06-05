/**
  * @file           : MyI2C.h
  * @brief          : 软件I2C总线驱动（HAL库版本，支持多引脚）
  * @author         : CHSGY
  * @date           : 2026-06-05
  *
  * @note           : 从标准库MyI2C移植，所有函数通过参数传入GPIO端口和引脚
  *                   支持多组I2C总线共用同一套驱动代码
  *                   每个使用方可在自己的头文件中定义调用宏，简化调用
  */

#ifndef __MYI2C_H
#define __MYI2C_H

#include "main.h"

void MyI2C_Start(GPIO_TypeDef *SCL_GPIOx, uint16_t SCL_Pin,
                 GPIO_TypeDef *SDA_GPIOx, uint16_t SDA_Pin);
void MyI2C_Stop(GPIO_TypeDef *SCL_GPIOx, uint16_t SCL_Pin,
                GPIO_TypeDef *SDA_GPIOx, uint16_t SDA_Pin);
void MyI2C_SendByte(GPIO_TypeDef *SCL_GPIOx, uint16_t SCL_Pin,
                    GPIO_TypeDef *SDA_GPIOx, uint16_t SDA_Pin,
                    uint8_t Byte);
uint8_t MyI2C_ReceiveByte(GPIO_TypeDef *SCL_GPIOx, uint16_t SCL_Pin,
                          GPIO_TypeDef *SDA_GPIOx, uint16_t SDA_Pin);
void MyI2C_SendAck(GPIO_TypeDef *SCL_GPIOx, uint16_t SCL_Pin,
                   GPIO_TypeDef *SDA_GPIOx, uint16_t SDA_Pin,
                   uint8_t AckBit);
uint8_t MyI2C_ReceiveAck(GPIO_TypeDef *SCL_GPIOx, uint16_t SCL_Pin,
                         GPIO_TypeDef *SDA_GPIOx, uint16_t SDA_Pin);

#endif
