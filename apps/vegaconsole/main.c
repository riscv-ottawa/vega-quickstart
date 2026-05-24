#include <stdint.h>
#include <string.h>
#include "board.h"
#include "fsl_debug_console.h"
#include "fsl_gpio.h"

/*
 * VegaConsole: a tiny line-buffered REPL over LPUART0.
 *
 * Read characters one at a time, accumulate into a line buffer, and on
 * Enter tokenize the buffer into argv[] and dispatch through a fixed
 * command table. Everything is polled and synchronous - main blocks
 * inside getchar() until the user types something. Section 3 will fix
 * that.
 */

#define LINE_MAX 80
#define ARGV_MAX 8
#define PROMPT "vega> "

typedef int (*cmd_fn)(int argc, char **argv);

typedef struct
{
    const char *name;
    const char *help;
    cmd_fn run;
} command_t;

/* ---------- LED helpers ---------- */

static const struct
{
    const char *name;
    uint32_t pin;
} leds[] = {
    {"red", BOARD_LED_RED_PIN},
    {"green", BOARD_LED_GREEN_PIN},
    {"blue", BOARD_LED_BLUE_PIN},
};

static void leds_init(void)
{
    gpio_pin_config_t cfg = {kGPIO_DigitalOutput, 0}; /* LEDs are active-high; 0 = off */
    for (unsigned i = 0; i < sizeof(leds) / sizeof(leds[0]); ++i)
        GPIO_PinInit(BOARD_LED_GPIO, leds[i].pin, &cfg);
}

static int led_find(const char *name)
{
    for (unsigned i = 0; i < sizeof(leds) / sizeof(leds[0]); ++i)
        if (strcmp(name, leds[i].name) == 0)
            return (int)i;
    return -1;
}

/* ---------- commands ---------- */

extern const command_t commands[];
extern const unsigned commands_count;

static int cmd_help(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    PRINTF("commands:\r\n");
    for (unsigned i = 0; i < commands_count; ++i)
        PRINTF("  %s\t%s\r\n", commands[i].name, commands[i].help);
    return 0;
}

static int cmd_led(int argc, char **argv)
{
    if (argc != 3)
    {
        PRINTF("usage: led <red|green|blue> <on|off>\r\n");
        return 1;
    }
    int idx = led_find(argv[1]);
    if (idx < 0)
    {
        PRINTF("unknown color: %s\r\n", argv[1]);
        return 1;
    }
    uint8_t level;
    if (strcmp(argv[2], "on") == 0)
        level = 1; // active-high
    else if (strcmp(argv[2], "off") == 0)
        level = 0;
    else
    {
        PRINTF("expected 'on' or 'off', got: %s\r\n", argv[2]);
        return 1;
    }
    GPIO_WritePinOutput(BOARD_LED_GPIO, leds[idx].pin, level);
    return 0;
}

static int cmd_echo(int argc, char **argv)
{
    for (int i = 1; i < argc; ++i)
        PRINTF("%s%s", argv[i], (i + 1 < argc) ? " " : "");
    PRINTF("\r\n");
    return 0;
}

const command_t commands[] = {
    {"help", "list available commands", cmd_help},
    {"led", "led <color> <on|off>", cmd_led},
    {"echo", "echo <text...>", cmd_echo},
};
const unsigned commands_count = sizeof(commands) / sizeof(commands[0]);

/* ---------- line editor and dispatcher ---------- */

static int tokenize(char *line, char **argv, int max)
{
    int argc = 0;
    char *p = line;
    while (*p && argc < max)
    {
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

static void dispatch(char *line)
{
    char *argv[ARGV_MAX];
    int argc = tokenize(line, argv, ARGV_MAX);
    if (argc == 0)
        return;

    for (unsigned i = 0; i < commands_count; ++i)
    {
        if (strcmp(argv[0], commands[i].name) == 0)
        {
            commands[i].run(argc, argv);
            return;
        }
    }
    PRINTF("unknown command: %s (try 'help')\r\n", argv[0]);
}

static void repl(void)
{
    char line[LINE_MAX];
    int len = 0;

    PRINTF(PROMPT);
    for (;;)
    {
        int c = GETCHAR();

        if (c == '\r' || c == '\n')
        {
            PRINTF("\r\n");
            line[len] = '\0';
            dispatch(line);
            len = 0;
            PRINTF(PROMPT);
        }
        else if ((c == 0x7f || c == 0x08) && len > 0)
        { /* DEL or BS */
            --len;
            PRINTF("\b \b");
        }
        else if (c == 0x1b)
        { /* ESC: swallow a CSI/SS3 sequence (arrow keys, function keys, ...) */
            int c1 = GETCHAR();
            if (c1 == '[' || c1 == 'O')
            {
                int c2;
                do { c2 = GETCHAR(); } while (c2 >= 0x20 && c2 <= 0x3f);
            }
        }
        else if (c >= 0x20 && c < 0x7f && len < LINE_MAX - 1)
        {
            line[len++] = (char)c;
            PUTCHAR(c);
        }
        /* Anything else (Ctrl-C, ...) is silently dropped. */
    }
}

int main(void)
{
    BOARD_InitPins();
    BOARD_BootClockRUN();
    BOARD_InitDebugConsole();
    leds_init();

    PRINTF("\r\n=== VegaConsole ===\r\n");
    PRINTF("type 'help' to see what's available.\r\n");
    repl();
    return 0; /* unreachable; repl() loops forever */
}
