//pin assignments for Waveshare 1.43" amoled esp32-s3 module

#pragma once

/***********************config*************************/

// Display
#define SPI_FREQUENCY         20000000 
#define TFT_SPI_MODE          SPI_MODE0
#define TFT_SPI_HOST          SPI2_HOST

#define LCD_SDIO0 11    
#define LCD_SDIO1 12
#define LCD_SDIO2 13
#define LCD_SDIO3 14
#define LCD_SCLK  10
#define LCD_RESET 21
#define LCD_CS    9

#define LCD_WIDTH  466
#define LCD_HEIGHT 466

// ==== TOUCH (I2C) ====
#define IIC_SDA 47  // pins_config and bsp_config
#define IIC_SCL 48
#define FT3168_I2C_ADDRESS 0x38
#define TOUCH_INT GPIO_NUM_17
#define TOUCH_RST -1

// Battery Voltage ADC
#define BATTERY_VOLTAGE_ADC_DATA 4

// SD
#define SD_CS 38
#define SD_MOSI 39
#define SD_MISO 40
#define SD_SCLK 41

// RTC
#define PCF8563_INT 15

//IMU
#define IMU_INT 8