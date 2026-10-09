/**
 * CyberPhoneOS - TFT_eSPI User_Setup.h
 *
 * WHERE THIS FILE GOES:
 *   <Arduino libraries>/TFT_eSPI/User_Setup.h   (overwrite the existing one)
 *
 * Then open <Arduino libraries>/TFT_eSPI/User_Setup_Select.h and make sure
 * ONLY this line is uncommented:
 *   #include <User_Setup.h>
 * (comment out the default "User_Setup_Select.h" includes like PCBFR pin
 * setups that ship enabled by default).
 *
 * Panel assumed: 2.8" ILI9341 320x240 SPI TFT with XPT2046 resistive touch,
 * touch chained on the same SPI bus (T_CLK/T_DI/T_DO tied to TFT SCLK/MOSI/MISO).
 * If your panel uses a different driver chip, swap the *_DRIVER line below.
 */

#define ILI9341_DRIVER

// ---- Shared VSPI bus ----
#define TFT_MISO 19
#define TFT_MOSI 23
#define TFT_SCLK 18

// ---- Unique per-device pins (see wiring table) ----
#define TFT_CS   17   // Chip select
#define TFT_DC   16   // Data/Command
#define TFT_RST   5   // Reset
#define TFT_BL   32   // Backlight (PWM)
#define TFT_BACKLIGHT_ON HIGH

// ---- Touch controller (shares the bus above, own CS only) ----
#define TOUCH_CS 21

// ---- Fonts baked into TFT_eSPI (not the LVGL ones, used for early boot logging only) ----
#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF
#define SMOOTH_FONT

// ---- SPI frequency ----
#define SPI_FREQUENCY       40000000
#define SPI_READ_FREQUENCY  20000000
#define SPI_TOUCH_FREQUENCY  2500000

// IMPORTANT: SD card is NOT configured here. It shares the same physical
// SCLK/MOSI/MISO lines but uses its own CS (GPIO12) driven directly by the
// SD.h library in sd_manager.cpp - do not add SD pins to this file.
