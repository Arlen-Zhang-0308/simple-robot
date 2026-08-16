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
#define ANIMATION_PHASE_FRAMES  40U

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

static void draw_pixel(int16_t x, int16_t y, bool value)
{
    size_t offset;
    uint8_t mask;

    if ((x < 0) || (x >= (int16_t)DISPLAY_WIDTH) ||
        (y < 0) || (y >= (int16_t)DISPLAY_HEIGHT)) {
        return;
    }
    offset = (size_t)(y / 8) * DISPLAY_WIDTH + (uint16_t)x;
    mask = (uint8_t)(1U << (y & 7));
    if (value) {
        frame_buffer[offset] |= mask;
    } else {
        frame_buffer[offset] &= (uint8_t)~mask;
    }
}

static void draw_rect(int16_t x, int16_t y, int16_t width, int16_t height, bool value)
{
    int16_t xx;
    int16_t yy;

    for (yy = y; yy < (y + height); yy++) {
        for (xx = x; xx < (x + width); xx++) {
            draw_pixel(xx, yy, value);
        }
    }
}

static void draw_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1)
{
    int16_t dx = (x0 < x1) ? (x1 - x0) : (x0 - x1);
    int16_t sx = (x0 < x1) ? 1 : -1;
    int16_t dy = (y0 < y1) ? (y0 - y1) : (y1 - y0);
    int16_t sy = (y0 < y1) ? 1 : -1;
    int16_t error = (int16_t)(dx + dy);

    for (;;) {
        int16_t error2;
        draw_pixel(x0, y0, true);
        if ((x0 == x1) && (y0 == y1)) {
            break;
        }
        error2 = (int16_t)(2 * error);
        if (error2 >= dy) {
            error = (int16_t)(error + dy);
            x0 = (int16_t)(x0 + sx);
        }
        if (error2 <= dx) {
            error = (int16_t)(error + dx);
            y0 = (int16_t)(y0 + sy);
        }
    }
}

static void draw_outline(int16_t x, int16_t y, int16_t width, int16_t height)
{
    draw_rect(x, y, width, 1, true);
    draw_rect(x, (int16_t)(y + height - 1), width, 1, true);
    draw_rect(x, y, 1, height, true);
    draw_rect((int16_t)(x + width - 1), y, 1, height, true);
}

static void draw_circle(int16_t cx, int16_t cy, int16_t radius)
{
    int16_t x = radius;
    int16_t y = 0;
    int16_t error = (int16_t)(1 - x);

    while (x >= y) {
        draw_pixel((int16_t)(cx + x), (int16_t)(cy + y), true);
        draw_pixel((int16_t)(cx + y), (int16_t)(cy + x), true);
        draw_pixel((int16_t)(cx - y), (int16_t)(cy + x), true);
        draw_pixel((int16_t)(cx - x), (int16_t)(cy + y), true);
        draw_pixel((int16_t)(cx - x), (int16_t)(cy - y), true);
        draw_pixel((int16_t)(cx - y), (int16_t)(cy - x), true);
        draw_pixel((int16_t)(cx + y), (int16_t)(cy - x), true);
        draw_pixel((int16_t)(cx + x), (int16_t)(cy - y), true);
        y++;
        if (error < 0) {
            error = (int16_t)(error + (2 * y) + 1);
        } else {
            x--;
            error = (int16_t)(error + (2 * (y - x + 1)));
        }
    }
}

static void invert_buffer(void)
{
    size_t index;
    for (index = 0U; index < sizeof(frame_buffer); index++) {
        frame_buffer[index] ^= 0xFFU;
    }
}

static void draw_animation_border(uint16_t frame)
{
    int16_t position;

    draw_outline(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    for (position = (int16_t)((frame / 2U) % 8U); position < DISPLAY_WIDTH; position += 8) {
        draw_pixel(position, 1, true);
        draw_pixel((int16_t)(DISPLAY_WIDTH - 1 - position), DISPLAY_HEIGHT - 2, true);
    }
    for (position = (int16_t)((frame / 4U) % 8U); position < DISPLAY_HEIGHT; position += 8) {
        draw_pixel(1, position, true);
        draw_pixel(DISPLAY_WIDTH - 2, (int16_t)(DISPLAY_HEIGHT - 1 - position), true);
    }
}

static void draw_checker(int16_t x, int16_t y, int16_t width, int16_t height, uint16_t offset)
{
    int16_t xx;
    int16_t yy;

    for (yy = 0; yy < height; yy++) {
        for (xx = 0; xx < width; xx++) {
            if ((((uint16_t)xx + (uint16_t)yy + offset) & 1U) == 0U) {
                draw_pixel((int16_t)(x + xx), (int16_t)(y + yy), true);
            }
        }
    }
}

static void draw_creature(int16_t cx, int16_t cy, uint16_t frame, int16_t split)
{
    int16_t breathe = ((frame / 4U) & 1U) != 0U ? 2 : 0;
    int16_t feeler = (int16_t)((frame / 2U) % 5U);
    int16_t foot = (int16_t)((frame / 2U) % 4U);
    int16_t x;

    draw_outline((int16_t)(cx - 18 - split), (int16_t)(cy - 9), 15, (int16_t)(18 + breathe));
    draw_outline((int16_t)(cx + 4 + split), (int16_t)(cy - 9), 15, (int16_t)(18 + breathe));
    draw_checker((int16_t)(cx - 2), (int16_t)(cy - 7), 5, 14, frame / 2U);
    draw_rect((int16_t)(cx - 13 - split), (int16_t)(cy - 4), 5, 5, true);
    draw_rect((int16_t)(cx + 9 + split), (int16_t)(cy - 4), 5, 5, true);
    draw_pixel((int16_t)(cx - 11 - split), (int16_t)(cy - 2), false);
    draw_pixel((int16_t)(cx + 11 + split), (int16_t)(cy - 2), false);
    draw_line((int16_t)(cx - 18 - split), (int16_t)(cy - 6),
              (int16_t)(cx - 29 - split), (int16_t)(cy - 15 + feeler));
    draw_line((int16_t)(cx + 18 + split), (int16_t)(cy - 6),
              (int16_t)(cx + 29 + split), (int16_t)(cy - 15 + feeler));
    draw_line((int16_t)(cx - 16 - split), (int16_t)(cy + 8),
              (int16_t)(cx - 25 - split), (int16_t)(cy + 18 - foot));
    draw_line((int16_t)(cx + 16 + split), (int16_t)(cy + 8),
              (int16_t)(cx + 25 + split), (int16_t)(cy + 18 - foot));
    draw_rect((int16_t)(cx - 8), (int16_t)(cy + 13), 17, 1, true);
    for (x = (int16_t)(cx - 7); x <= (cx + 7); x += 4) {
        draw_rect(x, (int16_t)(cy + 13), 1, (int16_t)(3 + ((x + frame) & 3)), true);
    }
}

static void draw_seed(uint16_t local)
{
    uint16_t index;
    int16_t pulse = (int16_t)((local / 4U) % 6U);

    draw_circle(64, 32, (int16_t)(3 + pulse));
    draw_circle(64, 32, (int16_t)(9 + ((local / 8U) % 4U)));
    draw_line((int16_t)(28 + local), 32, 53, 32);
    draw_line((int16_t)(100 - local), 32, 75, 32);
    for (index = 0U; index < 12U; index++) {
        draw_pixel((int16_t)(((index * 23U + local * 3U) % 124U) + 2U),
                   (int16_t)(((index * 11U + local) % 60U) + 2U), true);
    }
    draw_rect(61, 29, 7, 7, true);
    draw_rect(63, 31, 3, 3, false);
}

static void draw_assemble(uint16_t local)
{
    uint16_t index;
    int16_t reach = (local / 2U) < 18U ? (int16_t)(local / 2U) : 18;

    draw_outline((int16_t)(64 - reach), (int16_t)(32 - reach / 2),
                 (reach > 0) ? (int16_t)(reach * 2) : 1, (reach > 0) ? reach : 1);
    draw_creature(64, 31, local, 0);
    for (index = 0U; index < 8U; index++) {
        int16_t y = (int16_t)(8U + index * 6U);
        int16_t x = (int16_t)(8U + ((local * (index + 1U) + index * 13U) % 112U));
        draw_rect(x, y, (int16_t)(3U + (index & 3U)), 1, true);
    }
}

static void draw_crawl(uint16_t local)
{
    int16_t y;
    int16_t x;
    int16_t bob = ((local / 4U) & 1U) != 0U ? 1 : -1;
    int16_t cx = (int16_t)(48U + ((local * 2U) % 34U));

    draw_creature(cx, (int16_t)(31 + bob), local, 0);
    for (y = 8; y < 57; y += 8) {
        int16_t shift = (int16_t)((local * (((y & 15) != 0) ? 2U : 3U)) % 16U);
        for (x = (int16_t)-shift; x < DISPLAY_WIDTH; x += 16) {
            draw_rect(x, y, 7, 1, true);
        }
    }
    draw_rect(4, 53, 120, 1, true);
    for (x = 6; x < 124; x += 6) {
        draw_pixel((int16_t)(((x - (int16_t)(local * 2U) + 248) % 124) + 2), 55, true);
    }
}

static void draw_divide(uint16_t local)
{
    int16_t y;
    int16_t split = (local / 2U) < 20U ? (int16_t)(local / 2U) : 20;

    draw_creature(64, 31, local, split);
    draw_rect(64, 5, 1, 54, true);
    for (y = 5; y < 59; y += 4) {
        draw_pixel((int16_t)(61 - (local % 12U)), y, true);
        draw_pixel((int16_t)(67 + (local % 12U)), (int16_t)(63 - y), true);
    }
    if (split > 10) {
        draw_circle(32, 32, (int16_t)(7U + ((local / 4U) & 3U)));
        draw_circle(96, 32, (int16_t)(7U + (((local / 4U) + 2U) & 3U)));
    }
}

static void draw_inversion(uint16_t local)
{
    int16_t band = (int16_t)((local * 4U) % DISPLAY_HEIGHT);

    draw_creature(64, 31, local, 5);
    draw_checker(8, 8, 24, 48, local / 2U);
    draw_checker(96, 8, 24, 48, local / 2U + 1U);
    draw_rect(2, band, 124, 5, true);
    draw_rect(2, (int16_t)(band + 1), 124, 3, false);
    if ((local % 10U) < 5U) {
        invert_buffer();
    }
}

static void draw_collapse(uint16_t local)
{
    uint16_t index;
    int16_t shrink = (int16_t)(22 - local / 2U);
    int16_t distance = (local < 38U) ? (int16_t)(38U - local) : 0;

    if (shrink < 2) {
        shrink = 2;
    }
    draw_outline((int16_t)(64 - shrink), (int16_t)(32 - shrink / 2),
                 (int16_t)(shrink * 2), shrink);
    draw_circle(64, 32, (shrink / 2) > 2 ? (int16_t)(shrink / 2) : 2);
    for (index = 0U; index < 24U; index++) {
        int16_t x = (int16_t)(64 + ((((int16_t)((index * 17U) % 31U) - 15) * distance) >> 5));
        int16_t y = (int16_t)(32 + ((((int16_t)((index * 11U) % 25U) - 12) * distance) >> 5));
        draw_pixel(x, y, true);
    }
    if (local > 32U) {
        draw_rect(61, 29, 7, 7, true);
        draw_rect(63, 31, 3, 3, false);
    }
}

static void draw_animation_frame(uint16_t frame)
{
    uint16_t phase = (uint16_t)((frame % APP_DISPLAY_ANIMATION_FRAMES) / ANIMATION_PHASE_FRAMES);
    uint16_t local = (uint16_t)(frame % ANIMATION_PHASE_FRAMES);

    clear_buffer();
    draw_animation_border(frame);
    switch (phase) {
    case 0U: draw_seed(local); break;
    case 1U: draw_assemble(local); break;
    case 2U: draw_crawl(local); break;
    case 3U: draw_divide(local); break;
    case 4U: draw_inversion(local); break;
    default: draw_collapse(local); break;
    }
    if (local < 2U) {
        invert_buffer();
    }
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

void display_render_animation(uint16_t frame)
{
#if !APP_ENABLE_OLED || (APP_OLED_IMPL == APP_OLED_IMPL_NONE) || \
    (APP_OLED_IMPL == APP_OLED_IMPL_STUB)
    (void)frame;
#elif APP_OLED_IMPL == APP_OLED_IMPL_SSD1306_SPI
    if (!display_ready) {
        return;
    }

    draw_animation_frame(frame);
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
