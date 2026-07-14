# Pulpissimo OLED Driver

A simple, lightweight C driver for OLED displays (SSD1306 / SH1106) on the Pulpissimo RISC-V platform. It uses hardware I2C and supports basic graphics and text drawing.

## Quick Start

1. Connect your OLED to the I2C pins (default address `0x3C`).
2. Set up the Pulpissimo environment:
   ```bash
   source /path/to/pulp-runtime/configs/pulpissimo.sh
   ```
3. Build and run the project:
   ```bash
   make clean all run platform=fpga
   ```
*(Use `platform=gvsoc` to run on the software simulator).*

## Available Functions

### Setup & Control
- `void OLED_Init(void)`: Initializes the display.
- `void OLED_Clear(void)`: Clears the screen buffer.
- `void OLED_Update(void)`: Sends the buffer to the screen. Call this after drawing!
- `void OLED_DisplayOn(void)` / `OLED_DisplayOff(void)`: Turns the screen on or off.

### Drawing Shapes
- `void OLED_DrawPixel(x, y, color)`: Draws a single dot.
- `void OLED_DrawLine(x0, y0, x1, y1, color)`: Draws a straight line.
- `void OLED_DrawRect(x, y, w, h, color)`: Draws a rectangle outline.
- `void OLED_FillRect(x, y, w, h, color)`: Draws a solid rectangle.
- `void OLED_DrawCircle(x, y, r, color)`: Draws a circle.

### Drawing Text
- `void OLED_DrawChar(x, y, char, color)`: Draws a single character.
- `void OLED_DrawString(x, y, "Text", color)`: Draws a text string. Supports standard letters, numbers, and symbols.

> **Note:** The `color` parameter should be `true` (white/on) or `false` (black/off).

## Example Code

```c
#include "oled.h"

int main(void) {
    OLED_Init();
    OLED_Clear();

    // Draw some text and shapes
    OLED_DrawString(10, 10, "Hello 123!", true);
    OLED_FillRect(20, 30, 40, 20, true); // Draw a filled rectangle

    OLED_Update(); // Send changes to the screen
    
    while(1) {}
    return 0;
}
```
