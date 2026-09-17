# ADF4350 Controller — ESP32-C3

Управление синтезатором частот ADF4350 от ESP32-C3 через SPI.

## Структура проекта

```
├── platformio.ini          — конфигурация PlatformIO
├── include/
│   └── adf4350.h           — драйвер ADF4350 (пины, параметры)
├── src/
│   ├── main.cpp            — основной код (setup/loop, команды)
│   └── adf4350.cpp         — реализация драйвера
```

## Подключение (по умолчанию)

| ESP32-C3 | ADF4350 | Назначение |
|----------|---------|------------|
| GPIO7    | SDATA   | SPI MOSI (данные) |
| GPIO4    | SCLK    | SPI Clock |
| GPIO5    | LE      | Latch Enable |
| GPIO6    | CE      | Chip Enable |
| GPIO3    | MUXOUT  | Lock Detect (вход) |
| 3.3V     | AVDD/DVDD | Питание |
| GND      | GND     | Земля |

## Как изменить распиновку

Открой `include/adf4350.h` и измени:

```cpp
#define ADF4350_PIN_MOSI    7    // твой пин
#define ADF4350_PIN_SCLK    4    // твой пин
#define ADF4350_PIN_LE      5    // твой пин
#define ADF4350_PIN_CE      6    // твой пин
#define ADF4350_PIN_MUXOUT  3    // твой пин
```

## Команды через Serial Monitor (115200)

| Команда | Описание |
|---------|----------|
| `f435.0` | Установить частоту 435.0 МГц |
| `s` | Sweep по таблице частот |
| `l` | Проверить статус PLL lock |
| `p` | Toggle power down/up |
| `r` | Показать все регистры |
| `+` | Power up |
| `-` | Power down |

## Сборка и прошивка

```bash
# Установить PlatformIO CLI или использовать VS Code с расширением PlatformIO

# Собрать
pio run

# Прошить
pio run --target upload

# Монитор
pio device monitor
```

## Формула частоты

```
RFout = PFD × (INT + FRAC/MOD)
PFD = REF × (1+Doubler) / (R × (1+Div2))
```

## Диапазон ADF4350

- Выход: 34.6 – 4400 МГц
- PFD: до 32 МГц (до 100 МГц с умножителем)
- SPI: CPOL=0, CPHA=0, макс 20 МГц
- Питание: 3.0–3.6V
