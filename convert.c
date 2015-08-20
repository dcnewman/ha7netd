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

#include "xml_const.h"

float
convert_dist_ft2m(float f)
{
     /*
      *   2.54 cm / inch by definition
      *   => 1 ft * 12 in / ft * 2.54 cm / in * 1 m / 100 cm
      *   => 1 ft = 0.3048 m
      */
     return(0.3048 * f);
}


float
convert_dist_m2ft(float m)
{
     return(m / 0.3048);
}


float
convert_temp_c2f(float c)
{
     return(c * (9.0 / 5.0) + 32.0);
}


float
convert_temp_f2c(float f)
{
     return((f - 32.0) * (5.0 / 9.0));
}


float
convert_temp(float t, int units_in, int units_out)
{
     if (units_in == units_out)
	  return(t);

     /*
      *  Small enough list that we might as well not bother with
      *  converting to a cannonical unit and then converting from
      *  there to the target units.
      */
     switch(units_out)
     {
     case DEV_UNIT_C :
	  if (units_in == DEV_UNIT_F)
	       return((t - 32.0) * (5.0 / 9.0));
	  else if (units_in == DEV_UNIT_K)
	       return(t - 273.15);
	  else
	       return(t);

     case DEV_UNIT_K :
	  if (units_in == DEV_UNIT_F)
	       return(273.15 + (t - 32.0) * (5.0 / 9.0));
	  else if (units_in == DEV_UNIT_C)
	       return(t + 273.15);
	  else
	       return(t);

     case DEV_UNIT_F :
	  if (units_in == DEV_UNIT_C)
	       return(t * (9.0 / 5.0) + 32.0);
	  else if (units_in == DEV_UNIT_K)
	       return((t - 273.15) * (9.0 / 5.0) + 32.0);
	  else
	       return(t);

     default :
	  return(t);
     }
}


float
convert_humidity(float h, int units_in, int units_out)
{
     return(h);  /* We only handle _RH -> _RH and for anything else we
		    we treat as an error and return our input */
}


#define UTYPE_UNKNOWN    0
#define UTYPE_HUMIDITY   1
#define UTYPE_LENGTH     2
#define UTYPE_PRESSURE   3
#define UTYPE_TEMP       4
#define UTYPE_TIME       5 
#define UTYPE_VELOCITY   6

static int
convert_utype(int units)
{
     switch (units)
     {
     case DEV_UNIT_C :
     case DEV_UNIT_K :
     case DEV_UNIT_F :
	  return(UTYPE_TEMP);

     case DEV_UNIT_RH :
	  return(UTYPE_HUMIDITY);

     case DEV_UNIT_S :
     case DEV_UNIT_MIN :
     case DEV_UNIT_HR:
     case DEV_UNIT_D :
	  return(UTYPE_TIME);

     case DEV_UNIT_M :
     case DEV_UNIT_MM :
     case DEV_UNIT_CM :
     case DEV_UNIT_KM :
     case DEV_UNIT_FT :
     case DEV_UNIT_MI :
     case DEV_UNIT_IN :
	  return(UTYPE_LENGTH);

     case DEV_UNIT_KPH :
     case DEV_UNIT_MPH :
	  return(UTYPE_VELOCITY);

     case DEV_UNIT_ATM :
     case DEV_UNIT_PA :
     case DEV_UNIT_HPA :
     case DEV_UNIT_KPA :
     case DEV_UNIT_MBAR :
     case DEV_UNIT_MB :
     case DEV_UNIT_MMHG :
     case DEV_UNIT_TORR :
     case DEV_UNIT_INHG :
     case DEV_UNIT_AT :
	  return(UTYPE_PRESSURE);

     default :
	  return(UTYPE_UNKNOWN);
     }
}


int
convert_known(int units_in, int units_out)
{
     int utype_in, utype_out;

     if (units_out == units_in)
	  return(1);

     utype_in = convert_utype(units_in);
     utype_out = convert_utype(units_out);

     return((utype_in == utype_out) ? 1 : 0);
}
