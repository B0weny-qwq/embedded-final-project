#include "usb_cdc_port.h"

#include <string.h>

#include "app.h"
#include "app_config.h"
#include "board.h"
#include "stm32f1xx_hal.h"

#define USB_EP0_SIZE 64U
#define USB_CDC_CMD_EP 0x81U
#define USB_CDC_OUT_EP 0x02U
#define USB_CDC_IN_EP 0x83U
#define USB_CDC_DATA_SIZE 64U
#define USB_CDC_CMD_SIZE 8U

#define PMA_EP0_OUT 0x40U
#define PMA_EP0_IN 0x80U
#define PMA_CDC_CMD_IN 0xC0U
#define PMA_CDC_OUT 0x100U
#define PMA_CDC_IN 0x140U

#define USB_REQ_TYPE_MASK 0x60U
#define USB_REQ_STANDARD 0x00U
#define USB_REQ_CLASS 0x20U
#define USB_REQ_GET_STATUS 0x00U
#define USB_REQ_CLEAR_FEATURE 0x01U
#define USB_REQ_SET_FEATURE 0x03U
#define USB_REQ_SET_ADDRESS 0x05U
#define USB_REQ_GET_DESCRIPTOR 0x06U
#define USB_REQ_GET_CONFIGURATION 0x08U
#define USB_REQ_SET_CONFIGURATION 0x09U
#define USB_REQ_GET_INTERFACE 0x0AU
#define USB_REQ_SET_INTERFACE 0x0BU
#define USB_REQ_SET_LINE_CODING 0x20U
#define USB_REQ_GET_LINE_CODING 0x21U
#define USB_REQ_SET_CONTROL_LINE_STATE 0x22U

#define USB_DESC_DEVICE 0x01U
#define USB_DESC_CONFIGURATION 0x02U
#define USB_DESC_STRING 0x03U
#define USB_DESC_DEVICE_QUALIFIER 0x06U

typedef struct {
    uint8_t bmRequestType;
    uint8_t bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
} SetupPacket;

PCD_HandleTypeDef hpcd_usb_fs;

static volatile uint8_t configured;
static volatile uint8_t tx_busy;
static uint8_t control_buf[128];
static uint16_t control_len;
static uint16_t control_pos;
static uint8_t control_zlp;
static uint8_t ep0_out_buf[USB_EP0_SIZE];
static uint8_t cdc_rx_buf[USB_CDC_DATA_SIZE];
static uint8_t cdc_tx_packet[USB_CDC_DATA_SIZE];
static uint8_t line_coding[7] = {0x00, 0xC2, 0x01, 0x00, 0x00, 0x00, 0x08};
static uint8_t ep0_out_request;

static uint8_t rx_queue[APP_USB_RX_QUEUE_SIZE];
static volatile uint16_t rx_head;
static volatile uint16_t rx_tail;
static uint8_t tx_queue[APP_USB_TX_QUEUE_SIZE];
static volatile uint16_t tx_head;
static volatile uint16_t tx_tail;

static const uint8_t device_desc[] = {
    18, USB_DESC_DEVICE, 0x00, 0x02, 0x02, 0x00, 0x00, USB_EP0_SIZE,
    0x83, 0x04, 0x40, 0x57, 0x00, 0x01, 1, 2, 3, 1,
};

static const uint8_t config_desc[] = {
    9, USB_DESC_CONFIGURATION, 67, 0, 2, 1, 0, 0x80, 50,
    9, 4, 0, 0, 1, 0x02, 0x02, 0x01, 0,
    5, 0x24, 0x00, 0x10, 0x01,
    5, 0x24, 0x01, 0x00, 1,
    4, 0x24, 0x02, 0x02,
    5, 0x24, 0x06, 0, 1,
    7, 5, USB_CDC_CMD_EP, 0x03, USB_CDC_CMD_SIZE, 0, 16,
    9, 4, 1, 0, 2, 0x0A, 0x00, 0x00, 0,
    7, 5, USB_CDC_OUT_EP, 0x02, USB_CDC_DATA_SIZE, 0, 0,
    7, 5, USB_CDC_IN_EP, 0x02, USB_CDC_DATA_SIZE, 0, 0,
};

static const uint8_t lang_id_desc[] = {4, USB_DESC_STRING, 0x09, 0x04};
static const uint8_t manufacturer_desc[] = {
    20, USB_DESC_STRING, 'E', 0, 'm', 0, 'b', 0, 'e', 0, 'd', 0, 'F', 0, 'i', 0, 'r', 0, 'e', 0,
};
static const uint8_t product_desc[] = {
    46, USB_DESC_STRING,
    'S', 0, 'T', 0, 'M', 0, '3', 0, '2', 0, ' ', 0, 'T', 0, 'e', 0, 'm', 0, 'p', 0, ' ', 0,
    'M', 0, 'o', 0, 'n', 0, 'i', 0, 't', 0, 'o', 0, 'r', 0, ' ', 0, 'C', 0, 'D', 0, 'C', 0,
};
static const uint8_t serial_desc[] = {
    18, USB_DESC_STRING, '0', 0, '0', 0, '0', 0, '1', 0, 'F', 0, '1', 0, '0', 0, '3', 0,
};

static void parse_setup(SetupPacket *setup)
{
    const uint8_t *bytes = (const uint8_t *)hpcd_usb_fs.Setup;
    setup->bmRequestType = bytes[0];
    setup->bRequest = bytes[1];
    setup->wValue = (uint16_t)bytes[2] | ((uint16_t)bytes[3] << 8);
    setup->wIndex = (uint16_t)bytes[4] | ((uint16_t)bytes[5] << 8);
    setup->wLength = (uint16_t)bytes[6] | ((uint16_t)bytes[7] << 8);
}

static void ep0_send_next(void)
{
    uint16_t remaining = (uint16_t)(control_len - control_pos);
    uint16_t chunk = remaining > USB_EP0_SIZE ? USB_EP0_SIZE : remaining;
    (void)HAL_PCD_EP_Transmit(&hpcd_usb_fs, 0x80U, &control_buf[control_pos], chunk);
    control_pos = (uint16_t)(control_pos + chunk);
}

static void ep0_send(const uint8_t *data, uint16_t len, uint16_t requested)
{
    if (len > requested) {
        len = requested;
    }
    if (len > sizeof(control_buf)) {
        len = sizeof(control_buf);
    }
    if (len > 0U) {
        memcpy(control_buf, data, len);
    }
    control_len = len;
    control_pos = 0U;
    control_zlp = (len < requested && (len % USB_EP0_SIZE) == 0U) ? 1U : 0U;
    ep0_send_next();
}

static void ep0_status_in(void)
{
    control_len = 0U;
    control_pos = 0U;
    control_zlp = 0U;
    (void)HAL_PCD_EP_Transmit(&hpcd_usb_fs, 0x80U, control_buf, 0U);
}

static void ep0_status_out(void)
{
    (void)HAL_PCD_EP_Receive(&hpcd_usb_fs, 0x00U, ep0_out_buf, 0U);
}

static void ep0_stall(void)
{
    (void)HAL_PCD_EP_SetStall(&hpcd_usb_fs, 0x80U);
    (void)HAL_PCD_EP_SetStall(&hpcd_usb_fs, 0x00U);
}

static void cdc_open_endpoints(void)
{
    (void)HAL_PCD_EP_Open(&hpcd_usb_fs, USB_CDC_CMD_EP, USB_CDC_CMD_SIZE, EP_TYPE_INTR);
    (void)HAL_PCD_EP_Open(&hpcd_usb_fs, USB_CDC_OUT_EP, USB_CDC_DATA_SIZE, EP_TYPE_BULK);
    (void)HAL_PCD_EP_Open(&hpcd_usb_fs, USB_CDC_IN_EP, USB_CDC_DATA_SIZE, EP_TYPE_BULK);
    (void)HAL_PCD_EP_Receive(&hpcd_usb_fs, USB_CDC_OUT_EP, cdc_rx_buf, USB_CDC_DATA_SIZE);
}

static void cdc_close_endpoints(void)
{
    (void)HAL_PCD_EP_Close(&hpcd_usb_fs, USB_CDC_CMD_EP);
    (void)HAL_PCD_EP_Close(&hpcd_usb_fs, USB_CDC_OUT_EP);
    (void)HAL_PCD_EP_Close(&hpcd_usb_fs, USB_CDC_IN_EP);
    tx_busy = 0;
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
    while (tx_tail != tx_head && len < USB_CDC_DATA_SIZE) {
        cdc_tx_packet[len++] = tx_queue[tx_tail];
        tx_tail = (uint16_t)((tx_tail + 1U) % APP_USB_TX_QUEUE_SIZE);
    }

    tx_busy = 1;
    if (HAL_PCD_EP_Transmit(&hpcd_usb_fs, USB_CDC_IN_EP, cdc_tx_packet, len) != HAL_OK) {
        tx_busy = 0;
    }
}

static bool pins_are_initialized(void)
{
    if ((RCC->APB2ENR & RCC_APB2ENR_IOPDEN) == 0U ||
        (RCC->APB1ENR & RCC_APB1ENR_USBEN) == 0U) {
        return false;
    }
    return HAL_GPIO_ReadPin(USB_DISCONNECT_GPIO_Port, USB_DISCONNECT_Pin) == GPIO_PIN_RESET;
}

static void handle_standard_request(const SetupPacket *setup)
{
    switch (setup->bRequest) {
    case USB_REQ_GET_DESCRIPTOR: {
        uint8_t type = (uint8_t)(setup->wValue >> 8);
        uint8_t index = (uint8_t)setup->wValue;
        if (type == USB_DESC_DEVICE) {
            ep0_send(device_desc, sizeof(device_desc), setup->wLength);
        } else if (type == USB_DESC_CONFIGURATION) {
            ep0_send(config_desc, sizeof(config_desc), setup->wLength);
        } else if (type == USB_DESC_STRING && index == 0U) {
            ep0_send(lang_id_desc, sizeof(lang_id_desc), setup->wLength);
        } else if (type == USB_DESC_STRING && index == 1U) {
            ep0_send(manufacturer_desc, sizeof(manufacturer_desc), setup->wLength);
        } else if (type == USB_DESC_STRING && index == 2U) {
            ep0_send(product_desc, sizeof(product_desc), setup->wLength);
        } else if (type == USB_DESC_STRING && index == 3U) {
            ep0_send(serial_desc, sizeof(serial_desc), setup->wLength);
        } else if (type == USB_DESC_DEVICE_QUALIFIER) {
            ep0_stall();
        } else {
            ep0_stall();
        }
        break;
    }
    case USB_REQ_SET_ADDRESS:
        (void)HAL_PCD_SetAddress(&hpcd_usb_fs, (uint8_t)(setup->wValue & 0x7FU));
        ep0_status_in();
        break;
    case USB_REQ_SET_CONFIGURATION:
        configured = (uint8_t)(setup->wValue & 0xFFU);
        if (configured != 0U) {
            cdc_open_endpoints();
        } else {
            cdc_close_endpoints();
        }
        ep0_status_in();
        break;
    case USB_REQ_GET_CONFIGURATION:
        control_buf[0] = configured;
        ep0_send(control_buf, 1U, setup->wLength);
        break;
    case USB_REQ_GET_STATUS:
        control_buf[0] = 0U;
        control_buf[1] = 0U;
        ep0_send(control_buf, 2U, setup->wLength);
        break;
    case USB_REQ_GET_INTERFACE:
        control_buf[0] = 0U;
        ep0_send(control_buf, 1U, setup->wLength);
        break;
    case USB_REQ_SET_INTERFACE:
    case USB_REQ_CLEAR_FEATURE:
    case USB_REQ_SET_FEATURE:
        ep0_status_in();
        break;
    default:
        ep0_stall();
        break;
    }
}

static void handle_class_request(const SetupPacket *setup)
{
    switch (setup->bRequest) {
    case USB_REQ_GET_LINE_CODING:
        ep0_send(line_coding, sizeof(line_coding), setup->wLength);
        break;
    case USB_REQ_SET_LINE_CODING:
        ep0_out_request = USB_REQ_SET_LINE_CODING;
        (void)HAL_PCD_EP_Receive(&hpcd_usb_fs, 0x00U, ep0_out_buf, sizeof(line_coding));
        break;
    case USB_REQ_SET_CONTROL_LINE_STATE:
        ep0_status_in();
        break;
    default:
        ep0_stall();
        break;
    }
}

void HAL_PCD_MspInit(PCD_HandleTypeDef *hpcd)
{
    if (hpcd->Instance != USB) {
        return;
    }

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_USB_CLK_ENABLE();

    HAL_NVIC_SetPriority(USB_LP_CAN1_RX0_IRQn, 1U, 0U);
    HAL_NVIC_EnableIRQ(USB_LP_CAN1_RX0_IRQn);
}

void HAL_PCD_MspDeInit(PCD_HandleTypeDef *hpcd)
{
    if (hpcd->Instance != USB) {
        return;
    }

    HAL_NVIC_DisableIRQ(USB_LP_CAN1_RX0_IRQn);
    __HAL_RCC_USB_CLK_DISABLE();
}

void HAL_PCD_ResetCallback(PCD_HandleTypeDef *hpcd)
{
    if (hpcd->Instance != USB) {
        return;
    }

    configured = 0;
    tx_busy = 0;
    control_len = 0U;
    control_pos = 0U;
    control_zlp = 0U;
    ep0_out_request = 0;
    (void)HAL_PCD_EP_Open(hpcd, 0x00U, USB_EP0_SIZE, EP_TYPE_CTRL);
    (void)HAL_PCD_EP_Open(hpcd, 0x80U, USB_EP0_SIZE, EP_TYPE_CTRL);
}

void HAL_PCD_SetupStageCallback(PCD_HandleTypeDef *hpcd)
{
    if (hpcd->Instance != USB) {
        return;
    }

    SetupPacket setup;
    parse_setup(&setup);
    ep0_out_request = 0;

    if ((setup.bmRequestType & USB_REQ_TYPE_MASK) == USB_REQ_STANDARD) {
        handle_standard_request(&setup);
    } else if ((setup.bmRequestType & USB_REQ_TYPE_MASK) == USB_REQ_CLASS) {
        handle_class_request(&setup);
    } else {
        ep0_stall();
    }
}

void HAL_PCD_DataOutStageCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum)
{
    if (hpcd->Instance != USB) {
        return;
    }

    if (epnum == 0U) {
        if (ep0_out_request == USB_REQ_SET_LINE_CODING &&
            HAL_PCD_EP_GetRxCount(hpcd, 0x00U) == sizeof(line_coding)) {
            memcpy(line_coding, ep0_out_buf, sizeof(line_coding));
            ep0_out_request = 0;
            ep0_status_in();
        } else {
            ep0_out_request = 0;
            ep0_status_in();
        }
    } else if (epnum == (USB_CDC_OUT_EP & 0x7FU)) {
        uint32_t count = HAL_PCD_EP_GetRxCount(hpcd, USB_CDC_OUT_EP);
        if (count > USB_CDC_DATA_SIZE) {
            count = USB_CDC_DATA_SIZE;
        }
        queue_rx(cdc_rx_buf, (uint16_t)count);
        (void)HAL_PCD_EP_Receive(hpcd, USB_CDC_OUT_EP, cdc_rx_buf, USB_CDC_DATA_SIZE);
    }
}

void HAL_PCD_DataInStageCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum)
{
    if (hpcd->Instance != USB) {
        return;
    }

    if (epnum == (USB_CDC_IN_EP & 0x7FU)) {
        tx_busy = 0;
        cdc_try_tx();
    } else if (epnum == 0U) {
        if (control_pos < control_len) {
            ep0_send_next();
        } else if (control_zlp) {
            control_zlp = 0U;
            (void)HAL_PCD_EP_Transmit(hpcd, 0x80U, control_buf, 0U);
        } else {
            ep0_status_out();
        }
    }
}

void HAL_PCD_SuspendCallback(PCD_HandleTypeDef *hpcd)
{
    (void)hpcd;
}

void HAL_PCD_ResumeCallback(PCD_HandleTypeDef *hpcd)
{
    (void)hpcd;
}

void HAL_PCD_ConnectCallback(PCD_HandleTypeDef *hpcd)
{
    (void)hpcd;
}

void HAL_PCD_DisconnectCallback(PCD_HandleTypeDef *hpcd)
{
    if (hpcd->Instance == USB) {
        configured = 0;
        tx_busy = 0;
    }
}

void HAL_PCD_SOFCallback(PCD_HandleTypeDef *hpcd)
{
    (void)hpcd;
}

void HAL_PCD_ISOOUTIncompleteCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum)
{
    (void)hpcd;
    (void)epnum;
}

void HAL_PCD_ISOINIncompleteCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum)
{
    (void)hpcd;
    (void)epnum;
}

void UsbCdc_IrqHandler(void)
{
    HAL_PCD_IRQHandler(&hpcd_usb_fs);
}

void UsbCdc_Init(void)
{
    configured = 0;
    tx_busy = 0;
    rx_head = rx_tail = tx_head = tx_tail = 0;
    control_len = 0U;
    control_pos = 0U;
    control_zlp = 0U;
    ep0_out_request = 0;

    __HAL_RCC_GPIOD_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {0};
    gpio.Pin = USB_DISCONNECT_Pin;
    gpio.Mode = GPIO_MODE_OUTPUT_OD;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(USB_DISCONNECT_GPIO_Port, &gpio);

    HAL_GPIO_WritePin(USB_DISCONNECT_GPIO_Port, USB_DISCONNECT_Pin, GPIO_PIN_SET);
    HAL_Delay(20U);

    hpcd_usb_fs.Instance = USB;
    hpcd_usb_fs.Init.dev_endpoints = 8U;
    hpcd_usb_fs.Init.speed = PCD_SPEED_FULL;
    hpcd_usb_fs.Init.ep0_mps = PCD_EP0MPS_64;
    hpcd_usb_fs.Init.phy_itface = PCD_PHY_EMBEDDED;
    hpcd_usb_fs.Init.Sof_enable = DISABLE;
    hpcd_usb_fs.Init.low_power_enable = DISABLE;
    hpcd_usb_fs.Init.lpm_enable = DISABLE;
    hpcd_usb_fs.Init.battery_charging_enable = DISABLE;

    if (HAL_PCD_Init(&hpcd_usb_fs) != HAL_OK) {
        Error_Handler();
    }

    (void)HAL_PCDEx_PMAConfig(&hpcd_usb_fs, 0x00U, PCD_SNG_BUF, PMA_EP0_OUT);
    (void)HAL_PCDEx_PMAConfig(&hpcd_usb_fs, 0x80U, PCD_SNG_BUF, PMA_EP0_IN);
    (void)HAL_PCDEx_PMAConfig(&hpcd_usb_fs, USB_CDC_CMD_EP, PCD_SNG_BUF, PMA_CDC_CMD_IN);
    (void)HAL_PCDEx_PMAConfig(&hpcd_usb_fs, USB_CDC_OUT_EP, PCD_SNG_BUF, PMA_CDC_OUT);
    (void)HAL_PCDEx_PMAConfig(&hpcd_usb_fs, USB_CDC_IN_EP, PCD_SNG_BUF, PMA_CDC_IN);

    if (HAL_PCD_Start(&hpcd_usb_fs) != HAL_OK) {
        Error_Handler();
    }

    HAL_GPIO_WritePin(USB_DISCONNECT_GPIO_Port, USB_DISCONNECT_Pin, GPIO_PIN_RESET);
    if (!pins_are_initialized()) {
        Error_Handler();
    }
}

void UsbCdc_Poll(void)
{
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
