# Waveshare ESP32-S3 AMOLED 1.43"

Project using the Waveshare round AMOLED 1.43” display.

- Product information: https://www.waveshare.com/esp32-s3-touch-amoled-1.43.htm
- Waveshare wiki: https://www.waveshare.com/wiki/ESP32-S3-Touch-AMOLED-1.43


## Project Goals

This project will use **PlatformIO** toolset for development

The goal of this repository is to demonstrate features or applications:
- Bring the board up cleanly in **PlatformIO**
- Initialize the AMOLED display (SH8601 / CO5300)
- Enable **FT3168 capacitive touch**
- Enable **buttons** for user actions, not version 1
- Serve as a stable foundation for future projects

Display:
- Monitor Audio Guestbook Status
- Nice clock
- Customisation (brightness, colours etc.)


## Hardware

- **Board:** Waveshare ESP32-S3 Touch AMOLED 1.43"
- **Display:** AMOLED (SH8601 / CO5300)
- **Touch Controller:** FT3168
- **Interface:** QSPI (display), I²C (touch)

## Software Stack

- **PlatformIO**
- **Arduino framework (ESP32-S3)**

## Pictures

![Board Connections - High Level](./docs/board_connections.png)

![Board Explanation](./docs/display_back.png)

![Display Ideas](./docs/DisplayIdeas.png)

## Diary

22/8/26: Have a working version with touch though due to Espressif it seems they've broken something to do with i2c as I get the following error; 

116914][E][esp32-hal-i2c-ng.c:372] i2cWriteReadNonStop(): i2c_master_transmit_receive failed: [259] ESP_ERR_INVALID_STATE

[116925][E][Wire.cpp:532] requestFrom(): i2cWriteReadNonStop returned Error 259

Start ESP-NOW (Espressif wireless protocol) to communicate with the Audio Guestbook admin monitor so I can use this round display to show what's going on instead of a phone. Also need to look at using an Interrupt for the touch display to know something has happened and stop calling a read all the time.