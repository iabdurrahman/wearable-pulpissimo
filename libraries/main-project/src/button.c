/* The GPIO function in this module is adapted from the Arduino-Ported
 * Pulpissimo Library */

#include "button.h"
#include "pulp.h"

static inline uint32_t digitalPinToPad(uint8_t pin){

	/* Ensure the pin index doesn't exceed the mapping table's size*/
    if (pin >= (sizeof(digital_pin_to_pad) / sizeof(digital_pin_to_pad[0])))
        return NOT_A_PIN;
    return digital_pin_to_pad[pin];
}

static void setPadmuxToGPIO(uint32_t pad_index)
{
    uint32_t reg_addr;
    uint32_t shift = (pad_index % 16) * 2;

    if (pad_index < 16)
        reg_addr = PADMUX_0;
    else
        reg_addr = PADMUX_1;

    uint32_t reg_val = pulp_read32(reg_addr);

    reg_val &= ~(0x3 << shift);
    reg_val |=  (0x1 << shift);

    pulp_write32(reg_addr, reg_val);
}

void buttonHardwareInit(void)
{
    /* Padmux: connect button pads to GPIO */
    setPadmuxToGPIO(digitalPinToPad(BOARD_BTN1_PAD_MUX_INDEX));
    setPadmuxToGPIO(digitalPinToPad(BOARD_BTN2_PAD_MUX_INDEX));

    /* Enable button GPIOs */
    uint32_t current_en = hal_gpio_en_get();
    current_en |= (1 << BTN1) | (1 << BTN2);
    hal_gpio_en_set(current_en);

    /* Configure GPIOs as inputs */
    hal_gpio_set_dir(1 << BTN1, ARCHI_GPIO_PADDIR_IN);
    hal_gpio_set_dir(1 << BTN2, ARCHI_GPIO_PADDIR_IN);
}

void buttonInit(ButtonState *btn)
{
    btn->lastState = !BUTTON_ACTIVE;
    btn->pressed = 0;
    btn->holdReported = 0;
    btn->pressTime = 0;
    btn->lastChange = 0;
}

int buttonUpdate(ButtonState *btn, int pin)
{
    uint32_t portValue = hal_gpio_get_value();
    int state = (portValue >> pin) & 0x1;

    uint64_t now = pos_tick_get_counter_us();

    if (state != btn->lastState)
    {
        if ((now - btn->lastChange) < DEBOUNCE_US)
            return BUTTON_NONE;

        btn->lastChange = now;
        btn->lastState = state;

        if (state == BUTTON_ACTIVE)
        {
            btn->pressed = 1;
            btn->holdReported = 0;
            btn->pressTime = now;
        }
        else
        {
            if (btn->pressed)
            {
                btn->pressed = 0;

                if (!btn->holdReported)
                    return BUTTON_SHORT;
            }
        }
    }

    if (btn->pressed && !btn->holdReported)
    {
        if ((now - btn->pressTime) >= HOLD_TIME_US)
        {
            btn->holdReported = 1;
            return BUTTON_HOLD;
        }
    }

    return BUTTON_NONE;
}