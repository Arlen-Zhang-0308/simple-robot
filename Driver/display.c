#include "display.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

#include "app_config.h"
#include "display_stub.h"
#include "tetra_trig_lut.h"

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
#define TETRA_Q16_SHIFT             16
#define TETRA_Q16_ONE               (1L << TETRA_Q16_SHIFT)
#define TETRA_TWO_PI_Q16            411775L
#define TETRA_HALF_PI_Q16           102944L
#define TETRA_UNIT_Q16               37837L
#define TETRA_SCALE_Q16             983040L
#define TETRA_CAMERA_Q16            242483L
#define TETRA_INITIAL_X_Q16        3407872L
#define TETRA_INITIAL_Y_Q16        2031616L
/* Web animation at the selected 2x speed and 50 ms simulation step. */
#define TETRA_DX_Q16                 85197L
#define TETRA_DY_Q16                 52429L
#define TETRA_DAX_Q16                 4719L
#define TETRA_DAY_Q16                 6750L
#define TETRA_DAZ_Q16                 3080L

typedef struct {
    int32_t x_q16;
    int32_t y_q16;
    int32_t dx_q16;
    int32_t dy_q16;
    int32_t ax_q16;
    int32_t ay_q16;
    int32_t az_q16;
    uint32_t tick;
} TetraState;

static uint8_t frame_buffer[DISPLAY_BUFFER_SIZE];
static bool display_ready;
static TetraState tetra_state = {
    TETRA_INITIAL_X_Q16, TETRA_INITIAL_Y_Q16,
    TETRA_DX_Q16, TETRA_DY_Q16,
    13107L, 36045L, 6554L, 0U
};

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

static void draw_animation_border(uint32_t tick)
{
    int16_t position;

    draw_outline(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    for (position = (int16_t)((tick >> 1U) & 7U); position < DISPLAY_WIDTH; position += 8) {
        draw_pixel(position, 1, true);
        draw_pixel((int16_t)(DISPLAY_WIDTH - 1 - position), DISPLAY_HEIGHT - 2, true);
    }
}

static int32_t q16_wrap_angle(int32_t angle_q16)
{
    angle_q16 %= TETRA_TWO_PI_Q16;
    return (angle_q16 < 0L) ? (angle_q16 + TETRA_TWO_PI_Q16) : angle_q16;
}

static int16_t sin_q15(int32_t angle_q16)
{
    uint32_t phase_q16;
    uint16_t index;
    uint16_t fraction;
    int32_t first;
    int32_t second;

    angle_q16 = q16_wrap_angle(angle_q16);
    phase_q16 = (uint32_t)(((int64_t)angle_q16 * TETRA_TRIG_LUT_STEPS << 16) / TETRA_TWO_PI_Q16);
    index = (uint16_t)((phase_q16 >> 16) & (TETRA_TRIG_LUT_STEPS - 1U));
    fraction = (uint16_t)phase_q16;
    first = tetra_sin_q15[index];
    second = tetra_sin_q15[index + 1U];
    return (int16_t)(first + (((second - first) * fraction + 32768L) >> 16));
}

static int16_t cos_q15(int32_t angle_q16)
{
    return sin_q15(angle_q16 + TETRA_HALF_PI_Q16);
}

static int32_t q16_mul_q15(int32_t value_q16, int16_t factor_q15)
{
    return (int32_t)(((int64_t)value_q16 * factor_q15) >> 15);
}

static int16_t q16_round_to_i16(int32_t value_q16)
{
    return (value_q16 >= 0L) ?
        (int16_t)((value_q16 + 0x8000L) >> 16) :
        (int16_t)-(((-value_q16) + 0x8000L) >> 16);
}

static void rotate_tetra_point(int32_t *x_q16, int32_t *y_q16, int32_t *z_q16)
{
    int32_t x = *x_q16;
    int32_t y = *y_q16;
    int32_t z = *z_q16;
    int32_t y1;
    int32_t z1;
    int32_t x2;
    int32_t z2;
    int16_t sx = sin_q15(tetra_state.ax_q16);
    int16_t cx = cos_q15(tetra_state.ax_q16);
    int16_t sy = sin_q15(tetra_state.ay_q16);
    int16_t cy = cos_q15(tetra_state.ay_q16);
    int16_t sz = sin_q15(tetra_state.az_q16);
    int16_t cz = cos_q15(tetra_state.az_q16);

    y1 = q16_mul_q15(y, cx) - q16_mul_q15(z, sx);
    z1 = q16_mul_q15(y, sx) + q16_mul_q15(z, cx);
    x2 = q16_mul_q15(x, cy) + q16_mul_q15(z1, sy);
    z2 = -q16_mul_q15(x, sy) + q16_mul_q15(z1, cy);
    *x_q16 = q16_mul_q15(x2, cz) - q16_mul_q15(y1, sz);
    *y_q16 = q16_mul_q15(x2, sz) + q16_mul_q15(y1, cz);
    *z_q16 = z2;
}

static void tetra_points(int32_t points_q16[4][2], int32_t bounds_q16[4])
{
    static const int32_t vertices_q16[4][3] = {
        { TETRA_UNIT_Q16,  TETRA_UNIT_Q16,  TETRA_UNIT_Q16},
        { TETRA_UNIT_Q16, -TETRA_UNIT_Q16, -TETRA_UNIT_Q16},
        {-TETRA_UNIT_Q16,  TETRA_UNIT_Q16, -TETRA_UNIT_Q16},
        {-TETRA_UNIT_Q16, -TETRA_UNIT_Q16,  TETRA_UNIT_Q16}
    };
    uint8_t index;

    bounds_q16[0] = INT32_MAX; bounds_q16[1] = INT32_MIN;
    bounds_q16[2] = INT32_MAX; bounds_q16[3] = INT32_MIN;
    for (index = 0U; index < 4U; index++) {
        int32_t x_q16 = vertices_q16[index][0];
        int32_t y_q16 = vertices_q16[index][1];
        int32_t z_q16 = vertices_q16[index][2];
        int32_t perspective_q16;
        int32_t projected_x_q16;
        int32_t projected_y_q16;

        rotate_tetra_point(&x_q16, &y_q16, &z_q16);
        perspective_q16 = (int32_t)(((int64_t)TETRA_CAMERA_Q16 << 16) / (TETRA_CAMERA_Q16 - z_q16));
        projected_x_q16 = (int32_t)(((int64_t)((int64_t)x_q16 * TETRA_SCALE_Q16 >> 16) * perspective_q16) >> 16);
        projected_y_q16 = (int32_t)(((int64_t)((int64_t)y_q16 * TETRA_SCALE_Q16 >> 16) * perspective_q16) >> 16);
        points_q16[index][0] = projected_x_q16;
        points_q16[index][1] = projected_y_q16;
        if (projected_x_q16 < bounds_q16[0]) bounds_q16[0] = projected_x_q16;
        if (projected_x_q16 > bounds_q16[1]) bounds_q16[1] = projected_x_q16;
        if (projected_y_q16 < bounds_q16[2]) bounds_q16[2] = projected_y_q16;
        if (projected_y_q16 > bounds_q16[3]) bounds_q16[3] = projected_y_q16;
    }
}

static void step_tetra(void)
{
    int32_t points_q16[4][2];
    int32_t bounds_q16[4];

    tetra_state.ax_q16 = q16_wrap_angle(tetra_state.ax_q16 + TETRA_DAX_Q16);
    tetra_state.ay_q16 = q16_wrap_angle(tetra_state.ay_q16 + TETRA_DAY_Q16);
    tetra_state.az_q16 = q16_wrap_angle(tetra_state.az_q16 + TETRA_DAZ_Q16);
    tetra_state.x_q16 += tetra_state.dx_q16;
    tetra_state.y_q16 += tetra_state.dy_q16;
    tetra_points(points_q16, bounds_q16);
    if (tetra_state.x_q16 + bounds_q16[0] < 0L) {
        tetra_state.x_q16 = -bounds_q16[0];
        tetra_state.dx_q16 = (tetra_state.dx_q16 < 0L) ? -tetra_state.dx_q16 : tetra_state.dx_q16;
    } else if (tetra_state.x_q16 + bounds_q16[1] > ((DISPLAY_WIDTH - 1U) << 16)) {
        tetra_state.x_q16 = ((DISPLAY_WIDTH - 1U) << 16) - bounds_q16[1];
        tetra_state.dx_q16 = (tetra_state.dx_q16 > 0L) ? -tetra_state.dx_q16 : tetra_state.dx_q16;
    }
    if (tetra_state.y_q16 + bounds_q16[2] < 0L) {
        tetra_state.y_q16 = -bounds_q16[2];
        tetra_state.dy_q16 = (tetra_state.dy_q16 < 0L) ? -tetra_state.dy_q16 : tetra_state.dy_q16;
    } else if (tetra_state.y_q16 + bounds_q16[3] > ((DISPLAY_HEIGHT - 1U) << 16)) {
        tetra_state.y_q16 = ((DISPLAY_HEIGHT - 1U) << 16) - bounds_q16[3];
        tetra_state.dy_q16 = (tetra_state.dy_q16 > 0L) ? -tetra_state.dy_q16 : tetra_state.dy_q16;
    }
    tetra_state.tick++;
}

static void draw_animation_frame(void)
{
    static const uint8_t edges[6][2] = {{0U, 1U}, {0U, 2U}, {0U, 3U}, {1U, 2U}, {1U, 3U}, {2U, 3U}};
    int32_t points_q16[4][2];
    int32_t bounds_q16[4];
    uint8_t index;

    step_tetra();
    clear_buffer();
    draw_animation_border(tetra_state.tick);
    tetra_points(points_q16, bounds_q16);
    for (index = 0U; index < 6U; index++) {
        uint8_t a = edges[index][0];
        uint8_t b = edges[index][1];
        draw_line(q16_round_to_i16(points_q16[a][0] + tetra_state.x_q16),
                  q16_round_to_i16(points_q16[a][1] + tetra_state.y_q16),
                  q16_round_to_i16(points_q16[b][0] + tetra_state.x_q16),
                  q16_round_to_i16(points_q16[b][1] + tetra_state.y_q16));
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

void display_render_animation(void)
{
#if !APP_ENABLE_OLED || (APP_OLED_IMPL == APP_OLED_IMPL_NONE) || \
    (APP_OLED_IMPL == APP_OLED_IMPL_STUB)
#elif APP_OLED_IMPL == APP_OLED_IMPL_SSD1306_SPI
    if (!display_ready) {
        return;
    }

    draw_animation_frame();
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
