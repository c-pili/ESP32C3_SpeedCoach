# ESP32-C3 Speed Coach

Open DIY GPS and stroke coach for water paddle sports (dragon boat, rowing, and similar craft). Configured for ESP32-C3 with MPU6050 stroke detection, NEO-M8N GNSS pace/distance, an LVGL display, and one physical control button.

Firmware slug: `esp32c3_speed_coach`.

---

## Implemented now

| Feature | Description |
|--------|-------------|
| Fixed live dashboard | Pace `/500 m`, elapsed time, stroke rate (SPM), distance, and heart rate (bpm) always visible |
| Split row | Progress through the configured split, session average pace, last-split delta |
| GPS speed & distance | Instant and average pace from NEO-M8N GNSS over UART1; stale GPS is flagged and does not zero averages |
| Stroke detection | Count, SPM, drive/recovery times from MPU6050 over I2C |
| Interval training | Work/rest by time, distance, or strokes with a dominant remaining display |
| Activity logging | CSV logger retained; SD card access still uses the old S3 SDMMC wiring and needs an SPI storage port for C3 |
| Activity browser | Summary and split-level detail |
| Compact live status | Recording, GPS quality, battery (no full app bar on water screens) |
| Physical control | One PWR button starts/stops and saves a standard activity; a 5-second hold saves then powers off |
| Reflective display | No backlight; Sharp memory-LCD VCOM is maintained at 1 Hz and the screen remains visible during activity |
| Theme / orientation | Dark/light (corrected mapping); fixed landscape 400×240 |
| USB export | Not available on ESP32-C3; USB Serial/JTAG does not provide USB mass storage |
| BLE heart rate | Automatically discovers a Suunto HR broadcaster on first boot, saves it, and reconnects on later boots; live bpm is always on the dashboard |
| BLE RowPod | Per-stroke catch, finish, and arc from the pod summary characteristic. See [docs/ble_sensors.md](docs/ble_sensors.md) |

## On-water controls

- **PWR press and release** (under 5 seconds): start a standard activity, or stop and save the current activity
- **PWR hold** (5 seconds): save an active activity, then power off
- To provide bpm, enable **Heart Rate Broadcast** in the Suunto sport-mode settings and start that sport mode; the Speed Coach scans for the standard BLE heart-rate service and remembers the Suunto device

## Not in this firmware (see [docs/ui_refinement.md](docs/ui_refinement.md))

BLE power sensors, peer link mode, interval presets, and sport profiles. Race target-pace follow is on the Race tile. RowPod catch and arc are on the live metric cycle when a pod is connected.

---

## Bill of Materials

The base is an **ESP32-C3 Speed Coach** with MPU6050 and NEO-M8N. The display driver targets the Sharp LS027B7DH01 monochrome memory LCD. The bare panel requires 5 V on VDD and VDDA, accepts 3 V logic, has no backlight, and uses software VCOM with EXTMODE tied low. Confirm the exact C3 board pinout and the LCD module's power wiring before connecting it.

| # | Item | Notes |
|---|------|-------|
| 1 | ESP32-C3 board | MCU; exact board variant and available GPIOs must be confirmed |
| 2 | RTC backup battery | CR2032 on RTC header |
| 3 | LiPo 3.7 V | MX1.25 battery header |
| 4 | NEO-M8N GPS | UART1 9600; GPS TX→GPIO20 (RX), GPS RX→GPIO21 (TX) |
| 5 | MPU6050 | I2C address 0x68; SDA=GPIO0, SCL=GPIO1 (share with RTC if fitted) |
| 6 | microSD | Not ready on C3: port the helper to SPI and confirm GPIO/CS wiring before logging/export |

---

## Build

Requires **ESP-IDF 6.0.1** (6.0.x). On this machine the tree is `C:\Espressif\.espressif\v6.0.1\esp-idf`. Export it, then:

```
idf.py set-target esp32c3
idf.py build
idf.py flash monitor
```

Or use [docs/flash.html](docs/flash.html) for browser flashing (`esp32c3_speed_coach.bin`).

Landscape 400×240 is the reference layout. After a UI/font change run `idf.py size-components` and note free heap / LVGL stack high-water mark. Do not enable PSRAM just to hide RAM cost.

---

## Pins

| Module | Component | Default |
|--------|-----------|---------|
| IMU MPU6050 | [mpu6050](components/mpu6050) | I2C0 SDA=0 SCL=1, address 0x68 |
| GPS NEO-M8N | [gps_neo_m8n](components/gps_neo_m8n) | UART1 TX=21 RX=20, 9600 baud |
| Display | [lcd_sharp_ls027](components/lcd_sharp_ls027) | 400×240; SPI2 SI/MOSI=5 SCLK=4 SCS=10 DISP=3; tie EXTMODE and unused EXTCOMIN low for software VCOM; panel VDD/VDDA=5 V |
| Power | [pwr_key](components/pwr_key) | KEY=6 HOLD=7; confirm against the board |
| Battery | [battery_drv](components/battery_drv) | ADC1 channel 2 / GPIO2; verify divider and boot-strap impact on the board |
| RTC | [rtc_pcf85063](components/rtc_pcf85063) | Shares I2C0 SDA=0 SCL=1 with MPU6050 |
| SD | [sd_mmc_helper](components/sd_mmc_helper) | Legacy S3 SDMMC driver; C3 SPI storage port still required |

---

## Changelog

- 2026-10-02 — BLE heart-rate strap (`0x180D`) and RowPod per-stroke summary (`9b7e1005`): live HR, catch, and arc, plus appended CSV columns. See [docs/ble_sensors.md](docs/ble_sensors.md)
- 2026-08-19 — product rename to ESP32-S3 Speed Coach; portrait-first live UI; tabular pace fonts; snapshot UI path; GPS-average and activity-ID fixes
- 2026-03-09 — settings — USB device mode
- 2026-01-28 — ui — simplify UI
- 2026-01-13 — activity_log — refine log; race removed from menu
