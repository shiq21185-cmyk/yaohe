#include "stm32f10x.h"
#include "PWM.h"
#include "Servo.h"

/**
  * 函    数：舵机初始化（PB3）
  * 参    数：无
  * 返 回 值：无
  */
void Servo_Init(void)
{
	PWM_Init();									//初始化PB3的PWM
}

/**
  * 函    数：舵机设置角度
  * 参    数：Angle 要设置的舵机角度，范围：0~180
  * 返 回 值：无
  */
void Servo_SetAngle(float Angle)
{
	// 限制角度范围
	if(Angle < 0) Angle = 0;
	if(Angle > 180) Angle = 180;
	
	// 角度转脉宽：0度=0.5ms(500), 180度=2.5ms(2500)
	uint16_t pulse = (uint16_t)(Angle / 180.0f * 2000.0f + 500.0f);
	PWM_SetCompare2(pulse);						//使用通道2
}

/**
  * 函    数：舵机设置脉宽（直接控制）
  * 参    数：pulse 脉宽值，范围：500~2500（微秒）
  * 返 回 值：无
  */
void Servo_SetPulse(uint16_t pulse)
{
	if(pulse < 500) pulse = 500;
	if(pulse > 2500) pulse = 2500;
	PWM_SetCompare2(pulse);
}
