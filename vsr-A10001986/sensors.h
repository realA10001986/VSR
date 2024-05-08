/*
 * -------------------------------------------------------------------
 * Voltage Systems Regulator
 * (C) 2024 Thomas Winischhofer (A10001986)
 * https://github.com/realA10001986/VSR
 * https://vsr.out-a-ti.me
 *
 * Sensor Class: Temperature sensor handling
 *
 * This is designed for 
 * - MCP9808, TMP117, BMx280, SHT4x-Ax, SI7012, AHT20/AM2315C, HTU31D, 
 *   MS8607 temperature/humidity sensors. Only temperature is used.
 * -------------------------------------------------------------------
 * License: MIT
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, 
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF 
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. 
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY 
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, 
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE 
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef _VSRSENSOR_H
#define _VSRSENSOR_H

#if defined(VSR_HAVETEMP)

class tcSensor {

    protected:

        void     prepareRead(uint16_t regno);
        uint16_t read16(uint16_t regno, bool LSBfirst = false);
        void     read16x2(uint16_t regno, uint16_t& t1, uint16_t& t2);
        uint8_t  read8(uint16_t regno);
        void     write16(uint16_t regno, uint16_t value, bool LSBfirst = false);
        void     write8(uint16_t regno, uint8_t value);

        uint8_t  crc8(uint8_t initVal, uint8_t poly, uint8_t len, uint8_t *buf);

        uint8_t _address;

        // Ptr to custom delay function
        void (*_customDelayFunc)(unsigned long) = NULL;
};

#endif

#ifdef VSR_HAVETEMP    // -----------------------------------------

enum {
    MCP9808 = 0,      // 0x18 (unsupported: 0x19-0x1f)
    BMx280,           // 0x77 (unsupported: 0x76)
    SHT40,            // 0x44 (unsupported: SHT4x-Bxxx with address 0x45)
    SI7021,           // 0x40
    TMP117,           // 0x49 [non-default] (unsupported: 0x48)
    AHT20,            // 0x38
    HTU31,            // 0x41 [non-default] (unsupported: 0x40)
    MS8607            // 0x76+0x40
};

class tempSensor : tcSensor {

    public:

        tempSensor(int numTypes, uint8_t addrArr[]);
        bool begin(unsigned long powerupTime, void (*myDelay)(unsigned long));

        float readTemp(bool celsius = true);
        float readLastTemp() { return _lastTemp; };

        void setOffset(float myOffs);

        bool haveHum() { return _haveHum; };
        int  readHum() { return _hum; };

    private:

        int     _numTypes = 0;
        uint8_t _addrArr[8*2];    // up to 8 sensor types fit here
        int8_t  _st = -1;
        int8_t  _hum = -1;
        bool    _haveHum = false;
        unsigned long _delayNeeded = 0;

        float  _lastTemp = NAN;

        float  _userOffset = 0.0;

        uint32_t _BMx280_CD_T1;
        int32_t  _BMx280_CD_T2;
        int32_t  _BMx280_CD_T3;
        uint32_t _BMx280_CD_H1;
        int32_t  _BMx280_CD_H2;
        uint32_t _BMx280_CD_H3;
        int32_t  _BMx280_CD_H4;
        union {
            int32_t  _BMx280_CD_H5;
            int32_t  _MS8607_C5;
        };
        union {
            int32_t  _BMx280_CD_H6;
            float    _MS8607_FA;
        };

        unsigned long _tempReadNow = 0;

        float BMx280_CalcTemp(uint32_t ival, uint32_t hval);
};
#endif

#endif  // _VSRSENSOR_H
