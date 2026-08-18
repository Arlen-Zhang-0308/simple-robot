#include "nrf24l01.h"

#include "app_config.h"
#include "main.h"
#include "communication_system.h"
#include "transport.h"

extern SPI_HandleTypeDef hspi2;

#define NRF24_CMD_R_REGISTER       0x00U
#define NRF24_CMD_W_REGISTER       0x20U
#define NRF24_CMD_R_RX_PAYLOAD     0x61U
#define NRF24_CMD_W_TX_PAYLOAD     0xA0U
#define NRF24_CMD_FLUSH_TX         0xE1U
#define NRF24_CMD_FLUSH_RX         0xE2U
#define NRF24_CMD_NOP              0xFFU

#define NRF24_REG_CONFIG            0x00U
#define NRF24_REG_EN_AA             0x01U
#define NRF24_REG_EN_RXADDR         0x02U
#define NRF24_REG_SETUP_AW          0x03U
#define NRF24_REG_SETUP_RETR        0x04U
#define NRF24_REG_RF_CH             0x05U
#define NRF24_REG_RF_SETUP          0x06U
#define NRF24_REG_STATUS            0x07U
#define NRF24_REG_RX_ADDR_P0        0x0AU
#define NRF24_REG_TX_ADDR           0x10U
#define NRF24_REG_RX_PW_P0          0x11U
#define NRF24_REG_FIFO_STATUS       0x17U

#define NRF24_CONFIG_EN_CRC         0x08U
#define NRF24_CONFIG_CRCO           0x04U
#define NRF24_CONFIG_PWR_UP         0x02U
#define NRF24_CONFIG_PRIM_RX        0x01U
#define NRF24_STATUS_RX_DR          0x40U
#define NRF24_STATUS_TX_DS          0x20U
#define NRF24_STATUS_MAX_RT         0x10U
#define NRF24_FIFO_RX_EMPTY         0x01U
#define NRF24_PAYLOAD_SIZE          32U
#define NRF24_MAX_USER_PAYLOAD_SIZE (NRF24_PAYLOAD_SIZE - 1U)
#define NRF24_SPI_TIMEOUT_MS        10U

static const uint8_t nrf24_address[5] = {0xE7U, 0xE7U, 0xE7U, 0xE7U, 0xE7U};
static volatile bool irq_pending;
static bool ready;

static void csn_set(GPIO_PinState state)
{
    HAL_GPIO_WritePin(NRF_CSN_GPIO_Port, NRF_CSN_Pin, state);
}

static void ce_set(GPIO_PinState state)
{
    HAL_GPIO_WritePin(NRF_CE_GPIO_Port, NRF_CE_Pin, state);
}

static bool transfer(const uint8_t *tx, uint8_t *rx, uint16_t length)
{
    bool success;

    csn_set(GPIO_PIN_RESET);
    success = HAL_SPI_TransmitReceive(&hspi2, (uint8_t *)tx, rx, length, NRF24_SPI_TIMEOUT_MS) == HAL_OK;
    csn_set(GPIO_PIN_SET);
    return success;
}

static uint8_t command(uint8_t value)
{
    uint8_t tx[1] = {value};
    uint8_t rx[1] = {0U};

    (void)transfer(tx, rx, sizeof(tx));
    return rx[0];
}

static bool write_register(uint8_t reg, const uint8_t *data, uint8_t length)
{
    uint8_t tx[6];
    uint8_t rx[6];

    if ((data == NULL) || (length == 0U) || (length > 5U)) {
        return false;
    }
    tx[0] = (uint8_t)(NRF24_CMD_W_REGISTER | reg);
    for (uint8_t index = 0U; index < length; index++) {
        tx[index + 1U] = data[index];
    }
    return transfer(tx, rx, (uint16_t)length + 1U);
}

static uint8_t read_register(uint8_t reg)
{
    uint8_t tx[2] = {reg, NRF24_CMD_NOP};
    uint8_t rx[2] = {0U, 0U};

    (void)transfer(tx, rx, sizeof(tx));
    return rx[1];
}

static void enter_rx_mode(void)
{
    uint8_t config = NRF24_CONFIG_EN_CRC | NRF24_CONFIG_CRCO |
                     NRF24_CONFIG_PWR_UP | NRF24_CONFIG_PRIM_RX;

    ce_set(GPIO_PIN_RESET);
    (void)write_register(NRF24_REG_CONFIG, &config, 1U);
    HAL_Delay(2U);
    ce_set(GPIO_PIN_SET);
}

static void enter_tx_mode(void)
{
    uint8_t config = NRF24_CONFIG_EN_CRC | NRF24_CONFIG_CRCO |
                     NRF24_CONFIG_PWR_UP;

    ce_set(GPIO_PIN_RESET);
    (void)write_register(NRF24_REG_CONFIG, &config, 1U);
    HAL_Delay(2U);
}

bool nrf24l01_init(void)
{
    uint8_t value;

    if (!APP_ENABLE_TRANSPORT_NRF24L01) {
        return false;
    }

    ce_set(GPIO_PIN_RESET);
    csn_set(GPIO_PIN_SET);
    HAL_Delay(5U);

    value = 0x01U;
    if (!write_register(NRF24_REG_EN_AA, &value, 1U)) {
        return false;
    }
    value = 0x01U;
    if (!write_register(NRF24_REG_EN_RXADDR, &value, 1U) ||
        !write_register(NRF24_REG_SETUP_AW, &(uint8_t){0x03U}, 1U) ||
        !write_register(NRF24_REG_SETUP_RETR, &(uint8_t){0x2FU}, 1U) ||
        !write_register(NRF24_REG_RF_CH, &(uint8_t){76U}, 1U) ||
        !write_register(NRF24_REG_RF_SETUP, &(uint8_t){0x06U}, 1U) ||
        !write_register(NRF24_REG_RX_ADDR_P0, nrf24_address, sizeof(nrf24_address)) ||
        !write_register(NRF24_REG_TX_ADDR, nrf24_address, sizeof(nrf24_address)) ||
        !write_register(NRF24_REG_RX_PW_P0, &(uint8_t){NRF24_PAYLOAD_SIZE}, 1U)) {
        return false;
    }

    (void)command(NRF24_CMD_FLUSH_RX);
    (void)command(NRF24_CMD_FLUSH_TX);
    value = NRF24_STATUS_RX_DR | NRF24_STATUS_TX_DS | NRF24_STATUS_MAX_RT;
    (void)write_register(NRF24_REG_STATUS, &value, 1U);
    irq_pending = false;
    ready = true;
    enter_rx_mode();
    return true;
}

void nrf24l01_irq_handler(void)
{
    irq_pending = true;
}

void nrf24l01_process_rx(void)
{
    if (!ready || !irq_pending) {
        return;
    }

    irq_pending = false;
    while ((read_register(NRF24_REG_FIFO_STATUS) & NRF24_FIFO_RX_EMPTY) == 0U) {
        uint8_t tx[NRF24_PAYLOAD_SIZE + 1U] = {NRF24_CMD_R_RX_PAYLOAD};
        uint8_t rx[NRF24_PAYLOAD_SIZE + 1U] = {0U};
        uint8_t clear = NRF24_STATUS_RX_DR | NRF24_STATUS_TX_DS | NRF24_STATUS_MAX_RT;

        if (!transfer(tx, rx, sizeof(tx))) {
            break;
        }
        for (uint8_t index = 0U; index < NRF24_PAYLOAD_SIZE; index++) {
            (void)communication_system_receive_byte(TRANSPORT_NRF24L01, rx[index + 1U]);
        }
        (void)write_register(NRF24_REG_STATUS, &clear, 1U);
    }
}

bool nrf24l01_send(const uint8_t *data, size_t length)
{
    uint8_t tx[NRF24_PAYLOAD_SIZE + 1U] = {NRF24_CMD_W_TX_PAYLOAD};
    uint8_t rx[NRF24_PAYLOAD_SIZE + 1U] = {0U};
    uint8_t clear = NRF24_STATUS_RX_DR | NRF24_STATUS_TX_DS | NRF24_STATUS_MAX_RT;
    uint32_t start;

    if (!ready || (data == NULL) || (length == 0U) ||
        (length > NRF24_MAX_USER_PAYLOAD_SIZE)) {
        return false;
    }
    enter_tx_mode();
    (void)command(NRF24_CMD_FLUSH_TX);

    /*
     * The USB-to-nRF24L01 serial adapter reserves air-payload byte 0 for the
     * valid user-data length. Supplying it lets that adapter forward exactly
     * this application response to its host serial port instead of treating
     * the AA protocol header as an invalid length.
     */
    tx[1] = (uint8_t)length;
    for (size_t index = 0U; index < length; index++) {
        tx[index + 2U] = data[index];
    }
    if (!transfer(tx, rx, sizeof(tx))) {
        enter_rx_mode();
        return false;
    }
    ce_set(GPIO_PIN_SET);
    HAL_Delay(1U);
    ce_set(GPIO_PIN_RESET);

    start = HAL_GetTick();
    while ((HAL_GetTick() - start) < 20U) {
        uint8_t status = command(NRF24_CMD_NOP);
        if ((status & NRF24_STATUS_TX_DS) != 0U) {
            (void)write_register(NRF24_REG_STATUS, &clear, 1U);
            enter_rx_mode();
            return true;
        }
        if ((status & NRF24_STATUS_MAX_RT) != 0U) {
            (void)command(NRF24_CMD_FLUSH_TX);
            (void)write_register(NRF24_REG_STATUS, &clear, 1U);
            break;
        }
    }
    enter_rx_mode();
    return false;
}
