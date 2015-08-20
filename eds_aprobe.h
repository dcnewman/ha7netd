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
#if !defined(__EDS_APROBE_H__)

#define __EDS_APROBE_H__

#include "device_eds_aprobe.h"

/*
 *  eds_analog_probe_t
 *  During manufacturing, EDS writes calibration & measurement information
 *  to the one-time programmable (OTP) memory of a DS2406.  We use this
 *  data structure to store this information after we retrieve it from
 *  the DS2406.  It is stored in pages 0, 1, 2, and 3 of the DS2406.
 */
#define EDS_OTHER  0
#define EDS_AOUT   1  /* Analog output             */
#define EDS_PRES   2  /* Barometric pressure probe */
#define EDS_RHRH   3  /* Relative humidity probe   */
#define EDS_TEMP   4  /* Temperature               */

#define EDS_INIT      0
#define EDS_INITDONE  1
#define EDS_READONCE  2

typedef struct {

     int           device_state;    /* Probe calibration read? Probe value?  */
     int           device_type;     /* Probe type (e.g., EDS_RH, EDS_BP)     */
     char          device_ctype[5]; /* Probe type string from OTP memory     */

     device_t     *ds18s20;         /* Associated DS18S20, high prec. therm. */

     unsigned int  dwell;           /* Dwell time in milliseconds            */
     int           nsamples;        /* Number of recommended samples to take */
     unsigned char flags;           /* Device flags (e.g., strong pullup)    */

     float         calib1_eng;      /* 1st calibration point in eng. units   */
     int           calib1_raw;      /* 1st calibration point in eng. units   */
     float         calib2_eng;      /* 2nd calibration point in eng. units   */
     int           calib2_raw;      /* 2nd calibration point in eng. units   */
     float         temp_calib;      /* Temp. calibration                     */
     int           temp_coeff;      /* Temp. coefficient                     */

     float         scale;           /* (c2_eng - c1_eng) / (c2_raw - c1_raw) */
     float         offset;          /* c1_eng - (c1_raw * scale)             */

} eds_aprobe_t;


#endif /* !defined(__EDS_APROBE_H__) */

