/**
  * @file           : MyI2C.c
  * @brief          : 软件I2C总线驱动（HAL库版本，支持多引脚）
  * @author         : CHSGY
  * @date           : 2026-06-05
  *
  * @note           : 从标准库MyI2C移植
  *                   GPIO操作：GPIO_WriteBit → HAL_GPIO_WritePin/ReadPin
  *                   延时操作：Delay_us → delay_us（DWT实现）
  *                   引脚配置：硬编码 → 参数化传入，支持多组I2C总线
  *
  *                   GPIO引脚由各模块的调用方通过CubeMX配置（开漏输出模式）
  *                   此模块不负责GPIO初始化，仅提供I2C时序协议
  */

#include "MyI2C.h"
#include "delay.h"

/*引脚配置层*********************/

/**
  * @brief  I2C写SCL引脚电平
  * @param  SCL_GPIOx: SCL引脚所在GPIO端口
  * @param  SCL_Pin: SCL引脚号
  * @param  BitValue: 要写入的电平值，范围：0/1
  */
static void MyI2C_W_SCL(GPIO_TypeDef *SCL_GPIOx, uint16_t SCL_Pin, uint8_t BitValue)
{
	HAL_GPIO_WritePin(SCL_GPIOx, SCL_Pin, (GPIO_PinState)BitValue);
}

/**
  * @brief  I2C写SDA引脚电平
  * @param  SDA_GPIOx: SDA引脚所在GPIO端口
  * @param  SDA_Pin: SDA引脚号
  * @param  BitValue: 要写入的电平值，范围：0/1
  */
static void MyI2C_W_SDA(GPIO_TypeDef *SDA_GPIOx, uint16_t SDA_Pin, uint8_t BitValue)
{
	HAL_GPIO_WritePin(SDA_GPIOx, SDA_Pin, (GPIO_PinState)BitValue);
}

/**
  * @brief  I2C读SDA引脚电平
  * @param  SDA_GPIOx: SDA引脚所在GPIO端口
  * @param  SDA_Pin: SDA引脚号
  * @retval SDA引脚电平值，范围：0/1
  */
static uint8_t MyI2C_R_SDA(GPIO_TypeDef *SDA_GPIOx, uint16_t SDA_Pin)
{
	return HAL_GPIO_ReadPin(SDA_GPIOx, SDA_Pin);
}

/*********************引脚配置层*/


/*协议层*********************/

/**
  * @brief  I2C起始
  * @param  SCL_GPIOx, SCL_Pin: SCL引脚参数
  * @param  SDA_GPIOx, SDA_Pin: SDA引脚参数
  */
void MyI2C_Start(GPIO_TypeDef *SCL_GPIOx, uint16_t SCL_Pin,
                 GPIO_TypeDef *SDA_GPIOx, uint16_t SDA_Pin)
{
	MyI2C_W_SDA(SDA_GPIOx, SDA_Pin, 1);		/* 释放SDA，确保SDA为高电平 */
	MyI2C_W_SCL(SCL_GPIOx, SCL_Pin, 1);		/* 释放SCL，确保SCL为高电平 */
	MyI2C_W_SDA(SDA_GPIOx, SDA_Pin, 0);		/* 在SCL高电平期间，拉低SDA，产生起始信号 */
	MyI2C_W_SCL(SCL_GPIOx, SCL_Pin, 0);		/* 起始后把SCL也拉低，即为了占用总线，也为了方便总线时序的拼接 */
}

/**
  * @brief  I2C终止
  * @param  SCL_GPIOx, SCL_Pin: SCL引脚参数
  * @param  SDA_GPIOx, SDA_Pin: SDA引脚参数
  */
void MyI2C_Stop(GPIO_TypeDef *SCL_GPIOx, uint16_t SCL_Pin,
                GPIO_TypeDef *SDA_GPIOx, uint16_t SDA_Pin)
{
	MyI2C_W_SDA(SDA_GPIOx, SDA_Pin, 0);		/* 拉低SDA，确保SDA为低电平 */
	MyI2C_W_SCL(SCL_GPIOx, SCL_Pin, 1);		/* 释放SCL，使SCL呈现高电平 */
	MyI2C_W_SDA(SDA_GPIOx, SDA_Pin, 1);		/* 在SCL高电平期间，释放SDA，产生终止信号 */
}

/**
  * @brief  I2C发送一个字节
  * @param  SCL_GPIOx, SCL_Pin: SCL引脚参数
  * @param  SDA_GPIOx, SDA_Pin: SDA引脚参数
  * @param  Byte: 要发送的一个字节数据，范围：0x00~0xFF
  */
void MyI2C_SendByte(GPIO_TypeDef *SCL_GPIOx, uint16_t SCL_Pin,
                    GPIO_TypeDef *SDA_GPIOx, uint16_t SDA_Pin,
                    uint8_t Byte)
{
	uint8_t i;
	/* 循环8次，主机依次发送数据的每一位 */
	for (i = 0; i < 8; i++)
	{
		/* 两个!的作用是，让所有非零的值变为1 */
		MyI2C_W_SDA(SDA_GPIOx, SDA_Pin, !!(Byte & (0x80 >> i)));
		MyI2C_W_SCL(SCL_GPIOx, SCL_Pin, 1);	/* 释放SCL，从机在SCL高电平期间读取SDA */
		MyI2C_W_SCL(SCL_GPIOx, SCL_Pin, 0);	/* 拉低SCL，主机开始发送下一位数据 */
	}
}

/**
  * @brief  I2C接收一个字节
  * @param  SCL_GPIOx, SCL_Pin: SCL引脚参数
  * @param  SDA_GPIOx, SDA_Pin: SDA引脚参数
  * @retval 接收到的一个字节数据，范围：0x00~0xFF
  */
uint8_t MyI2C_ReceiveByte(GPIO_TypeDef *SCL_GPIOx, uint16_t SCL_Pin,
                          GPIO_TypeDef *SDA_GPIOx, uint16_t SDA_Pin)
{
	uint8_t i, Byte = 0x00;
	MyI2C_W_SDA(SDA_GPIOx, SDA_Pin, 1);		/* 接收前，主机先确保释放SDA，避免干扰从机的数据发送 */
	for (i = 0; i < 8; i++)
	{
		MyI2C_W_SCL(SCL_GPIOx, SCL_Pin, 1);	/* 释放SCL，主机在SCL高电平期间读取SDA */
		if (MyI2C_R_SDA(SDA_GPIOx, SDA_Pin)) {Byte |= (0x80 >> i);}
		MyI2C_W_SCL(SCL_GPIOx, SCL_Pin, 0);	/* 拉低SCL，从机在SCL低电平期间写入SDA */
	}
	return Byte;
}

/**
  * @brief  I2C发送应答位
  * @param  SCL_GPIOx, SCL_Pin: SCL引脚参数
  * @param  SDA_GPIOx, SDA_Pin: SDA引脚参数
  * @param  AckBit: 要发送的应答位，范围：0~1，0表示应答，1表示非应答
  */
void MyI2C_SendAck(GPIO_TypeDef *SCL_GPIOx, uint16_t SCL_Pin,
                   GPIO_TypeDef *SDA_GPIOx, uint16_t SDA_Pin,
                   uint8_t AckBit)
{
	MyI2C_W_SDA(SDA_GPIOx, SDA_Pin, AckBit);	/* 主机把应答位数据放到SDA线 */
	MyI2C_W_SCL(SCL_GPIOx, SCL_Pin, 1);		/* 释放SCL，从机在SCL高电平期间，读取应答位 */
	MyI2C_W_SCL(SCL_GPIOx, SCL_Pin, 0);		/* 拉低SCL，开始下一个时序模块 */
}

/**
  * @brief  I2C接收应答位
  * @param  SCL_GPIOx, SCL_Pin: SCL引脚参数
  * @param  SDA_GPIOx, SDA_Pin: SDA引脚参数
  * @retval 接收到的应答位，范围：0~1，0表示应答，1表示非应答
  */
uint8_t MyI2C_ReceiveAck(GPIO_TypeDef *SCL_GPIOx, uint16_t SCL_Pin,
                         GPIO_TypeDef *SDA_GPIOx, uint16_t SDA_Pin)
{
	uint8_t AckBit;
	MyI2C_W_SDA(SDA_GPIOx, SDA_Pin, 1);		/* 接收前，主机先确保释放SDA，避免干扰从机的数据发送 */
	MyI2C_W_SCL(SCL_GPIOx, SCL_Pin, 1);		/* 释放SCL，主机在SCL高电平期间读取SDA */
	AckBit = MyI2C_R_SDA(SDA_GPIOx, SDA_Pin);	/* 将应答位存储到变量里 */
	MyI2C_W_SCL(SCL_GPIOx, SCL_Pin, 0);		/* 拉低SCL，开始下一个时序模块 */
	return AckBit;
}

/*********************协议层*/
