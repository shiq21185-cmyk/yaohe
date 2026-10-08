#include "key.h"
#include "FreeRTOS.h"
#include "task.h"

/* 四个按键分别连接到 PB8、PB9、PB6、PB7。 */
#define KEY1_GPIO_PORT GPIOB
#define KEY1_GPIO_PIN GPIO_Pin_8
#define KEY2_GPIO_PORT GPIOB
#define KEY2_GPIO_PIN GPIO_Pin_9
#define KEY3_GPIO_PORT GPIOB
#define KEY3_GPIO_PIN GPIO_Pin_6
#define KEY4_GPIO_PORT GPIOB
#define KEY4_GPIO_PIN GPIO_Pin_7

#define KEY_DEBOUNCE_MS 30

static Key_t keys[KEY_NUM];

/* 初始化四个下拉输入按键及其软件状态。 */
void Key_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPD;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;

    GPIO_InitStructure.GPIO_Pin = KEY1_GPIO_PIN;
    GPIO_Init(KEY1_GPIO_PORT, &GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Pin = KEY2_GPIO_PIN;
    GPIO_Init(KEY2_GPIO_PORT, &GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Pin = KEY3_GPIO_PIN;
    GPIO_Init(KEY3_GPIO_PORT, &GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Pin = KEY4_GPIO_PIN;
    GPIO_Init(KEY4_GPIO_PORT, &GPIO_InitStructure);

    for(uint8_t i = 0; i < KEY_NUM; i++)
    {
        keys[i].state = KEY_IDLE;
        keys[i].press_time = 0;
        keys[i].press_flag = 0;
    }
    keys[KEY1].GPIOx = KEY1_GPIO_PORT; keys[KEY1].GPIO_Pin = KEY1_GPIO_PIN;
    keys[KEY2].GPIOx = KEY2_GPIO_PORT; keys[KEY2].GPIO_Pin = KEY2_GPIO_PIN;
    keys[KEY3].GPIOx = KEY3_GPIO_PORT; keys[KEY3].GPIO_Pin = KEY3_GPIO_PIN;
    keys[KEY4].GPIOx = KEY4_GPIO_PORT; keys[KEY4].GPIO_Pin = KEY4_GPIO_PIN;
}

/* 下拉输入中，高电平表示按键被按下。 */
static uint8_t Key_ReadPin(uint8_t key_num)
{
    if(key_num >= KEY_NUM) return 0;
    return GPIO_ReadInputDataBit(keys[key_num].GPIOx, keys[key_num].GPIO_Pin) == Bit_SET;
}

/* 轮询按键并执行按下、消抖、释放三态检测。 */
void Key_Scan(void)
{
    for(uint8_t i = 0; i < KEY_NUM; i++)
    {
        uint8_t pin_state = Key_ReadPin(i);
        uint32_t current_tick = xTaskGetTickCount();
        switch(keys[i].state)
        {
            case KEY_IDLE:
                if(pin_state) { keys[i].state = KEY_PRESS; keys[i].press_time = current_tick; }
                break;
            case KEY_PRESS:
                if(pin_state)
                {
                    if((current_tick - keys[i].press_time) >= KEY_DEBOUNCE_MS)
                    {
                        keys[i].press_flag = 1;
                        keys[i].state = KEY_RELEASE;
                    }
                }
                else keys[i].state = KEY_IDLE;
                break;
            case KEY_RELEASE:
                if(!pin_state) keys[i].state = KEY_IDLE;
                break;
            default:
                keys[i].state = KEY_IDLE;
                break;
        }
    }
}

/* 读取并清除一次按下事件。 */
uint8_t Key_GetPress(uint8_t key_num)
{
    if(key_num >= KEY_NUM) return 0;
    if(keys[key_num].press_flag)
    {
        keys[key_num].press_flag = 0;
        return 1;
    }
    return 0;
}
