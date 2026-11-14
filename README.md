# ESP32-C3 Speed Coach / Coach Vitesse ESP32-C3

**English** | [Français](#français)

---

## English

Open DIY GPS and stroke coach for water paddle sports (dragon boat, rowing, and similar craft). Configured for ESP32-C3 with MPU6050 stroke detection, NEO-M8N GNSS pace/distance, an LVGL display, and one physical control button.

Firmware slug: `esp32c3_speed_coach`.

### Features Implemented

| Feature | Description |
|---------|-------------|
| Live display | Select pace, speed, or a live longitudinal acceleration graph from the Wi-Fi page |
| Split logging | CSV summaries at the configured distance (1000 m by default) |
| GPS speed & distance | Instant and average pace from NEO-M8N GNSS over UART1; stale GPS is flagged and does not zero averages |
| Stroke detection | Count, SPM, drive/recovery times from MPU6050 over I2C |
| Activity logging | Stroke CSV, split CSV, and TCX files stored in the ESP32-C3 SPIFFS partition |
| Workout setup | Web selection for free, timed, distance, or interval sessions; timed and distance targets stop automatically |
| Wi-Fi setup and export | AP starts at boot before optional sensors; BLE starts after the phone joins Wi-Fi and loads the page. Configure the coach and download TCX at `http://192.168.4.1` |
| Physical control | One PWR button: short press = start/pause/resume; double press = cycle display; long press = save+summary (if paused) or shutdown (if idle) |
| Reflective display | No backlight; Sharp memory-LCD VCOM maintained at 1 Hz; display sleeps after 45 minutes without user activity and stays awake while recording |
| BLE heart rate | After Wi-Fi page loads, automatically pairs only with `Suunto Watch HR 1029`, saves it, and reconnects; live BPM and link state shown on the page |
| BLE RowPod | Optional paired motion-pod telemetry logged when connected; see [docs/ble_sensors.md](docs/ble_sensors.md) |

### On-Water Controls

- **Short press** (< 5 s): Start activity if idle; pause if recording; resume if paused
- **Double press**: Cycle display (pace/time/SPM/distance → speed/time/SPM/distance → acceleration graph)
- **Long press** (3 s): If paused → save + show summary; if idle → shutdown device; if recording → ignored (pause first)
- **To provide BPM**: Enable **Heart Rate Broadcast** in Suunto sport-mode settings and start that sport mode

### Hardware Notes

Wi-Fi AP initializes before optional sensors. MPU6050 may be absent during bench setup; stroke metrics and acceleration graph remain unavailable while web page and activity export still work. GPS and heart-rate require corresponding hardware.

Battery percentage measured through ADC divider on GPIO2 — only meaningful when board's battery and divider are connected and calibrated. No configured charger status input, so charging state cannot be reliably detected.

### Bill of Materials

| # | Item | Notes |
|---|------|-------|
| 1 | ESP32-C3 board | MCU; exact board variant and available GPIOs must be confirmed |
| 2 | RTC backup battery | CR2032 on RTC header |
| 3 | LiPo 3.7 V | MX1.25 battery header |
| 4 | NEO-M8N GPS | UART1 9600; GPS TX→GPIO20 (RX), GPS RX→GPIO21 (TX) |
| 5 | MPU6050 | I2C address 0x68; SDA=GPIO0, SCL=GPIO1 (share with RTC if fitted) |
| 6 | Flash storage | Internal 4 MB flash; SPIFFS partition initialized once with generated image |

### Build

Requires **ESP-IDF 6.0.1** (6.0.x):

```bash
idf.py set-target esp32c3
idf.py build
idf.py flash monitor
```

For Wi-Fi setup and activity export, connect to `SpeedCoach_Wi-Fi` (password `SpeedCoach`) and open `http://192.168.4.1`. The access point starts at boot and stops during activity recording. See [docs/WIFI_EXPORT.md](docs/WIFI_EXPORT.md) for troubleshooting and one-time SPIFFS initialization.

Landscape 400×240 is the reference layout. After UI/font change run `idf.py size-components` and note free heap / LVGL stack high-water mark. Do not enable PSRAM just to hide RAM cost.

### Pin Mapping

| Module | Component | Default |
|--------|-----------|---------|
| IMU MPU6050 | [mpu6050](components/mpu6050) | I2C0 SDA=0 SCL=1, address 0x68 |
| GPS NEO-M8N | [gps_neo_m8n](components/gps_neo_m8n) | UART1 TX=21 RX=20, 9600 baud |
| Display | [lcd_sharp_ls027](components/lcd_sharp_ls027) | 400×240; SPI2 SI/MOSI=5 SCLK=4 SCS=10 DISP=3; tie EXTMODE and unused EXTCOMIN low for software VCOM; panel VDD/VDDA=5 V |
| Power | [pwr_key](components/pwr_key) | KEY=6 HOLD=7; confirm against the board |
| Battery | [battery_drv](components/battery_drv) | ADC1 channel 2 / GPIO2; verify divider and boot-strap impact on the board |
| RTC | [rtc_pcf85063](components/rtc_pcf85063) | Shares I2C0 SDA=0 SCL=1 with MPU6050 |
| Storage | [flash_storage](components/flash_storage) | SPIFFS, partition label `storage`, mounted at `/spiffs` |

### Memory Optimizations (for BLE + WiFi + Display coexistence)

- LVGL heap: 24 KB
- NimBLE buffers: MSYS_1=2, MSYS_2=3, ACL=2, EVT=4
- BT Controller: RX/TX=2, BLE TX/RX=2, max connections=1
- WiFi static/dynamic buffers: 8 each
- Task stacks: activity_worker=6KB, stroke=6KB
- Full WiFi deinit before BLE init (1s delay)

### Changelog

- 2026-11-14 — Full-screen acceleration graph with HR/SPM overlay; end-of-session summary (distance/avg HR/avg SPM); deep sleep on shutdown; wake-on-long-press; time format MM:SS; unified 32px font
- 2026-10-02 — BLE heart-rate strap (0x180D) and RowPod per-stroke summary (9b7e1005): live HR and optional pod metrics in activity CSV logs
- 2026-10-29 — Remove legacy S3/SD/USB paths; add internal SPIFFS storage and Wi-Fi TCX export
- 2026-01-28 — UI simplification
- 2026-10-29 — Remove unused interval-workout, race-pacing, and step-test code paths

---

## Français

Coach GPS et cadence DIY open-source pour sports de pagaie (dragon boat, aviron, embarcations similaires). Conçu pour ESP32-C3 avec détection de coups MPU6050, GNSS NEO-M8N allure/distance, affichage LVGL et un unique bouton de contrôle physique.

Identifiant firmware : `esp32c3_speed_coach`.

### Fonctionnalités Implémentées

| Fonctionnalité | Description |
|--------------|-------------|
| Affichage en direct | Sélection allure, vitesse ou courbe d'accélération longitudinale en direct depuis la page Wi-Fi |
| Journalisation splits | Résumés CSV à distance configurable (1000 m par défaut) |
| GPS vitesse & distance | Allure instantanée et moyenne depuis NEO-M8N GNSS sur UART1 ; GPS périmé signalé sans remettre les moyennes à zéro |
| Détection de coups | Comptage, SPM, temps propulsion/récupération via MPU6050 I2C |
| Journal activités | CSV coups, CSV splits, fichiers TCX stockés dans partition SPIFFS ESP32-C3 |
| Configuration séance | Interface web : libre, temps unique, distance unique, intervalles ; arrêt auto sur objectifs temps/distance |
| Wi-Fi setup & export | AP démarre au boot avant capteurs optionnels ; BLE démarre après connexion téléphone et chargement page. Configuration et téléchargement TCX sur `http://192.168.4.1` |
| Contrôle physique | 1 bouton PWR : appui court = démarrer/pause/reprendre ; double appui = changer affichage ; appui long = sauvegarde+résumé (si pause) ou arrêt (si inactif) |
| Affichage réflexif | Sans rétroéclairage ; VCOM Sharp memory-LCD maintenu 1 Hz ; veille après 45 min sans activité, reste allumé en enregistrement |
| BLE fréquence cardiaque | Après chargement page Wi-Fi, appairage auto uniquement avec `Suunto Watch HR 1029`, mémorisation et reconnexion ; BPM live et état liaison affichés |
| BLE RowPod | Télémétrie pod mouvement optionnelle journalisée si connecté ; voir [docs/ble_sensors.md](docs/ble_sensors.md) |

### Contrôles sur l'Eau

- **Appui court** (< 5 s) : Démarre si inactif ; met en pause si enregistrement ; reprend si en pause
- **Double appui** : Cycle affichage (allure/temps/SPM/distance → vitesse/temps/SPM/distance → courbe accélération)
- **Appui long** (3 s) : Si pause → sauvegarde + résumé ; si inactif → arrêt appareil ; si enregistrement → ignoré (pause d'abord)
- **Pour fournir BPM** : Activer **Broadcast FC** dans réglages mode sport Suunto et démarrer ce mode sport

### Notes Matérielles

Le point d'accès Wi-Fi s'initialise avant les capteurs optionnels. Le MPU6050 peut être absent lors des tests sur banc ; les métriques de coups et la courbe d'accélération restent alors indisponibles, tandis que la page web et l'export d'activités fonctionnent. GPS et fréquence cardiaque nécessitent leur matériel respectif.

Le pourcentage de batterie est mesuré via diviseur ADC sur GPIO2 — significatif seulement si la batterie de la carte et son diviseur sont connectés et calibrés. Aucune entrée d'état chargeur n'est configurée, l'état de charge ne peut donc pas être détecté de façon fiable.

### Nomenclature (Bill of Materials)

| # | Élément | Notes |
|---|---------|-------|
| 1 | Carte ESP32-C3 | MCU ; variante exacte et GPIOs disponibles à confirmer |
| 2 | Batterie secours RTC | CR2032 sur connecteur RTC |
| 3 | LiPo 3,7 V | Connecteur MX1.25 batterie |
| 4 | GPS NEO-M8N | UART1 9600 ; GPS TX→GPIO20 (RX), GPS RX→GPIO21 (TX) |
| 5 | MPU6050 | Adresse I2C 0x68 ; SDA=GPIO0, SCL=GPIO1 (partagé avec RTC si présent) |
| 6 | Stockage Flash | Flash interne 4 Mo ; partition SPIFFS initialisée une fois avec image générée |

### Compilation

Nécessite **ESP-IDF 6.0.1** (série 6.0.x) :

```bash
idf.py set-target esp32c3
idf.py build
idf.py flash monitor
```

Pour le Wi-Fi setup et l'export d'activités, connectez-vous à `SpeedCoach_Wi-Fi` (mot de passe `SpeedCoach`) et ouvrez `http://192.168.4.1`. Le point d'accès démarre au boot et s'arrête pendant l'enregistrement. Voir [docs/WIFI_EXPORT.md](docs/WIFI_EXPORT.md) pour le dépannage et l'initialisation SPIFFS unique.

Mise en page paysage 400×240 de référence. Après changement UI/police, lancez `idf.py size-components` et notez heap libre / LVGL stack high-water mark. N'activez pas la PSRAM juste pour masquer le coût RAM.

### Brochage (Pin Mapping)

| Module | Composant | Défaut |
|--------|-----------|--------|
| IMU MPU6050 | [mpu6050](components/mpu6050) | I2C0 SDA=0 SCL=1, adresse 0x68 |
| GPS NEO-M8N | [gps_neo_m8n](components/gps_neo_m8n) | UART1 TX=21 RX=20, 9600 bauds |
| Affichage | [lcd_sharp_ls027](components/lcd_sharp_ls027) | 400×240 ; SPI2 SI/MOSI=5 SCLK=4 SCS=10 DISP=3 ; lier EXTMODE et EXTCOMIN inutilisé à la masse pour VCOM logiciel ; panneau VDD/VDDA=5 V |
| Alimentation | [pwr_key](components/pwr_key) | KEY=6 HOLD=7 ; confirmer selon la carte |
| Batterie | [battery_drv](components/battery_drv) | ADC1 canal 2 / GPIO2 ; vérifier diviseur et impact bootstrap sur la carte |
| RTC | [rtc_pcf85063](components/rtc_pcf85063) | Partage I2C0 SDA=0 SCL=1 avec MPU6050 |
| Stockage | [flash_storage](components/flash_storage) | SPIFFS, label partition `storage`, monté sur `/spiffs` |

### Optimisations Mémoire (coexistence BLE + WiFi + Affichage)

- Tas LVGL : 24 Ko
- Tampons NimBLE : MSYS_1=2, MSYS_2=3, ACL=2, EVT=4
- Contrôleur BT : RX/TX=2, BLE TX/RX=2, max connexions=1
- Tampons WiFi statiques/dynamiques : 8 chacun
- Piles tâches : activity_worker=6 Ko, stroke=6 Ko
- Déinitialisation complète WiFi avant init BLE (délai 1 s)

### Journal des Modifications

- 2026-11-14 — Courbe accélération plein écran avec superposition HR/SPM ; résumé fin de session (distance/HR moy/SPM moy) ; deep sleep à l'arrêt ; réveil appui long ; format temps MM:SS ; police unifiée 32 px
- 2026-10-02 — Ceinture cardio BLE (0x180D) et RowPod résumé par coup (9b7e1005) : HR live et métriques pod optionnelles dans logs CSV activités
- 2026-10-29 — Suppression chemins legacy S3/SD/USB ; ajout stockage SPIFFS interne et export TCX Wi-Fi
- 2026-01-28 — Simplification UI
- 2026-10-29 — Suppression code inutilisé interval-workout, race-pacing, step-test