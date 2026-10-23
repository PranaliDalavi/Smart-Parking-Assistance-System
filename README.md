# Smart Parking Assistance System (STM32F407)

An ultrasonic parking sensor built on the **STM32F407G-DISC1** board. An HC-SR04 sensor measures the distance to an obstacle; the firmware sorts it into a warning zone, shows the zone on the board's LEDs (blinking faster as the obstacle gets closer, like a car's parking beeper), and prints the state and distance to a debug console.

The project is written in C with STM32CubeIDE and the STM32 HAL, and was built step by step with a focus on clean, industry-style embedded code: no magic numbers, small single-purpose functions, non-blocking timing, interrupt-driven measurement and fail-safe behaviour.

---

## Features

- **Distance measurement** with an HC-SR04 ultrasonic sensor at 1 µs resolution
- **Interrupt-driven echo capture** using TIM2 input capture (no busy-waiting on the echo pin)
- **Five system states:** SAFE, WARNING, DANGER, CRITICAL and SENSOR ERROR
- **Anti-flicker filter:** a new state is only accepted after 3 consecutive matching readings
- **Non-blocking LED blinking** whose speed increases as the obstacle gets closer
- **Fail-safe sensor error detection:** a missing or implausible echo is reported as an error, never as "safe"
- **Glitch rejection:** echo pulses shorter than a plausible minimum are discarded
- **Debug console over SWV/ITM:** immediate print on every state change plus a 500 ms heartbeat
- **Runs with or without the debugger:** the ITM output is guarded so `printf` can never hang the system

---

## Hardware

| Part | Notes |
|---|---|
| STM32F407G-DISC1 (STM32F407VG, Cortex-M4) | Programmed and powered via the on-board ST-LINK USB |
| HC-SR04 ultrasonic sensor | 5 V supply, 2–400 cm range |
| 2 × 1 kΩ resistors | Voltage divider for the 5 V echo signal |
| Breadboard and jumper wires | |
| On-board LEDs | PD12 green, PD13 orange, PD14 red, PD15 blue |

### Wiring

| HC-SR04 | STM32F407G-DISC1 |
|---|---|
| VCC | 5V |
| GND | GND |
| TRIG | PA1 (GPIO output) |
| ECHO | PA2 (TIM2_CH3) through a voltage divider |

```
HC-SR04 ECHO ──[1 kΩ]──┬──── PA2 (TIM2_CH3)
                       │
                     [1 kΩ]
                       │
                      GND
```

The HC-SR04 echo output is 5 V. The divider brings it down to about 2.5 V, which is a safe logic-high level for the STM32's 3.3 V input.

---

## How it works

### Main loop

The main loop follows an **input → process → output** structure. Each line is one function with one job:

```c
while (1)
{
    distance_cm = measure_distance();             // input
    new_state   = distance_to_state(distance_cm); // process
    state       = filter_state(new_state);        // process
    update_leds(state);                           // output
    print_status(state, distance_cm);             // output
    HAL_Delay(LOOP_DELAY_MS);
}
```

| Function | Job |
|---|---|
| `measure_distance()` | Reads the echo captured since the previous pass, checks it is plausible, converts it to cm, then sends the next trigger pulse |
| `distance_to_state()` | Maps a distance to a zone. Returns `STATE_SENSOR_ERROR` when there is no valid echo |
| `filter_state()` | Anti-flicker filter. Keeps its memory in `static` local variables |
| `blink_interval_ms()` | Returns the blink speed for a state (0 = steady) |
| `update_leds()` | Lights the LED for the current state and blinks it using `HAL_GetTick()` |
| `print_status()` | Prints on every state change, plus a heartbeat every 500 ms |
| `delay_us()` | Microsecond busy-wait on the TIM2 counter, used only for the 11 µs trigger pulse |
| `HAL_TIM_IC_CaptureCallback()` | Interrupt callback: records the rising edge and computes the echo width on the falling edge |

### Distance measurement with input capture

TIM2 runs as a free-running 32-bit counter ticking once per microsecond. Channel 3 (PA2) captures the counter value on **both edges** of the echo pulse in hardware and raises an interrupt:

```
ECHO:  ____|‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾|______
           ↑ rising          ↑ falling
           store start time  width = end − start, set ready flag
```

The measurement is **pipelined**: each loop pass first reads the result of the *previous* trigger, then sends a new one. The echo is timed by hardware in the background while the loop does other work. Because the loop period (~60 ms) is longer than the longest possible echo (~38 ms), every echo is finished before the next trigger, as the HC-SR04 datasheet requires.

```
distance [cm] = echo width [µs] / 58
```

(Sound travels about 1 cm in 29 µs; the echo covers the distance twice.)

All time differences use unsigned subtraction (`now - start`), so they stay correct when the counter wraps around.

### States and LEDs

| State | Distance (desk-test zones) | LED |
|---|---|---|
| SAFE | ≥ 30 cm | Green, steady |
| WARNING | 20–29 cm | Orange, slow blink |
| DANGER | 10–19 cm | Red, fast blink |
| CRITICAL | < 10 cm | Blue, fastest blink |
| SENSOR ERROR | No valid echo | All four LEDs on |

The zone limits are `#define`s (`PARK_WARNING_LIMIT_CM` etc.) and can be changed in one place, for example to real parking distances of 100 / 50 / 20 cm.

### Fail-safe design

A parking sensor must never show "safe" when it cannot see. If no valid echo arrives (broken wire, dead sensor, no power), `measure_distance()` returns 0, and after 3 confirmations the system enters **SENSOR ERROR** with all four LEDs lit, a pattern no normal state uses. A ready flag (`ic_ready`) ensures that an old measurement is never reused once the sensor stops responding.

---

## Clock and timer configuration

| Setting | Value |
|---|---|
| System clock | 168 MHz (HSE 8 MHz, PLL M=8, N=336, P=2) |
| APB1 | /4 (TIM2 clock = 84 MHz) |
| TIM2 prescaler | 83 → 1 MHz tick (1 µs) |
| TIM2 period | 0xFFFFFFFF (32-bit, free-running) |
| TIM2 channel 3 | Input capture, both edges, no prescaler, filter 0 |
| NVIC | TIM2 global interrupt enabled |

---

## Debug console (SWV / ITM)

`printf` is redirected to the Cortex-M4 ITM stimulus port 0 through `_write()` in `syscalls.c`. The output is read in STM32CubeIDE's **SWV ITM Data Console**.

Before writing, `ITM_SendChar()` checks the ITM enable bit (`ITM_TCR`, bit 0). When no debugger is attached the character is simply dropped, so the firmware runs normally from USB power alone.

**To see the output:**
1. Debug Configurations → Debugger → enable **Serial Wire Viewer (SWV)**, Core Clock **168 MHz**.
2. Start a debug session and open **Window → Show View → SWV → SWV ITM Data Console**.
3. In the console settings, enable **port 0**, then press **Start Trace**.

Example output:
```
Parking sensor started
State: WARNING, distance: 27 cm
State: DANGER, distance: 19 cm
State: CRITICAL, distance: 9 cm
State: SAFE, distance: 45 cm
```

---

## Building and flashing

1. Clone the repository and open the project in **STM32CubeIDE**.
2. Connect the board with a USB cable to the ST-LINK port.
3. Build the project (hammer icon).
4. Run or debug it (the ST-LINK flashes the board automatically).

> **Note on code generation:** all custom code lives between CubeMX `USER CODE BEGIN` / `USER CODE END` markers. Code outside these markers is overwritten when the `.ioc` file is regenerated. Back up `main.c` before regenerating.

---

## Project structure

```
Core/
  Inc/          Header files
  Src/
    main.c      Application code (measurement, states, LEDs, printing, callback)
    syscalls.c  printf redirection to ITM, with the debugger guard
*.ioc           CubeMX configuration (pins, clock, TIM2)
```

---

## Roadmap

- [x] HC-SR04 measurement with a 1 µs hardware timer
- [x] State machine with anti-flicker filter
- [x] Refactor into single-purpose functions with named constants
- [x] ITM output that is safe without a debugger
- [x] Fail-safe SENSOR ERROR state
- [x] Non-blocking LED blinking and heartbeat printing
- [x] Interrupt-driven echo measurement with input capture
- [ ] Internal pull-down on PA2 so a broken wire can never leave the input floating
- [ ] Upper plausibility limit for the echo width (reject pulses longer than ~38 ms)
- [ ] Hysteresis at zone borders to stop flipping between neighbouring states
- [ ] Non-blocking scheduler to replace `HAL_Delay()`, plus loop timing measurement
- [ ] Optional: median filter, decimal distance, user button to switch zone sets, real parking zones, multiple sensors, OLED display

---

## Lessons learned

- **Never single-step timing code.** The timer keeps counting while the CPU is paused.
- **A loose wire and a software bug can look identical.** Check hardware early and use Live Expressions and Suspend to see what the CPU is really doing.
- **Floating inputs pick up mains hum.** An unconnected pin produced fake ~9 ms "echoes" (half a 50 Hz cycle). Every input needs a defined level.
- **Variables written in an interrupt must be `volatile`**, and callbacks must stay short (no `printf`, no waiting).
- **Clear a "data ready" flag after using the data**, or stale values will look valid forever.

---

## Author

**Pranali** · GitHub: [@PranaliDalavi](https://github.com/PranaliDalavi)
