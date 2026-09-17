/**
 * @file adf4350.h
 * @brief Драйвер синтезатора ADF4350 для ESP32-C3
 * 
 * SPI интерфейс: CPOL=0, CPHA=0, MSB first, макс 20 МГц
 * Логика: 3.3V (совместимо с ESP32-C3)
 */

#ifndef ADF4350_H
#define ADF4350_H

#include <Arduino.h>
#include <SPI.h>

// ============================================
// РАСПИНОВКА — ИЗМЕНИ ПОД СВОЮ СХЕМУ
// ============================================
#define ADF4350_PIN_MOSI    7    // SDATA — данные от ESP32 к ADF4350
#define ADF4350_PIN_SCLK    4    // SCLK  — тактовый сигнал
#define ADF4350_PIN_LE      5    // LE    — защёлка (Load Enable), активный LOW
#define ADF4350_PIN_CE      6    // CE    — включение чипа, активный HIGH
#define ADF4350_PIN_MUXOUT  3    // MUXOUT — выход (Lock Detect)

// ============================================
// ПАРАМЕТРЫ СИНТЕЗАТОРА
// ============================================
#define ADF4350_REF_FREQ_MHZ    25.0    // Опорная частота (МГц)
#define ADF4350_R_COUNTER       1       // Делитель опорной частоты
#define ADF4350_REF_DOUBLER     false   // Удвоитель опорной
#define ADF4350_REF_DIV2        false   // Делитель опорной /2

// ============================================
// АДРЕСА РЕГИСТРОВ (биты [2:0])
// ============================================
#define ADF4350_REG0  0
#define ADF4350_REG1  1
#define ADF4350_REG2  2
#define ADF4350_REG3  3
#define ADF4350_REG4  4
#define ADF4350_REG5  5

// ============================================
// СТРУКТУРА КОНФИГУРАЦИИ
// ============================================
typedef struct {
    uint32_t reg[6];      // Значения всех 6 регистров
    uint16_t int_val;     // Целая часть делителя N
    uint16_t frac_val;    // Дробная часть делителя
    uint16_t mod_val;     // Модуль дробного делителя
    float    pfd_freq;    // Частота PFD (МГц)
    float    actual_freq; // Реальная выходная частота (МГц)
} ADF4350Config_t;


class ADF4350 {
public:
    ADF4350();
    
    /**
     * @brief Инициализация SPI и пинов
     */
    void begin();
    
    /**
     * @brief Записать один регистр
     * @param data 32-битное значение регистра
     */
    void writeRegister(uint32_t data);
    
    /**
     * @brief Записать все 6 регистров в правильном порядке (5->0)
     */
    void writeAllRegisters();
    
    /**
     * @brief Рассчитать регистры для заданной частоты
     * @param freq_mhz Желаемая выходная частота (МГц)
     * @param config Структура для сохранения результатов
     * @return true если расчёт успешен
     */
    bool calculateFrequency(float freq_mhz, ADF4350Config_t& config);
    
    /**
     * @brief Установить частоту (рассчитать и записать регистры)
     * @param freq_mhz Желаемая выходная частота (МГц)
     * @return true если PLL залочился
     */
    bool setFrequency(float freq_mhz);
    
    /**
     * @brief Обновить только Reg0 и Reg1 (быстрая смена частоты)
     * @param reg0 Новое значение регистра 0
     * @param reg1 Новое значение регистра 1
     */
    void updateFrequency(uint32_t reg0, uint32_t reg1);
    
    /**
     * @brief Проверить статус PLL lock
     * @return true если залочен
     */
    bool isLocked();
    
    /**
     * @brief Ожидание блокировки PLL
     * @param timeout_ms Таймаут в миллисекундах
     * @return true если залочился до таймаута
     */
    bool waitForLock(uint32_t timeout_ms = 5000);
    
    /**
     * @brief Включить/выключить RF выход
     * @param enable true — включить
     */
    void setRFOutput(bool enable);
    
    /**
     * @brief Установить мощность RF выхода
     * @param power 0=-4dBm, 1=-1dBm, 2=+2dBm, 3=+5dBm
     */
    void setRFPower(uint8_t power);
    
    /**
     * @brief Перевести в режим power-down
     */
    void powerDown();
    
    /**
     * @brief Выйти из power-down
     */
    void powerUp();
    
    /**
     * @brief Получить текущую конфигурацию
     */
    const ADF4350Config_t& getConfig() const { return _config; }

private:
    ADF4350Config_t _config;
    float _refFreq;
    float _pfdFreq;
    uint16_t _rCounter;
    
    void _calculatePFD();
    void _updateReg0(uint16_t int_val, uint16_t frac_val);
    void _updateReg1(uint16_t mod_val);
};

#endif // ADF4350_H
