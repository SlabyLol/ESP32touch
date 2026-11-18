// Diese Werte in die TFT_eSPI User_Setup.h uebernehmen
// (Arduino libraries/TFT_eSPI/User_Setup.h)

#define USER_SETUP_LOADED
#define ST7789_DRIVER
#define TFT_RGB_ORDER TFT_RGB
#define TFT_WIDTH  240
#define TFT_HEIGHT 320

#define TFT_MISO 12
#define TFT_MOSI 13
#define TFT_SCLK 14
#define TFT_CS   15
#define TFT_DC    2
#define TFT_RST  35
#define TFT_BL   27
#define TFT_BACKLIGHT_ON HIGH

#define TOUCH_CS 33

#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF
#define SMOOTH_FONT

#define SPI_FREQUENCY  40000000
#define USE_HSPI_PORT
