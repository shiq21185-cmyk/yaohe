#ifndef __SERVO_H
#define __SERVO_H

#include "stm32f10x.h"

void Servo_Init(void);
void Servo_SetAngle(float Angle);
void Servo_SetPulse(uint16_t pulse);			//Ö±½ÓÉèÖÃÂö¿í

#endif
