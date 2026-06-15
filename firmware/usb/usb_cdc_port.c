#include "usb_cdc_port.h"

#include <string.h>

#include "app.h"
#include "app_config.h"
#include "board.h"
#include "stm32f1xx_hal.h"

#define USB_EP0 0U
#define USB_EP_CDC_CMD 1U
#define USB_EP_CDC_OUT 2U
#define USB_EP_CDC_IN 3U

#define EP0_SIZE 64U
#define CDC_DATA_SIZE 64U
#define CDC_CMD_SIZE 8U

#define USB_BTABLE_ADDR 0x00U
#define PMA_EP0_RX 0x40U
#define PMA_EP0_TX 0x80U
#define PMA_CDC_CMD_TX 0xC0U
#define PMA_CDC_OUT_RX 0x100U
#define PMA_CDC_IN_TX 0x140U

#define USB_REQ_TYPE_MASK 0x60U
#define USB_REQ_STANDARD 0x00U
#define USB_REQ_CLASS 0x20U
#define USB_REQ_GET_DESCRIPTOR 0x06U
#define USB_REQ_SET_ADDRESS 0x05U
#define USB_REQ_SET_CONFIGURATION 0x09U
#define USB_REQ_GET_CONFIGURATION 0x08U
#define USB_REQ_GET_STATUS 0x00U
#define USB_REQ_SET_LINE_CODING 0x20U
#define USB_REQ_GET_LINE_CODING 0x21U
#define USB_REQ_SET_CONTROL_LINE_STATE 0x22U

#define USB_DESC_DEVICE 0x01U
#define USB_DESC_CONFIGURATION 0x02U
#define USB_DESC_STRING 0x03U
#define USB_DESC_DEVICE_QUALIFIER 0x06U

#define USB_EP_STAT_RX 0x3000U
#define USB_EP_STAT_TX 0x0030U

#define EP_TYPE_BULK 0x0000U
#define EP_TYPE_CONTROL 0x0200U
#define EP_TYPE_INTERRUPT 0x0600U

#define EP_RX_DIS 0x0000U
#define EP_RX_STALL 0x1000U
#define EP_RX_NAK 0x2000U
#define EP_RX_VALID 0x3000U
#define EP_TX_DIS 0x0000U
#define EP_TX_STALL 0x0010U
#define EP_TX_NAK 0x0020U
#define EP_TX_VALID 0x0030U

#define EF_GPIO_CRL_CFG(pin_index, mode_bits, cnf_bits) \
    (((uint32_t)(mode_bits) | ((uint32_t)(cnf_bits) << 2U)) << ((pin_index) * 4U))

#define EF_GPIO_CRH_CFG(pin_index, mode_bits, cnf_bits) \
    (((uint32_t)(mode_bits) | ((uint32_t)(cnf_bits) << 2U)) << (((pin_index) - 8U) * 4U))

typedef struct {
    uint8_t bmRequestType;
    uint8_t bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
} SetupPacket;

static volatile uint8_t configured;
static volatile uint8_t tx_busy;
static volatile uint8_t pending_address;
static uint8_t control_in[128];
static uint16_t control_len;
static uint16_t control_pos;
static uint8_t ep0_out_expect;
static uint8_t line_coding[7] = {0x00, 0xC2, 0x01, 0x00, 0x00, 0x00, 0x08};

static uint8_t rx_queue[APP_USB_RX_QUEUE_SIZE];
static volatile uint16_t rx_head;
static volatile uint16_t rx_tail;
static uint8_t tx_queue[APP_USB_TX_QUEUE_SIZE];
static volatile uint16_t tx_head;
static volatile uint16_t tx_tail;
static uint8_t tx_packet[CDC_DATA_SIZE];

static const uint8_t device_desc[] = {
    18, USB_DESC_DEVICE, 0x00, 0x02, 0x02, 0x00, 0x00, EP0_SIZE,
    0x83, 0x04, 0x40, 0x57, 0x00, 0x01, 1, 2, 3, 1,
};

static const uint8_t config_desc[] = {
    9, USB_DESC_CONFIGURATION, 67, 0, 2, 1, 0, 0x80, 50,
    9, 4, 0, 0, 1, 0x02, 0x02, 0x01, 0,
    5, 0x24, 0x00, 0x10, 0x01,
    5, 0x24, 0x01, 0x00, 1,
    4, 0x24, 0x02, 0x02,
    5, 0x24, 0x06, 0, 1,
    7, 5, 0x80 | USB_EP_CDC_CMD, 0x03, CDC_CMD_SIZE, 0, 16,
    9, 4, 1, 0, 2, 0x0A, 0x00, 0x00, 0,
    7, 5, USB_EP_CDC_OUT, 0x02, CDC_DATA_SIZE, 0, 0,
    7, 5, 0x80 | USB_EP_CDC_IN, 0x02, CDC_DATA_SIZE, 0, 0,
};

static const uint8_t lang_id_desc[] = {4, USB_DESC_STRING, 0x09, 0x04};
static const uint8_t manufacturer_desc[] = {
    18, USB_DESC_STRING, 'E', 0, 'm', 0, 'b', 0, 'e', 0, 'd', 0, 'F', 0, 'i', 0, 'r', 0, 'e', 0,
};
static const uint8_t product_desc[] = {
    46, USB_DESC_STRING,
    'S', 0, 'T', 0, 'M', 0, '3', 0, '2', 0, ' ', 0, 'T', 0, 'e', 0, 'm', 0, 'p', 0, ' ', 0,
    'M', 0, 'o', 0, 'n', 0, 'i', 0, 't', 0, 'o', 0, 'r', 0, ' ', 0, 'C', 0, 'D', 0, 'C', 0,
};
static const uint8_t serial_desc[] = {
    18, USB_DESC_STRING, '0', 0, '0', 0, '0', 0, '1', 0, 'F', 0, '1', 0, '0', 0, '3', 0,
};

static volatile uint16_t *ep_reg(uint8_t ep)
{
    return (volatile uint16_t *)((uint32_t)USB + ep * 4U);
}

static volatile uint16_t *pma16(uint16_t byte_addr)
{
    return (volatile uint16_t *)(USB_PMAADDR + ((uint32_t)byte_addr * 2U));
}

static void pma_write(uint16_t addr, const uint8_t *data, uint16_t len)
{
    volatile uint16_t *dst = pma16(addr);
    for (uint16_t i = 0; i < len; i += 2U) {
        uint16_t value = data[i];
        if ((i + 1U) < len) {
            value |= (uint16_t)data[i + 1U] << 8;
        }
        *dst++ = value;
    }
}

static void pma_read(uint16_t addr, uint8_t *data, uint16_t len)
{
    volatile uint16_t *src = pma16(addr);
    for (uint16_t i = 0; i < len; i += 2U) {
        uint16_t value = *src++;
        data[i] = (uint8_t)value;
        if ((i + 1U) < len) {
            data[i + 1U] = (uint8_t)(value >> 8);
        }
    }
}

static void set_rx_count(uint8_t ep, uint16_t count)
{
    volatile uint16_t *reg = pma16(USB_BTABLE_ADDR + ep * 8U + 6U);
    if (count > 62U) {
        uint16_t blocks = (uint16_t)((count + 31U) / 32U);
        *reg = (uint16_t)(0x8000U | ((blocks - 1U) << 10));
    } else {
        uint16_t blocks = (uint16_t)((count + 1U) / 2U);
        *reg = (uint16_t)(blocks << 10);
    }
}

static void set_tx_count(uint8_t ep, uint16_t count)
{
    *pma16(USB_BTABLE_ADDR + ep * 8U + 2U) = count;
}

static uint16_t rx_count(uint8_t ep)
{
    return (uint16_t)(*pma16(USB_BTABLE_ADDR + ep * 8U + 6U) & 0x03FFU);
}

static void set_ep_addr(uint8_t ep, uint16_t tx_addr, uint16_t rx_addr)
{
    *pma16(USB_BTABLE_ADDR + ep * 8U + 0U) = tx_addr;
    *pma16(USB_BTABLE_ADDR + ep * 8U + 4U) = rx_addr;
}

static void set_stat_tx(uint8_t ep, uint16_t stat)
{
    volatile uint16_t *reg = ep_reg(ep);
    uint16_t value = *reg;
    value &= (uint16_t)(USB_EP_TYPE_MASK | USB_EP_KIND | USB_EPADDR_FIELD | USB_EP_STAT_RX);
    value ^= (uint16_t)((value & USB_EP_STAT_TX) ^ stat);
    *reg = value;
}

static void set_stat_rx(uint8_t ep, uint16_t stat)
{
    volatile uint16_t *reg = ep_reg(ep);
    uint16_t value = *reg;
    value &= (uint16_t)(USB_EP_TYPE_MASK | USB_EP_KIND | USB_EPADDR_FIELD | USB_EP_STAT_TX);
    value ^= (uint16_t)((value & USB_EP_STAT_RX) ^ stat);
    *reg = value;
}

static void clear_ctr_tx(uint8_t ep)
{
    volatile uint16_t *reg = ep_reg(ep);
    uint16_t value = *reg;
    value &= (uint16_t)(USB_EP_TYPE_MASK | USB_EP_KIND | USB_EPADDR_FIELD | USB_EP_STAT_RX | USB_EP_STAT_TX);
    *reg = (uint16_t)(value & ~USB_EP_CTR_TX);
}

static void clear_ctr_rx(uint8_t ep)
{
    volatile uint16_t *reg = ep_reg(ep);
    uint16_t value = *reg;
    value &= (uint16_t)(USB_EP_TYPE_MASK | USB_EP_KIND | USB_EPADDR_FIELD | USB_EP_STAT_RX | USB_EP_STAT_TX);
    *reg = (uint16_t)(value & ~USB_EP_CTR_RX);
}

static void ep_init(uint8_t ep, uint16_t type, uint16_t tx_addr, uint16_t rx_addr, uint16_t rx_size)
{
    set_ep_addr(ep, tx_addr, rx_addr);
    set_rx_count(ep, rx_size);
    *ep_reg(ep) = (uint16_t)(type | ep);
    set_stat_tx(ep, EP_TX_NAK);
    set_stat_rx(ep, rx_size ? EP_RX_VALID : EP_RX_DIS);
}

static void start_tx(uint8_t ep, uint16_t pma, const uint8_t *data, uint16_t len)
{
    pma_write(pma, data, len);
    set_tx_count(ep, len);
    set_stat_tx(ep, EP_TX_VALID);
}

static void ep0_send_next(void)
{
    uint16_t remaining = (uint16_t)(control_len - control_pos);
    uint16_t chunk = remaining > EP0_SIZE ? EP0_SIZE : remaining;
    start_tx(USB_EP0, PMA_EP0_TX, &control_in[control_pos], chunk);
    control_pos = (uint16_t)(control_pos + chunk);
}

static void ep0_send(const uint8_t *data, uint16_t len, uint16_t requested)
{
    if (len > requested) {
        len = requested;
    }
    if (len > sizeof(control_in)) {
        len = sizeof(control_in);
    }
    memcpy(control_in, data, len);
    control_len = len;
    control_pos = 0;
    ep0_send_next();
}

static void ep0_status_in(void)
{
    control_len = 0;
    control_pos = 0;
    start_tx(USB_EP0, PMA_EP0_TX, control_in, 0);
}

static void ep0_stall(void)
{
    set_stat_tx(USB_EP0, EP_TX_STALL);
    set_stat_rx(USB_EP0, EP_RX_STALL);
}

static void parse_setup(SetupPacket *setup)
{
    uint8_t bytes[8];
    pma_read(PMA_EP0_RX, bytes, sizeof(bytes));
    setup->bmRequestType = bytes[0];
    setup->bRequest = bytes[1];
    setup->wValue = (uint16_t)bytes[2] | ((uint16_t)bytes[3] << 8);
    setup->wIndex = (uint16_t)bytes[4] | ((uint16_t)bytes[5] << 8);
    setup->wLength = (uint16_t)bytes[6] | ((uint16_t)bytes[7] << 8);
}

static void handle_setup(void)
{
    SetupPacket setup;
    parse_setup(&setup);
    ep0_out_expect = 0;

    if ((setup.bmRequestType & USB_REQ_TYPE_MASK) == USB_REQ_STANDARD) {
        switch (setup.bRequest) {
        case USB_REQ_GET_DESCRIPTOR: {
            uint8_t type = (uint8_t)(setup.wValue >> 8);
            uint8_t index = (uint8_t)setup.wValue;
            if (type == USB_DESC_DEVICE) {
                ep0_send(device_desc, sizeof(device_desc), setup.wLength);
            } else if (type == USB_DESC_CONFIGURATION) {
                ep0_send(config_desc, sizeof(config_desc), setup.wLength);
            } else if (type == USB_DESC_STRING && index == 0) {
                ep0_send(lang_id_desc, sizeof(lang_id_desc), setup.wLength);
            } else if (type == USB_DESC_STRING && index == 1) {
                ep0_send(manufacturer_desc, sizeof(manufacturer_desc), setup.wLength);
            } else if (type == USB_DESC_STRING && index == 2) {
                ep0_send(product_desc, sizeof(product_desc), setup.wLength);
            } else if (type == USB_DESC_STRING && index == 3) {
                ep0_send(serial_desc, sizeof(serial_desc), setup.wLength);
            } else if (type == USB_DESC_DEVICE_QUALIFIER) {
                ep0_stall();
            } else {
                ep0_stall();
            }
            break;
        }
        case USB_REQ_SET_ADDRESS:
            pending_address = (uint8_t)(setup.wValue & 0x7FU);
            ep0_status_in();
            break;
        case USB_REQ_SET_CONFIGURATION:
            configured = (uint8_t)(setup.wValue & 0xFFU);
            ep_init(USB_EP_CDC_CMD, EP_TYPE_INTERRUPT, PMA_CDC_CMD_TX, 0, 0);
            set_stat_tx(USB_EP_CDC_CMD, EP_TX_NAK);
            ep_init(USB_EP_CDC_OUT, EP_TYPE_BULK, 0, PMA_CDC_OUT_RX, CDC_DATA_SIZE);
            ep_init(USB_EP_CDC_IN, EP_TYPE_BULK, PMA_CDC_IN_TX, 0, 0);
            set_stat_tx(USB_EP_CDC_IN, EP_TX_NAK);
            tx_busy = 0;
            ep0_status_in();
            break;
        case USB_REQ_GET_CONFIGURATION:
            control_in[0] = configured;
            ep0_send(control_in, 1, setup.wLength);
            break;
        case USB_REQ_GET_STATUS:
            control_in[0] = 0;
            control_in[1] = 0;
            ep0_send(control_in, 2, setup.wLength);
            break;
        default:
            ep0_stall();
            break;
        }
    } else if ((setup.bmRequestType & USB_REQ_TYPE_MASK) == USB_REQ_CLASS) {
        switch (setup.bRequest) {
        case USB_REQ_GET_LINE_CODING:
            ep0_send(line_coding, sizeof(line_coding), setup.wLength);
            break;
        case USB_REQ_SET_LINE_CODING:
            ep0_out_expect = sizeof(line_coding);
            set_stat_rx(USB_EP0, EP_RX_VALID);
            break;
        case USB_REQ_SET_CONTROL_LINE_STATE:
            ep0_status_in();
            break;
        default:
            ep0_stall();
            break;
        }
    } else {
        ep0_stall();
    }
}

static void queue_rx(const uint8_t *data, uint16_t len)
{
    for (uint16_t i = 0; i < len; ++i) {
        uint16_t next = (uint16_t)((rx_head + 1U) % APP_USB_RX_QUEUE_SIZE);
        if (next == rx_tail) {
            break;
        }
        rx_queue[rx_head] = data[i];
        rx_head = next;
    }
}

static void cdc_try_tx(void)
{
    if (!configured || tx_busy || tx_head == tx_tail) {
        return;
    }

    uint16_t len = 0;
    while (tx_tail != tx_head && len < CDC_DATA_SIZE) {
        tx_packet[len++] = tx_queue[tx_tail];
        tx_tail = (uint16_t)((tx_tail + 1U) % APP_USB_TX_QUEUE_SIZE);
    }
    tx_busy = 1;
    start_tx(USB_EP_CDC_IN, PMA_CDC_IN_TX, tx_packet, len);
}

static void handle_ep0_rx(void)
{
    uint16_t reg = *ep_reg(USB_EP0);
    if (reg & USB_EP_SETUP) {
        clear_ctr_rx(USB_EP0);
        handle_setup();
        return;
    }

    uint16_t count = rx_count(USB_EP0);
    if (ep0_out_expect && count == ep0_out_expect) {
        pma_read(PMA_EP0_RX, line_coding, sizeof(line_coding));
        ep0_out_expect = 0;
        clear_ctr_rx(USB_EP0);
        ep0_status_in();
    } else {
        clear_ctr_rx(USB_EP0);
        set_stat_rx(USB_EP0, EP_RX_VALID);
    }
}

static void handle_ep0_tx(void)
{
    clear_ctr_tx(USB_EP0);
    if (pending_address) {
        USB->DADDR = (uint16_t)(USB_DADDR_EF | pending_address);
        pending_address = 0;
    }
    if (control_pos < control_len) {
        ep0_send_next();
    } else {
        set_stat_rx(USB_EP0, EP_RX_VALID);
    }
}

static void handle_cdc_out(void)
{
    uint8_t data[CDC_DATA_SIZE];
    uint16_t count = rx_count(USB_EP_CDC_OUT);
    if (count > CDC_DATA_SIZE) {
        count = CDC_DATA_SIZE;
    }
    pma_read(PMA_CDC_OUT_RX, data, count);
    queue_rx(data, count);
    clear_ctr_rx(USB_EP_CDC_OUT);
    set_stat_rx(USB_EP_CDC_OUT, EP_RX_VALID);
}

static bool pins_are_initialized(void)
{
    const uint32_t pa11_af_pp_high = EF_GPIO_CRH_CFG(11U, 3U, 2U);
    const uint32_t pa12_af_pp_high = EF_GPIO_CRH_CFG(12U, 3U, 2U);
    const uint32_t pd3_out_od_low = EF_GPIO_CRL_CFG(3U, 2U, 1U);
    const uint32_t gpioa_mask = GPIO_CRH_CNF11 | GPIO_CRH_MODE11 |
                                GPIO_CRH_CNF12 | GPIO_CRH_MODE12;
    const uint32_t gpiod_mask = GPIO_CRL_CNF3 | GPIO_CRL_MODE3;

    if ((RCC->APB2ENR & RCC_APB2ENR_IOPAEN) == 0U ||
        (RCC->APB2ENR & RCC_APB2ENR_IOPDEN) == 0U ||
        (RCC->APB1ENR & RCC_APB1ENR_USBEN) == 0U) {
        return false;
    }
    if ((GPIOA->CRH & gpioa_mask) != (pa11_af_pp_high | pa12_af_pp_high)) {
        return false;
    }
    if ((GPIOD->CRL & gpiod_mask) != pd3_out_od_low) {
        return false;
    }
    return (GPIOD->ODR & USB_DISCONNECT_Pin) == 0U;
}

void UsbCdc_Init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_USB_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {0};
    gpio.Pin = USB_DM_Pin | USB_DP_Pin;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &gpio);

    gpio.Pin = USB_DISCONNECT_Pin;
    gpio.Mode = GPIO_MODE_OUTPUT_OD;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(USB_DISCONNECT_GPIO_Port, &gpio);

    HAL_GPIO_WritePin(USB_DISCONNECT_GPIO_Port, USB_DISCONNECT_Pin, GPIO_PIN_SET);
    HAL_Delay(20);
    HAL_GPIO_WritePin(USB_DISCONNECT_GPIO_Port, USB_DISCONNECT_Pin, GPIO_PIN_RESET);
    if (!pins_are_initialized()) {
        Error_Handler();
    }

    USB->CNTR = USB_CNTR_FRES;
    USB->CNTR = 0;
    USB->BTABLE = USB_BTABLE_ADDR;
    USB->ISTR = 0;
    USB->DADDR = USB_DADDR_EF;

    ep_init(USB_EP0, EP_TYPE_CONTROL, PMA_EP0_TX, PMA_EP0_RX, EP0_SIZE);
    set_stat_tx(USB_EP0, EP_TX_NAK);
    configured = 0;
    tx_busy = 0;
    rx_head = rx_tail = tx_head = tx_tail = 0;
}

void UsbCdc_Poll(void)
{
    uint16_t istr = USB->ISTR;
    while (istr & USB_ISTR_CTR) {
        uint8_t ep = (uint8_t)(istr & USB_ISTR_EP_ID);
        uint16_t reg = *ep_reg(ep);
        if ((reg & USB_EP_CTR_RX) != 0U) {
            if (ep == USB_EP0) {
                handle_ep0_rx();
            } else if (ep == USB_EP_CDC_OUT) {
                handle_cdc_out();
            } else {
                clear_ctr_rx(ep);
            }
        }
        if ((reg & USB_EP_CTR_TX) != 0U) {
            if (ep == USB_EP0) {
                handle_ep0_tx();
            } else if (ep == USB_EP_CDC_IN) {
                tx_busy = 0;
                clear_ctr_tx(ep);
            } else {
                clear_ctr_tx(ep);
            }
        }
        istr = USB->ISTR;
    }

    if (USB->ISTR & USB_ISTR_RESET) {
        USB->ISTR = (uint16_t)~USB_ISTR_RESET;
        UsbCdc_Init();
    }
    cdc_try_tx();
}

bool UsbCdc_IsConfigured(void)
{
    return configured != 0U;
}

size_t UsbCdc_Write(const uint8_t *data, size_t len)
{
    size_t written = 0;
    while (written < len) {
        uint16_t next = (uint16_t)((tx_head + 1U) % APP_USB_TX_QUEUE_SIZE);
        if (next == tx_tail) {
            break;
        }
        tx_queue[tx_head] = data[written++];
        tx_head = next;
    }
    cdc_try_tx();
    return written;
}

int UsbCdc_ReadByte(void)
{
    if (rx_tail == rx_head) {
        return -1;
    }
    uint8_t value = rx_queue[rx_tail];
    rx_tail = (uint16_t)((rx_tail + 1U) % APP_USB_RX_QUEUE_SIZE);
    return value;
}
