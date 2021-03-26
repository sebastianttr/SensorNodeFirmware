#ifndef Sensor_h
#define Sensor_h

#include <Arduino.h>
#include <stdio.h>
#include <string.h>
#include <Wire.h>
#include "driver/i2c.h"
#include "sdkconfig.h"
#include <vl53l0x.h>

extern "C"
{
    // Set up I2C and create the vl53l0x structure, NULL means could not see device on I2C
    vl53l0x_t *vl53l0x_config(int8_t port, int8_t scl, int8_t sda, int8_t xshut, uint8_t address, uint8_t io_2v8);
    // Initialise the VL53L0X
    const char *vl53l0x_init(vl53l0x_t *);
    uint16_t vl53l0x_readRangeContinuousMillimeters(vl53l0x_t *);
    uint16_t vl53l0x_readRangeSingleMillimeters(vl53l0x_t *);
    void vl53l0x_setTimeout(vl53l0x_t *, uint16_t timeout);
}

class Sensor
{
private:
    uint16_t dig_T1;
    int16_t dig_T2;
    int16_t dig_T3;
    uint16_t dig_P1;
    int16_t dig_P2;
    int16_t dig_P3;
    int16_t dig_P4;
    int16_t dig_P5;
    int16_t dig_P6;
    int16_t dig_P7;
    int16_t dig_P8;
    int16_t dig_P9;
    uint16_t dig[12];
    int32_t t_fine;

    uint8_t writeReg(uint8_t sensor_addr, uint8_t reg, uint8_t data);
    uint8_t writeRegMulti(uint8_t sensor_addr, uint8_t reg, uint8_t *data);
    esp_err_t Done(uint8_t sensor_addr, i2c_cmd_handle_t i);
    i2c_cmd_handle_t Read(uint8_t sensor_addr, uint8_t reg);
    void readReg8Bit(uint8_t sensor_addr, uint8_t reg, uint8_t data[]);
    void readReg16Bit(int8_t sensor_addr, uint8_t reg, uint8_t data[]);
    void readReg32Bit(int8_t sensor_addr, uint8_t reg, uint8_t data[]);
    uint16_t GetDeltaHighLow(const char *dir);
    void readCoefficients();

    vl53l0x_t *vl53l0x__CONF;

public:
    uint8_t initSensors();

    /*BMP280 / BME280*/
    float getTemperature();
    float getPressure();
    float getHumidity();
    float getAltitude();

    /*MPU6500 Acceleromter*/
    float getAccelX_G();
    float getAccelY_G();
    float getAccelZ_G();

    /*MPU6500 Gyroscope*/
    float getGyroX_rads();
    float getGyroY_rads();
    float getGyroZ_rads();

    /*VL53L0X Time of Flight Sensor*/
    uint16_t getDistance_cm();
    float getDistance_m();
    uint16_t getDistance_mm();
};

#endif
