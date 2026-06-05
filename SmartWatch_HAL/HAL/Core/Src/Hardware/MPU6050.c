/**
  * @file           : MPU6050.c
  * @brief          : MPU6050六轴传感器驱动（HAL库版本）
  * @author         : CHSGY
  * @date           : 2026-06-05
  *
  * @note           : 从标准库移植，使用独立MyI2C模块进行I2C通信
  *                   MPU6050使用PB10(SCL)/PB11(SDA)，引脚由CubeMX配置为开漏输出
  */

#include "Hardware/MPU6050.h"
#include "MyI2C.h"
#include "delay.h"

#define MPU6050_ADDRESS		0xD0		/* MPU6050的I2C从机地址 */

/* 调用宏：简化MyI2C函数调用，隐藏引脚参数 */
#define MPU_I2C_Start()			MyI2C_Start(MPU_SCL_GPIO_Port, MPU_SCL_Pin, MPU_SDA_GPIO_Port, MPU_SDA_Pin)
#define MPU_I2C_Stop()			MyI2C_Stop(MPU_SCL_GPIO_Port, MPU_SCL_Pin, MPU_SDA_GPIO_Port, MPU_SDA_Pin)
#define MPU_I2C_SendByte(Byte)		MyI2C_SendByte(MPU_SCL_GPIO_Port, MPU_SCL_Pin, MPU_SDA_GPIO_Port, MPU_SDA_Pin, Byte)
#define MPU_I2C_ReceiveByte()		MyI2C_ReceiveByte(MPU_SCL_GPIO_Port, MPU_SCL_Pin, MPU_SDA_GPIO_Port, MPU_SDA_Pin)
#define MPU_I2C_SendAck(Ack)		MyI2C_SendAck(MPU_SCL_GPIO_Port, MPU_SCL_Pin, MPU_SDA_GPIO_Port, MPU_SDA_Pin, Ack)
#define MPU_I2C_ReceiveAck()		MyI2C_ReceiveAck(MPU_SCL_GPIO_Port, MPU_SCL_Pin, MPU_SDA_GPIO_Port, MPU_SDA_Pin)

/**
  * @brief  MPU6050写寄存器
  * @param  RegAddress: 寄存器地址，范围：参考MPU6050_Reg.h中的定义
  * @param  Data: 要写入寄存器的数据，范围：0x00~0xFF
  */
void MPU6050_WriteReg(uint8_t RegAddress, uint8_t Data)
{
	MPU_I2C_Start();						/* I2C起始 */
	MPU_I2C_SendByte(MPU6050_ADDRESS);		/* 发送从机地址，读写位为0，表示写 */
	MPU_I2C_ReceiveAck();					/* 接收应答 */
	MPU_I2C_SendByte(RegAddress);			/* 发送寄存器地址 */
	MPU_I2C_ReceiveAck();					/* 接收应答 */
	MPU_I2C_SendByte(Data);					/* 发送要写入寄存器的数据 */
	MPU_I2C_ReceiveAck();					/* 接收应答 */
	MPU_I2C_Stop();							/* I2C终止 */

	delay_us(2);							/* 写入后短暂延时，等待MPU6050处理 */
}

/**
  * @brief  MPU6050读寄存器
  * @param  RegAddress: 寄存器地址，范围：参考MPU6050_Reg.h中的定义
  * @retval 读取到的寄存器数据，范围：0x00~0xFF
  */
uint8_t MPU6050_ReadReg(uint8_t RegAddress)
{
	uint8_t Data;

	MPU_I2C_Start();						/* I2C起始 */
	MPU_I2C_SendByte(MPU6050_ADDRESS);		/* 发送从机地址，读写位为0，表示写 */
	MPU_I2C_ReceiveAck();					/* 接收应答 */
	MPU_I2C_SendByte(RegAddress);			/* 发送寄存器地址 */
	MPU_I2C_ReceiveAck();					/* 接收应答 */

	MPU_I2C_Start();						/* I2C重复起始 */
	MPU_I2C_SendByte(MPU6050_ADDRESS | 0x01);	/* 发送从机地址，读写位为1，表示读 */
	MPU_I2C_ReceiveAck();					/* 接收应答 */
	Data = MPU_I2C_ReceiveByte();			/* 接收指定寄存器的数据 */
	MPU_I2C_SendAck(1);						/* 发送非应答，结束读取 */
	MPU_I2C_Stop();							/* I2C终止 */

	return Data;
}

/**
  * @brief  MPU6050初始化
  * @retval 无
  * @note   GPIO引脚由CubeMX在MX_GPIO_Init()中完成配置（PB10-SCL, PB11-SDA 开漏输出）
  *         此函数仅负责通过I2C配置MPU6050寄存器
  */
void MPU6050_Init(void)
{
	MPU6050_WriteReg(MPU6050_PWR_MGMT_1, 0x01);	/* 解除睡眠，选择陀螺仪时钟 */
	MPU6050_WriteReg(MPU6050_PWR_MGMT_2, 0x00);	/* 6个轴均不待机 */
	MPU6050_WriteReg(MPU6050_SMPLRT_DIV, 0x09);	/* 采样分频为10 */
	MPU6050_WriteReg(MPU6050_CONFIG, 0x06);			/* 滤波参数给最大值 */
	MPU6050_WriteReg(MPU6050_GYRO_CONFIG, 0x18);	/* 陀螺仪选择最大量程 */
	MPU6050_WriteReg(MPU6050_ACCEL_CONFIG, 0x18);	/* 加速度计选择最大量程 */
}

/**
  * @brief  MPU6050获取ID号
  * @retval MPU6050的ID号，正常应为0x68
  */
uint8_t MPU6050_GetID(void)
{
	return MPU6050_ReadReg(MPU6050_WHO_AM_I);
}

/**
  * @brief  MPU6050获取六轴数据
  * @param  AccX: 加速度计X轴数据指针
  * @param  AccY: 加速度计Y轴数据指针
  * @param  AccZ: 加速度计Z轴数据指针
  * @param  GyroX: 陀螺仪X轴数据指针
  * @param  GyroY: 陀螺仪Y轴数据指针
  * @param  GyroZ: 陀螺仪Z轴数据指针
  * @retval 无
  * @note   输出为有符号16位整数，除以对应灵敏度即为物理量
  */
void MPU6050_GetData(int16_t *AccX, int16_t *AccY, int16_t *AccZ,
                     int16_t *GyroX, int16_t *GyroY, int16_t *GyroZ)
{
	uint8_t DataH, DataL;

	DataH = MPU6050_ReadReg(MPU6050_ACCEL_XOUT_H);
	DataL = MPU6050_ReadReg(MPU6050_ACCEL_XOUT_L);
	*AccX = (DataH << 8) | DataL;

	DataH = MPU6050_ReadReg(MPU6050_ACCEL_YOUT_H);
	DataL = MPU6050_ReadReg(MPU6050_ACCEL_YOUT_L);
	*AccY = (DataH << 8) | DataL;

	DataH = MPU6050_ReadReg(MPU6050_ACCEL_ZOUT_H);
	DataL = MPU6050_ReadReg(MPU6050_ACCEL_ZOUT_L);
	*AccZ = (DataH << 8) | DataL;

	DataH = MPU6050_ReadReg(MPU6050_GYRO_XOUT_H);
	DataL = MPU6050_ReadReg(MPU6050_GYRO_XOUT_L);
	*GyroX = (DataH << 8) | DataL;

	DataH = MPU6050_ReadReg(MPU6050_GYRO_YOUT_H);
	DataL = MPU6050_ReadReg(MPU6050_GYRO_YOUT_L);
	*GyroY = (DataH << 8) | DataL;

	DataH = MPU6050_ReadReg(MPU6050_GYRO_ZOUT_H);
	DataL = MPU6050_ReadReg(MPU6050_GYRO_ZOUT_L);
	*GyroZ = (DataH << 8) | DataL;
}
