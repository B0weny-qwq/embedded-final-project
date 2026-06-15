#include "app.h"
#include "system_clock.h"

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    App_Init();

    while (1) {
        App_Tick();
    }
}

void Error_Handler(void)
{
    __disable_irq();
    while (1) {
    }
}
