/**
 * ============================================================
 * ADF4350 Synthesizer Controller — ESP32-C3
 * ============================================================
 * 
 * Среда: PlatformIO + Arduino Framework
 * Плата: ESP32-C3
 * 
 * ПОЛНАЯ РАСПИНОВКА:
 * ─────────────────────────────────────────
 * ADF4350:
 *   GPIO1  → CE    (Chip Enable)
 *   GPIO3  → LE    (Latch Enable)
 *   GPIO6  → SCLK  (SPI Clock)
 *   GPIO7  → MOSI  (SPI Data / SDATA)
 *   GPIO2  ← MUXOUT (Lock Detect)
 * 
 * Дисплей I2C (OLED SSD1306):
 *   GPIO8  → SCL
 *   GPIO9  → SDA
 * 
 * Периферия:
 *   GPIO0  → ADC_BAT (измерение батареи)
 *   GPIO4  → Button 1
 *   GPIO5  → Button 3
 *   GPIO10 → LED
 *   GPIO18/19 → USB
 * ─────────────────────────────────────────
 * 
 * Опорный генератор: 25 МГц (настрой в adf4350.h)
 */

#include <Arduino.h>
#include <Wire.h>
#include "adf4350.h"

// ============================================
// ПИНЫ ПЕРИФЕРИИ
// ============================================
#define PIN_LED         10    // LED индикатор
#define PIN_BUTTON1     4     // Кнопка 1
#define PIN_BUTTON3     5     // Кнопка 3
#define PIN_ADC_BAT     0     // ADC для измерения батареи

// I2C пины для дисплея
#define PIN_I2C_SDA     9     // GPIO9 — SDA
#define PIN_I2C_SCL     8     // GPIO8 — SCL

// ============================================
// ПАРАМЕТРЫ ДИСПЛЕЯ (SSD1306 128x64)
// ============================================
#define DISPLAY_WIDTH   128
#define DISPLAY_HEIGHT  64
#define DISPLAY_ADDR    0x3C  // I2C адрес OLED

// ============================================
// ПАРАМЕТРЫ БАТАРЕИ
// ============================================
#define BAT_ADC_MAX     4095    // Максимальное значение ADC (12 бит)
#define BAT_VREF        3.3     // Опорное напряжение ADC
#define BAT_DIVIDER     2.0     // Делитель напряжения (если есть)

// ============================================
// КОНФИГУРАЦИЯ ЧАСТОТ
// ============================================
#define START_FREQ_MHZ  435.0f

// Таблица частот для переключения кнопками
const float freq_table[] = {
    144.0f,    // 2m
    145.5f,
    430.0f,    // 70cm
    435.0f,
    438.0f,
    915.0f,    // ISM
    1296.0f,   // 23cm
    2400.0f,   // 13cm
};
#define FREQ_TABLE_SIZE (sizeof(freq_table) / sizeof(freq_table[0]))

// ============================================
// ГЛОБАЛЬНЫЕ ОБЪЕКТЫ
// ============================================
ADF4350 synth;
int current_freq_index = 3;  // Индекс в таблице (435.0 МГц)
bool pll_locked = false;
float battery_voltage = 0.0;

// ============================================
// ФУНКЦИИ ДИСПЛЕЯ
// ============================================

// Буфер дисплея (упрощённый, без библиотеки)
uint8_t display_buffer[DISPLAY_WIDTH * DISPLAY_HEIGHT / 8];
bool display_initialized = false;

void display_init() {
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    Wire.setClock(400000);  // 400 kHz I2C
    
    delay(100);
    
    // Проверка наличия дисплея
    Wire.beginTransmission(DISPLAY_ADDR);
    if (Wire.endTransmission() == 0) {
        display_initialized = true;
        Serial.println("[DISPLAY] OLED found at 0x3C");
        
        // Инициализация SSD1306
        display_command(0xAE); // Display OFF
        display_command(0xD5); // Set display clock divide ratio
        display_command(0x80);
        display_command(0xA8); // Set multiplex ratio
        display_command(0x3F); // 1/64
        display_command(0xD3); // Set display offset
        display_command(0x00);
        display_command(0x40); // Set start line
        display_command(0x8D); // Charge pump
        display_command(0x14); // Enable
        display_command(0x20); // Memory addressing mode
        display_command(0x00); // Horizontal
        display_command(0xA1); // Segment re-map
        display_command(0xC8); // COM output scan direction
        display_command(0xDA); // Set COM pins
        display_command(0x12);
        display_command(0x81); // Set contrast
        display_command(0xCF);
        display_command(0xD9); // Set pre-charge period
        display_command(0xF1);
        display_command(0xDB); // Set VCOMH deselect level
        display_command(0x40);
        display_command(0xA4); // Resume to RAM content
        display_command(0xA6); // Normal display
        display_command(0xAF); // Display ON
        
        display_clear();
    } else {
        Serial.println("[DISPLAY] OLED not found!");
    }
}

void display_command(uint8_t cmd) {
    Wire.beginTransmission(DISPLAY_ADDR);
    Wire.write(0x00); // Command mode
    Wire.write(cmd);
    Wire.endTransmission();
}

void display_data(uint8_t data) {
    Wire.beginTransmission(DISPLAY_ADDR);
    Wire.write(0x40); // Data mode
    Wire.write(data);
    Wire.endTransmission();
}

void display_clear() {
    memset(display_buffer, 0, sizeof(display_buffer));
    display_update();
}

void display_update() {
    if (!display_initialized) return;
    
    display_command(0x21); // Set column address
    display_command(0);
    display_command(127);
    display_command(0x22); // Set page address
    display_command(0);
    display_command(7);
    
    Wire.beginTransmission(DISPLAY_ADDR);
    Wire.write(0x40);
    for (int i = 0; i < sizeof(display_buffer); i++) {
        Wire.write(display_buffer[i]);
    }
    Wire.endTransmission();
}

// Простейший вывод текста (5x7 шрифт)
const uint8_t font5x7[][5] = {
    {0x00,0x00,0x00,0x00,0x00}, // space
    {0x00,0x00,0x5F,0x00,0x00}, // !
    {0x00,0x07,0x00,0x07,0x00}, // "
    {0x14,0x7F,0x14,0x7F,0x14}, // #
    {0x24,0x2A,0x7F,0x2A,0x12}, // $
    {0x23,0x13,0x08,0x64,0x62}, // %
    {0x36,0x49,0x55,0x22,0x50}, // &
    {0x00,0x05,0x03,0x00,0x00}, // '
    {0x00,0x1C,0x22,0x41,0x00}, // (
    {0x00,0x41,0x22,0x1C,0x00}, // )
    {0x08,0x2A,0x1C,0x2A,0x08}, // *
    {0x08,0x08,0x3E,0x08,0x08}, // +
    {0x00,0x50,0x30,0x00,0x00}, // ,
    {0x08,0x08,0x08,0x08,0x08}, // -
    {0x00,0x60,0x60,0x00,0x00}, // .
    {0x20,0x10,0x08,0x04,0x02}, // /
    {0x3E,0x51,0x49,0x45,0x3E}, // 0
    {0x00,0x42,0x7F,0x40,0x00}, // 1
    {0x72,0x49,0x49,0x49,0x46}, // 2
    {0x21,0x41,0x45,0x4B,0x31}, // 3
    {0x18,0x14,0x12,0x7F,0x10}, // 4
    {0x27,0x45,0x45,0x45,0x39}, // 5
    {0x3C,0x4A,0x49,0x49,0x30}, // 6
    {0x01,0x71,0x09,0x05,0x03}, // 7
    {0x36,0x49,0x49,0x49,0x36}, // 8
    {0x06,0x49,0x49,0x29,0x1E}, // 9
    {0x00,0x36,0x36,0x00,0x00}, // :
    {0x00,0x56,0x36,0x00,0x00}, // ;
    {0x00,0x08,0x14,0x22,0x41}, // <
    {0x14,0x14,0x14,0x14,0x14}, // =
    {0x41,0x22,0x14,0x08,0x00}, // >
    {0x02,0x01,0x51,0x09,0x06}, // ?
    {0x32,0x49,0x79,0x41,0x3E}, // @
    {0x7E,0x11,0x11,0x11,0x7E}, // A
    {0x7F,0x49,0x49,0x49,0x36}, // B
    {0x3E,0x41,0x41,0x41,0x22}, // C
    {0x7F,0x41,0x41,0x22,0x1C}, // D
    {0x7F,0x49,0x49,0x49,0x41}, // E
    {0x7F,0x09,0x09,0x01,0x01}, // F
    {0x3E,0x41,0x41,0x51,0x32}, // G
    {0x7F,0x08,0x08,0x08,0x7F}, // H
    {0x00,0x41,0x7F,0x41,0x00}, // I
    {0x20,0x40,0x41,0x3F,0x01}, // J
    {0x7F,0x08,0x14,0x22,0x41}, // K
    {0x7F,0x40,0x40,0x40,0x40}, // L
    {0x7F,0x02,0x04,0x02,0x7F}, // M
    {0x7F,0x04,0x08,0x10,0x7F}, // N
    {0x3E,0x41,0x41,0x41,0x3E}, // O
    {0x7F,0x09,0x09,0x09,0x06}, // P
    {0x3E,0x41,0x51,0x21,0x5E}, // Q
    {0x7F,0x09,0x19,0x29,0x46}, // R
    {0x46,0x49,0x49,0x49,0x31}, // S
    {0x01,0x01,0x7F,0x01,0x01}, // T
    {0x3F,0x40,0x40,0x40,0x3F}, // U
    {0x1F,0x20,0x40,0x20,0x1F}, // V
    {0x7F,0x20,0x18,0x20,0x7F}, // W
    {0x63,0x14,0x08,0x14,0x63}, // X
    {0x03,0x04,0x78,0x04,0x03}, // Y
    {0x61,0x51,0x49,0x45,0x43}, // Z
};

void display_draw_char(char c, int x, int y) {
    if (c < ' ' || c > 'Z') c = ' ';
    int idx = c - ' ';
    
    for (int i = 0; i < 5; i++) {
        uint8_t col = font5x7[idx][i];
        for (int j = 0; j < 8; j++) {
            if (col & (1 << j)) {
                int px = x + i;
                int py = y + j;
                if (px >= 0 && px < DISPLAY_WIDTH && py >= 0 && py < DISPLAY_HEIGHT) {
                    int byte_idx = px + (py / 8) * DISPLAY_WIDTH;
                    display_buffer[byte_idx] |= (1 << (py % 8));
                }
            }
        }
    }
}

void display_draw_string(const char* str, int x, int y) {
    while (*str) {
        display_draw_char(*str, x, y);
        x += 6;
        str++;
    }
}

void display_show_frequency(float freq, bool locked) {
    if (!display_initialized) return;
    
    display_clear();
    
    // Заголовок
    display_draw_string("ADF4350", 40, 0);
    
    // Частота
    char freq_str[20];
    snprintf(freq_str, sizeof(freq_str), "%.3f MHz", freq);
    display_draw_string(freq_str, 20, 20);
    
    // Статус
    if (locked) {
        display_draw_string("LOCKED", 40, 40);
    } else {
        display_draw_string("NO LOCK", 40, 40);
    }
    
    // Батарея
    char bat_str[20];
    snprintf(bat_str, sizeof(bat_str), "BAT:%.1fV", battery_voltage);
    display_draw_string(bat_str, 0, 56);
    
    display_update();
}

// ============================================
// ФУНКЦИИ БАТАРЕИ
// ============================================
void read_battery() {
    int adc_value = analogRead(PIN_ADC_BAT);
    float voltage = (adc_value / (float)BAT_ADC_MAX) * BAT_VREF * BAT_DIVIDER;
    battery_voltage = voltage;
}

// ============================================
// ФУНКЦИИ КНОПОК
// ============================================
void handle_buttons() {
    static unsigned long last_button_time = 0;
    unsigned long now = millis();
    
    // Debounce 200ms
    if (now - last_button_time < 200) return;
    
    // Button 1 — предыдущая частота
    if (digitalRead(PIN_BUTTON1) == LOW) {
        last_button_time = now;
        current_freq_index--;
        if (current_freq_index < 0) current_freq_index = FREQ_TABLE_SIZE - 1;
        
        Serial.printf("[BTN] Previous freq: %.1f MHz\n", freq_table[current_freq_index]);
        pll_locked = synth.setFrequency(freq_table[current_freq_index]);
        
        // Мигнём LED
        digitalWrite(PIN_LED, HIGH);
        delay(50);
        digitalWrite(PIN_LED, LOW);
    }
    
    // Button 3 — следующая частота
    if (digitalRead(PIN_BUTTON3) == LOW) {
        last_button_time = now;
        current_freq_index++;
        if (current_freq_index >= FREQ_TABLE_SIZE) current_freq_index = 0;
        
        Serial.printf("[BTN] Next freq: %.1f MHz\n", freq_table[current_freq_index]);
        pll_locked = synth.setFrequency(freq_table[current_freq_index]);
        
        // Мигнём LED
        digitalWrite(PIN_LED, HIGH);
        delay(50);
        digitalWrite(PIN_LED, LOW);
    }
}

// ============================================
// SETUP
// ============================================
void setup() {
    Serial.begin(115200);
    delay(2000);
    
    Serial.println();
    Serial.println("============================================");
    Serial.println("  ADF4350 Synthesizer — ESP32-C3");
    Serial.println("============================================");
    Serial.println();
    
    // Инициализация LED
    pinMode(PIN_LED, OUTPUT);
    digitalWrite(PIN_LED, LOW);
    
    // Инициализация кнопок (INPUT_PULLUP — кнопки на GND)
    pinMode(PIN_BUTTON1, INPUT_PULLUP);
    pinMode(PIN_BUTTON3, INPUT_PULLUP);
    
    // Инициализация ADC для батареи
    analogReadResolution(12);  // 12 бит
    analogSetAttenuation(ADC_11db);  // Полный диапазон 0-3.3V
    
    // Инициализация дисплея
    display_init();
    
    // Инициализация синтезатора
    synth.begin();
    
    // Устанавливаем начальную частоту
    Serial.printf("\n[MAIN] Setting frequency to %.3f MHz...\n", freq_table[current_freq_index]);
    pll_locked = synth.setFrequency(freq_table[current_freq_index]);
    
    if (pll_locked) {
        Serial.println("[MAIN] Frequency set successfully!");
        digitalWrite(PIN_LED, HIGH);  // LED горит при lock
    } else {
        Serial.println("[MAIN] WARNING: PLL did not lock!");
    }
    
    // Читаем батарею
    read_battery();
    Serial.printf("[MAIN] Battery: %.2f V\n", battery_voltage);
    
    // Обновляем дисплей
    display_show_frequency(freq_table[current_freq_index], pll_locked);
    
    // Выводим конфигурацию
    const ADF4350Config_t& cfg = synth.getConfig();
    Serial.println("\n--- Configuration ---");
    Serial.printf("  PFD:    %.3f MHz\n", cfg.pfd_freq);
    Serial.printf("  INT:    %d\n", cfg.int_val);
    Serial.printf("  FRAC:   %d\n", cfg.frac_val);
    Serial.printf("  MOD:    %d\n", cfg.mod_val);
    Serial.printf("  Actual: %.6f MHz\n", cfg.actual_freq);
    Serial.println("---------------------\n");
    
    Serial.println("[MAIN] Commands via Serial:");
    Serial.println("  f<freq>  — set frequency (e.g. 'f435.0')");
    Serial.println("  l        — check lock status");
    Serial.println("  b        — read battery");
    Serial.println("  r        — show registers");
    Serial.println();
}

// ============================================
// ОБРАБОТКА Serial команд
// ============================================
void handleSerialCommand() {
    if (!Serial.available()) return;
    
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    
    if (cmd.length() == 0) return;
    
    char command = cmd.charAt(0);
    
    switch (command) {
        case 'f':
        case 'F': {
            float freq = cmd.substring(1).toFloat();
            if (freq > 0) {
                Serial.printf("\n[CMD] Setting frequency: %.3f MHz\n", freq);
                pll_locked = synth.setFrequency(freq);
                
                if (pll_locked) {
                    const ADF4350Config_t& cfg = synth.getConfig();
                    Serial.printf("[CMD] OK! Actual: %.6f MHz\n", cfg.actual_freq);
                    digitalWrite(PIN_LED, HIGH);
                } else {
                    Serial.println("[CMD] FAILED!");
                    digitalWrite(PIN_LED, LOW);
                }
                
                display_show_frequency(freq, pll_locked);
            }
            break;
        }
        
        case 'l':
        case 'L': {
            pll_locked = synth.isLocked();
            if (pll_locked) {
                Serial.println("\n[CMD] PLL: LOCKED ✓");
                digitalWrite(PIN_LED, HIGH);
            } else {
                Serial.println("\n[CMD] PLL: NOT LOCKED ✗");
                digitalWrite(PIN_LED, LOW);
            }
            display_show_frequency(freq_table[current_freq_index], pll_locked);
            break;
        }
        
        case 'b':
        case 'B': {
            read_battery();
            Serial.printf("\n[CMD] Battery: %.2f V (ADC: %d)\n", 
                          battery_voltage, analogRead(PIN_ADC_BAT));
            display_show_frequency(freq_table[current_freq_index], pll_locked);
            break;
        }
        
        case 'r':
        case 'R': {
            const ADF4350Config_t& cfg = synth.getConfig();
            Serial.println("\n[CMD] Registers:");
            for (int i = 0; i < 6; i++) {
                Serial.printf("  Reg%d: 0x%08X\n", i, cfg.reg[i]);
            }
            break;
        }
        
        default:
            Serial.printf("\n[CMD] Unknown: '%c'\n", command);
            break;
    }
}

// ============================================
// LOOP
// ============================================
void loop() {
    // Обрабатываем команды из Serial
    handleSerialCommand();
    
    // Обрабатываем кнопки
    handle_buttons();
    
    // Периодически обновляем статус
    static unsigned long last_update = 0;
    if (millis() - last_update > 2000) {
        last_update = millis();
        
        // Проверяем lock
        pll_locked = synth.isLocked();
        digitalWrite(PIN_LED, pll_locked ? HIGH : LOW);
        
        // Читаем батарею
        read_battery();
        
        // Обновляем дисплей
        display_show_frequency(freq_table[current_freq_index], pll_locked);
        
        if (!pll_locked) {
            Serial.println("[WARN] PLL lost lock!");
        }
    }
    
    delay(10);
}
