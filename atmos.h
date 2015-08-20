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

#if !defined(__ATMOS_H__)

#if defined(__cplusplus)
extern "C" {
#endif

/*
 *  float atmos_geopotential_alt(float Z)
 *
 *    Return the geopotential altitude corresponding to the geometric
 *    altitude Z.  Both values are in units of meters.  The Earth radius
 *    used is that from the US Standard Atmosphere, 1976.
 *
 *  Call arguments:
 *
 *    float Z
 *      Geometric altitude in meters to compute the corresponding
 *      geopotential altitude of.  Used for input only.
 *
 *  Return value:
 *
 *    Geopotential altitude in meters.
 */
float atmos_geopotential_alt(float Z);


/*
 *  float atmos_geopotential_alt_km(float Z)
 *
 *    Return the geopotential altitude corresponding to the geometric
 *    altitude Z.  Both values are in units of kilometers.  The Earth
 *    radius used is that from the US Standard Atmosphere, 1976.
 *
 *  Call arguments:
 *
 *    float Z
 *      Geometric altitude in kilometers to compute the corresponding
 *      geopotential altitude of.  Used for input only.
 *
 *  Return value:
 *
 *    Geopotential altitude in kilometers.
 */
float atmos_geopotential_alt_km(float Z);


/*
 *  float atmos_press_adjust2a(float Z2, float Z1, float T1)
 *
 *    Compute the reduction factor R to convert a measured pressure P1 at
 *    geometric altitude Z1 (meters, < 11,019 m) and temperature T1 (Celcius)
 *    to the corresponding pressure P2 = R * P1 at altitude Z2 (meters) and
 *    derived temperature, T2.
 *
 *      P2 = R * P1
 *
 *             / T2 \  - g M / R L
 *       R =  | ---- |
 *             \ T1 /
 *
 *      T2 = T1 + L (H2 - H1)
 *
 *      H1 = E * Z1 / (E + Z1)
 *      H2 = E * Z2 / (E + Z2)
 *
 *    where L is the constant gradient of temperature in degress K per meter,
 *    g is the acceleration due to gravity at zero geometric altitude, M is
 *    the mean molecular weight of air (assumed constant up to 86 km), R is
 *    the specific gas constant, and E is the radius of the Earth.
 *
 *  Call arguments:
 *
 *    float Z2
 *      Geometric altitude in meters to compute the pressure adjustment to
 *      from the measured pressure P1 at altitude Z1.  The value Z2 must
 *      be in units of meters.  Used for input only.
 *
 *    float Z1
 *      Geometric altitude of the station where the pressure P1 was
 *      measured.  Must be in units of meters.  Used for input only.
 *
 *    float T1
 *      Temperature at the station where the pressure P1 was measured.
 *      Must be in units of degrees Celsius.  Used for input only.
 *
 *   Return values:
 *
 *     The reduction factor R.
 */
float atmos_press_adjust2a(float Z2, float Z1, float T1);


/*
 *  float atmos_press_adjust2b(float H2, float H1, float T1)
 *
 *    Compute the reduction factor R to convert a measured pressure P1 at
 *    geometric altitude Z1 (meters, < 11,019 m) and temperature T1 (Celcius)
 *    to the corresponding pressure P2 = R * P1 at altitude Z2 (meters) and
 *    derived temperature, T2.
 *
 *      P2 = R * P1
 *
 *             / T2 \  - g M / R L
 *       R =  | ---- |
 *             \ T1 /
 *
 *      T2 = T1 + L (H2 - H1)
 * 
 *    where L is the constant gradient of temperature in degress K per meter,
 *    g is the acceleration due to gravity at zero geometric altitude, and M
 *    is the mean molecular weight of air (assumed constant up to 86 km).
 *
 *  Call arguments:
 *
 *    float H2
 *      Geopotential altitude in meters to compute the pressure adjustment to
 *      from the measured pressure P1 at altitude H1.  The value H2 must
 *      be in units of meters.  Used for input only.
 *
 *    float H1
 *      Geopotential altitude of the station where the pressure P1 was
 *      measured.  Must be in units of meters.  Used for input only.
 *
 *    float T1
 *      Temperature at the station where the pressure P1 was measured.
 *      Must be in units of degrees Celsius.  Used for input only.
 *
 *   Return values:
 *
 *     The reduction factor R.
 */
float atmos_press_adjust2b(float H2, float H1, float T1);

/*
 *  float atmos_press_adjust(float Z2, float Z1, float T1, float RH1)
 *
 *    Compute the pressure adjustment ratio R such that the pressure P2
 *    at geometric altitude Z2 (meters) will be given by P2 = P1 * R
 *    where P1 is the pressure at altitude Z1 (meters) with corresponding
 *    temperature T1 (Celsius) and relative humidity RH1.
 *
 *    It is customary for T1 to be the average of the current temperature
 *    and the temperature 12 hours previously at geometric altitude Z1.
 *
 *    The method used is that described for Tables 48 A - D of Smithsonian
 *    Meteorological Tables, Robert J. List, Sixth Revised Edition,
 *    Smithsonian Institution Press, Washington DC, 1966.
 *
 *  Call arguments:
 *
 *    float Td
 *      Dew point in degrees Celsius.  Used for input only.
 *
 *    float Z2
 *      Geometric altitude in meters to compute the adjustment ratio for.
 *      Used for input only.
 *
 *    float Z1
 *      Geometric altitude in meters of the station.  Used for input only.
 *
 *    float T1
 *      Temperature in degrees Celsius at the station.  See the note above
 *      concerning the use of an average.  Used for input only.
 *
 *    float RH1
 *      Relative humidity at the station.  To omit the correction due to
 *      vapor pressure, supply a value < 0.  Used for input only.
 *
 *  Return value:
 *
 *    Pressure adjustment ratio, R.
 */

float atmos_press_adjust(float Z2, float Z1, float T1, float RH1);

#if defined(__cplusplus)
}
#endif

#endif /* __ATMOS_H___ */
