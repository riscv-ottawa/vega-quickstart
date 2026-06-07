/*
 * VegaConsole, interrupts edition.
 *
 * Same REPL as apps/vegaconsole, plus:
 *
 *   - An LPTMR ISR drives a heartbeat blink on the blue LED at 1 Hz,
 *     completely independent of what the REPL is doing.
 *
 *   - A `crash` command deliberately executes an illegal instruction.
 *     The trap handler catches it, prints mcause and mepc, steps past
 *     the bad insn, and returns. The program keeps running.
 *
 *   - A `slow` command burns CPU for two seconds. The REPL is frozen
 *     while it runs, but the heartbeat keeps blinking. That's the
 *     whole point of moving the heartbeat into an interrupt.
 *
 *   - A `ticks` command prints the LPTMR tick counter, mostly useful
 *     for sanity-checking that the timer is actually running.
 */
#include "board.h"
#include "fsl_common.h"
#include "fsl_debug_console.h"
#include "fsl_gpio.h"
#include "peripherals.h"
#include <stdint.h>
#include <string.h>

#define LINE_MAX 80
#define ARGV_MAX 8
#define PROMPT "vega> "

/* Heartbeat: half-period in ticks. 50 ticks at 100 Hz = 500 ms,
 * so a toggle every 500 ms = 1 Hz blink. */
#define HEARTBEAT_HALF_TICKS (TICK_RATE_HZ / 2u)

typedef int (*cmd_fn)(int argc, char **argv);

typedef struct {
  const char *name;
  const char *help;
  cmd_fn run;
} command_t;

/* ---------- LEDs ---------- */

static const struct {
  const char *name;
  uint32_t pin;
} leds[] = {
    {"red", BOARD_LED_RED_PIN},
    {"green", BOARD_LED_GREEN_PIN},
    {"blue", BOARD_LED_BLUE_PIN},
};

static void leds_init(void) {
  gpio_pin_config_t cfg = {kGPIO_DigitalOutput, 0};
  for (unsigned i = 0; i < sizeof(leds) / sizeof(leds[0]); ++i)
    GPIO_PinInit(BOARD_LED_GPIO, leds[i].pin, &cfg);
}

static int led_find(const char *name) {
  for (unsigned i = 0; i < sizeof(leds) / sizeof(leds[0]); ++i)
    if (strcmp(name, leds[i].name) == 0)
      return (int)i;
  return -1;
}

static inline void heartbeat_toggle(void) {
  BOARD_LED_GPIO->PTOR = 1u << BOARD_LED_BLUE_PIN;
}

/* ---------- LPTMR ISR ---------- */

volatile uint32_t g_ticks;

void lptmr0_isr(void) {
  /*
   * Ack the compare flag (write-1-to-clear of TCF).
   * Note: Renode's LowPower_Timer model uses a descending
   * counter that gets stuck at 0 after firing, so we
   * also write CNR to reload the counter to the limit. CNR is
   * read-only on real hardware so the second write is a no-op there.
   */
  LPTMR0->CSR |= LPTMR_CSR_TCF_MASK;
  LPTMR0->CNR = LPTMR0->CMR;
  g_ticks++;
  if ((g_ticks % HEARTBEAT_HALF_TICKS) == 0u) {
    heartbeat_toggle();
  }
}

/* ---------- commands ---------- */

extern const command_t commands[];
extern const unsigned commands_count;

static int cmd_help(int argc, char **argv) {
  (void)argc;
  (void)argv;
  PRINTF("commands:\r\n");
  for (unsigned i = 0; i < commands_count; ++i)
    PRINTF("  %-6s %s\r\n", commands[i].name, commands[i].help);
  return 0;
}

static int cmd_led(int argc, char **argv) {
  if (argc != 3) {
    PRINTF("usage: led <red|green|blue> <on|off>\r\n");
    return 1;
  }
  int idx = led_find(argv[1]);
  if (idx < 0) {
    PRINTF("unknown color: %s\r\n", argv[1]);
    return 1;
  }
  uint8_t level;
  if (strcmp(argv[2], "on") == 0)
    level = 1;
  else if (strcmp(argv[2], "off") == 0)
    level = 0;
  else {
    PRINTF("expected 'on' or 'off', got: %s\r\n", argv[2]);
    return 1;
  }
  GPIO_WritePinOutput(BOARD_LED_GPIO, leds[idx].pin, level);
  return 0;
}

static int cmd_echo(int argc, char **argv) {
  for (int i = 1; i < argc; ++i)
    PRINTF("%s%s", argv[i], (i + 1 < argc) ? " " : "");
  PRINTF("\r\n");
  return 0;
}

/*
 * Execute an illegal instruction on purpose. unimp expands to a word
 * the chip is guaranteed to reject; the core traps with mcause = 2.
 * trap.c::exception_handler prints mcause/mepc, advances mepc past
 * the offending insn, and mret resumes execution at the next line.
 */
static int cmd_crash(int argc, char **argv) {
  (void)argc;
  (void)argv;
  PRINTF("about to do something illegal...\r\n");
  __asm__ volatile("unimp");
  PRINTF("...and back!\r\n");
  return 0;
}

/*
 * Two seconds of busy-looping. The REPL is frozen while this runs,
 * but the heartbeat LED keeps blinking.
 */
static int cmd_slow(int argc, char **argv) {
  (void)argc;
  (void)argv;
  PRINTF("burning CPU for ~2 seconds...\r\n");
  for (volatile uint32_t i = 0; i < 20000000u; ++i) {
  }
  PRINTF("done. did the heartbeat keep blinking?\r\n");
  return 0;
}

static int cmd_ticks(int argc, char **argv) {
  (void)argc;
  (void)argv;
  uint32_t t = g_ticks;
  PRINTF("g_ticks=%u (~%u s since boot)\r\n", (unsigned)t,
         (unsigned)(t / TICK_RATE_HZ));
  return 0;
}

const command_t commands[] = {
    {"help", "list available commands", cmd_help},
    {"led", "led <red|green|blue> <on|off>", cmd_led},
    {"echo", "echo <text...>", cmd_echo},
    {"crash", "execute an illegal instruction and recover", cmd_crash},
    {"slow", "busy-loop ~2 seconds (watch the heartbeat)", cmd_slow},
    {"ticks", "print the LPTMR tick counter", cmd_ticks},
};
const unsigned commands_count = sizeof(commands) / sizeof(commands[0]);

/* ---------- line editor and dispatcher ---------- */

static int tokenize(char *line, char **argv, int max) {
  int argc = 0;
  char *p = line;
  while (*p && argc < max) {
    while (*p == ' ' || *p == '\t')
      *p++ = '\0';
    if (!*p)
      break;
    argv[argc++] = p;
    while (*p && *p != ' ' && *p != '\t')
      ++p;
  }
  return argc;
}

static void dispatch(char *line) {
  char *argv[ARGV_MAX];
  int argc = tokenize(line, argv, ARGV_MAX);
  if (argc == 0)
    return;
  for (unsigned i = 0; i < commands_count; ++i) {
    if (strcmp(argv[0], commands[i].name) == 0) {
      commands[i].run(argc, argv);
      return;
    }
  }
  PRINTF("unknown command: %s (try 'help')\r\n", argv[0]);
}

static void repl(void) {
  char line[LINE_MAX];
  int len = 0;
  PRINTF(PROMPT);

  while(1) {
    int c = GETCHAR();

    if (c == '\r' || c == '\n') {
      PRINTF("\r\n");
      line[len] = '\0';
      dispatch(line);
      len = 0;
      PRINTF(PROMPT);
    } else if ((c == 0x7f || c == 0x08) && len > 0) { /* DEL or BS */
      --len;
      PRINTF("\b \b");
    } else if (c ==
               0x1b) { /* ESC: swallow CSI/SS3 sequences (arrow keys, ...) */
      int c1 = GETCHAR();
      if (c1 == '[' || c1 == 'O') {
        int c2;
        do {
          c2 = GETCHAR();
        } while (c2 >= 0x20 && c2 <= 0x3f);
      }
    } else if (c >= 0x20 && c < 0x7f && len < LINE_MAX - 1) {
      line[len++] = (char)c;
      PUTCHAR(c);
    }
  }
}

int main(void) {
  BOARD_InitPins();
  BOARD_BootClockRUN();
  BOARD_InitDebugConsole();
  leds_init();

  install_trap_handler();
  intmux0_enable(LPTMR0_INTMUX_CHANNEL, LPTMR0_INTMUX_BIT);
  eventunit_enable(INTMUX0_0_IRQn);
  set_mstatus_mie();
  /* Start the LPTMR last. Renode's NXP_INTMUX appears to be edge-
   * sensitive, so the channel mask must be in place before the
   * first compare-match fires. */
  lptmr_init_hz(TICK_RATE_HZ);

  PRINTF("\r\n=== VegaConsole (interrupts edition) ===\r\n");
  PRINTF("heartbeat on the blue LED. type 'help' for commands.\r\n");
  repl();
  return 0; /* unreachable; repl() loops forever */
}
