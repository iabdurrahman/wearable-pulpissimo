# Wearable Device Layer Integration

Embedded wearable device application for the PULPissimo platform using a RISC-V core and the PULP runtime. The project integrates the display, buttons, sensors, application logic, and UI layers into a single hardware build.

## Features

- Watchface with software-based clock and date display
- Alarm setting and cancellation
- Notification display
- Step counting using the LIS3DHTR accelerometer
- Activity classification
- Heart rate measurement using MAX30102
- SpO2 measurement using MAX30102
- Battery status display
- OLED-based graphical user interface
- Button input with short-click and hold detection
- Shared I2C bus management for multiple peripherals

## Project Structure

```text
main-project/
├── include/          Application headers
├── src/              Main application and UI layers
│   └── layers/       Individual smartwatch layers
├── sensors/          Sensor drivers used by this application
├── Makefile          PULP runtime build configuration
└── README.md
```

Additional drivers and application modules are referenced from the parent project, including the OLED, RTC, accelerometer, MPU6050, MAX30102, step counter, activity classifier, and heart rate modules.

## Hardware Interface

### Buttons

| Button | GPIO | Function |
|---|---:|---|
| S16 | 14 | Left button |
| S15 | 15 | Right button |

Both buttons are configured as GPIO inputs during `buttonHardwareInit()`.

- Short press: navigate or interact with the current layer
- Hold: perform the secondary action of the current layer
- Debounce time: 20 ms
- Hold threshold: 1 second

## UI Layers

The application is organized into independent layers managed by the layer manager.

| Layer | Purpose |
|---|---|
| Watchface | Time, date, and alarm |
| Notification | Pending notification status |
| Step Count | Step tracking |
| Activity | Activity classification |
| Heart Rate | Heart rate measurement |
| SpO2 | Blood oxygen measurement |

The dedicated layer is rendered together with the active application layer.

## Shared I2C Bus

Several peripherals share the same I2C bus. Before communicating with a device, the application selects its 7-bit address and required bus speed through `i2c_shared_select()`.

Example:

```c
i2c_shared_select(device_address, baudrate);
```

Use the corresponding helper when available:

```c
i2c_shared_select_oled();
i2c_shared_select_rtc();
```

The shared I2C module caches the currently selected device configuration to avoid unnecessary bus reconfiguration.

## Important Notes

- Step counting is processed while the Step Count layer is active.
- Heart rate and SpO2 processing include timeouts so the UI does not remain blocked when the sensor does not provide valid data.
- The application assumes the required sensors are connected to the expected hardware interfaces and addresses.
- Keep the I2C device selection before any transaction because multiple peripherals share the same bus.

## Application Flow

At startup, the application initializes the system tick, OLED, GUI, dedicated layer, layer manager, and button hardware.

The main loop then:

1. Reads button events.
2. Handles layer navigation and interaction.
3. Checks watchface and alarm state.
4. Polls the active sensor layer.
5. Updates the display when required.

This keeps sensor processing tied to the active layer while maintaining a single main application loop.
