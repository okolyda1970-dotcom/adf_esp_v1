/**
 * @file adf4350.cpp
 * @brief Реализация драйвера ADF4350
 */

#include "adf4350.h"

ADF4350::ADF4350() {
    _refFreq = ADF4350_REF_FREQ_MHZ;
    _rCounter = ADF4350_R_COUNTER;
    _pfdFreq = 0;
    memset(&_config, 0, sizeof(_config));
}

void ADF4350::begin() {
    // Настройка пинов
    pinMode(ADF4350_PIN_LE, OUTPUT);
    pinMode(ADF4350_PIN_CE, OUTPUT);
    pinMode(ADF4350_PIN_MUXOUT, INPUT_PULLUP);
    
    // Начальные состояния
    digitalWrite(ADF4350_PIN_LE, HIGH);   // LE неактивен
    digitalWrite(ADF4350_PIN_CE, HIGH);   // CE активен (чип включён)
    
    // Инициализация SPI
    SPI.begin(ADF4350_PIN_SCLK, -1, ADF4350_PIN_MOSI, ADF4350_PIN_LE);
    SPI.setFrequency(10000000);  // 10 МГц (ADF4350 макс 20 МГц)
    SPI.setBitOrder(MSBFIRST);
    SPI.setDataMode(SPI_MODE0);  // CPOL=0, CPHA=0
    
    delay(100);  // Ждём стабилизации питания
    
    // Рассчитываем PFD частоту
    _calculatePFD();
    
    // Инициализируем регистры по умолчанию
    // Reg 5 — должен быть записан первым!
    _config.reg[5] = (5 << 29) | (1 << 22) | (1 << 19) | ADF4350_REG5;
    // LD pin mode = digital lock detect (бит 22)
    // Muxout = digital lock detect (биты 20:19 = 01)
    
    // Reg 4 — RF output, VCO
    _config.reg[4] = (4 << 29) |
                     (1 << 23) |    // Feedback select = fundamental
                     (0 << 11) |    // VCO power-down = 0 (включён)
                     (1 << 10) |    // Mute till lock detect = 1
                     (0 << 8)  |    // Aux output disable = 0
                     (1 << 5)  |    // RF output enable = 1
                     (3 << 3)  |    // RF output power = +5 dBm
                     ADF4350_REG4;
    
    // Reg 3 — CSR, phase
    _config.reg[3] = (3 << 29) |
                     (1 << 18) |    // Cycle slip reduction = 1
                     ADF4350_REG3;
    
    // Reg 2 — контроль петли
    _config.reg[2] = (2 << 29) |
                     (0 << 27) |    // Power-down = 0
                     (0 << 26) |    // Counter reset = 0
                     (0 << 25) |    // Charge pump three-state = 0
                     (0 << 24) |    // Power-down 2 = 0
                     (1 << 8)  |    // LDF = 1 (FRAC-N)
                     (0 << 9)  |    // LDP = 0 (6 ns)
                     (0 << 13) |    // CP gain = 2.5 mA (00)
                     (_rCounter << 14) |  // R counter
                     ADF4350_REG2;
    
    // Reg 1 — MOD, phase
    _config.reg[1] = (1 << 31) |    // Запись регистра 1
                     (1 << 27) |    // Prescaler = 8/9
                     (1 << 15) |    // Phase = 1
                     (4095 << 3) |  // MOD = 4095 (по умолчанию)
                     ADF4350_REG1;
    
    // Reg 0 — INT, FRAC
    _config.reg[0] = (0 << 3) |     // FRAC = 0
                     (75 << 15) |   // INT = 75 (пример)
                     ADF4350_REG0;
    
    // Записываем все регистры
    writeAllRegisters();
    
    Serial.println("[ADF4350] Initialized");
    Serial.printf("[ADF4350] PFD = %.3f MHz\n", _pfdFreq);
}

void ADF4350::_calculatePFD() {
    float fRef = _refFreq;
    if (ADF4350_REF_DOUBLER) fRef *= 2.0;
    if (ADF4350_REF_DIV2) fRef /= 2.0;
    _pfdFreq = fRef / _rCounter;
}

void ADF4350::writeRegister(uint32_t data) {
    // LE LOW — начало передачи
    digitalWrite(ADF4350_PIN_LE, LOW);
    delayMicroseconds(1);
    
    // Передаём 32 бита, MSB first
    SPI.transfer((data >> 24) & 0xFF);
    SPI.transfer((data >> 16) & 0xFF);
    SPI.transfer((data >> 8) & 0xFF);
    SPI.transfer(data & 0xFF);
    
    delayMicroseconds(1);
    // LE HIGH — защёлкиваем данные
    digitalWrite(ADF4350_PIN_LE, HIGH);
    delayMicroseconds(5);  // Минимальное время LE high = 5 нс, но берём с запасом
}

void ADF4350::writeAllRegisters() {
    // Порядок записи: 5, 4, 3, 2, 1, 0
    for (int i = 5; i >= 0; i--) {
        writeRegister(_config.reg[i]);
        delayMicroseconds(100);
    }
}

bool ADF4350::calculateFrequency(float freq_mhz, ADF4350Config_t& config) {
    // Проверка диапазона
    if (freq_mhz < 34.6 || freq_mhz > 4400.0) {
        Serial.printf("[ADF4350] ERROR: Frequency %.3f MHz out of range (34.6-4400)\n", freq_mhz);
        return false;
    }
    
    // Рассчитываем N = freq / PFD
    float n_total = freq_mhz / _pfdFreq;
    
    // Целая часть
    uint16_t int_val = (uint16_t)n_total;
    
    // Проверка INT
    if (int_val < 23 || int_val > 65535) {
        Serial.printf("[ADF4350] ERROR: INT value %d out of range\n", int_val);
        return false;
    }
    
    // Дробная часть
    float frac_part = n_total - int_val;
    
    // MOD = 4095 (максимальный для лучшего разрешения)
    uint16_t mod_val = 4095;
    
    // FRAC
    uint16_t frac_val = (uint16_t)(frac_part * mod_val + 0.5);
    
    // Проверка FRAC
    if (frac_val >= mod_val) {
        frac_val = 0;
        int_val++;
    }
    
    // Реальная частота
    float actual = ((float)int_val + (float)frac_val / (float)mod_val) * _pfdFreq;
    
    // Заполняем структуру
    config.int_val = int_val;
    config.frac_val = frac_val;
    config.mod_val = mod_val;
    config.pfd_freq = _pfdFreq;
    config.actual_freq = actual;
    
    // Рассчитываем регистры
    // Reg 0: [INT(16)][FRAC(12)][00]
    config.reg[0] = ((uint32_t)int_val << 15) | ((uint32_t)frac_val << 3) | ADF4350_REG0;
    
    // Reg 1: [1][000][Prescaler=1][0][Phase(12)][000][MOD(12)][001]
    config.reg[1] = (1UL << 31) | (1UL << 27) | (1UL << 15) | 
                    ((uint32_t)mod_val << 3) | ADF4350_REG1;
    
    // Reg 2-5 остаются как при инициализации
    config.reg[2] = _config.reg[2];
    config.reg[3] = _config.reg[3];
    config.reg[4] = _config.reg[4];
    config.reg[5] = _config.reg[5];
    
    Serial.printf("[ADF4350] Calculated: INT=%d FRAC=%d MOD=%d\n", int_val, frac_val, mod_val);
    Serial.printf("[ADF4350] Actual freq: %.6f MHz (error: %.3f kHz)\n", 
                  actual, (actual - freq_mhz) * 1000.0);
    
    return true;
}

bool ADF4350::setFrequency(float freq_mhz) {
    ADF4350Config_t newConfig;
    
    if (!calculateFrequency(freq_mhz, newConfig)) {
        return false;
    }
    
    // Сохраняем конфигурацию
    _config = newConfig;
    
    // Записываем Reg1 и Reg0 (для смены частоты достаточно этих двух)
    writeRegister(_config.reg[1]);
    delayMicroseconds(100);
    writeRegister(_config.reg[0]);
    
    // Ждём lock
    return waitForLock(5000);
}

void ADF4350::updateFrequency(uint32_t reg0, uint32_t reg1) {
    writeRegister(reg1);
    delayMicroseconds(100);
    writeRegister(reg0);
}

bool ADF4350::isLocked() {
    return digitalRead(ADF4350_PIN_MUXOUT) == HIGH;
}

bool ADF4350::waitForLock(uint32_t timeout_ms) {
    unsigned long start = millis();
    
    while ((millis() - start) < timeout_ms) {
        if (isLocked()) {
            Serial.println("[ADF4350] PLL LOCKED!");
            return true;
        }
        delay(10);
    }
    
    Serial.println("[ADF4350] Lock TIMEOUT!");
    return false;
}

void ADF4350::setRFOutput(bool enable) {
    if (enable) {
        _config.reg[4] |= (1 << 5);   // RF output enable
    } else {
        _config.reg[4] &= ~(1 << 5);  // RF output disable
    }
    writeRegister(_config.reg[4]);
}

void ADF4350::setRFPower(uint8_t power) {
    if (power > 3) power = 3;
    _config.reg[4] &= ~(0x03 << 3);       // Clear power bits
    _config.reg[4] |= ((power & 0x03) << 3); // Set new power
    writeRegister(_config.reg[4]);
}

void ADF4350::powerDown() {
    _config.reg[2] |= (1 << 27);  // Set PDN bit
    writeRegister(_config.reg[2]);
    Serial.println("[ADF4350] Power DOWN");
}

void ADF4350::powerUp() {
    _config.reg[2] &= ~(1 << 27);  // Clear PDN bit
    writeRegister(_config.reg[2]);
    Serial.println("[ADF4350] Power UP");
}
