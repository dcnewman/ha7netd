/*
 *  Copyright (c) 2005, Daniel C. Newman <dan.newman@mtbaldy.us>
 *  All rights reserved.
 *  
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *  
 *   + Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  
 *   + Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *  
 *   + Neither the name of mtbaldy.us nor the names of its contributors
 *     may be used to endorse or promote products derived from this
 *     software without specific prior written permission.
 *  
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 *  OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 *  AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 *  OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 *  OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 *  SUCH DAMAGE.
 */
#if !defined(__OWIRE_DEVICES_H__)

#define __OWIRE_DEVICES_H__

#if defined(__cplusplus)
extern "C" {
#endif

/*
 * Length of a 1Wire device ID: 64 bits encoded in hex
 */
#define OWIRE_ID_LEN 16

#define OWIRE_DEV_2401      0x01  /* Silicon serial number                   */
#define OWIRE_DEV_2411      0x01  /* Silicon serial number, Vcc input        */
#define OWIRE_DEV_1425      0x02  /* Multikey 1152-bit secure memory         */
#define OWIRE_DEV_2404      0x04  /* 4k NVRAM memory, clock, timer, alarms   */
#define OWIRE_DEV_2405      0x05  /* Addressable switch                      */
#define OWIRE_DEV_2502      0x09  /* 1K EPROM                                */
#define OWIRE_DEV_2505      0x0b  /* 16K EPROM                               */
#define OWIRE_DEV_2506      0x0f  /* 64K EPROM                               */
#define OWIRE_DEV_18S20     0x10  /* High prec. digital thermometer          */
#define OWIRE_DEV_2406      0x12  /* Dual addressable switch + 1k mem        */
#define OWIRE_DEV_2407      0x12  /* Dual addressable switch + 1k mem        */
#define OWIRE_DEV_2430A     0x14  /* 256 EEPROM                              */
#define OWIRE_DEV_1963S     0x18  /* 4K NVRAM with SHA-1 engine              */
#define OWIRE_DEV_1963L     0x1a  /* 4K NVRAM with write cycle counters      */
#define OWIRE_DEV_2436      0x1b  /* Battery id/monitor                      */
#define OWIRE_DEV_28E04_100 0x1c  /* 4K EEPROM with PIO                      */
#define OWIRE_DEV_2423      0x1d  /* 4K NVRAM with external counters         */
#define OWIRE_DEV_2409      0x1f  /* 2 channel addressable coupler 4 subnet  */
#define OWIRE_DEV_2450      0x20  /* Quad A/D converter                      */
#define OWIRE_DEV_1822      0x22  /* Econotemperature                        */
#define OWIRE_DEV_2433      0x23  /* 4K EEPROM                               */
#define OWIRE_DEV_2415      0x24  /* Real time clock (RTC)                   */
#define OWIRE_DEV_2438      0x26  /* Temperature, A/D                        */
#define OWIRE_DEV_2417      0x27  /* RTC with interrupt                     */
#define OWIRE_DEV_18B20     0x28  /* Adjustable resolution temperature       */
#define OWIRE_DEV_2408      0x29  /* 8 channel addressable switch            */
#define OWIRE_DEV_2890      0x2c  /* Signle channel digital potentiometer    */
#define OWIRE_DEV_2431      0x2d  /* 1K EEPROM                               */
#define OWIRE_DEV_2770      0x2e  /* Battery monitor & charge controller     */
#define OWIRE_DEV_28E01_100 0x2f  /* 1K EEPROM with SHA-1                    */
#define OWIRE_DEV_2760      0x30  /* High precision Li+ battery monitor      */
#define OWIRE_DEV_2761      0x30  /* High precision Li+ battery monitor      */
#define OWIRE_DEV_2762      0x30  /* High prec. Li+ battery monitor w/alerts */
#define OWIRE_DEV_2720      0x31  /* Addressable Li+ protection  xxx         */
#define OWIRE_DEV_2432      0x33  /* 1K protected EEPROM with SHA-1          */
#define OWIRE_DEV_2740      0x36  /* High prec. coulomb counter              */
#define OWIRE_DEV_2413      0x3a  /* Dual channel addressable switch         */
#define OWIRE_DEV_2422      0x41  /* Temperature logger w/8K memory          */
#define OWIRE_DEV_2751      0x51  /* Multi-chemistry battery fuel guage      */
#define OWIRE_DEV_2404S     0x84  /* Dual port plus time                     */
#define OWIRE_DEV_2502_E48  0x89  /* 48bit node address chip                 */
#define OWIRE_DEV_2502_UNW  0x89  /* 48bit node address chip                 */
#define OWIRE_DEV_2505_UNW  0x8b  /* 16K add-only uniqueware                 */
#define OWIRE_DEV_2506_UNW  0x8f  /* 64K add-only uniqueware                 */

#define OWIRE_IBUT_1990A    0x01  /* Silicon serial number                   */
#define OWIRE_IBUT_1991     0x02  /* Multikey 1153bit secure                 */
#define OWIRE_IBUT_1994     0x04  /* EconoRAM time chip                      */
#define OWIRE_IBUT_1993     0x06  /* 4K memory iButton                       */
#define OWIRE_IBUT_1992     0x08  /* 1K memory iButton                       */
#define OWIRE_IBUT_1982     0x09  /* 1K add-only memory                      */
#define OWIRE_IBUT_1995     0x0a  /* 16K memory iButton                      */
#define OWIRE_IBUT_1985     0x0b  /* 16K add-only memory                     */
#define OWIRE_IBUT_1996     0x0c  /* 64K memory iButton                      */
#define OWIRE_IBUT_1986     0x0f  /* 64K add-only memory                     */
#define OWIRE_IBUT_1920     0x10  /* High prec. digital thermometer          */
#define OWIRE_IBUT_1971     0x14  /* 256-bit EEPROM and 64-bit OTP register  */
#define OWIRE_IBUT_1963S    0x18  /* 4K NVRAM with SHA-1 engine              */
#define OWIRE_IBUT_1963L    0x1a  /* 4K NVRAM with write cycle counters      */
#define OWIRE_IBUT_1921     0x21  /* Thermacron temperature logger           */
#define OWIRE_IBUT_1921H    0x21  /* Thermacron temperature logger           */
#define OWIRE_IBUT_1921Z    0x21  /* Thermacron temperature logger           */
#define OWIRE_IBUT_1973     0x23  /* 4K EEPROM                               */
#define OWIRE_IBUT_1904     0x24  /* Real-time clock (RTC)                   */
#define OWIRE_IBUT_1961S    0x33  /* 1K protected EEPROM with SHA-1          */
#define OWIRE_IBUT_1977     0x37  /* Password protected 32K EEPROM           */
#define OWIRE_IBUT_1922L    0x41  /* Temperature logger w/8K memory          */
#define OWIRE_IBUT_1922T    0x41  /* Temperature logger w/8K memory          */
#define OWIRE_IBUT_1923     0x41  /* Temperature logger w/8K memory          */
#define OWIRE_IBUT_2751     0x51  /* Multi-chemistry battery fuel guage      */
#define OWIRE_IBUT_1420     0x81  /* Serial ID iButton                       */
#define OWIRE_IBUT_1982U    0x89  /* 48bit node address chip                 */
#define OWIRE_IBUT_1985U    0x8b  /* 16K add-only uniqueware                 */
#define OWIRE_IBUT_1986U    0x8f  /* 64K add-only uniqueware                 */
#define OWIRE_IBUT_1891     0x91  /* 512-bit EPROM memory (uniqueware only)  */
#define OWIRE_IBUT_1955     0x96  /* Java-powered crypto                     */
#define OWIRE_IBUT_1957B    0x96  /* Java-powered crypto                     */

#endif /* !defined(__OWIRE_DEVICES_H__) */

#if defined(__cpluplus)
}
#endif
