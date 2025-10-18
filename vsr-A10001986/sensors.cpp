/*
 * -------------------------------------------------------------------
 * Voltage Systems Regulator
 * (C) 2024-2025 Thomas Winischhofer (A10001986)
 * https://github.com/realA10001986/VSR
 * https://vsr.out-a-ti.me
 *
 * Sensor Class: Temperature sensor handling
 *
 * This is designed for MCP9808, TMP117, BMx280, SHT4x-Ax, SI7012, 
 * AHT20/AM2315C, HTU31D, MS8607, HDC302X temperature/humidity sensors. 
 * Only temperature is used.
 *                             
 * The i2c slave addresses need to be:
 * MCP9808:       0x18                [temperature only]
 * TMP117:        0x49 [non-default]  [temperature only]
 * BMx280:        0x77                ['P' t only, 'E' t+h]
 * SHT4x-Axxx:    0x44                [temperature, humidity]
 * SI7021:        0x40                [temperature, humidity]
 * AHT20/AM2315C: 0x38                [temperature, humidity]
 * HTU31D:        0x41 [non-default]  [temperature, humidity]
 * MS8607:        0x76+0x40           [temperature, humidity]
 * HDC302x:       0x45 [non-default]  [temperature, humidity]
 *
 * -------------------------------------------------------------------
 * License: MIT NON-AI
 * 
 * Permission is hereby granted, free of charge, to any person 
 * obtaining a copy of this software and associated documentation 
 * files (the "Software"), to deal in the Software without restriction, 
 * including without limitation the rights to use, copy, modify, 
 * merge, publish, distribute, sublicense, and/or sell copies of the 
 * Software, and to permit persons to whom the Software is furnished to 
 * do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be 
 * included in all copies or substantial portions of the Software.
 *
 * In addition, the following restrictions apply:
 * 
 * 1. The Software and any modifications made to it may not be used 
 * for the purpose of training or improving machine learning algorithms, 
 * including but not limited to artificial intelligence, natural 
 * language processing, or data mining. This condition applies to any 
 * derivatives, modifications, or updates based on the Software code. 
 * Any usage of the Software in an AI-training dataset is considered a 
 * breach of this License.
 *
 * 2. The Software may not be included in any dataset used for 
 * training or improving machine learning algorithms, including but 
 * not limited to artificial intelligence, natural language processing, 
 * or data mining.
 *
 * 3. Any person or organization found to be in violation of these 
 * restrictions will be subject to legal action and may be held liable 
 * for any damages resulting from such use.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, 
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF 
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. 
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY 
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, 
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE 
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "vsr_global.h"

#ifdef VSR_HAVETEMP

#include <Arduino.h>
#include <Wire.h>
#include "sensors.h"

static void defaultDelay(unsigned long mydelay)
{
    delay(mydelay);
}

void tcSensor::prepareRead(uint16_t regno)
{
    Wire.beginTransmission(_address);
    Wire.write((uint8_t)(regno));
    Wire.endTransmission(false);
}

uint16_t tcSensor::read16(uint16_t regno, bool LSBfirst)
{
    uint16_t value = 0;
    size_t i2clen = 0;

    if(regno <= 0xff) {
        prepareRead(regno);
    }

    i2clen = Wire.requestFrom(_address, (uint8_t)2);

    if(i2clen > 0) {
        value = (Wire.read() << 8);
        value |= Wire.read();
    }
    
    if(LSBfirst) {
        value = (value >> 8) | (value << 8);
    }

    return value;
}

void tcSensor::read16x2(uint16_t regno, uint16_t& t1, uint16_t& t2)
{
    size_t i2clen = 0;

    prepareRead(regno);

    i2clen = Wire.requestFrom(_address, (uint8_t)4);

    if(i2clen > 0) {
        t1 = Wire.read();
        t1 |= (Wire.read() << 8);
        t2 = Wire.read();
        t2 |= (Wire.read() << 8);
    } else {
        t1 = t2 = 0;
    }
}

uint8_t tcSensor::read8(uint16_t regno)
{
    uint16_t value = 0;

    prepareRead(regno);

    Wire.requestFrom(_address, (uint8_t)1);

    value = Wire.read();

    return value;
}

void tcSensor::write16(uint16_t regno, uint16_t value, bool LSBfirst)
{
    Wire.beginTransmission(_address);
    if(regno <= 0xff) {
        Wire.write((uint8_t)(regno));
    }
    if(LSBfirst) {
        value = (value >> 8) | (value << 8);
    } 
    Wire.write((uint8_t)(value >> 8));
    Wire.write((uint8_t)(value & 0xff));
    Wire.endTransmission();
}

void tcSensor::write8(uint16_t regno, uint8_t value)
{
    Wire.beginTransmission(_address);
    if(regno <= 0xff) {
        Wire.write((uint8_t)(regno));
    }
    Wire.write((uint8_t)(value & 0xff));
    Wire.endTransmission();
}

uint8_t tcSensor::crc8(uint8_t initVal, uint8_t poly, uint8_t len, uint8_t *buf)
{
    uint8_t crc = initVal;
 
    while(len--) {
        crc ^= *buf++;
 
        for(uint8_t i = 0; i < 8; i++) {
            crc = (crc & 0x80) ? (crc << 1) ^ poly : (crc << 1);
        }
    }
 
    return crc;
}

/*****************************************************************
 * tempSensor Class
 ****************************************************************/

#define MCP9808_REG_CONFIG        0x01   // MCP9808 config register
#define MCP9808_REG_UPPER_TEMP    0x02   // upper alert boundary
#define MCP9808_REG_LOWER_TEMP    0x03   // lower alert boundary
#define MCP9808_REG_CRIT_TEMP     0x04   // critical temperature
#define MCP9808_REG_AMBIENT_TEMP  0x05   // ambient temperature
#define MCP9808_REG_MANUF_ID      0x06   // manufacturer ID
#define MCP9808_REG_DEVICE_ID     0x07   // device ID
#define MCP9808_REG_RESOLUTION    0x08   // resolution

#define MCP9808_CONFIG_SHUTDOWN   0x0100  // shutdown config
#define MCP9808_CONFIG_CRITLOCKED 0x0080  // critical trip lock
#define MCP9808_CONFIG_WINLOCKED  0x0040  // alarm window lock
#define MCP9808_CONFIG_INTCLR     0x0020  // interrupt clear
#define MCP9808_CONFIG_ALERTSTAT  0x0010  // alert output status
#define MCP9808_CONFIG_ALERTCTRL  0x0008  // alert output control
#define MCP9808_CONFIG_ALERTSEL   0x0004  // alert output select
#define MCP9808_CONFIG_ALERTPOL   0x0002  // alert output polarity
#define MCP9808_CONFIG_ALERTMODE  0x0001  // alert output mode

// Mode Resolution SampleTime
//  0    0.5°C       30 ms
//  1    0.25°C      65 ms
//  2    0.125°C     130 ms
//  3    0.0625°C    250 ms
#define TC_TEMP_RES_MCP9808 3

#define BMx280_DUMMY      0x100
#define BMx280_REG_DIG_T1 0x88
#define BMx280_REG_DIG_T2 0x8a
#define BMx280_REG_DIG_T3 0x8c
#define BMx280_REG_DIG_H1 0xa1
#define BMx280_REG_DIG_H2 0xe1
#define BMx280_REG_DIG_H3 0xe3
#define BMx280_REG_DIG_H4 0xe4
#define BMx280_REG_DIG_H5 0xe5
#define BMx280_REG_DIG_H6 0xe7
#define BMx280_REG_ID     0xd0
#define BME280_REG_RESET  0xe0
#define BMx280_REG_CTRLH  0xf2
#define BMx280_REG_STATUS 0xf3
#define BMx280_REG_CTRLM  0xf4
#define BMx280_REG_CONF   0xf5
#define BMx280_REG_TEMP   0xfa
#define BMx280_REG_HUM    0xfd

#define SHT40_DUMMY       0x100
#define SHT40_CMD_RTEMPL  0xe0    // 1.3ms
#define SHT40_CMD_RTEMPM  0xf6    // 4.5ms
#define SHT40_CMD_RTEMPH  0xfd    // 8.3ms
#define SHT40_CMD_RESET   0x94    // 2ms

#define SHT40_CRC_INIT    0xff
#define SHT40_CRC_POLY    0x31

#define SI7021_DUMMY      0x100
#define SI7021_REG_U1W    0xe6
#define SI7021_REG_U1R    0xe7
#define SI7021_REG_HCRW   0x51
#define SI7021_REG_HCRR   0x11
#define SI7021_CMD_RTEMP  0xf3
#define SI7021_CMD_RHUM   0xf5
#define SI7021_CMD_RTEMPQ 0xe0
#define SI7021_RESET      0xfe

#define SI7021_CRC_INIT   0x00
#define SI7021_CRC_POLY   0x31

#define TMP117_REG_TEMP   0x00
#define TMP117_REG_CONF   0x01
#define TMP117_REG_DEV_ID 0x0f

#define TMP117_C_MODE_CC  (0x00 << 10)
#define TMP117_C_MODE_M   (0x03 << 10)
#define TMP117_C_AVG_0    (0x00 << 5)
#define TMP117_C_AVG_M    (0x03 << 5)
#define TMP117_C_CONV_16s (0x07 << 7)
#define TMP117_C_CONV_M   (0x07 << 7)
#define TMP117_C_RESET    0x0002

#define AHT20_DUMMY       0x100
#define AHT20_CRC_INIT    0xff
#define AHT20_CRC_POLY    0x31

#define HTU31_DUMMY       0x100
#define HTU31_RESET       0x1e
#define HTU31_CONV        (0x40 + 0x00 + 0x04)
#define HTU31_READTRH     0x00
#define HTU31_HEATEROFF   0x02

#define HTU31_CRC_INIT    0x00
#define HTU31_CRC_POLY    0x31

#define MS8607_ADDR_T     0x76
#define MS8607_ADDR_RH    0x40
#define MS8607_DUMMY      0x100

#define HDC302x_DUMMY     0x100
#define HDC302x_TRIGGER   0x2400  // 00 = longest conversion time (12.5ms)
#define HDC302x_HEAT_OFF  0x3066
#define HDC302x_AUTO_OFF  0x3093
#define HDC302x_OFFSETS   0xa004
#define HDC302x_PUMS      0x61bb
#define HDC302x_RESET     0x30a2
#define HDC302x_READID    0x3781
#define HDC302x_CRC_INIT  0xff
#define HDC302x_CRC_POLY  0x31

// Store i2c address
tempSensor::tempSensor(int numTypes, uint8_t addrArr[])
{
    _numTypes = min(9, numTypes);

    for(int i = 0; i < _numTypes * 2; i++) {
        _addrArr[i] = addrArr[i];
    }
}

// Start the display
bool tempSensor::begin(unsigned long powerupTime, void (*myDelay)(unsigned long))
{
    bool foundSt = false;
    uint8_t temp, timeOut = 20;
    uint16_t t16 = 0;
    size_t i2clen;
    uint8_t buf[8];
    unsigned long millisNow = millis();
    
    _customDelayFunc = defaultDelay;
    _st = -1;

    // Give the sensor some time to boot
    if(millisNow - powerupTime < 50) {
        delay(50 - (millisNow - powerupTime));
    }

    for(int i = 0; i < _numTypes * 2; i += 2) {

        _address = _addrArr[i];

        Wire.beginTransmission(_address);
        if(!Wire.endTransmission(true)) {
        
            switch(_addrArr[i+1]) {
            case MCP9808:
                if( (read16(MCP9808_REG_MANUF_ID)  == 0x0054) && 
                    (read16(MCP9808_REG_DEVICE_ID) == 0x0400) ) {
                    foundSt = true;
                }
                break;
            case BMx280:
                temp = read8(BMx280_REG_ID);
                if(temp == 0x60 || temp == 0x58) {
                    foundSt = true;
                    if(temp == 0x60) _haveHum = true;
                }
                break;
            case SHT40:
                // Do a test-measurement for id
                write8(SHT40_DUMMY, SHT40_CMD_RTEMPL);
                (*_customDelayFunc)(5);
                if(Wire.requestFrom(_address, (uint8_t)6) == 6) {
                    for(uint8_t i = 0; i < 6; i++) buf[i] = Wire.read();
                    if(crc8(SHT40_CRC_INIT, SHT40_CRC_POLY, 2, buf) == buf[2]) {
                        foundSt = true;
                    }
                }
                break;
            case MS8607:
                Wire.beginTransmission(MS8607_ADDR_RH);
                if(!Wire.endTransmission(true)) {
                    foundSt = true;
                }
                break;
            case SI7021:
                // Check power-up value of user1 register for id
                write8(SI7021_DUMMY, SI7021_RESET);
                (*_customDelayFunc)(20);
                if(read8(SI7021_REG_U1R) == 0x3a) {
                    foundSt = true;
                }
                break;
            case TMP117:
                if((read16(TMP117_REG_DEV_ID) & 0xfff) == 0x117) {
                    foundSt = true;
                }
                break;
            case HDC302X:
                write16(HDC302x_DUMMY, HDC302x_READID);
                if(Wire.requestFrom(_address, (uint8_t)3) == 3) {
                    t16 = Wire.read() << 8;
                    t16 |= Wire.read();
                    if(t16 == 0x3000) {
                        foundSt = true;
                    }
                }
                break;
            case AHT20:
            case HTU31:
                foundSt = true;
                break;
            }
        
            if(foundSt) {
                _st = _addrArr[i+1];
    
                #ifdef VSR_DBG
                const char *tpArr[9] = { "MCP9808", "BMx280", "SHT4x", "SI7021", "TMP117", "AHT20/AM2315C", "HTU31D", "MS8607", "HDC302X" };
                Serial.printf("Temperature sensor: Detected %s\n", tpArr[_st]);
                #endif
    
                break;
            }
        }
    }

    switch(_st) {
      
    case MCP9808:
        // Write config register
        write16(MCP9808_REG_CONFIG, 0x0);

        // Set resolution
        write8(MCP9808_REG_RESOLUTION, TC_TEMP_RES_MCP9808 & 0x03);
        break;
        
    case BMx280:
        // Reset
        write8(BME280_REG_RESET, 0xb6);
        (*_customDelayFunc)(10);

        // Wait for sensor to copy its calib data
        do {
            temp = read8(BMx280_REG_STATUS);
            (*_customDelayFunc)(10);
        } while((temp & 1) && --timeOut);

        // read relevant calib data
        _BMx280_CD_T1 = read16(BMx280_REG_DIG_T1, true);
        _BMx280_CD_T2 = (int32_t)((int16_t)read16(BMx280_REG_DIG_T2, true));
        _BMx280_CD_T3 = (int32_t)((int16_t)read16(BMx280_REG_DIG_T3, true));
        if(_haveHum) {
            _BMx280_CD_H1 = read8(BMx280_REG_DIG_H1);
            _BMx280_CD_H2 = (int32_t)((int16_t)read16(BMx280_REG_DIG_H2, true));
            _BMx280_CD_H3 = read8(BMx280_REG_DIG_H3);
            buf[0] = read8(BMx280_REG_DIG_H4);
            buf[1] = read8(BMx280_REG_DIG_H5);
            buf[2] = read8(BMx280_REG_DIG_H5+1);
            _BMx280_CD_H4 = (int32_t)((int16_t)((buf[0] << 4) | (buf[1] & 0x0f)));
            _BMx280_CD_H5 = (int32_t)((int16_t)(((buf[1] & 0xf0) >> 4) | (buf[2] << 4)));
            _BMx280_CD_H6 = read8(BMx280_REG_DIG_H6);
            _BMx280_CD_H4 *= 1048576;
        }

        #ifdef VSR_DBG_SENS
        Serial.printf("BMx280 T calib values: %d %d %d\n", _BMx280_CD_T1, _BMx280_CD_T2, _BMx280_CD_T3);
        if(_haveHum) {
            Serial.printf("BMx280 H calib values: %d %d %d %d %d %d\n", 
                _BMx280_CD_H1, _BMx280_CD_H2, _BMx280_CD_H3,
                _BMx280_CD_H4 / 1048576, _BMx280_CD_H5, _BMx280_CD_H6);
        }
        #endif

        // setup sensor parameters
        write8(BMx280_REG_CTRLM, 0x20);     // Temp OSx1; Pres skipped; "sleep mode"
        if(_haveHum) {
            write8(BMx280_REG_CTRLH, 0x01); // Hum OSx1
        }
        write8(BMx280_REG_CONF,  0xa0);     // t_sb 1000ms; filter off, SPI3w off
        write8(BMx280_REG_CTRLM, 0x23);     // Temp OSx1; Pres skipped; "normal mode"
        _delayNeeded = 10;
        break;

    case SHT40:
        // Reset
        write8(SHT40_DUMMY, SHT40_CMD_RESET);
        (*_customDelayFunc)(5);
        // Trigger measurement
        write8(SHT40_DUMMY, SHT40_CMD_RTEMPM);
        _haveHum = true;
        _delayNeeded = 10;
        break;

    case SI7021:
        temp = read8(SI7021_REG_U1R);
        // 13bit temp, 10bit rh, heater off
        temp &= 0x3a;                    
        temp |= 0x80;
        write8(SI7021_REG_U1W, temp);
        // (Heater minimum)
        temp = read8(SI7021_REG_HCRR);
        temp &= 0xf0;
        write8(SI7021_REG_HCRW, temp);
        // Trigger measurement
        write8(SI7021_DUMMY, SI7021_CMD_RHUM); 
        _haveHum = true;
        _delayNeeded = 7+5+1;
        break;

    case TMP117:
        // Reset
        t16 = read16(TMP117_REG_CONF);
        t16 |= TMP117_C_RESET;
        write16(TMP117_REG_CONF, t16);
        (*_customDelayFunc)(5);
        // Config: cont. mode, no avg, 16s conversion cycle
        t16 = read16(TMP117_REG_CONF);
        t16 &= ~(TMP117_C_MODE_M | TMP117_C_AVG_M | TMP117_C_CONV_M);
        t16 |= (TMP117_C_MODE_CC | TMP117_C_AVG_0 | TMP117_C_CONV_16s);
        write16(TMP117_REG_CONF, t16);
        break;

    case AHT20:
        // reset
        write8(AHT20_DUMMY, 0xba); 
        (*_customDelayFunc)(21);
        // init (calibrate)
        write16(0xbe, 0x0800);     
        (*_customDelayFunc)(11);
        // trigger measurement
        write16(0xac, 0x3300);
        _haveHum = true;
        _delayNeeded = 85;
        break;

    case HTU31:
        // reset
        write8(HTU31_DUMMY, HTU31_RESET);
        (*_customDelayFunc)(20);
        // heater off
        write8(HTU31_DUMMY, HTU31_HEATEROFF);
        // Trigger conversion
        write8(HTU31_DUMMY, HTU31_CONV);
        _haveHum = true;
        _delayNeeded = 20;
        break;

    case MS8607:
        // Reset
        _address = MS8607_ADDR_T;
        write8(MS8607_DUMMY, 0x1e);
        _address = MS8607_ADDR_RH;
        write8(MS8607_DUMMY, 0xfe);
        delay(15);
        // Read t prom data
        _address = MS8607_ADDR_T;
        _MS8607_C5 = (int32_t)((uint16_t)read16(0xaa) << 8);
        _MS8607_FA = (float)((uint16_t)read16(0xac)) / 8388608.0F;
        // Trigger conversion t
        write8(MS8607_DUMMY, 0x54); // OSR 1024  (0x50=256..0x52..0x54..0x5a=8192)
        // Set rH user register
        _address = MS8607_ADDR_RH;
        temp = (read8(0xe7) & ~0x81) | 0x80;
        write8(0xe6, temp);   // rH user register: 0x81 OSR 256, 0x80 1024, 0x01 2048, 0x00 4096
        // Trigger conversion rH
        write8(MS8607_DUMMY, 0xf5);
        _haveHum = true;
        _delayNeeded = 20;
        break;

    case HDC302X:
        write16(HDC302x_DUMMY, HDC302x_RESET);
        (*_customDelayFunc)(10);
        // Heater off
        write16(HDC302x_DUMMY, HDC302x_HEAT_OFF);
        // Exit auto mode, put into sleep
        write16(HDC302x_DUMMY, HDC302x_AUTO_OFF);
        (*_customDelayFunc)(2);
        // Check (and reset) power-up measurement mode
        HDC302x_setDefault(HDC302x_PUMS, 0x00, 0x00);
        // Check (and reset) offsets
        HDC302x_setDefault(HDC302x_OFFSETS, 0, 0);
        // Trigger new conversion
        write16(HDC302x_DUMMY, HDC302x_TRIGGER);
        _haveHum = true;
        _delayNeeded = 20;
        break;
        
    default:
        return false;
    }

    _tempReadNow = millis();

    _customDelayFunc = myDelay;

    return true;
}

// Read temperature
float tempSensor::readTemp(bool celsius)
{
    float temp = NAN;
    uint16_t t = 0, h = 0;
    uint8_t buf[8];
    
    if(_delayNeeded > 0) {
        unsigned long elapsed = millis() - _tempReadNow;
        if(elapsed < _delayNeeded) (*_customDelayFunc)(_delayNeeded - elapsed);
    }

    switch(_st) {

    case MCP9808:
        t = read16(MCP9808_REG_AMBIENT_TEMP);
        if(t != 0xffff) {
            temp = ((float)(t & 0x0fff)) / 16.0;
            if(t & 0x1000) temp = 256.0 - temp;
        }
        break;

    case BMx280:
        write8(BMx280_DUMMY, BMx280_REG_TEMP);
        t = _haveHum ? 5 : 3;
        if(Wire.requestFrom(_address, (uint8_t)t) == t) {
            uint32_t t1; 
            uint16_t t2 = 0;
            for(uint8_t i = 0; i < t; i++) buf[i] = Wire.read();
            t1 = (buf[0] << 16) | (buf[1] << 8) | buf[2];
            if(_haveHum) t2 = (buf[3] << 8) | buf[4];
            temp = BMx280_CalcTemp(t1, t2);
        }
        break;

    case SHT40:
        if(readAndCheck6(buf, t, h, SHT40_CRC_INIT, SHT40_CRC_POLY)) {
            temp = ((175.0 * (float)t) / 65535.0) - 45.0;
            _hum = (int8_t)(((125.0 * (float)h) / 65535.0) - 6.0);
           if(_hum < 0) _hum = 0;
        }
        write8(SHT40_DUMMY, SHT40_CMD_RTEMPM);    // Trigger new measurement
        break;

    case SI7021:
        if(Wire.requestFrom(_address, (uint8_t)3) == 3) {
            for(uint8_t i = 0; i < 3; i++) buf[i] = Wire.read();
            if(crc8(SI7021_CRC_INIT, SI7021_CRC_POLY, 2, buf) == buf[2]) {
                t = (buf[0] << 8) | buf[1];
                _hum = (int8_t)(((125.0 * (float)t) / 65536.0) - 6.0);
                if(_hum < 0) _hum = 0;
            }
        }
        write8(SI7021_DUMMY, SI7021_CMD_RTEMPQ);
        if(Wire.requestFrom(_address, (uint8_t)2) == 2) {
            for(uint8_t i = 0; i < 2; i++) buf[i] = Wire.read();
            t = (buf[0] << 8) | buf[1];
            temp = ((175.72 * (float)t) / 65536.0) - 46.85;
        }
        write8(SI7021_DUMMY, SI7021_CMD_RHUM);    // Trigger new measurement
        break;

    case TMP117:
        t = read16(TMP117_REG_TEMP);
        if(t != 0x8000) {
            temp = (float)((int16_t)t) / 128.0;
        }
        break;

    case AHT20:
        if(Wire.requestFrom(_address, (uint8_t)7) == 7) {
            for(uint8_t i = 0; i < 7; i++) buf[i] = Wire.read();
            if(crc8(AHT20_CRC_INIT, AHT20_CRC_POLY, 6, buf) == buf[6]) {
                _hum = ((uint32_t)((buf[1] << 12) | (buf[2] << 4) | (buf[3] >> 4))) * 100 >> 20; // / 1048576;
                temp = ((float)((uint32_t)(((buf[3] & 0x0f) << 16) | (buf[4] << 8) | buf[5]))) * 200.0 / 1048576.0 - 50.0;
            }
        }
        write16(0xac, 0x3300);    // Trigger new measurement
        break;

    case HTU31:
        write8(HTU31_DUMMY, HTU31_READTRH);  // Read t+rh
        if(readAndCheck6(buf, t, h, HTU31_CRC_INIT, HTU31_CRC_POLY)) {
            temp = ((165.0 * (float)t) / 65535.0) - 40.0;
            _hum = (int8_t)((100.0 * (float)h) / 65535.0);
            if(_hum < 0) _hum = 0;
        }
        write8(HTU31_DUMMY, HTU31_CONV);  // Trigger new conversion
        break;

    case MS8607:
        _address = MS8607_ADDR_T;
        write8(MS8607_DUMMY, 0x00);
        if(Wire.requestFrom(_address, (uint8_t)3) == 3) {
            int32_t dT = 0;
            for(uint8_t i = 0; i < 3; i++) { dT<<=8; dT |= Wire.read(); }
            dT -= _MS8607_C5;
            temp = (2000.0F + ((float)dT * _MS8607_FA)) / 100.0F;
        }
        // Trigger new conversion t
        write8(MS8607_DUMMY, 0x54);
        _address = MS8607_ADDR_RH;
        if(Wire.requestFrom(_address, (uint8_t)3) == 3) {
            t = Wire.read() << 8; 
            t |= Wire.read();
            t &= ~0x03;
            Wire.read();
            _hum = (int8_t)(((((float)t * (12500.0 / 65536.0))) - 600.0F) / 100.0F);
            //if(temp > 0.0F && temp <= 85.0F) {
            //    _hum += ((int8_t)((float)(20.0F - temp) * -0.18F));   // rh compensated; not worth the computing time
            //}
            if(_hum < 0) _hum = 0;
        }
        // Trigger new conversion rH
        write8(MS8607_DUMMY, 0xf5);
        break;

    case HDC302X:
        if(readAndCheck6(buf, t, h, HDC302x_CRC_INIT, HDC302x_CRC_POLY)) {
            temp = ((175.0 * (float)t) / 65535.0) - 45.0;
            _hum = (int8_t)((100.0 * (float)h) / 65535.0);
            if(_hum < 0) _hum = 0;
        }
        write16(HDC302x_DUMMY, HDC302x_TRIGGER);  // Trigger new conversion
        break;
    }

    _tempReadNow = millis();

    if(!isnan(temp)) {
        if(!celsius) temp = temp * 9.0 / 5.0 + 32.0;
        temp += _userOffset;
        _lastTempNan = false;
    } else {
        _lastTempNan = true;
    }

    // We use only 2 digits, so truncate
    if(_hum > 99) _hum = 99;
    
    #ifdef VSR_DBG
    Serial.printf("Sensor temp+offset: %f\n", temp);
    if(_haveHum) {
        Serial.printf("Sensor humidity: %d\n", _hum);
    }
    #endif

    _lastTemp = temp;

    return temp;
}

void tempSensor::setOffset(float myOffs)
{
    _userOffset = myOffs;
}

// Private functions ###########################################################

float tempSensor::BMx280_CalcTemp(uint32_t ival, uint32_t hval)
{
    int32_t var1, var2, fine_t, temp;

    if(ival == 0x800000) return NAN;

    ival >>= 4;
    
    var1 = ((((ival >> 3) - (_BMx280_CD_T1 << 1))) * _BMx280_CD_T2) / 2048;
    var2 = (ival >> 4) - _BMx280_CD_T1;
    var2 = (((var2 * var2) / 4096) * _BMx280_CD_T3) / 16384;

    fine_t = var1 + var2;

    if(_haveHum) {
        temp = fine_t - 76800;
    
        temp = ((((hval << 14) - _BMx280_CD_H4 - (_BMx280_CD_H5 * temp)) + 16384) / 32768) * 
               ( ( ((( ((temp * _BMx280_CD_H6) / 1024) * (((temp * _BMx280_CD_H3) >> 11) + 32768)) / 1024) + 2097152) * 
                  _BMx280_CD_H2 + 8192 ) / 16384);
        temp -= (((((temp / 32768) * (temp / 32768)) / 128) * _BMx280_CD_H1) / 16);
        if(temp < 0) temp = 0;
    
        _hum = temp >> 22;
    }
    
    return (float)((fine_t * 5 + 128) / 256) / 100.0;
}

void tempSensor::HDC302x_setDefault(uint16_t reg, uint8_t val1, uint8_t val2)
{
    uint8_t buf[8];
    
    write16(HDC302x_DUMMY, reg);
    (*_customDelayFunc)(5);
    if(Wire.requestFrom(_address, (uint8_t)3) == 3) {
        for(uint8_t i = 0; i < 3; i++) buf[i] = Wire.read();
        if(crc8(HDC302x_CRC_INIT, HDC302x_CRC_POLY, 2, buf) == buf[2]) {
            #ifdef VSR_DBG
            Serial.printf("HDC302x: Read 0x%x\n", reg);
            #endif
            if(buf[0] != val1 || buf[1] != val2) {
                #ifdef VSR_DBG
                Serial.printf("HDC302x: EEPROM mismatch: 0x%x <> 0x%x, 0x%x <> 0x%x\n", buf[0], val1, buf[1], val2);
                #endif
                buf[0] = reg >> 8; buf[1] = reg & 0xff;
                buf[2] = val1; buf[3] = val2;
                buf[4] = crc8(HDC302x_CRC_INIT, HDC302x_CRC_POLY, 2, &buf[2]);
                Wire.beginTransmission(_address);
                for(int i=0; i < 5; i++) {
                    Wire.write(buf[i]);
                }
                Wire.endTransmission();
                (*_customDelayFunc)(80);
            } else {
                #ifdef VSR_DBG
                Serial.printf("HDC302x: EEPROM match: 0x%x, 0x%x\n", buf[0], buf[1]);
                #endif
            }
        } else {
            #ifdef VSR_DBG
            Serial.printf("HDC302x: EEPROM reading 0x%x failed CRC check\n", reg);
            #endif
        }
    } else {
        #ifdef VSR_DBG
        Serial.printf("HDC302x: EEPROM reading 0x%x failed on i2c level\n", reg);
        #endif
    }
}

bool tempSensor::readAndCheck6(uint8_t *buf, uint16_t& t, uint16_t& h, uint8_t crcinit, uint8_t crcpoly)
{
    if(Wire.requestFrom(_address, (uint8_t)6) == 6) {
        for(int i = 0; i < 6; i++) buf[i] = Wire.read();
        if(crc8(crcinit, crcpoly, 2, buf) == buf[2]) {
            t = (buf[0] << 8) | buf[1];
            if(crc8(crcinit, crcpoly, 2, buf+3) == buf[5]) {
                h = (buf[3] << 8) | buf[4];
                return true;
            }
        }
    }

    return false;
}

#endif // VSR_HAVETEMP
