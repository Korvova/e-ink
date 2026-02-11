# E-Ink Display Project (Waveshare ESP32-S3-ETH + GoodDisplay 10.85")

Рабочий проект для `GDEM1085T51` через драйверы GoodDisplay (`Display_EPD_W21*`) и PlatformIO.

## Важно

- Для этого дисплея нужны обе линии выбора кристаллов: `CS` и `CS2`.
- Сейчас в `platformio.ini` активна распиновка **экрана 2** (через `build_flags`).
- Данные (`SDI`) и такт (`SCLK`) можно шарить между экранами.

## Распиновка

### Экран 1

| GoodDisplay | ESP32-S3-ETH |
|---|---|
| SDI | IO35 |
| SCLK | IO36 |
| CS | IO37 |
| CS2 | IO40 |
| RES | IO38 |
| BUSY | IO39 |
| 3.3V | 3V3 |
| GND | GND |

### Экран 2

| GoodDisplay | ESP32-S3-ETH |
|---|---|
| SDI | IO35 (shared) |
| SCLK | IO36 (shared) |
| CS | IO41 |
| CS2 | IO42 |
| RES | IO45 |
| BUSY | IO1 |
| 3.3V | 3V3 (shared) |
| GND | GND (shared) |

## Команды в Serial (`115200`)

- `1` - white
- `2` - black
- `3` - test pattern
- `4` - `PRIVET RMS EKRAN 2` (для текущей версии)
- `r` - reset pulse
- `b` - busy state

## Сборка и загрузка

```bash
pio run -t upload
```

Monitor:

```bash
pio device monitor --baud 115200 --port COM9
```

## Как переключить проект на экран 1

В `platformio.ini` поменяйте `build_flags` на:

```ini
-DEPD_PIN_MOSI=35
-DEPD_PIN_CLK=36
-DEPD_PIN_CS=37
-DEPD_PIN_CS2=40
-DEPD_PIN_RST=38
-DEPD_PIN_BUSY=39
```


```
На другом компе достаточно:

-e-ink.git
-открыть проект в PlatformIO
-проверить upload_port/monitor_port
-pio run -t upload
И можно сразу пользоваться.
```
