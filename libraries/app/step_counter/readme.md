# Step Counter

Simple threshold-based step counter using 3-axis accelerometer data. The algorithm is sensor-independent as long as the acceleration input is provided in **mg (milli-g)**.

## Features

- Threshold-based step detection
- Moving average filtering to reduce noise
- Debounce to prevent multiple detections for a single step
- Compatible with different accelerometer drivers

---

## API

### `void step_counter_init(step_counter_t *sc)`

Initializes the step counter instance.

```c
step_counter_t sc;
step_counter_init(&sc);
```

---

### `int step_counter_update(step_counter_t *sc,
                             int32_t x_mg,
                             int32_t y_mg,
                             int32_t z_mg,
                             long now_ms)`

Processes one accelerometer sample.

**Parameters**

| Parameter |        Description       |
|-----------|--------------------------|
| `x_mg`    | X-axis acceleration (mg) |
| `y_mg`    | Y-axis acceleration (mg) |
| `z_mg`    | Z-axis acceleration (mg) |
| `now_ms`  | Current timestamp in ms  |

**Return**

- `1` : New step detected
- `0` : No step detected

Example:

```c
int stepped = step_counter_update(
    &sc,
    accel.x,
    accel.y,
    accel.z,
    now_ms
);

if (stepped) {
    printf("Step detected\n");
}
```

---

### `uint32_t step_counter_get(const step_counter_t *sc)`

Returns the accumulated number of detected steps.

```c
uint32_t total = step_counter_get(&sc);
```

---

### `void step_counter_reset(step_counter_t *sc)`

Resets the step count.

```c
step_counter_reset(&sc);
```

---

# Sensor Requirements

The algorithm is independent of the accelerometer model.

The driver only needs to provide:

- X-axis acceleration in **mg**
- Y-axis acceleration in **mg**
- Z-axis acceleration in **mg**

Sampling rate around **50–100 Hz** is recommended.

---

# Using with MPU6050

The MPU6050 driver should output acceleration values in **mg**.

Example:

```c
accel_data_t accel;

mpu6050_read_accel_mg(&accel);

step_counter_update(
    &sc,
    accel.x,
    accel.y,
    accel.z,
    now_ms
);
```

---

# Using with LIS3DHTR

The provided LIS3DHTR driver already returns acceleration in **mg**.

Default configuration:

|     Parameter     |  Value   |
|-------------------|----------|
|       Range       |    ±2 g  |
|  Output Data Rate |  100 Hz  |
|    Sensitivity    | 4 mg/LSB |

Example:

```c
accel_data_t accel;

lis3dhtr_read_accel(i2c, &accel);

step_counter_update(
    &sc,
    accel.x,
    accel.y,
    accel.z,
    now_ms
);
```

---

# Example

```c
step_counter_t sc;
step_counter_init(&sc);

while (1)
{
    long now_ms = pos_tick_get_counter_ms();

    accel_data_t accel;
    lis3dhtr_read_accel(i2c, &accel);

    if (step_counter_update(
            &sc,
            accel.x,
            accel.y,
            accel.z,
            now_ms))
    {
        printf("Steps: %lu\n",
               (unsigned long)step_counter_get(&sc));
    }
}
```

---

# Notes

- Threshold values may require tuning depending on sensor placement and application.
- For best performance, keep the accelerometer output data rate consistent (recommended 50–100 Hz).
- The algorithm uses moving average filtering and a debounce interval to reduce false step detections.