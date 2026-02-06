# E-Ink Display Project

Проект для управления двумя e-ink дисплеями с помощью ESP32-S3-ETH

## Оборудование

- **ESP32-S3-ETH**
- **2x Goodisplay DESPI-C1085** (драйверы e-ink)
- **Дисплей FPC-8617B**

## Схема подключения

### Дисплей 1 (DESPI-C1085 #1)

| DESPI-C1085 | ESP32-S3-ETH |
|-------------|--------------|
| SDI         | IO35         |
| SCLK        | IO36         |
| CS          | IO37         |
| RES         | IO38         |
| BUSY        | IO39         |
| 3.3V        | 3V3          |
| GND         | GND          |

### Дисплей 2 (DESPI-C1085 #2)

| DESPI-C1085 | ESP32-S3-ETH |
|-------------|--------------|
| SDI         | IO35 (shared)|
| SCLK        | IO36 (shared)|
| CS          | IO40         |
| RES         | IO41         |
| BUSY        | IO42         |
| 3.3V        | 3V3          |
| GND         | GND          |

## Сборка и загрузка

1. Установите [PlatformIO](https://platformio.org/)
2. Подключите ESP32-S3 через USB Type-C
3. Войдите в boot mode: зажмите BOOT, нажмите RESET, отпустите BOOT
4. Загрузите прошивку:
```bash
pio run --target upload
```

## COM порт

Текущий COM порт: **COM9**

Для изменения отредактируйте `platformio.ini`:
```ini
upload_port = COM9
monitor_port = COM9
```
