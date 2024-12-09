# AtomS3 WS2812B SPI control

https://github.com/user-attachments/assets/24d0adf8-48e1-4dc2-ac7b-b6b0a5849b1c

<div><video controls src="./ws2812b.mp4" muted="true"></video></div>

This project runs on AtomS3 and set designated RGB pattern WS2812B strip.

## Usage

1. Connect 5V, GND and GPIO8 on AtomS3 to WS2812B strip.
2. Supply power through USB port on AtomS3

## Supported Hardware

* [M5Stack AtomS3](https://docs.m5stack.com/en/core/AtomS3)
* [M5Stack AtomS3 Lite](https://docs.m5stack.com/en/core/AtomS3%20Lite)

## Build Environment

* PlatformIO

## Dependencies

* m5stack/M5Unified@^0.2.1
* hideakitai/ESP32DMASPI@^0.6.4
