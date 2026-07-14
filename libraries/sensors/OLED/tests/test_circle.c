#include "../oled.h"

void Test_Circle(void)
{
    // Draw some circles of varying sizes
    OLED_DrawCircle(32, 32, 10, true);
    OLED_DrawCircle(64, 32, 20, true);
    OLED_DrawCircle(96, 32, 30, true);
}
