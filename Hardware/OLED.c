#include "stm32f10x.h"
#include "OLED_Font.h"
#include "OLED.h"

/*引脚配置*/
#define OLED_W_SCL(x)		GPIO_WriteBit(GPIOA, GPIO_Pin_12, (BitAction)(x))
#define OLED_W_SDA(x)		GPIO_WriteBit(GPIOA, GPIO_Pin_11, (BitAction)(x))

// OLED互斥锁定义
SemaphoreHandle_t xOLEDMutex = NULL;

/*引脚初始化*/
void OLED_I2C_Init(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
	
	GPIO_InitTypeDef GPIO_InitStructure;
 	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12;
 	GPIO_Init(GPIOA, &GPIO_InitStructure);
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11;
 	GPIO_Init(GPIOA, &GPIO_InitStructure);
	
	OLED_W_SCL(1);
	OLED_W_SDA(1);
}

/**
  * @brief  I2C开始
  * @param  无
  * @retval 无
  */
void OLED_I2C_Start(void)
{
	OLED_W_SDA(1);
	OLED_W_SCL(1);
	OLED_W_SDA(0);
	OLED_W_SCL(0);
}

/**
  * @brief  I2C停止
  * @param  无
  * @retval 无
  */
void OLED_I2C_Stop(void)
{
	OLED_W_SDA(0);
	OLED_W_SCL(1);
	OLED_W_SDA(1);
}

/**
  * @brief  I2C发送一个字节
  * @param  Byte 要发送的一个字节
  * @retval 无
  */
void OLED_I2C_SendByte(uint8_t Byte)
{
	uint8_t i;
	for (i = 0; i < 8; i++)
	{
		OLED_W_SDA(!!(Byte & (0x80 >> i)));
		OLED_W_SCL(1);
		OLED_W_SCL(0);
	}
	OLED_W_SCL(1);	//额外的一个时钟，不处理应答信号
	OLED_W_SCL(0);
}

/**
  * @brief  OLED写命令
  * @param  Command 要写入的命令
  * @retval 无
  */
void OLED_WriteCommand(uint8_t Command)
{
	OLED_I2C_Start();
	OLED_I2C_SendByte(0x78);		//从机地址
	OLED_I2C_SendByte(0x00);		//写命令
	OLED_I2C_SendByte(Command); 
	OLED_I2C_Stop();
}

/**
  * @brief  OLED写数据
  * @param  Data 要写入的数据
  * @retval 无
  */
void OLED_WriteData(uint8_t Data)
{
	OLED_I2C_Start();
	OLED_I2C_SendByte(0x78);		//从机地址
	OLED_I2C_SendByte(0x40);		//写数据
	OLED_I2C_SendByte(Data);
	OLED_I2C_Stop();
}

/**
  * @brief  OLED设置光标位置
  * @param  Y 以左上角为原点，向下方向的坐标，范围：0~7
  * @param  X 以左上角为原点，向右方向的坐标，范围：0~127
  * @retval 无
  */
void OLED_SetCursor(uint8_t Y, uint8_t X)
{
	OLED_WriteCommand(0xB0 | Y);					//设置Y位置
	OLED_WriteCommand(0x10 | ((X & 0xF0) >> 4));	//设置X位置高4位
	OLED_WriteCommand(0x00 | (X & 0x0F));			//设置X位置低4位
}

/**
  * @brief  OLED清屏
  * @param  无
  * @retval 无
  */
void OLED_Clear(void)
{  
	uint8_t i, j;
	
	// 获取互斥锁，保护整个清屏操作
	if(xOLEDMutex != NULL) {
		xSemaphoreTake(xOLEDMutex, portMAX_DELAY);
	}
	
	for (j = 0; j < 8; j++)
	{
		OLED_SetCursor(j, 0);
		for(i = 0; i < 128; i++)
		{
			OLED_WriteData(0x00);
		}
	}
	
	// 释放互斥锁
	if(xOLEDMutex != NULL) {
		xSemaphoreGive(xOLEDMutex);
	}
}

/**
  * @brief  OLED显示一个字符
  * @param  Line 行位置，范围：1~4
  * @param  Column 列位置，范围：1~16
  * @param  Char 要显示的一个字符，范围：ASCII可见字符
  * @retval 无
  */
void OLED_ShowChar(uint8_t Line, uint8_t Column, char Char)
{      	
	uint8_t i;
	
	// 获取互斥锁
	if(xOLEDMutex != NULL) {
		xSemaphoreTake(xOLEDMutex, portMAX_DELAY);
	}
	
	OLED_SetCursor((Line - 1) * 2, (Column - 1) * 8);		//设置光标位置在上半部分
	for (i = 0; i < 8; i++)
	{
		OLED_WriteData(OLED_F8x16[Char - ' '][i]);			//显示上半部分内容
	}
	OLED_SetCursor((Line - 1) * 2 + 1, (Column - 1) * 8);	//设置光标位置在下半部分
	for (i = 0; i < 8; i++)
	{
		OLED_WriteData(OLED_F8x16[Char - ' '][i + 8]);		//显示下半部分内容
	}
	
	// 释放互斥锁
	if(xOLEDMutex != NULL) {
		xSemaphoreGive(xOLEDMutex);
	}
}

/**
  * @brief  OLED显示字符串
  * @param  Line 起始行位置，范围：1~4
  * @param  Column 起始列位置，范围：1~16
  * @param  String 要显示的字符串，范围：ASCII可见字符
  * @retval 无
  */
void OLED_ShowString(uint8_t Line, uint8_t Column, char *String)
{
	uint8_t i;
	
	// 获取互斥锁（保护整个字符串显示）
	if(xOLEDMutex != NULL) {
		xSemaphoreTake(xOLEDMutex, portMAX_DELAY);
	}
	
	for (i = 0; String[i] != '\0'; i++)
	{
		uint8_t j;
		char Char = String[i];
		
		OLED_SetCursor((Line - 1) * 2, (Column + i - 1) * 8);
		for (j = 0; j < 8; j++)
		{
			OLED_WriteData(OLED_F8x16[Char - ' '][j]);
		}
		OLED_SetCursor((Line - 1) * 2 + 1, (Column + i - 1) * 8);
		for (j = 0; j < 8; j++)
		{
			OLED_WriteData(OLED_F8x16[Char - ' '][j + 8]);
		}
	}
	
	// 释放互斥锁
	if(xOLEDMutex != NULL) {
		xSemaphoreGive(xOLEDMutex);
	}
}

/**
  * @brief  OLED次方函数
  * @retval 返回值等于X的Y次方
  */
uint32_t OLED_Pow(uint32_t X, uint32_t Y)
{
	uint32_t Result = 1;
	while (Y--)
	{
		Result *= X;
	}
	return Result;
}

/**
  * @brief  OLED显示数字（十进制，正数）
  * @param  Line 起始行位置，范围：1~4
  * @param  Column 起始列位置，范围：1~16
  * @param  Number 要显示的数字，范围：0~4294967295
  * @param  Length 要显示数字的长度，范围：1~10
  * @retval 无
  */
void OLED_ShowNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length)
{
	uint8_t i;
	
	// 获取互斥锁
	if(xOLEDMutex != NULL) {
		xSemaphoreTake(xOLEDMutex, portMAX_DELAY);
	}
	
	for (i = 0; i < Length; i++)							
	{
		uint8_t j;
		char Char = Number / OLED_Pow(10, Length - i - 1) % 10 + '0';
		
		OLED_SetCursor((Line - 1) * 2, (Column + i - 1) * 8);
		for (j = 0; j < 8; j++)
		{
			OLED_WriteData(OLED_F8x16[Char - ' '][j]);
		}
		OLED_SetCursor((Line - 1) * 2 + 1, (Column + i - 1) * 8);
		for (j = 0; j < 8; j++)
		{
			OLED_WriteData(OLED_F8x16[Char - ' '][j + 8]);
		}
	}
	
	// 释放互斥锁
	if(xOLEDMutex != NULL) {
		xSemaphoreGive(xOLEDMutex);
	}
}

/**
  * @brief  OLED显示数字（十进制，带符号数）
  * @param  Line 起始行位置，范围：1~4
  * @param  Column 起始列位置，范围：1~16
  * @param  Number 要显示的数字，范围：-2147483648~2147483647
  * @param  Length 要显示数字的长度，范围：1~10
  * @retval 无
  */
void OLED_ShowSignedNum(uint8_t Line, uint8_t Column, int32_t Number, uint8_t Length)
{
	uint8_t i;
	uint32_t Number1;
	
	// 获取互斥锁
	if(xOLEDMutex != NULL) {
		xSemaphoreTake(xOLEDMutex, portMAX_DELAY);
	}
	
	if (Number >= 0)
	{
		OLED_SetCursor((Line - 1) * 2, (Column - 1) * 8);
		OLED_WriteData(OLED_F8x16['+' - ' '][0]);
		OLED_WriteData(OLED_F8x16['+' - ' '][1]);
		OLED_WriteData(OLED_F8x16['+' - ' '][2]);
		OLED_WriteData(OLED_F8x16['+' - ' '][3]);
		OLED_WriteData(OLED_F8x16['+' - ' '][4]);
		OLED_WriteData(OLED_F8x16['+' - ' '][5]);
		OLED_WriteData(OLED_F8x16['+' - ' '][6]);
		OLED_WriteData(OLED_F8x16['+' - ' '][7]);
		OLED_SetCursor((Line - 1) * 2 + 1, (Column - 1) * 8);
		OLED_WriteData(OLED_F8x16['+' - ' '][8]);
		OLED_WriteData(OLED_F8x16['+' - ' '][9]);
		OLED_WriteData(OLED_F8x16['+' - ' '][10]);
		OLED_WriteData(OLED_F8x16['+' - ' '][11]);
		OLED_WriteData(OLED_F8x16['+' - ' '][12]);
		OLED_WriteData(OLED_F8x16['+' - ' '][13]);
		OLED_WriteData(OLED_F8x16['+' - ' '][14]);
		OLED_WriteData(OLED_F8x16['+' - ' '][15]);
		Number1 = Number;
	}
	else
	{
		OLED_SetCursor((Line - 1) * 2, (Column - 1) * 8);
		OLED_WriteData(OLED_F8x16['-' - ' '][0]);
		OLED_WriteData(OLED_F8x16['-' - ' '][1]);
		OLED_WriteData(OLED_F8x16['-' - ' '][2]);
		OLED_WriteData(OLED_F8x16['-' - ' '][3]);
		OLED_WriteData(OLED_F8x16['-' - ' '][4]);
		OLED_WriteData(OLED_F8x16['-' - ' '][5]);
		OLED_WriteData(OLED_F8x16['-' - ' '][6]);
		OLED_WriteData(OLED_F8x16['-' - ' '][7]);
		OLED_SetCursor((Line - 1) * 2 + 1, (Column - 1) * 8);
		OLED_WriteData(OLED_F8x16['-' - ' '][8]);
		OLED_WriteData(OLED_F8x16['-' - ' '][9]);
		OLED_WriteData(OLED_F8x16['-' - ' '][10]);
		OLED_WriteData(OLED_F8x16['-' - ' '][11]);
		OLED_WriteData(OLED_F8x16['-' - ' '][12]);
		OLED_WriteData(OLED_F8x16['-' - ' '][13]);
		OLED_WriteData(OLED_F8x16['-' - ' '][14]);
		OLED_WriteData(OLED_F8x16['-' - ' '][15]);
		Number1 = -Number;
	}
	for (i = 0; i < Length; i++)							
	{
		uint8_t j;
		char Char = Number1 / OLED_Pow(10, Length - i - 1) % 10 + '0';
		
		OLED_SetCursor((Line - 1) * 2, (Column + i) * 8);
		for (j = 0; j < 8; j++)
		{
			OLED_WriteData(OLED_F8x16[Char - ' '][j]);
		}
		OLED_SetCursor((Line - 1) * 2 + 1, (Column + i) * 8);
		for (j = 0; j < 8; j++)
		{
			OLED_WriteData(OLED_F8x16[Char - ' '][j + 8]);
		}
	}
	
	// 释放互斥锁
	if(xOLEDMutex != NULL) {
		xSemaphoreGive(xOLEDMutex);
	}
}

/**
  * @brief  OLED显示数字（十六进制，正数）
  * @param  Line 起始行位置，范围：1~4
  * @param  Column 起始列位置，范围：1~16
  * @param  Number 要显示的数字，范围：0~0xFFFFFFFF
  * @param  Length 要显示数字的长度，范围：1~8
  * @retval 无
  */
void OLED_ShowHexNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length)
{
	uint8_t i, SingleNumber;
	
	// 获取互斥锁
	if(xOLEDMutex != NULL) {
		xSemaphoreTake(xOLEDMutex, portMAX_DELAY);
	}
	
	for (i = 0; i < Length; i++)							
	{
		uint8_t j;
		SingleNumber = Number / OLED_Pow(16, Length - i - 1) % 16;
		char Char;
		if (SingleNumber < 10)
		{
			Char = SingleNumber + '0';
		}
		else
		{
			Char = SingleNumber - 10 + 'A';
		}
		
		OLED_SetCursor((Line - 1) * 2, (Column + i - 1) * 8);
		for (j = 0; j < 8; j++)
		{
			OLED_WriteData(OLED_F8x16[Char - ' '][j]);
		}
		OLED_SetCursor((Line - 1) * 2 + 1, (Column + i - 1) * 8);
		for (j = 0; j < 8; j++)
		{
			OLED_WriteData(OLED_F8x16[Char - ' '][j + 8]);
		}
	}
	
	// 释放互斥锁
	if(xOLEDMutex != NULL) {
		xSemaphoreGive(xOLEDMutex);
	}
}

/**
  * @brief  OLED显示数字（二进制，正数）
  * @param  Line 起始行位置，范围：1~4
  * @param  Column 起始列位置，范围：1~16
  * @param  Number 要显示的数字，范围：0~1111 1111 1111 1111
  * @param  Length 要显示数字的长度，范围：1~16
  * @retval 无
  */
void OLED_ShowBinNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length)
{
	uint8_t i;
	
	// 获取互斥锁
	if(xOLEDMutex != NULL) {
		xSemaphoreTake(xOLEDMutex, portMAX_DELAY);
	}
	
	for (i = 0; i < Length; i++)							
	{
		uint8_t j;
		char Char = Number / OLED_Pow(2, Length - i - 1) % 2 + '0';
		
		OLED_SetCursor((Line - 1) * 2, (Column + i - 1) * 8);
		for (j = 0; j < 8; j++)
		{
			OLED_WriteData(OLED_F8x16[Char - ' '][j]);
		}
		OLED_SetCursor((Line - 1) * 2 + 1, (Column + i - 1) * 8);
		for (j = 0; j < 8; j++)
		{
			OLED_WriteData(OLED_F8x16[Char - ' '][j + 8]);
		}
	}
	
	// 释放互斥锁
	if(xOLEDMutex != NULL) {
		xSemaphoreGive(xOLEDMutex);
	}
}

/**
  * @brief  OLED初始化
  * @param  无
  * @retval 无
  */
void OLED_Init(void)
{
	uint32_t i, j;
	
	for (i = 0; i < 1000; i++)			//上电延时
	{
		for (j = 0; j < 1000; j++);
	}
	
	OLED_I2C_Init();			//端口初始化
	
	OLED_WriteCommand(0xAE);	//关闭显示
	
	OLED_WriteCommand(0xD5);	//设置显示时钟分频比/振荡器频率
	OLED_WriteCommand(0x80);
	
	OLED_WriteCommand(0xA8);	//设置多路复用率
	OLED_WriteCommand(0x3F);
	
	OLED_WriteCommand(0xD3);	//设置显示偏移
	OLED_WriteCommand(0x00);
	
	OLED_WriteCommand(0x40);	//设置显示开始行
	
	OLED_WriteCommand(0xA1);	//设置左右方向，0xA1正常 0xA0左右反置
	
	OLED_WriteCommand(0xC8);	//设置上下方向，0xC8正常 0xC0上下反置

	OLED_WriteCommand(0xDA);	//设置COM引脚硬件配置
	OLED_WriteCommand(0x12);
	
	OLED_WriteCommand(0x81);	//设置对比度控制
	OLED_WriteCommand(0xCF);

	OLED_WriteCommand(0xD9);	//设置预充电周期
	OLED_WriteCommand(0xF1);

	OLED_WriteCommand(0xDB);	//设置VCOMH取消选择级别
	OLED_WriteCommand(0x30);

	OLED_WriteCommand(0xA4);	//设置整个显示打开/关闭

	OLED_WriteCommand(0xA6);	//设置正常/倒转显示

	OLED_WriteCommand(0x8D);	//设置充电泵
	OLED_WriteCommand(0x14);

	OLED_WriteCommand(0xAF);	//开启显示
		
	OLED_Clear();				//OLED清屏
}

//Chinese
void OLED_ShowChinese(uint8_t Line, uint8_t Column, uint8_t Index)
{      	
	uint8_t i;
	
	// 获取互斥锁
	if(xOLEDMutex != NULL) {
		xSemaphoreTake(xOLEDMutex, portMAX_DELAY);
	}
	
	OLED_SetCursor((Line - 1) * 2, (Column - 1) * 8);		//设置光标位置在上半部分
	for (i = 0; i < 16; i++)
	{
		OLED_WriteData(OLED_F16x16[Index][i]);			//显示上半部分内容
	}
	OLED_SetCursor((Line - 1) * 2 + 1, (Column - 1) * 8);	//设置光标位置在下半部分
	for (i = 0; i < 16; i++)
	{
		OLED_WriteData(OLED_F16x16[Index][i + 16]);		//显示下半部分内容
	}
	
	// 释放互斥锁
	if(xOLEDMutex != NULL) {
		xSemaphoreGive(xOLEDMutex);
	}
}

/**
  * @brief  OLED显示大号数字（8x16字体双倍宽度显示，16x16效果）
  * @param  Line 起始行位置（1~3，因为占两行）
  * @param  Column 起始列位置（1~15，因为每个数字占2列）
  * @param  Number 要显示的数字（0-9）
  * @retval 无
  */
void OLED_ShowBigNum(uint8_t Line, uint8_t Column, uint8_t Number)
{
    uint8_t i;
    const uint8_t *pFont = OLED_F8x16[Number + 16]; // 数字0-9在字模库中从16开始
    
    // 获取互斥锁
    if(xOLEDMutex != NULL) {
        xSemaphoreTake(xOLEDMutex, portMAX_DELAY);
    }
    
    // 上半部分（第Line行）- 每个像素列显示两次，实现双倍宽度
    OLED_SetCursor((Line - 1) * 2, (Column - 1) * 8);
    for (i = 0; i < 8; i++)
    {
        uint8_t data = pFont[i];
        // 将8位数据扩展为16位效果（每列显示两次）
        OLED_WriteData(data);  // 原始列
        OLED_WriteData(data);  // 重复列，实现双倍宽度
    }
    
    // 下半部分（第Line+1行）
    OLED_SetCursor((Line - 1) * 2 + 1, (Column - 1) * 8);
    for (i = 0; i < 8; i++)
    {
        uint8_t data = pFont[i + 8];
        OLED_WriteData(data);  // 原始列
        OLED_WriteData(data);  // 重复列
    }
    
    // 释放互斥锁
    if(xOLEDMutex != NULL) {
        xSemaphoreGive(xOLEDMutex);
    }
}

/**
  * @brief  OLED显示大号字符（8x16双倍宽度）
  * @param  Line 起始行位置
  * @param  Column 起始列位置
  * @param  Char 要显示的字符
  * @retval 无
  */
void OLED_ShowBigChar(uint8_t Line, uint8_t Column, char Char)
{
    uint8_t i;
    const uint8_t *pFont = OLED_F8x16[Char - ' '];
    
    if(xOLEDMutex != NULL) {
        xSemaphoreTake(xOLEDMutex, portMAX_DELAY);
    }
    
    // 上半部分
    OLED_SetCursor((Line - 1) * 2, (Column - 1) * 8);
    for (i = 0; i < 8; i++)
    {
        uint8_t data = pFont[i];
        OLED_WriteData(data);
        OLED_WriteData(data);
    }
    
    // 下半部分
    OLED_SetCursor((Line - 1) * 2 + 1, (Column - 1) * 8);
    for (i = 0; i < 8; i++)
    {
        uint8_t data = pFont[i + 8];
        OLED_WriteData(data);
        OLED_WriteData(data);
    }
    
    if(xOLEDMutex != NULL) {
        xSemaphoreGive(xOLEDMutex);
    }
}

/**
  * @brief  OLED显示大号数字（占两行高度，正常宽度）
  * @param  Line 起始行（1或2，因为占两行）
  * @param  Column 起始列（1-16）
  * @param  Number 数字0-9
  * @retval 无
  */
void OLED_ShowTallNum(uint8_t Line, uint8_t Column, uint8_t Number)
{
    uint8_t i;
    const uint8_t *pFont = OLED_F8x16[Number + 16]; // 数字0-9从索引16开始
    
    if(xOLEDMutex != NULL) {
        xSemaphoreTake(xOLEDMutex, portMAX_DELAY);
    }
    
    // 第1行（上半部分）：显示字模的上半部分（8行像素）
    OLED_SetCursor((Line - 1) * 2, (Column - 1) * 8);
    for (i = 0; i < 8; i++)
    {
        OLED_WriteData(pFont[i]);
    }
    
    // 第2行（下半部分）：显示字模的下半部分（8行像素）
    OLED_SetCursor((Line - 1) * 2 + 1, (Column - 1) * 8);
    for (i = 0; i < 8; i++)
    {
        OLED_WriteData(pFont[i + 8]);
    }
    
    if(xOLEDMutex != NULL) {
        xSemaphoreGive(xOLEDMutex);
    }
}

/**
  * @brief  OLED显示大号字符（占两行高度）
  */
void OLED_ShowTallChar(uint8_t Line, uint8_t Column, char Char)
{
    uint8_t i;
    const uint8_t *pFont = OLED_F8x16[Char - ' '];
    
    if(xOLEDMutex != NULL) {
        xSemaphoreTake(xOLEDMutex, portMAX_DELAY);
    }
    
    // 上半部分
    OLED_SetCursor((Line - 1) * 2, (Column - 1) * 8);
    for (i = 0; i < 8; i++)
    {
        OLED_WriteData(pFont[i]);
    }
    
    // 下半部分
    OLED_SetCursor((Line - 1) * 2 + 1, (Column - 1) * 8);
    for (i = 0; i < 8; i++)
    {
        OLED_WriteData(pFont[i + 8]);
    }
    
    if(xOLEDMutex != NULL) {
        xSemaphoreGive(xOLEDMutex);
    }
}
/**
  * @brief  OLED显示16x32超大字符（数字0-9或冒号:）
  * @param  Line 起始行位置（1或2，因为占4行）
  * @param  Column 起始列位置（1-15，每个字符占2列）
  * @param  Char 字符'0'-'9'或':'
  * @retval 无
  */
void OLED_Show16x32Char(uint8_t Line, uint8_t Column, char Char)
{
    uint8_t i;
    uint8_t idx;
    uint8_t page;
    
    // 转换索引
    if(Char >= '0' && Char <= '9') idx = Char - '0';
    else if(Char == ':') idx = 10;
    else return;
    
    if(Column > 15) return;
    if(Line < 1 || Line > 3) return;  // 只能从第1-3字符行开始
    
    if(xOLEDMutex != NULL) xSemaphoreTake(xOLEDMutex, portMAX_DELAY);
    
    // 16×32 = 4个Page（每Page 8行像素）
    // Line=2 对应 Page 2,3,4,5（第16-47像素行）
    uint8_t startPage = (Line - 1) * 2;  // 字符行2 → Page 2
    
    for(page = 0; page < 4; page++) {
        OLED_SetCursor(startPage + page, (Column - 1) * 8);
        // 每个Page写16字节（2列 × 8行）
        for(i = 0; i < 16; i++) {
            OLED_WriteData(OLED_F16x32[idx][page * 16 + i]);
        }
    }
    
    if(xOLEDMutex != NULL) xSemaphoreGive(xOLEDMutex);
}
