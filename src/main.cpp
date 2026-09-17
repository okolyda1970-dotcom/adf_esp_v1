/**
 * ============================================================
 * ADF4350 Synthesizer Controller — ESP32-C3
 * ============================================================
 * 
 * Среда: PlatformIO + Arduino Framework
 * Плата: ESP32-C3-DevKitM-1
 * 
 * Подключение (по умолчанию, меняй в adf4350.h):
 *   ESP32-C3 GPIO7  → ADF4350 SDATA (MOSI)
 *   ESP32-C3 GPIO4  → ADF4350 SCLK
 *   ESP32-C3 GPIO5  → ADF4350 LE
 *   ESP32-C3 GPIO6  → ADF4350 CE
 *   ESP32-C3 GPIO3  ← ADF4350 MUXOUT
 *   
 *   3.3V → ADF4350 AVDD, DVDD
 *   GND  → ADF4350 GND
 * 
 * Опорный генератор: 25 МГц (настрой в adf4350.h)
 */

#include <Arduino.h>
#include "adf4350.h"

// Создаём объект синтезатора
ADF4350 synth;

// ============================================
// КОНФИГУРАЦИЯ
// ============================================

// Начальная частота (МГц)
#define START_FREQ_MHZ      435.0f

// Список частот для переключения (МГц)
const float freq_table[] = {
    144.0f,    // 2m диапазон
    145.5f,
    430.0f,    // 70cm диапазон
    435.0f,
    438.0f,
    915.0f,    // ISM
    1296.0f,   // 23cm
    2400.0f,   // 13cm / WiFi
};
#define FREQ_TABLE_SIZE (sizeof(freq_table) / sizeof(freq_table[0]))

// ============================================
// SETUP
// ============================================
void setup() {
    Serial.begin(115200);
    delay(2000);  // Ждём открытия монитора
    
    Serial.println();
    Serial.println("============================================");
    Serial.println("  ADF4350 Synthesizer — ESP32-C3");
    Serial.println("============================================");
    Serial.println();
    
    // Инициализация синтезатора
    synth.begin();
    
    // Устанавливаем начальную частоту
    Serial.printf("\n[MAIN] Setting frequency to %.3f MHz...\n", START_FREQ_MHZ);
    
    if (synth.setFrequency(START_FREQ_MHZ)) {
        Serial.println("[MAIN] Frequency set successfully!");
    } else {
        Serial.println("[MAIN] WARNING: PLL did not lock!");
    }
    
    // Выводим текущую конфигурацию
    const ADF4350Config_t& cfg = synth.getConfig();
    Serial.println("\n--- Current Configuration ---");
    Serial.printf("  PFD:       %.3f MHz\n", cfg.pfd_freq);
    Serial.printf("  INT:       %d\n", cfg.int_val);
    Serial.printf("  FRAC:      %d\n", cfg.frac_val);
    Serial.printf("  MOD:       %d\n", cfg.mod_val);
    Serial.printf("  Actual:    %.6f MHz\n", cfg.actual_freq);
    Serial.printf("  Reg0:      0x%08X\n", cfg.reg[0]);
    Serial.printf("  Reg1:      0x%08X\n", cfg.reg[1]);
    Serial.printf("  Reg2:      0x%08X\n", cfg.reg[2]);
    Serial.printf("  Reg3:      0x%08X\n", cfg.reg[3]);
    Serial.printf("  Reg4:      0x%08X\n", cfg.reg[4]);
    Serial.printf("  Reg5:      0x%08X\n", cfg.reg[5]);
    Serial.println("-----------------------------\n");
    
    Serial.println("[MAIN] Commands via Serial:");
    Serial.println("  f<freq>  — set frequency (e.g. 'f435.0')");
    Serial.println("  s        — sweep through frequency table");
    Serial.println("  l        — check lock status");
    Serial.println("  p        — toggle power");
    Serial.println("  r        — show registers");
    Serial.println("  +        — power up");
    Serial.println("  -        — power down");
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
            // Установка частоты: f435.0
            float freq = cmd.substring(1).toFloat();
            if (freq > 0) {
                Serial.printf("\n[CMD] Setting frequency: %.3f MHz\n", freq);
                if (synth.setFrequency(freq)) {
                    const ADF4350Config_t& cfg = synth.getConfig();
                    Serial.printf("[CMD] OK! Actual: %.6f MHz (error: %.1f Hz)\n", 
                                  cfg.actual_freq, (cfg.actual_freq - freq) * 1e6);
                } else {
                    Serial.println("[CMD] FAILED!");
                }
            }
            break;
        }
        
        case 's':
        case 'S': {
            // Sweep по таблице частот
            Serial.println("\n[CMD] Frequency sweep:");
            for (int i = 0; i < FREQ_TABLE_SIZE; i++) {
                Serial.printf("  [%d] %.1f MHz ... ", i, freq_table[i]);
                if (synth.setFrequency(freq_table[i])) {
                    Serial.println("LOCKED");
                } else {
                    Serial.println("FAIL");
                }
                delay(500);
            }
            // Возвращаемся на начальную частоту
            synth.setFrequency(START_FREQ_MHZ);
            Serial.println("[CMD] Sweep done.");
            break;
        }
        
        case 'l':
        case 'L': {
            // Проверка lock
            if (synth.isLocked()) {
                Serial.println("\n[CMD] PLL: LOCKED ✓");
            } else {
                Serial.println("\n[CMD] PLL: NOT LOCKED ✗");
            }
            break;
        }
        
        case 'p':
        case 'P': {
            // Toggle power
            static bool powered = true;
            powered = !powered;
            if (powered) {
                synth.powerUp();
            } else {
                synth.powerDown();
            }
            break;
        }
        
        case 'r':
        case 'R': {
            // Показать регистры
            const ADF4350Config_t& cfg = synth.getConfig();
            Serial.println("\n[CMD] Registers:");
            for (int i = 0; i < 6; i++) {
                Serial.printf("  Reg%d: 0x%08X  (bin: ", i, cfg.reg[i]);
                for (int b = 31; b >= 0; b--) {
                    Serial.print((cfg.reg[i] >> b) & 1);
                    if (b % 8 == 0 && b > 0) Serial.print(" ");
                }
                Serial.println(")");
            }
            break;
        }
        
        case '+':
            synth.powerUp();
            break;
            
        case '-':
            synth.powerDown();
            break;
        
        default:
            Serial.printf("\n[CMD] Unknown command: '%c'\n", command);
            break;
    }
}

// ============================================
// LOOP
// ============================================
void loop() {
    // Обрабатываем команды из Serial
    handleSerialCommand();
    
    // Периодически проверяем lock
    static unsigned long lastCheck = 0;
    if (millis() - lastCheck > 10000) {
        lastCheck = millis();
        if (!synth.isLocked()) {
            Serial.println("[WARN] PLL lost lock!");
        }
    }
    
    delay(10);
}
