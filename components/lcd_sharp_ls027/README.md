# Sharp LS027B7DH01 on ESP32-C3

This component drives the 400×240 Sharp memory-in-pixel panel through SPI2 and exposes it to LVGL as a landscape display. LVGL renders RGB565 tiles; the flush callback thresholds each pixel to black or white, updates the 1-bit shadow framebuffer, then sends the affected full rows to the panel.

The Sharp panel is not a command/register LCD such as ST7789, so the ESP-IDF `esp_lcd` ST7789 panel driver cannot be reused. The wire protocol follows the Sharp LS027B7DH01 specification. SCS is active high, SPI is mode 0 with LSB-first transfers at 2 MHz, and line addresses start at 1. The component also toggles software VCOM every 500 ms; tie EXTMODE low to select this mode.

## Default ESP32-C3 signal assignment

| Panel signal | ESP32-C3 | Notes |
|---|---:|---|
| SCLK | GPIO4 | SPI2 clock |
| SI | GPIO5 | SPI2 MOSI |
| SCS | GPIO10 | Active high |
| DISP | GPIO3 | Panel enable |
| EXTMODE | GND | Select software VCOM |
| EXTCOMIN | GND | Unused in software VCOM mode |
| VDD, VDDA | 5 V | Both rails must rise together or VDD first |
| VSS, VSSA | GND | Common ground with ESP32-C3 |

Sharp specifies 4.8–5.5 V for VDD and VDDA. The SPI/control pins accept 3 V logic, compatible with ESP32-C3 3.3 V outputs. The panel has no backlight; `DISP` is used for panel enable only. Check that the purchased module exposes the bare 10-pin panel interface and provides the required 5 V rails before wiring; carrier boards may add regulators or different signal names.

SPI pins and clock can be changed in `idf.py menuconfig` under **Sharp LS027B7DH01 Memory LCD**. The display remains landscape at 400×240; UI orientation selection changes layout only.

References: [Sharp LS027B7DH01 specification](https://media.digikey.com/pdf/Data%20Sheets/Sharp%20PDFs/LS027B7DH01.pdf), [ESP32 Memory LCD example](https://github.com/LaskaKit/LaskaKit_2_7-Memory-Diplay).
