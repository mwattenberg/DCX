# Copilot Instructions for HV IBC 800V/50V Project

## Project Overview
Embedded firmware for PSOC™ Control C3M5 Digital Power Control Card (KIT_PSC3M5_CC1) implementing an isolated DCX converter (DCX_ISOP) with spread spectrum modulation for power conversion applications.

**Hardware:** Infineon PSoC Control C3M5 MCU (Cortex-M33)  
**Toolchain:** ModusToolbox 3.3+, GCC_ARM (default), ARM, IAR  
**Programming:** SEGGER J-Link required

## Architecture & Components

### Core Application Structure
- **`main.c`**: Minimal initialization, calls `cybsp_init()`, then `DCX_ISOP_init()` and `DCX_ISOP_start()`. Main loop is intentionally empty - control happens via interrupts.
- **`DCX_ISOP.c/.h`**: Primary/Secondary PWM controller implementing dual-phase isolated DC-DC converter with spread spectrum frequency modulation.
- **Hardware Configuration**: `bsps/TARGET_APP_KIT_PSC3M5_CC1/config/design.modus` - Device Configurator generates code in `config/GeneratedSource/cycfg_*.{c,h}`

### PWM Control Design Pattern
The DCX_ISOP module controls **two synchronized TCPWM blocks** (`PWM_PRIM` on TCPWM0[0], `PWM_SEC` on TCPWM0[1]):
- **Phase-shifted operation**: Secondary PWM offset by configurable phase for isolated DC-DC topology
- **Common trigger coordination**: Both PWMs share trigger inputs (TR_IN6=swap, TR_IN7=start, TR_IN8=kill)
- **Shadow register workflow**: Period/compare updates go to Period1/Compare0Buf, then software trigger TR_IN6 swaps values atomically
- **Spread spectrum**: ISR (`DCX_IsrHandler`) dynamically adjusts PWM period ±6 counts in triangular pattern to reduce EMI

### Clock Configuration (BSP)
- **CLK_HF3**: 240 MHz (Path 2) - Used for TCPWM PWM generation
- System queries actual frequency via `Cy_SysClk_ClkHfGetFrequency(3)` to calculate period counts

## Development Workflow

### Building & Programming
```bash
# Use modus-shell (ModusToolbox bash) on Windows, not PowerShell for make commands
make build TOOLCHAIN=GCC_ARM CONFIG=Debug
make program TOOLCHAIN=GCC_ARM CONFIG=Debug
make qprogram  # Quick program (no rebuild)
make clean
```

**VS Code Tasks Available**: Rebuild, Build, Clean, Erase Device, Build & Program, Quick Program  
**Current Target**: `APP_KIT_PSC3M5_CC1` (custom target name in Makefile, based on KIT_PSC3M5_CC1)

### Hardware Configuration Changes
1. Open `bsps/TARGET_APP_KIT_PSC3M5_CC1/config/design.modus` in Device Configurator
2. Modify peripherals (TCPWM, clocks, pins, triggers)
3. Device Configurator regenerates `config/GeneratedSource/cycfg_*.{c,h}` automatically
4. **Never manually edit generated files** - they include `PWM_PRIM_config`, `PWM_SEC_config` structures and pin definitions

### Debugging Setup
- **Debugger**: On-board isolated J-Link (USB Type-C)
- **UART**: Virtual COM port available through J-Link
- Install SEGGER J-Link software separately (not included in ModusToolbox)

## Code Conventions & Patterns

### Peripheral Initialization Pattern
```c
// 1. Initialize PDL driver with generated config
Cy_TCPWM_PWM_Init(PWM_PRIM_HW, PWM_PRIM_NUM, &PWM_PRIM_config);

// 2. Configure dynamic parameters
DCX_ISOP_setFsw(800000);  // Sets period, then duty

// 3. Enable peripheral
Cy_TCPWM_PWM_Enable(PWM_PRIM_HW, PWM_PRIM_NUM);

// 4. Setup trigger connections (common start/stop/swap)
Cy_TCPWM_InputTriggerSetup(...);

// 5. Start via software trigger
Cy_TrigMux_SwTrigger(TRIG_OUT_MUX_10_TCPWM0_TR_IN7, CY_TRIGGER_TWO_CYCLES);
```

### TCPWM Period/Duty Update Protocol
**Critical**: Period0 only updates from Period1 shadow register while counter is running:
```c
// Before starting (counter stopped):
Cy_TCPWM_PWM_SetPeriod0(PWM_PRIM_HW, PWM_PRIM_NUM, period0);

// After starting (counter running):
Cy_TCPWM_PWM_SetPeriod1(PWM_PRIM_HW, PWM_PRIM_NUM, new_period);
Cy_TCPWM_PWM_SetCompare0BufVal(PWM_PRIM_HW, PWM_PRIM_NUM, new_compare);
Cy_TrigMux_SwTrigger(TRIG_OUT_MUX_10_TCPWM0_TR_IN6, CY_TRIGGER_TWO_CYCLES);  // Swap
```

### Naming Conventions
- **Hardware blocks**: `PWM_PRIM_HW`, `PWM_SEC_HW` (TCPWM base pointers from generated code)
- **Instance numbers**: `PWM_PRIM_NUM`, `PWM_SEC_NUM` (counter indices)
- **Configuration structs**: `DCX_ISOP_config_t` (typedef with `_t` suffix)
- **Global state**: `DCX_ISOP_config` (extern in `.h`, defined in `.c`)

### Float vs Fixed-Point
- Duty cycle stored as float (0.0-1.0) for API convenience
- Immediately converted to uint16_t counts: `compare = period0 * duty`
- Frequency calculations use uint32_t exclusively

## Critical Dependencies

### ModusToolbox Libraries (`.mtb` files in `deps/` and `libs/`)
- **mtb-pdl-cat1**: Peripheral Driver Library - low-level register access (`Cy_TCPWM_*`, `Cy_TrigMux_*`)
- **mtb-hal-psc3**: Hardware Abstraction Layer for PSC3 devices
- **core-lib**: System utilities and CMSIS interface
- Shared repo location: `../mtb_shared/` (one level up from project)

### BSP (Board Support Package)
- Lives in `bsps/TARGET_APP_KIT_PSC3M5_CC1/`
- Provides `cybsp_init()`, startup code, clock config, power management
- **Key files**: `cybsp.h`, `cybsp.c`, `system_psc3.c`

## Common Pitfalls

1. **modus-shell requirement**: Standard Windows PowerShell/cmd cannot run `make` commands - use modus-shell from ModusToolbox
2. **Phase shift before start**: `DCX_setPhaseShift()` calls `SetCounter()` which only works when timers are stopped
3. **Spread spectrum interrupt priority**: Set to priority 1 (see `DCX_enableSpreadSpectrum()`)
4. **Dead-time both directions**: Must set both positive and negative dead-time registers (`PWMDeadTime` and `PWMDeadTimeN`)
5. **Target name mismatch**: Makefile uses `APP_KIT_PSC3M5_CC1` (custom), BSP folder is `TARGET_APP_KIT_PSC3M5_CC1`

## Testing & Verification

### Switching Frequency Check
Default: 800 kHz (±6 counts spread spectrum when enabled)  
Verify with scope on PWM output pins (see pin assignments in `cycfg_pins.h`)

### Duty Cycle / Phase Shift
- Duty cycle: 50% default (`DCX_ISOP_config.duty = 0.5f`)
- Phase shift: 6 counts default (configured in `DCX_ISOP_init()`)
- Dead-time: Primary=16 counts, Secondary=30 counts

## When Making Changes

### Adding New Peripherals
1. Open `design.modus` in Device Configurator
2. Add peripheral, configure triggers/clocks
3. Generate code
4. Include `cycfg_peripherals.h` in your `.c` file
5. Initialize with generated `*_config` structure

### Modifying PWM Behavior
- **Frequency changes**: Use `DCX_ISOP_setFsw()` - recalculates period and updates both PWMs
- **Duty changes**: Use `DCX_setDuty()` - updates compare values atomically
- **Spread spectrum tuning**: Modify `maxSpreadSpectrumCounts` (default 6) in `DCX_ISOP.c`

### Power/Clock Configuration
- Edit clock tree in `design.modus`
- Verify CLK_HF3 frequency matches expectations (240 MHz for this target)
- System queries clock via `Cy_SysClk_ClkHfGetFrequency(3)` - changes propagate automatically
