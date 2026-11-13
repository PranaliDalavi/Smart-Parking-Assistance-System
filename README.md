# Smart Parking Assistance System (STM32F407)

An ultrasonic parking sensor built on the **STM32F407G-DISC1** board. An HC-SR04 sensor measures the distance to an obstacle; the firmware sorts it into a warning zone, shows the zone on the board's LEDs (blinking faster as the obstacle gets closer, like a car's parking beeper), and prints the state and distance to a debug console.

The firmware is written in C with STM32CubeIDE and the STM32 HAL. It is structured into small modules and written with a focus on safety-oriented embedded practice: interrupt-driven measurement, non-blocking timing, fail-safe behaviour, a hardware watchdog, and code following the MISRA C:2012 guidelines.

---

## Features

- **Distance measurement** with an HC-SR04 ultrasonic sensor at 1 µs resolution
- **Interrupt-driven echo capture** with TIM2 input capture (no busy-waiting on the echo pin)
- **Five system states:** SAFE, WARNING, DANGER, CRITICAL and SENSOR ERROR
- **Anti-flicker filter:** a new state is only accepted after 3 consecutive matching readings
- **Non-blocking LED blinking** whose speed increases as the obstacle gets closer
- **Fail-safe sensor handling:** a missing or implausible echo is reported as SENSOR ERROR, never as "safe"
- **Plausibility checks:** echo pulses shorter than 100 µs or longer than 40 ms are rejected as glitches
- **Independent watchdog (IWDG):** the MCU resets if the main loop stops running
- **Defined safe state on faults:** `Error_Handler()` and `HardFault_Handler()` switch all four LEDs on
- **Race-free ISR data exchange:** shared data is copied and cleared inside a short critical section
- **Modular structure:** sensor driver, parking logic, LED output and status output in separate modules
- **Debug console over SWV/ITM** in Debug builds; removed completely in Release builds

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

| HC-SR04 | STM32F407G-DISC1 | CubeMX label |
|---|---|---|
| VCC | 5V | |
| GND | GND | |
| TRIG | PA1 (GPIO output) | `TRIG` |
| ECHO | PA2 (TIM2_CH3) through a voltage divider | `ECHO` |

```
HC-SR04 ECHO ──[1 kΩ]──┬──── PA2 (TIM2_CH3)
                       │
                     [1 kΩ]
                       │
                      GND
```

The HC-SR04 echo output is 5 V. The divider brings it down to about 2.5 V, a safe logic-high level for the STM32's 3.3 V input. The lower resistor also holds PA2 low if the sensor is disconnected, so the input never floats.

---

## Software architecture

### Modules

| Module | Files | Responsibility | Public interface |
|---|---|---|---|
| Sensor driver | `hcsr04_sensor.c/.h` | Trigger pulse, input-capture ISR, plausibility checks, µs → cm | `hcsr04_init()`, `hcsr04_measure_cm()` |
| Parking logic | `park_logic.c/.h` | Distance → zone, anti-flicker filter, state names. No HAL code, so it can be unit-tested on a PC | `park_distance_to_state()`, `filter_state()`, `park_state_name()` |
| LED output | `leds.c/.h` | LED pattern per state, non-blocking blinking, fault safe state | `update_leds()`, `leds_safe_state()` |
| Status output | `status.c/.h` | Console print on state change plus a 500 ms heartbeat (Debug builds only) | `print_status()` |
| Application | `main.c` | Initialisation, main loop, watchdog refresh, error handler | — |

Each module keeps its data and helper functions `static` (private to the file) and exposes only the functions listed above. The sensor driver receives the timer handle from `main.c` at initialisation instead of using the global `htim2` directly.

### Main loop

The loop follows an **input → process → output** structure:

```c
while (1)
{
    distance_cm = hcsr04_measure_cm();                // input
    raw_state   = park_distance_to_state(distance_cm); // process
    state       = filter_state(raw_state);             // process
    update_leds(state);                                // output
    print_status(state, distance_cm);                  // output
    (void)HAL_IWDG_Refresh(&hiwdg);                    // watchdog
    HAL_Delay(LOOP_DELAY_MS);                          // ~60 ms period
}
```

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

All time differences use unsigned subtraction (`now - start`), so they stay correct when a counter wraps around.

### Data shared with the interrupt

The ISR writes `ic_width` and sets `ic_ready`; the main loop reads and clears them. Both are `volatile`. The read-and-clear in `hcsr04_measure_cm()` runs inside a short critical section (`__get_PRIMASK()` / `__disable_irq()` / `__set_PRIMASK()`), so a new capture can never arrive between copying the width and clearing the flag and be lost.

### States and LEDs

| State | Distance (desk-test zones) | LED |
|---|---|---|
| SAFE | ≥ 30 cm | Green, steady |
| WARNING | 20–29 cm | Orange, blinks every 400 ms |
| DANGER | 10–19 cm | Red, blinks every 200 ms |
| CRITICAL | < 10 cm | Blue, blinks every 100 ms |
| SENSOR ERROR | No valid echo | All four LEDs on |

The zone limits are named constants in `park_logic.c` and can be changed in one place, for example to real parking distances of 100 / 50 / 20 cm.

---

## Safety design

| Fault | Detection | Reaction |
|---|---|---|
| Sensor disconnected, dead or unpowered | No valid echo for 3 consecutive cycles | SENSOR ERROR: all four LEDs on |
| Glitch or impossible echo | Width < 100 µs or > 40 ms | Reading rejected (counts as "no valid echo") |
| Unknown state value | `default` case in the LED switch | Treated as a fault: all four LEDs on |
| Main loop hangs | Independent watchdog not refreshed | MCU reset |
| HAL initialisation error | `Error_Handler()` | All four LEDs on, then watchdog reset |
| CPU fault | `HardFault_Handler()` | All four LEDs on, then watchdog reset |

### Watchdog

- The IWDG runs from the internal LSI oscillator. Prescaler 32 and reload 499 give a **nominal timeout of 500 ms**.
- The LSI tolerance on the STM32F407 (about 17–47 kHz) puts the real timeout between about **340 ms and 940 ms**. The main loop takes about 65 ms, well below the shortest case.
- The watchdog is refreshed in **one place only**, at the end of the main loop, never in an interrupt.
- It is frozen while the debugger halts the CPU (`__HAL_DBGMCU_FREEZE_IWDG()`), so breakpoints don't cause resets.
- After a watchdog reset, Debug builds print `WARNING: reset by watchdog` at startup. The reset flags are then cleared.

`leds_safe_state()` writes the GPIO registers directly, without the HAL, so it also works in a HardFault and before `MX_GPIO_Init()` has run.

---

## Coding guidelines

The code follows the **MISRA C:2012** guidelines where practical, for example:

- essentially Boolean conditions (`bool`, explicit comparisons)
- a `default` case and a `break` in every switch, no unreachable code
- single exit point per function
- return values of HAL functions checked or explicitly discarded with `(void)`
- unsigned constants with a `U` suffix, fixed-width integer types
- a compile-time check (`_Static_assert`) that the state-name table matches the state enum
- bounds-checked array access

**Known deviations:** `printf` (Rule 21.6) is used for diagnostics in Debug builds only and compiled out in Release builds. Direct register access in `leds_safe_state()` uses the vendor's CMSIS definitions. ST's generated HAL and startup code is outside the scope.

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
| IWDG | LSI, prescaler 32, reload 499 (~500 ms nominal) |

---

## Debug console (SWV / ITM)

In Debug builds, `printf` is redirected to the Cortex-M4 ITM stimulus port 0 through `_write()` in `syscalls.c`, using the CMSIS function `ITM_SendChar()`. That function returns immediately when no debugger has enabled the ITM, so the firmware also runs normally from USB power alone.

**To see the output:**
1. Debug Configurations → Debugger → enable **Serial Wire Viewer (SWV)**, Core Clock **168 MHz**.
2. Start a debug session and open **Window → Show View → SWV → SWV ITM Data Console**.
3. In the console settings, tick **port 0**, then press **Start Trace**.

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
3. Choose the **Debug** build (with console output) or the **Release** build (without).
4. Build the project (hammer icon), then Run or Debug it.

> **Note on code generation:** custom code in `main.c` lives between the CubeMX `USER CODE BEGIN` / `USER CODE END` markers, and the modules are in their own files, which CubeMX never touches. Code outside the markers in generated files is overwritten when the `.ioc` file is regenerated.

---

## Project structure

```
Core/
  Inc/
    hcsr04_sensor.h   Sensor driver interface
    park_logic.h      ParkState type, zone and filter functions
    leds.h            LED output interface
    status.h          Status output interface
    main.h            CubeMX pin labels (TRIG, ECHO)
  Src/
    main.c            Initialisation, main loop, watchdog, error handler
    hcsr04_sensor.c   Trigger, input-capture callback, plausibility checks
    park_logic.c      Zones, anti-flicker filter, state names
    leds.c            LED patterns, blinking, safe state
    status.c          Console output (Debug builds)
    stm32f4xx_it.c    Interrupt handlers, HardFault safe state
    syscalls.c        printf redirection to ITM
*.ioc                 CubeMX configuration (pins, clock, TIM2, IWDG)
```

---

## Roadmap

- [x] HC-SR04 measurement with a 1 µs hardware timer
- [x] State machine with anti-flicker filter
- [x] Fail-safe SENSOR ERROR state
- [x] Non-blocking LED blinking and heartbeat printing
- [x] Interrupt-driven echo measurement with input capture
- [x] Modular structure with private module data
- [x] Lower and upper plausibility limits for the echo width
- [x] Independent watchdog and safe state in fault handlers
- [x] Race-free ISR data exchange (critical section)
- [x] Refactoring following the MISRA C:2012 guidelines
- [ ] Static analysis with cppcheck (MISRA add-on) and stricter compiler warnings
- [ ] Unit tests for the parking logic on a PC
- [ ] Hysteresis at zone borders to stop flipping between neighbouring states
- [ ] Non-blocking scheduler to replace `HAL_Delay()`, plus loop timing measurement
- [ ] Optional: median filter, user button to switch zone sets, real parking zones, OLED display

---

## Lessons learned

- **Never single-step timing code.** The timer keeps counting while the CPU is paused.
- **A loose wire and a software bug can look identical.** Check the hardware early, and watch what the CPU is really doing.
- **Floating inputs pick up mains hum.** An unconnected pin produced fake ~9 ms "echoes" (half a 50 Hz cycle). Every input needs a defined level.
- **Variables written in an interrupt must be `volatile`,** callbacks must stay short, and a read-then-clear of shared data needs a critical section.
- **Clear a "data ready" flag after using the data,** or stale values look valid forever.
- **A system that freezes must not keep showing its last state.** A watchdog and a defined safe state turn a hang into a visible, recoverable fault.

---

## Author

**Pranali** · GitHub: [@PranaliDalavi](https://github.com/PranaliDalavi)
