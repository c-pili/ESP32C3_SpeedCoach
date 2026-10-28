# ESP32-S3 Speed Coach

Open DIY GPS and stroke coach for water paddle sports (dragon boat, rowing, and similar craft). Built on ESP32-S3 with IMU stroke detection, GNSS pace/distance, an LVGL display, and one physical control button.

Firmware slug: `esp32s3_speed_coach`. Rename this directory from `waveshare2.8lcd_tryout` to `esp32s3_speed_coach` after closing the IDE if the folder is still the old bring-up name.

---

## Implemented now

| Feature | Description |
|--------|-------------|
| Fixed live dashboard | Pace `/500 m`, elapsed time, stroke rate (SPM), distance, and heart rate (bpm) always visible |
| Split row | Progress through the configured split, session average pace, last-split delta |
| GPS speed & distance | Instant and average pace from GT-U8 GNSS; stale GPS is flagged and does not zero averages |
| Stroke detection | Count, SPM, drive/recovery times from QMI8658 |
| Interval training | Work/rest by time, distance, or strokes with a dominant remaining display |
| Activity logging | CSV on SD (strokes + distance splits; interval phase rows) |
| Activity browser | Summary and split-level detail |
| Compact live status | Recording, GPS quality, battery (no full app bar on water screens) |
| Physical control | One PWR button starts/stops and saves a standard activity; a 5-second hold saves then powers off |
| Brightness / auto-dim | Settings slider; auto-dim after 15 s idle while recording |
| Theme / orientation | Dark/light (corrected mapping), portrait-first with landscape support |
| USB export | MSC "Export via USB" |
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

The base is an **ESP32-S3 Speed Coach** (QMI8658 + PCF85063 + TF). The display is configured separately; a Sharp LS027B7DH01 memory LCD requires its own SPI/VCOM driver and wiring.

| # | Item | Notes |
|---|------|-------|
| 1 | ESP32-S3 board | MCU and existing sensor/SD peripherals |
| 2 | RTC backup battery | CR2032 on RTC header |
| 3 | LiPo 3.7 V | MX1.25 battery header |
| 4 | GT-U8 GPS | UART 9600; TX→RXD GPIO44, RX→TXD GPIO43 |
| 5 | microSD | Optional; onboard TF slot |

---

## Build

Requires **ESP-IDF 6.0.1** (6.0.x). On this machine the tree is `C:\Espressif\.espressif\v6.0.1\esp-idf`. Export it, then:

```
idf.py set-target esp32s3
idf.py build
idf.py flash monitor
```

Or use [docs/flash.html](docs/flash.html) for browser flashing (`esp32s3_speed_coach.bin`).

Portrait 240×320 is the reference layout. After a UI/font change run `idf.py size-components` and note free heap / LVGL stack high-water mark. Do not enable PSRAM just to hide RAM cost.

---

## Pins

| Module | Component | Default |
|--------|-----------|---------|
| IMU QMI8658 | [qmi8658](components/qmi8658) | I2C1 SDA=11 SCL=10 |
| GPS GT-U8 | [gps_gtu8](components/gps_gtu8) | UART1 TX=43 RX=44 |
| Display | [lcd_st7789](components/lcd_st7789) | Existing ST7789 driver; Sharp LS027B7DH01 migration requires panel wiring/pin confirmation |
| Power | [pwr_key](components/pwr_key) | KEY=6 HOLD=7 |
| Battery | [battery_drv](components/battery_drv) | ADC GPIO8 |
| RTC | [rtc_pcf85063](components/rtc_pcf85063) | I2C1 shared with IMU |
| SD | [sd_mmc_helper](components/sd_mmc_helper) | CLK=14 CMD=17 D0=16 |

---

## Changelog

- 2026-10-02 — BLE heart-rate strap (`0x180D`) and RowPod per-stroke summary (`9b7e1005`): live HR, catch, and arc, plus appended CSV columns. See [docs/ble_sensors.md](docs/ble_sensors.md)
- 2026-08-19 — product rename to ESP32-S3 Speed Coach; portrait-first live UI; tabular pace fonts; snapshot UI path; GPS-average and activity-ID fixes
- 2026-03-09 — settings — USB device mode
- 2026-01-28 — ui — simplify UI
- 2026-01-13 — activity_log — refine log; race removed from menu
