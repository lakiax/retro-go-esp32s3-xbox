// Target definition
#define RG_TARGET_NAME             "ESP32S3-XBOX-096INCH"

// Storage
#define RG_STORAGE_ROOT             "/sd"
 #define RG_STORAGE_FLASH_PARTITION  "vfs"


// Audio
#define RG_AUDIO_USE_INT_DAC        0   // 0 = Disable, 1 = GPIO25, 2 = GPIO26, 3 = Both
#define RG_AUDIO_USE_EXT_DAC        1   // 0 = Disable, 1 = Enable

// Video
#define RG_SCREEN_DRIVER            0   // 0 = ILI9341/ST7789
#define RG_SCREEN_HOST              SPI2_HOST
#define RG_SCREEN_SPEED             SPI_MASTER_FREQ_40M // SPI_MASTER_FREQ_80M
#define RG_SCREEN_BACKLIGHT         1
#define RG_SCREEN_WIDTH             161  //1.3 inch 240  2.4 inch 320
#define RG_SCREEN_HEIGHT            106
#define RG_SCREEN_ROTATE            0
#define RG_SCREEN_VISIBLE_AREA      {1, 26, 0, 0}
#define RG_SCREEN_SAFE_AREA         {0, 0, 0, 0}
#define RG_SCREEN_INIT()                                                                                                        \
    ILI9341_CMD(0x01); /* Software Reset */                         \
    rg_usleep(150000); /* delayMS(150) */                           \
    ILI9341_CMD(0x11); /* Sleep Out */                              \
    rg_usleep(255000); /* delayMS(255) */                           \
    ILI9341_CMD(0xB1, 0x01, 0x2C, 0x2D); /* Frame Rate 1 */         \
    ILI9341_CMD(0xB2, 0x01, 0x2C, 0x2D); /* Frame Rate 2 */         \
    ILI9341_CMD(0xB3, 0x01, 0x2C, 0x2D, 0x01, 0x2C, 0x2D); /* FR3 */\
    ILI9341_CMD(0xB4, 0x07); /* Inversion Control */                \
    ILI9341_CMD(0xC0, 0xA2, 0x02, 0x84); /* Power Control 1 */      \
    ILI9341_CMD(0xC1, 0xC5); /* Power Control 2 */                  \
    ILI9341_CMD(0xC2, 0x0A, 0x00); /* Power Control 3 */            \
    ILI9341_CMD(0xC3, 0x8A, 0x2A); /* Power Control 4 */            \
    ILI9341_CMD(0xC4, 0x8A, 0xEE); /* Power Control 5 */            \
    ILI9341_CMD(0xC5, 0x0E); /* VCOM Control 1 */                   \
    ILI9341_CMD(0x20); /* Display Inversion Off */                  \
    ILI9341_CMD(0x36, 0x78); /* Memory Data Access (BGR) */         \
    ILI9341_CMD(0x3A, 0x05); /* Pixel Format 16-bit */              \
    ILI9341_CMD(0x2A, 0x00, 0x01, 0x00, 0xA0); /* Col Addr Set */   \
    ILI9341_CMD(0x2B, 0x00, 0x1A, 0x00, 0x69); /* Row Addr Set */   \
    ILI9341_CMD(0x21); /* Display Inversion On (Overrides 0x20) */  \
    /* Gamma (+) Correction */                                      \
    ILI9341_CMD(0xE0, 0x02, 0x1C, 0x07, 0x12, 0x37, 0x32, 0x29, 0x2D, 0x29, 0x25, 0x2B, 0x39, 0x00, 0x01, 0x03, 0x10); \
    /* Gamma (-) Correction */                                      \
    ILI9341_CMD(0xE1, 0x03, 0x1D, 0x07, 0x06, 0x2E, 0x2C, 0x29, 0x2D, 0x2E, 0x2E, 0x37, 0x3F, 0x00, 0x00, 0x02, 0x10); \
    ILI9341_CMD(0x13); /* Normal Display Mode On */                 \
    rg_usleep(10000);  /* delayMS(10) */                            \
    ILI9341_CMD(0x29); /* Display On */                             \
    rg_usleep(100000); /* delayMS(100) */
    
// Input
// Refer to rg_input.h to see all available RG_KEY_* and RG_GAMEPAD_*_MAP types
#define RG_GAMEPAD_GPIO_MAP {\
    {RG_KEY_UP,     .num = GPIO_NUM_7,  .pullup = 1, .level = 0},\
    {RG_KEY_RIGHT,  .num = GPIO_NUM_6,  .pullup = 1, .level = 0},\
    {RG_KEY_DOWN,   .num = GPIO_NUM_46, .pullup = 1, .level = 0},\
    {RG_KEY_LEFT,   .num = GPIO_NUM_45, .pullup = 1, .level = 0},\
    {RG_KEY_SELECT, .num = GPIO_NUM_16, .pullup = 1, .level = 0},\
    {RG_KEY_START,  .num = GPIO_NUM_17, .pullup = 1, .level = 0},\
    {RG_KEY_MENU,   .num = GPIO_NUM_18, .pullup = 1, .level = 0},\
    {RG_KEY_OPTION, .num = GPIO_NUM_8,  .pullup = 1, .level = 0},\
    {RG_KEY_A,      .num = GPIO_NUM_15, .pullup = 1, .level = 0},\
    {RG_KEY_B,      .num = GPIO_NUM_5,  .pullup = 1, .level = 0},\
}

// Battery
#define RG_BATTERY_DRIVER           0
#define RG_BATTERY_ADC_UNIT         ADC_UNIT_1
#define RG_BATTERY_ADC_CHANNEL      ADC_CHANNEL_3
#define RG_BATTERY_CALC_PERCENT(raw) (((raw) * 2.f - 3500.f) / (4200.f - 3500.f) * 100.f)
#define RG_BATTERY_CALC_VOLTAGE(raw) ((raw) * 2.f * 0.001f)

// Status LED
#define RG_GPIO_LED                 GPIO_NUM_38

// SPI Display (back up working)
#define RG_GPIO_LCD_MISO            GPIO_NUM_NC
#define RG_GPIO_LCD_MOSI            GPIO_NUM_12
#define RG_GPIO_LCD_CLK             GPIO_NUM_48
#define RG_GPIO_LCD_CS              GPIO_NUM_14
#define RG_GPIO_LCD_DC              GPIO_NUM_47
#define RG_GPIO_LCD_BCKL            GPIO_NUM_39
#define RG_GPIO_LCD_RST             GPIO_NUM_3

#define RG_GPIO_SDSPI_MISO          GPIO_NUM_9
#define RG_GPIO_SDSPI_MOSI          GPIO_NUM_11
#define RG_GPIO_SDSPI_CLK           GPIO_NUM_13
#define RG_GPIO_SDSPI_CS            GPIO_NUM_10

// External I2S DAC
#define RG_GPIO_SND_I2S_BCK         41
#define RG_GPIO_SND_I2S_WS          42
#define RG_GPIO_SND_I2S_DATA        40
// #define RG_GPIO_SND_AMP_ENABLE      18
