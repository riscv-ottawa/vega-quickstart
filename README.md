# RISC-V Embedded Systems Training (VEGA edition)

Baremetal example applications for the [RV32M1-VEGA](https://open-isa.org/) board,
built around the RI5CY (RISC-V) core. This repository is the hands-on companion to
the training book at **[vega.riscvottawa.ca](https://vega.riscvottawa.ca)** - each app maps to a section of the book, and the book walks through the concepts the code demonstrates.

Everything here runs two ways: flashed onto real VEGA hardware over OpenOCD, or fully
simulated in [Renode](https://renode.io/), so you can work through the material with or
without a board!

## Applications

Apps live under `apps/`, each in its own directory:

- [`blinky`](apps/blinky): the classic first program. Toggles an LED in a busy
  `delay()` loop and prints a banner over the UART.
- [`hello-uart`](apps/hello-uart): polled UART I/O. Reads characters one at a time and
  echoes them back capitalized; the foundation for talking to the board over serial.
- [`vegaconsole`](apps/vegaconsole): a tiny line-buffered REPL over LPUART0. Tokenizes
  each line into `argv[]` and dispatches through a command table (LED control and more).
  Everything is polled and synchronous.
- [`vegaconsole-irq`](apps/vegaconsole-irq): the REPL plus interrupts: an LPTMR ISR
  drives a heartbeat blink independent of the REPL, a `crash` command triggers an illegal
  instruction the trap handler recovers from, and a `slow` command shows the heartbeat
  surviving a busy main loop.
- [`blinky-better`](apps/blinky-better): blinky again, but the 1 Hz blink is driven
  entirely by an LPTMR interrupt. `main()` initializes and then drops into a `wfi` loop,
  sleeping between timer events.

## Getting started

### Prerequisites

You need the RISC-V toolchain (`riscv32-unknown-elf-gcc` and friends), plus OpenOCD for
flashing and Renode for simulation. The easiest way to get started is via the provided container, which
bundles everything needed.

See the [development environment chapter](https://vega.riscvottawa.ca/dev-environment.html) of the book for full instructions.

### Get the SDK

The RV32M1 SDK is pulled in as a git submodule, make sure to populate it after cloning this repo using:
```sh
git submodule update --init --recursive
```

You can refer to the [blinky chapter](https://vega.riscvottawa.ca/firmware/blinky.html) of the book for more information.

### Build and run

```sh
make list            # see which apps are available
make blinky          # build apps/blinky -> build/blinky/
make sim-blinky      # build and run blinky in Renode
make flash-blinky    # build and flash blinky to a connected board
make serial          # open a 115200 baud serial console to the board
```

## Make commands

All targets are driven from the top-level [`Makefile`](Makefile). `<app>` is any
directory name under `apps/` (run `make list` to see them).

| Command | What it does |
| --- | --- |
| `make <app>` | Build a single app. Artifacts (`.elf`, `.bin`, `.hex`) land in `build/<app>/`. |
| `make` / `make all` | Build every app. |
| `make sim-<app>` | Build `<app>` and run it in the Renode simulator. |
| `make flash-<app>` | Build `<app>` and flash it to a connected board via OpenOCD. |
| `make gdbserver` | Start OpenOCD as a GDB server listening on `:3333`. |
| `make gdb-<app>` | Launch GDB on `<app>.elf` and connect to a running gdbserver. |
| `make serial` | Open a serial console (minicom, 115200 baud) to the board. |
| `make list` | Print the list of available apps. |
| `make clean` | Remove all build artifacts (the `build/` directory). |

A typical debug session: run `make gdbserver` in one terminal, then `make gdb-blinky` in
another to attach.

### Adding your own app

Create a directory under `apps/` with your `.c` sources (`main.c` is linked first if
present), and it shows up automatically as a build target. To customize the build for
that app (a different `-march`, extra SDK drivers, additional defines), add an
`apps/<app>/config.mk` overriding any of the building-block variables (`ARCH_FLAGS`,
`APP_SRCS`, `SDK_SRCS`, `INCLUDES`, `DEFINES`, ...). See `apps/blinky-better/config.mk`
for an example.

## Repository layout

```
apps/            example applications, one directory each
build/           build output (git-ignored)
rv32m1-sdk/      RV32M1 SDK (git submodule)
support/
  openocd/       OpenOCD config for flashing the RI5CY core
  renode/        Renode platform, scripts, and custom peripheral models
  sdk/           linker script
Makefile         top-level build system
Containerfile    development environment image
```

## Hardware notes

The build targets the RI5CY core on the RV32M1 with `-march=rv32imc`. The serial console
runs at 115200 baud over LPUART0, which on the board appears as a USB CDC device
(`/dev/ttyACM*`, `/dev/ttyUSB*`, or `/dev/cu.usbmodem*`). The Renode simulation models the
same peripherals, including pinmux and pin interrupts, so simulated runs behave like the
real board.
