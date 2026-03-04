# GPPI (Generic Programmable Peripheral Interconnect) Example for nRF54L15

## Overview

This example demonstrates how to use the **GPPI (Generic Programmable Peripheral Interconnect)** API on the nRF54L15. The GPPI system is a hardware mechanism that allows peripherals to communicate directly without CPU intervention, significantly reducing latency and power consumption.

**Tested with**: nRF Connect SDK v3.2.3

## What is GPPI?

GPPI (Generic Programmable Peripheral Interconnect) is an abstraction layer over the Nordic PPI/DPPI systems:
- **PPI (Programmable Peripheral Interconnect)** - Used in older Nordic chips
- **DPPI (Distributed PPI)** - Used in newer Nordic chips like nRF54L15

The GPPI API provides a unified interface for connecting peripheral events to peripheral tasks, allowing hardware-level communication without CPU involvement.

## Features Demonstrated

1. **GPIOTE Configuration**: Setting up GPIO pins with GPIOTE (GPIO Tasks and Events)
2. **GPPI Channel Allocation**: Allocating GPPI channels for event-task connections
3. **Hardware Event-Task Connection**: Connecting button press event to LED toggle task
4. **Zero-Latency Response**: LED toggles instantly on button press via hardware

## Hardware Requirements

- **Board**: nRF54L15 DK (nrf54l15dk/nrf54l15/cpuapp)
- **Button**: Button 0 (sw0 alias in devicetree) - GPIO1 pin 13
- **LED**: LED 1 (led1 alias in devicetree) - GPIO1 pin 10

**Important**: This example uses LED1 instead of LED0 because **GPIO Port 2 does not support GPIOTE** on nRF54L15. 
- LED0 is on GPIO2 - **not compatible with GPIOTE**
- LED1 is on GPIO1 - **supports GPIOTE**
- Only GPIO ports with GPIOTE support can be used with GPPI connections

## How It Works

```
┌──────────┐      GPIOTE Event        ┌──────────┐       GPIOTE Task        ┌────────┐
│ Button 0 │ ──> (Input Channel) ──>  │   DPPI   │ ──> (Output Channel) ──> │ LED 1  │
│  Press   │                          │  Channel │                          │ Toggle │
└──────────┘                          └──────────┘                          └────────┘
   GPIO1.13                                                                  GPIO1.10
```

### The Process:

1. **Button Press**: When Button 0 is pressed, the GPIO state changes
2. **GPIOTE Event**: GPIOTE peripheral detects the change and generates an event on the DPPI bus
3. **GPPI Connection**: The DPPI hardware automatically routes the event to the connected task
4. **LED Toggle**: GPIOTE output task toggles LED 1

**All of this happens in hardware with zero CPU cycles!**

**Note**: On nRF54L15, **GPIO Port 2 does not support GPIOTE**, so only pins on GPIO0 and GPIO1 can be used with GPIOTE and GPPI. This is why LED1 (GPIO1.10) is used instead of LED0 (GPIO2.9).

## Building and Running

### Build the project:
```bash
west build -b nrf54l15dk/nrf54l15/cpuapp
```

**Note**: The build must be run from the NCS workspace directory (`v3.2.3`), not from the project directory.

### Flash to the board:
```bash
west flash
```

## Expected Output

When you run the application, you should see:
```
[00:00:00.xxx] <inf> gppi_example: ===========================================
[00:00:00.xxx] <inf> gppi_example: GPPI Example for nRF54L15
[00:00:00.xxx] <inf> gppi_example: Board: nrf54l15dk_nrf54l15_cpuapp
[00:00:00.xxx] <inf> gppi_example: ===========================================
[00:00:00.xxx] <inf> gppi_example: Initializing GPIOTE instance 0...
[00:00:00.xxx] <inf> gppi_example: Allocated input channel: 0
[00:00:00.xxx] <inf> gppi_example: Allocated output channel: 1
[00:00:00.xxx] <inf> gppi_example: Configuring input pin...
[00:00:00.xxx] <inf> gppi_example: Input pin configured successfully
[00:00:00.xxx] <inf> gppi_example: Configuring output pin...
[00:00:00.xxx] <inf> gppi_example: Output pin configured successfully
[00:00:00.xxx] <inf> gppi_example: Setting up GPPI connection...
[00:00:00.xxx] <inf> gppi_example: Input event address: 0x4000B100
[00:00:00.xxx] <inf> gppi_example: Output task address: 0x4000B030
[00:00:00.xxx] <inf> gppi_example: GPPI connection allocated (handle: 0x00000000)
[00:00:00.xxx] <inf> gppi_example: GPPI connection enabled!
[00:00:00.xxx] <inf> gppi_example: Hardware link established: Button Event -> LED Toggle Task
[00:00:00.xxx] <inf> gppi_example: Setup complete!
[00:00:00.xxx] <inf> gppi_example: Press Button 0 to toggle LED 0
```

When you press Button 0:
```
[00:00:xx.xxx] <inf> gppi_example: Button event callback #1 (LED toggled via GPPI hardware connection)
[00:00:xx.xxx] <inf> gppi_example: Total button press events: 1
```

## Key Configuration (prj.conf)

The project uses direct nrfx API calls rather than Zephyr's GPIO abstraction:

```properties
# Disable Zephyr GPIO driver (using nrfx directly)
CONFIG_GPIO=n

# Logging
CONFIG_LOG=y
CONFIG_LOG_MODE_IMMEDIATE=y

# Enable NRFX drivers for GPIOTE20 and GPPI
CONFIG_NRFX_GPIOTE20=y
CONFIG_NRFX_GPPI=y

# Main stack
CONFIG_MAIN_STACK_SIZE=2048
```

**Important Notes:**
- `CONFIG_GPIO=n` disables Zephyr's GPIO API to use nrfx directly
- `CONFIG_NRFX_GPIOTE20=y` enables the GPIOTE20 driver (nRF54L15 uses GPIOTE20, not GPIOTE0)
- Don't try to set `CONFIG_NRFX_GPIOTE` directly - it's selected automatically

### GPIOTE Functions:
- `nrfx_gpiote_init()` - Initialize GPIOTE peripheral
- `nrfx_gpiote_channel_alloc()` - Allocate GPIOTE channel
- `nrfx_gpiote_input_configure()` - Configure input pin with events
- `nrfx_gpiote_output_configure()` - Configure output pin with tasks
- `nrfx_gpiote_in_event_address_get()` - Get event register address
- `nrfx_gpiote_out_task_address_get()` - Get task register address

### GPPI Functions:
- `nrfx_gppi_conn_alloc()` - Allocate GPPI connection between event and task
- `nrfx_gppi_conn_enable()` - Enable the GPPI connection
- `nrfx_gppi_conn_disable()` - Disable the GPPI connection (not used in this example)
- `nrfx_gppi_conn_free()` - Free GPPI connection resources (not used in this example)

## Benefits of Using GPPI

1. **Ultra-Low Latency**: Hardware connections respond instantly (nanoseconds)
2. **Zero CPU Overhead**: No interrupt handling or task switching required
3. **Power Efficient**: CPU can stay in low-power mode while events are handled
4. **Deterministic**: Guaranteed response time regardless of CPU load
5. **Scalable**: Multiple peripheral connections can run in parallel

## Use Cases

GPPI is ideal for:
- **Sensor Triggering**: Timer triggers ADC conversion automatically
- **PWM Control**: Button press changes PWM duty cycle
- **LED Patterns**: Timer events drive LED sequences
- **Audio/Video Sync**: Precise peripheral synchronization
- **Low-Power Applications**: Event handling without waking CPU

## Additional Resources

- [nRF54L15 Product Specification](https://docs.nordicsemi.com/bundle/nRF54L15_PS)
- [nrfx Documentation](https://docs.nordicsemi.com/bundle/nrfx/page/index.html)
- [Zephyr GPIOTE Driver](https://docs.zephyrproject.org/latest/hardware/peripherals/gpiote.html)
- [Nordic DevZone](https://devzone.nordicsemi.com/)

## Troubleshooting

### Build fails with "NRFX_GPIOTE is assigned in a configuration file"
- Don't set `CONFIG_NRFX_GPIOTE=y` or `CONFIG_NRFX_GPIOTE0=y` directly
- Use `CONFIG_NRFX_GPIOTE20=y` for nRF54L15
- These are auto-selected based on the specific instance you enable

### Linking errors: "undefined reference to nrfx_gpiote_*"
- Ensure `CONFIG_NRFX_GPIOTE20=y` is set in prj.conf
- The GPIOTE driver library must be enabled explicitly

### LED doesn't toggle on button press
- **Ensure you're using a GPIO port that supports GPIOTE!** GPIO2 does NOT support GPIOTE on nRF54L15
- Only GPIO0 and GPIO1 support GPIOTE on nRF54L15
- Check that the button and LED pins are properly defined in your board's devicetree
- Verify GPPI connection is enabled (check logs)
- Ensure GPIOTE20 instance is properly initialized

### No serial output
- Check COM port with `nrfutil device list`
- Verify logging is enabled: `CONFIG_LOG=y`

## License

Copyright (c) 2026  
SPDX-License-Identifier: Apache-2.0
