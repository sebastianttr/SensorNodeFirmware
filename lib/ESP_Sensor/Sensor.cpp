#include "Sensor.h"

#include <Arduino.h>
#include <stdio.h>
#include <string.h>
#include "driver/i2c.h"
#include "sdkconfig.h"
#include <vl53l0x.h>

#define I2C_PORT I2C_NUM_0
#define PIN_SDA GPIO_NUM_21
#define PIN_SCL GPIO_NUM_22

#define ACK_CHECK_EN 0x1           /*!< I2C master will check ack from slave*/
#define ACK_CHECK_DIS 0x0          /*!< I2C master will not check ack from slave */
#define ACK_VAL 0x0                /*!< I2C ack value */
#define NACK_VAL 0x1               /*!< I2C nack value */
#define WRITE_BIT I2C_MASTER_WRITE /*!< I2C master write */
#define READ_BIT I2C_MASTER_READ   /*!< I2C master read */

#define MPU6050_REG_POWER 0x6B
#define MPU6050_REG_ACCEL_CONFIG 0x1C
#define MPU6050_REG_GYRO_CONFIG 0x1B
#define MPU_SLAVE_ADDR 0x68
#define MPU_ACC_X_HIGH 0x3B
#define MPU_ACC_X_LOW 0x3C
#define MPU_ACC_Y_HIGH 0x3D
#define MPU_ACC_Y_LOW 0x3E
#define MPU_ACC_Z_HIGH 0x3F
#define MPU_ACC_Z_LOW 0x40
#define MPU_Wake 0x6B
#define MPU_Full_Scale 0x1C

#define BME_SLAVE_ADDR 0x76
#define BME_CONTROL_ADDR 0xF4
#define BME_CONFIG_ADDR 0xF5
#define BME_TEMP_H 0xFA
#define BME_TEMP_L 0xFB
#define BME_PRES_H 0xF7
#define BME_PRES_L 0xF8
#define BME_REGISTER_DIG_T1 0x88
#define BME_REGISTER_DIG_T2 0x8A
#define BME_REGISTER_DIG_T3 0x8C
#define BME_REGISTER_DIG_P1 0x8E
#define BME_REGISTER_DIG_P2 0x90
#define BME_REGISTER_DIG_P3 0x92
#define BME_REGISTER_DIG_P4 0x94
#define BME_REGISTER_DIG_P5 0x96
#define BME_REGISTER_DIG_P6 0x98
#define BME_REGISTER_DIG_P7 0x8E
#define BME_REGISTER_DIG_P8 0x8E
#define BME_REGISTER_DIG_P9 0x8E
#define BME_REGISTER_TEMPDATA 0xFA
#define BME_REGISTER_SOFTRESET 0xE0

#define checkTimeoutExpired() (0)
#define startTimeout() (VL53L0X.timeout_start_ms = 0)

uint8_t Sensor::writeReg(uint8_t sensor_addr, uint8_t reg, uint8_t data)
{
    int ret;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, sensor_addr << 1 | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, reg, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, data, ACK_CHECK_EN);
    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    return ret;
}

uint8_t Sensor::writeRegMulti(uint8_t sensor_addr, uint8_t reg, uint8_t *data)
{
    int ret;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, sensor_addr << 1 | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, reg, ACK_CHECK_EN);
    for (uint8_t i = 0; i < sizeof(reg); i++)
    {
        i2c_master_write_byte(cmd, data[i], ACK_CHECK_EN);
    }
    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    return ret;
}

esp_err_t Sensor::Done(uint8_t sensor_addr, i2c_cmd_handle_t i)
{
    i2c_master_stop(i);
    esp_err_t err = i2c_master_cmd_begin(I2C_NUM_0, i, 1000 / portTICK_RATE_MS);
    if (err)
        Serial.println("I2C Done Error!");
    i2c_cmd_link_delete(i);
    return err;
}

i2c_cmd_handle_t Sensor::Read(uint8_t sensor_addr, uint8_t reg)
{ // Set up for read
    i2c_cmd_handle_t i = i2c_cmd_link_create();
    i2c_master_start(i);
    i2c_master_write_byte(i, (sensor_addr << 1), 1);
    i2c_master_write_byte(i, reg, 1);
    Done(sensor_addr, i);
    i = i2c_cmd_link_create();
    i2c_master_start(i);
    i2c_master_write_byte(i, (sensor_addr << 1) + 1, 1);
    return i;
}

void Sensor::readReg8Bit(uint8_t sensor_addr, uint8_t reg, uint8_t data[])
{ // Set up for read
    i2c_cmd_handle_t i = Read(sensor_addr, reg);
    i2c_master_read_byte(i, data + 0, I2C_MASTER_LAST_NACK);
    Done(sensor_addr, i);
}

void Sensor::readReg16Bit(int8_t sensor_addr, uint8_t reg, uint8_t data[])
{
    i2c_cmd_handle_t i = Read(sensor_addr, reg);
    i2c_master_read_byte(i, data + 0, I2C_MASTER_ACK);
    i2c_master_read_byte(i, data + 1, I2C_MASTER_NACK);
    Done(sensor_addr, i);
}

void Sensor::readReg32Bit(int8_t sensor_addr, uint8_t reg, uint8_t data[])
{
    i2c_cmd_handle_t i = Read(sensor_addr, reg);
    i2c_master_read_byte(i, data + 0, I2C_MASTER_ACK);
    i2c_master_read_byte(i, data + 1, I2C_MASTER_ACK);
    i2c_master_read_byte(i, data + 2, I2C_MASTER_ACK);
    i2c_master_read_byte(i, data + 3, I2C_MASTER_NACK);
    Done(sensor_addr, i);
}

uint8_t
Sensor::initSensors()
{
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = GPIO_NUM_21;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_io_num = GPIO_NUM_22;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = 400000UL;
    i2c_param_config(I2C_NUM_0, &conf);
    i2c_driver_install(I2C_NUM_0, conf.mode, 0, 0, 0);

    writeReg(MPU_SLAVE_ADDR, MPU_Wake, 0);                // wake up the mpu6500
    writeReg(MPU_SLAVE_ADDR, MPU_Full_Scale, 0b00010000); // digit 4-5 = 10 = 16

    readCoefficients();
    writeReg(BME_SLAVE_ADDR, BME_CONTROL_ADDR, 0x3F);

    vl53l0x__CONF = vl53l0x_config(I2C_NUM_0, GPIO_NUM_22, GPIO_NUM_21, 0, 0x29, 0);
    vl53l0x_setTimeout(vl53l0x__CONF, 500);
    Serial.println(vl53l0x_init(vl53l0x__CONF));

    //LONG RANGE MODE SETTINGS
    vl53l0x_setSignalRateLimit(vl53l0x__CONF, 0.1f);
    // increase laser pulse periods (defaults are 14 and 10 PCLKs)
    vl53l0x_setVcselPulsePeriod(vl53l0x__CONF, vl53l0x_vcselPeriodType::VcselPeriodPreRange, 18);
    vl53l0x_setVcselPulsePeriod(vl53l0x__CONF, vl53l0x_vcselPeriodType::VcselPeriodFinalRange, 14);

    return 1;
}

void Sensor::readCoefficients()
{
    //read the coefficients before proceeding
    uint8_t *buf_val = (uint8_t *)malloc(16);
    uint16_t value;

    readReg16Bit(BME_SLAVE_ADDR, BME_REGISTER_DIG_T1, buf_val);
    value = (buf_val[0] << 8) | buf_val[1];
    dig[0] = (value >> 8) | (value << 8);

    free(buf_val);

    for (int i = 1; i < 3; i++)
    {
        static uint8_t reg = 0x8A;
        buf_val = (uint8_t *)malloc(16);
        readReg16Bit(BME_SLAVE_ADDR, reg, buf_val);
        value = (buf_val[0] << 8) | buf_val[1];
        dig[i] = value;
        reg += 0x02;
        free(buf_val);
    }

    buf_val = (uint8_t *)malloc(16);
    readReg16Bit(BME_SLAVE_ADDR, BME_REGISTER_DIG_P1, buf_val);
    value = (buf_val[0] << 8) | buf_val[1];
    dig[3] = (value >> 8) | (value << 8);
    free(buf_val);

    for (int j = 4; j < 12; j++)
    {
        static uint8_t reg = 0x90;
        buf_val = (uint8_t *)malloc(14);
        readReg16Bit(BME_SLAVE_ADDR, reg, buf_val);
        value = (buf_val[0] << 8) | buf_val[1];
        dig[j] = value;
        reg += 0x02;
        free(buf_val);
    }

    dig_T1 = dig[0];
    dig_T2 = dig[1];
    dig_T3 = dig[2];
    dig_P1 = dig[3];
    dig_P2 = dig[4];
    dig_P3 = dig[5];
    dig_P4 = dig[6];
    dig_P5 = dig[7];
    dig_P6 = dig[8];
    dig_P7 = dig[9];
    dig_P8 = dig[10];
    dig_P9 = dig[11];
}

uint16_t
Sensor::GetDeltaHighLow(const char *dir)
{
    uint8_t *val_h = (uint8_t *)malloc(14);
    uint8_t *val_l = (uint8_t *)malloc(14);
    if (strcmp(dir, "AX") == 0)
    {
        readReg16Bit(MPU_SLAVE_ADDR, MPU_ACC_X_HIGH, val_h);
        readReg16Bit(MPU_SLAVE_ADDR, MPU_ACC_X_LOW, val_l);
    }
    else if (strcmp(dir, "AY") == 0)
    {
        readReg16Bit(MPU_SLAVE_ADDR, MPU_ACC_Y_HIGH, val_h);
        readReg16Bit(MPU_SLAVE_ADDR, MPU_ACC_Y_LOW, val_l);
    }
    else if (strcmp(dir, "AZ") == 0)
    {
        readReg16Bit(MPU_SLAVE_ADDR, MPU_ACC_Z_HIGH, val_h);
        readReg16Bit(MPU_SLAVE_ADDR, MPU_ACC_Z_LOW, val_l);
    }
    float val = (val_h[0] << 8) | val_l[0];
    free(val_h);
    free(val_l);
    return val;
}

float Sensor::getAccelX_G()
{
    float val = GetDeltaHighLow("AX");
    float gVal = (16000.00f / 65535.00f) * ((val < 32768) ? val : val - 65356) / 1000;
    return gVal * 9.81f;
}

float Sensor::getAccelY_G()
{
    float val = GetDeltaHighLow("AY");
    float gVal = (16000.00f / 65535.00f) * ((val < 32768) ? val : val - 65356) / 1000;
    return gVal * 9.81f;
}
float Sensor::getAccelZ_G()
{
    float val = GetDeltaHighLow("AZ");
    float gVal = (16000.00f / 65535.00f) * ((val < 32768) ? val : val - 65356) / 1000;
    return gVal * 9.81f;
}

float Sensor::getTemperature()
{
    int32_t var1, var2;
    uint8_t *buf_val = (uint8_t *)malloc(32);
    readReg32Bit(BME_SLAVE_ADDR, BME_REGISTER_TEMPDATA, buf_val);
    int32_t value = (int32_t)buf_val[0];
    value <<= 8;
    value |= (int32_t)buf_val[1];
    value <<= 8;
    value |= (int32_t)buf_val[2];
    value >>= 4;

    var1 = ((((value >> 3) - ((int32_t)dig_T1 << 1))) *
            ((int32_t)dig_T2)) >>
           11;

    var2 = (((((value >> 4) - ((int32_t)dig_T1)) *
              ((value >> 4) - ((int32_t)dig_T1))) >>
             12) *
            ((int32_t)dig_T3)) >>
           14;

    t_fine = var1 + var2;

    float T = -1 * (t_fine * 5 + 128) >> 8;
    free(buf_val);

    return T / 100;
}

float Sensor::getPressure()
{
    int64_t var1, var2, p;
    // Must be done first to get the t_fine variable set up
    getTemperature();

    uint8_t *buf_val = (uint8_t *)malloc(32);
    readReg32Bit(BME_SLAVE_ADDR, BME_REGISTER_TEMPDATA, buf_val);
    int32_t value = (int32_t)buf_val[0];
    value <<= 8;
    value |= (int32_t)buf_val[1];
    value <<= 8;
    value |= (int32_t)buf_val[2];
    value >>= 4;

    var1 = ((int64_t)t_fine) - 128000;
    var2 = var1 * var1 * (int64_t)dig_P6;
    var2 = var2 + ((var1 * (int64_t)dig_P5) << 17);
    var2 = var2 + (((int64_t)dig_P4) << 35);
    var1 = ((var1 * var1 * (int64_t)dig_P3) >> 8) +
           ((var1 * (int64_t)dig_P2) << 12);
    var1 = (((((int64_t)1) << 47) + var1)) * ((int64_t)dig_P1) >> 33;

    if (var1 == 0)
    {
        return 0; // avoid exception caused by division by zero
    }
    p = 1048576 - value;
    p = (((p << 31) - var2) * 3125) / var1;
    var1 = (((int64_t)dig_P9) * (p >> 13) * (p >> 13)) >> 25;
    var2 = (((int64_t)dig_P8) * p) >> 19;

    p = ((p + var1 + var2) >> 8) + (((int64_t)dig_P7) << 4);
    return (float)p / 256;
}

uint16_t Sensor::getDistance_mm()
{
    return vl53l0x_readRangeSingleMillimeters(vl53l0x__CONF);
}

uint16_t Sensor::getDistance_cm()
{
    return (vl53l0x_readRangeSingleMillimeters(vl53l0x__CONF)) / 10;
}

float Sensor::getDistance_m()
{
    return ((float)vl53l0x_readRangeSingleMillimeters(vl53l0x__CONF)) / 1000.0f;
}