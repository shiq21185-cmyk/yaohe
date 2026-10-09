#ifndef __BSP_I2C_H
#define __BSP_I2C_H

#include "stm32f10x.h"
#include <stdint.h>

/* ==========================================================================
 * 软件 I2C 主机语义层
 * --------------------------------------------------------------------------
 * SH1106 这类从机只需要「起始 -> 写字节 -> 停止」三个动作，本层把这套时序
 * 固化下来，引脚操作全部走 BSP_GPIO。上层驱动只表达语义：
 *     BSP_SoftI2C_WriteFrame(0x78, 0x00, &cmd, 1);
 * 驱动里不再出现 SCL/SDA 的位操作。
 *
 * 约定：
 *   1) 总线引脚在 BSP_SoftI2C_Init 时一次性注册并配置为开漏输出
 *      （高电平依赖外部上拉，可线与）。本工程只有一条软件 I2C 总线。
 *   2) 本层**不做并发保护**：总线占用属于显示语义，由调用方（任务）仲裁。
 *   3) 时序与原 OLED_I2C_* 实现逐拍等价：每个字节后多打一个时钟吃掉应答位，
 *      不解析 ACK（SH1106 单向写屏不需要应答判断）。
 * ========================================================================== */

typedef struct
{
    GPIO_TypeDef *scl_port;
    uint16_t      scl_pin;
    GPIO_TypeDef *sda_port;
    uint16_t      sda_pin;
} BSP_SoftI2C_Bus_t;

/* 注册并初始化一条软件 I2C 总线（SCL/SDA 开漏输出，空闲均为高） */
void BSP_SoftI2C_Init(const BSP_SoftI2C_Bus_t *bus);

/* 起始条件：SCL 为高时 SDA 由高变低 */
void BSP_SoftI2C_Start(void);

/* 停止条件：SCL 为高时 SDA 由低变高 */
void BSP_SoftI2C_Stop(void);

/* 写一个字节（MSB 在前），随后多打一个时钟吃掉从机应答位 */
void BSP_SoftI2C_WriteByte(uint8_t byte);

/* 一帧完整传输：起始 + 从机地址 + 控制字节 + data[0..len-1] + 停止。
 * control 取 0x00 表示后续为命令，0x40 表示后续为显示数据。 */
void BSP_SoftI2C_WriteFrame(uint8_t slave_addr, uint8_t control,
                            const uint8_t *data, uint16_t len);

#endif
