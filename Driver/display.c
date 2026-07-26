#include "display.h"

#include <stddef.h>
#include <string.h>

#include "app_config.h"
#include "display_stub.h"

#if APP_ENABLE_OLED && (APP_OLED_IMPL == APP_OLED_IMPL_SSD1306_SPI)
#include "main.h"
#include "stm32f1xx_hal.h"

extern SPI_HandleTypeDef hspi1;

#define DISPLAY_WIDTH          128U
#define DISPLAY_HEIGHT          64U
#define DISPLAY_PAGE_COUNT       8U
#define DISPLAY_BUFFER_SIZE   1024U
#define DISPLAY_SPI_TIMEOUT_MS 100U
#define DISPLAY_PAGE_STATUS      0U
#define DISPLAY_PAGE_MOTION      1U

static uint8_t frame_buffer[DISPLAY_BUFFER_SIZE];
static bool display_ready;

static const uint8_t font_5x7[][5] = {
    [' ' - ' '] = {0x00, 0x00, 0x00, 0x00, 0x00},
    ['%' - ' '] = {0x62, 0x64, 0x08, 0x13, 0x23},
    ['-' - ' '] = {0x08, 0x08, 0x08, 0x08, 0x08},
    ['.' - ' '] = {0x00, 0x60, 0x60, 0x00, 0x00},
    ['0' - ' '] = {0x3E, 0x51, 0x49, 0x45, 0x3E},
    ['1' - ' '] = {0x00, 0x42, 0x7F, 0x40, 0x00},
    ['2' - ' '] = {0x42, 0x61, 0x51, 0x49, 0x46},
    ['3' - ' '] = {0x21, 0x41, 0x45, 0x4B, 0x31},
    ['4' - ' '] = {0x18, 0x14, 0x12, 0x7F, 0x10},
    ['5' - ' '] = {0x27, 0x45, 0x45, 0x45, 0x39},
    ['6' - ' '] = {0x3C, 0x4A, 0x49, 0x49, 0x30},
    ['7' - ' '] = {0x01, 0x71, 0x09, 0x05, 0x03},
    ['8' - ' '] = {0x36, 0x49, 0x49, 0x49, 0x36},
    ['9' - ' '] = {0x06, 0x49, 0x49, 0x29, 0x1E},
    [':' - ' '] = {0x00, 0x36, 0x36, 0x00, 0x00},
    ['?' - ' '] = {0x02, 0x01, 0x51, 0x09, 0x06},
    ['A' - ' '] = {0x7E, 0x11, 0x11, 0x11, 0x7E},
    ['B' - ' '] = {0x7F, 0x49, 0x49, 0x49, 0x36},
    ['C' - ' '] = {0x3E, 0x41, 0x41, 0x41, 0x22},
    ['D' - ' '] = {0x7F, 0x41, 0x41, 0x22, 0x1C},
    ['E' - ' '] = {0x7F, 0x49, 0x49, 0x49, 0x41},
    ['F' - ' '] = {0x7F, 0x09, 0x09, 0x09, 0x01},
    ['G' - ' '] = {0x3E, 0x41, 0x49, 0x49, 0x7A},
    ['H' - ' '] = {0x7F, 0x08, 0x08, 0x08, 0x7F},
    ['I' - ' '] = {0x00, 0x41, 0x7F, 0x41, 0x00},
    ['K' - ' '] = {0x7F, 0x08, 0x14, 0x22, 0x41},
    ['L' - ' '] = {0x7F, 0x40, 0x40, 0x40, 0x40},
    ['M' - ' '] = {0x7F, 0x02, 0x0C, 0x02, 0x7F},
    ['N' - ' '] = {0x7F, 0x04, 0x08, 0x10, 0x7F},
    ['O' - ' '] = {0x3E, 0x41, 0x41, 0x41, 0x3E},
    ['P' - ' '] = {0x7F, 0x09, 0x09, 0x09, 0x06},
    ['R' - ' '] = {0x7F, 0x09, 0x19, 0x29, 0x46},
    ['S' - ' '] = {0x46, 0x49, 0x49, 0x49, 0x31},
    ['T' - ' '] = {0x01, 0x01, 0x7F, 0x01, 0x01},
    ['U' - ' '] = {0x3F, 0x40, 0x40, 0x40, 0x3F},
    ['V' - ' '] = {0x1F, 0x20, 0x40, 0x20, 0x1F},
    ['W' - ' '] = {0x3F, 0x40, 0x38, 0x40, 0x3F},
    ['Y' - ' '] = {0x07, 0x08, 0x70, 0x08, 0x07}
};

static bool write_bytes(GPIO_PinState dc_state, const uint8_t *data, uint16_t length)
{
    HAL_GPIO_WritePin(OLED_DC_GPIO_Port, OLED_DC_Pin, dc_state);
    return HAL_SPI_Transmit(&hspi1, (uint8_t *)data, length, DISPLAY_SPI_TIMEOUT_MS) == HAL_OK;
}

static bool write_commands(const uint8_t *commands, uint16_t length)
{
    return write_bytes(GPIO_PIN_RESET, commands, length);
}

static void clear_buffer(void)
{
    memset(frame_buffer, 0, sizeof(frame_buffer));
}

static const uint8_t *glyph_for(char character)
{
    static const uint8_t blank[5] = {0U, 0U, 0U, 0U, 0U};
    uint8_t index;

    if ((character >= 'a') && (character <= 'z')) {
        character = (char)(character - ('a' - 'A'));
    }
    if ((character < ' ') || (character > 'Y')) {
        character = '?';
    }

    index = (uint8_t)(character - ' ');
    if ((font_5x7[index][0] == 0U) && (font_5x7[index][1] == 0U) &&
        (font_5x7[index][2] == 0U) && (font_5x7[index][3] == 0U) &&
        (font_5x7[index][4] == 0U) && (character != ' ')) {
        return blank;
    }
    return font_5x7[index];
}

static void draw_text(uint8_t x, uint8_t page, const char *text)
{
    while ((*text != '\0') && (x <= (DISPLAY_WIDTH - 6U)) && (page < DISPLAY_PAGE_COUNT)) {
        const uint8_t *glyph = glyph_for(*text++);
        size_t offset = (size_t)page * DISPLAY_WIDTH + x;
        memcpy(&frame_buffer[offset], glyph, 5U);
        frame_buffer[offset + 5U] = 0U;
        x = (uint8_t)(x + 6U);
    }
}

static void append_uint(char *buffer, uint8_t *length, uint16_t value, uint8_t minimum_digits)
{
    char reversed[5];
    uint8_t count = 0U;

    do {
        reversed[count++] = (char)('0' + (value % 10U));
        value /= 10U;
    } while ((value > 0U) && (count < sizeof(reversed)));

    while (count < minimum_digits) {
        reversed[count++] = '0';
    }
    while (count > 0U) {
        buffer[(*length)++] = reversed[--count];
    }
}

static void draw_voltage(uint8_t page, uint16_t millivolts)
{
    char text[16];
    uint8_t length = 0U;

    memcpy(text, "POWER ", 6U);
    length = 6U;
    append_uint(text, &length, (uint16_t)(millivolts / 1000U), 1U);
    text[length++] = '.';
    append_uint(text, &length, (uint16_t)((millivolts % 1000U) / 10U), 2U);
    text[length++] = 'V';
    text[length] = '\0';
    draw_text(0U, page, text);
}

static const char *motion_name(MotionState motion)
{
    switch (motion) {
    case MOTION_STATE_STOPPED:        return "STOP";
    case MOTION_STATE_FORWARD:        return "FORWARD";
    case MOTION_STATE_BACKWARD:       return "BACKWARD";
    case MOTION_STATE_TURN_LEFT:      return "LEFT";
    case MOTION_STATE_TURN_RIGHT:     return "RIGHT";
    case MOTION_STATE_EMERGENCY_STOP: return "EMERGENCY";
    default:                          return "UNKNOWN";
    }
}

static void draw_motion(uint8_t page, const RobotState *state)
{
    char speed_text[16];
    uint8_t length = 0U;

    draw_text(0U, page, "MOTION");
    draw_text(48U, page, motion_name(state->motion));

    memcpy(speed_text, "SPEED ", 6U);
    length = 6U;
    append_uint(speed_text, &length, state->speed, 1U);
    speed_text[length++] = '%';
    speed_text[length] = '\0';
    draw_text(0U, (uint8_t)(page + 2U), speed_text);
}

static void draw_status_page(const RobotState *state)
{
    char error_text[16];
    uint8_t length = 0U;

    draw_text(0U, 0U, "SIMPLE ROBOT");
    draw_voltage(2U, state->sensor.input_voltage_mv);
    draw_text(0U, 4U, state->sensor.low_power ? "POWER LOW" : "POWER OK");
    draw_text(0U, 6U, "STATE");
    draw_text(42U, 6U, motion_name(state->motion));

    if (state->last_error != 0U) {
        memcpy(error_text, "ERR ", 4U);
        length = 4U;
        append_uint(error_text, &length, state->last_error, 1U);
        error_text[length] = '\0';
        draw_text(84U, 4U, error_text);
    }
}

static void draw_motion_page(const RobotState *state)
{
    draw_text(0U, 0U, "ACTION PAGE");
    draw_motion(2U, state);
    draw_text(0U, 6U, state->sensor.low_power ? "LOCK LOW POWER" : "DRIVE READY");
}

static bool flush_buffer(void)
{
    static const uint8_t address_commands[] = {
        0x21U, 0x00U, 0x7FU,
        0x22U, 0x00U, 0x07U
    };

    return write_commands(address_commands, sizeof(address_commands)) &&
           write_bytes(GPIO_PIN_SET, frame_buffer, sizeof(frame_buffer));
}

static bool ssd1306_init(void)
{
    static const uint8_t init_commands[] = {
        0xAEU,
        0xD5U, 0x80U,
        0xA8U, 0x3FU,
        0xD3U, 0x00U,
        0x40U,
        0x8DU, 0x14U,
        0x20U, 0x00U,
        0xA1U,
        0xC8U,
        0xDAU, 0x12U,
        0x81U, 0x7FU,
        0xD9U, 0xF1U,
        0xDBU, 0x40U,
        0xA4U,
        0xA6U,
        0xAFU
    };

    HAL_GPIO_WritePin(OLED_RST_GPIO_Port, OLED_RST_Pin, GPIO_PIN_RESET);
    HAL_Delay(10U);
    HAL_GPIO_WritePin(OLED_RST_GPIO_Port, OLED_RST_Pin, GPIO_PIN_SET);
    HAL_Delay(10U);

    if (!write_commands(init_commands, sizeof(init_commands))) {
        return false;
    }
    clear_buffer();
    return flush_buffer();
}
#endif

void display_init(void)
{
#if !APP_ENABLE_OLED || (APP_OLED_IMPL == APP_OLED_IMPL_NONE)
    return;
#elif APP_OLED_IMPL == APP_OLED_IMPL_STUB
    display_stub_init();
#elif APP_OLED_IMPL == APP_OLED_IMPL_SSD1306_SPI
    display_ready = ssd1306_init();
#else
#error "Selected OLED implementation is not available"
#endif
}

void display_render(const RobotState *state)
{
    if (state == NULL) {
        return;
    }

#if !APP_ENABLE_OLED || (APP_OLED_IMPL == APP_OLED_IMPL_NONE)
    (void)state;
#elif APP_OLED_IMPL == APP_OLED_IMPL_STUB
    display_stub_render(state);
#elif APP_OLED_IMPL == APP_OLED_IMPL_SSD1306_SPI
    if (!display_ready) {
        return;
    }

    clear_buffer();
    if (state->display_page == DISPLAY_PAGE_MOTION) {
        draw_motion_page(state);
    } else {
        draw_status_page(state);
    }
    if (!flush_buffer()) {
        display_ready = false;
    }
#endif
}

bool display_is_ready(void)
{
#if !APP_ENABLE_OLED || (APP_OLED_IMPL == APP_OLED_IMPL_NONE)
    return false;
#elif APP_OLED_IMPL == APP_OLED_IMPL_STUB
    return true;
#elif APP_OLED_IMPL == APP_OLED_IMPL_SSD1306_SPI
    return display_ready;
#endif
}
