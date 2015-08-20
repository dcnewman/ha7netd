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

#include "math.h"

float
goff_gratch(float t)
{
     float log10p, r, r1;

     /*
      *  Saturation vapor pressure of air over water using the Goff-Gratch
      *  formula, 1946.
      *
      *  Goff, J. A., and S. Gratch, Low-pressure properties of water from
      *  -160 to 212 F, in Transactions of the American Society of Heating
      *  and Ventilating Engineers, pp 95-122, presented at the 52nd Annual
      *  Meeting of the American Society of Heating and Ventilating Engineers,
      *  New York, 1946. 
      *
      *      t = temperature in degrees Kelvin (K)
      *   p(t) = saturation vapor pressure in millibars (mb, hPa)
      *
      *  log10(p(t)) =
      *     - 7.90298*(373.16/t - 1.0)
      *     + 5.02808*log10f(373.16 / t)
      *     - 1.3816e-7 * (powf(10.0, 11.344*(1.0 - t/373.16)) - 1.0)
      *     + 8.1328e-3 * (powf(10.0, -3.49149*(373.16/t - 1.0)) - 1.0)
      *     + log10f(1013.246);
      *
      */
     r = 373.16 / t;
     r1 = r - 1.0;
     log10p = -7.90298*r1
	  + 5.02808*log10f(r)
	  - 1.3816e-7 * (powf(10.0, 11.344*(1.0 - t / 373.16)) - 1.0)
	  + 8.1328e-3 * (powf(10.0, -3.49149*r1) - 1.0)
	  + 3.0057148979490314;
     return(powf(10.0, log10p));
}

float
goff(float t)
{
     float log10p, ra, ra1, rb;

     /*
      *  Saturation vapor pressure of air over water using the Goff
      *  equation, 1953.
      *
      *  Goff, J. A, Saturation Pressure of Water on the New Kelvin
      *  Temperature Scale, Transactions of the American Society of Heating
      *  and Ventilating Engineers, pp 347-354, presented at the semi-annual
      *  meeting of the American Society of Heating and Ventilating Engineers,
      *  Murray Bay, Quebec, Canada, 1957.
      *
      *  This is considered to be the intended formula recommeded by the
      *  World Meterological Organization: their recommendation published
      *  in 1988 appears to be this formula but with several typographical
      *  errors; a corrigendum issued in 2000 also has one error (a sign
      *  error in an exponent).
      *
      *      t = temperature in degrees Kelvin (K)
      *   p(t) = saturation vapor pressure in millibars (mb, hPa)
      *
      *  log10(p(t)) =
      *           10.79574*(1.0 - 273.16 / t)
      *         -  5.02800 * log10f(t / 273.16)
      *         +  1.50475e-4 * (1.0 - powf(10.0, -8.2969*(t / 273.16 - 1.0)))
      *         +  0.42873e-3 * (powf(10.0, 4.76955*(1.0 - 273.16 / t)) - 1.0)
f      *         +  0.78614;
      */
     ra = 273.16 / t;
     ra1 = 1.0 - ra;
     rb = t / 273.16;
     log10p = 10.79574 * ra1
	  - 5.02800 * log10f(rb)
	  + 1.50475e-4 * (1.0 - powf(10.0, -8.2969*(rb - 1.0)))
	  + 0.42873e-3 * (powf(10.0, 4.76955 * ra1) - 1.0)
	  + 0.78614;
     return(powf(10.0, log10p));
}


float
bolton(float t)
{
     /*
      *  Saturation vapor pressure of air over liquid water, Bolton, 1980.
      *
      *  Bolton, D., The Computation of Equivalent Potential Temperature,
      *  Monthly Weather Review, Volume 108, pp. 1046-1053, 1980.
      *
      *     t = temperature in degrees Celsius
      *  p(t) = saturation vapor pressure in millibars (mb, hPa)
      *
      *    p(t) = 6.112 * exp(17.76 * t / (t + 243.5))
      *
      *  This equation has significant deviation from Goff and Goff-Gratch for
      *  temperatures below -50 C but (1) gives a reasonable values for
      *  temperatures > -50 C, and (2) is quite easy to solve for temperature
      *  and thus lends itself well to dew point calculations for "ordinary"
      *  conditions.
      *
      *  Note that "poor" agreement to Goof and Goff-Gratch for temperatures
      *  below -50 C doesn't necessarily mean much in as much that it is
      *  by no means clear that Goff or Goff-Gratch give correct values for
      *  such temperatures either.  See, for instance, Fukuta, N. & C. M.
      *  Gramada, "Vapor Pressure Measurement of Supercooled Water",
      *  J. Atmos. Sci., 60, pp. 1871-1875, 2003.  That paper, which presents
      *  actual vapor pressure measurements for temperatures as low as -38 C,
      *  suggests that Goff and Goff-Gratch are off by as much as 10%
      *  at -38 C.
      */
     return(6.112 * expf(17.67 * t / (t + 243.5)));
}


float
dewpoint(float rh, float t)
{
     float l, rhd;

     /*
      *  rh = relative humidity (unitless)
      *   t = temperature in degrees Celsius (C)
      *
      *  Use the station temperature to compute the corresponding saturation
      *  vapor pressure.  Then, using the station relative humidity,
      *  solve
      *
      *    relative humidity =
      *         (vapor pressure at dew point / saturation vapor pressure) x 100
      *
      *  for the vapor pressure at the dew point.  Finally, solve Bolton (1980)
      *  for the dew point (temperature) using the vapor pressure at the
      *  dew point.  We use Bolton (1980) as it is (1) easy to solve for
      *  the temperature, and (2) appears reasonably accurate for the
      *  temperature range we are interested in (> -50 C).
      *
      *  Notes: 1. Input temperatures < -50 C may give inaccurate results.
      *         2. An input temperture of -243.5 K = -516.65 C will lead to
      *            a division by zero.  That's very, very cold and *way*
      *            outside the range of applicability for these equations
      *            and concepts.
      */
     if (rh < 0.0)
	  rhd = 0.0;
     else if (rh > 100.0)
	  rhd = 1.0;
     else
	  rhd = rh / 100.0;
     l = logf(bolton(t) * rhd / 6.112);
     return(l * 243.5 / (17.67 - l));
}
