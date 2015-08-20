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
#include "atmos.h"
#include "vapor.h"

/*
 *  For the 1976 US Standard Atmosphere, the sea level standard values
 *
 *     0 - 11 km geopotential alt,     dT/dh = -6.5 K/km
 *    11 - 20 km geopotential alt,     dT/dh =  0.0 K/km
 *    20 - 32 km geopotential alt,     dT/dh =  1.0 K/km
 *    32 - 47 km geopotential alt,     dT/dh =  2.8 K/km
 *    47 - 51 km geopotential alt,     dT/dh =  0.0 K/km
 *    51 - 71 km geopotential alt,     dT/dh = -2.8 K/km
 *    71 - 84.852 km geopotential alt, dT/dh = -2.0 K/km
 *
 *    N.B. 84.852 km geopotential altitude = 86 km geometric altitude
 *
 *     Sea level pressure    = 101325 N/m^2
 *     Sea level temperature = 288.15 K
 *     Sea level accel. due to gravity = g = 9.80665 m / s^2
 *     Molecular weight of dry air     = M = 28.96443 g / mole
 *                                         = 28.96443 x 10^-3 kg / mole
 *     Specific gas constant           = R = 8.31432 J / (K mole)
 *                                         = 8.31432 kg m^2 / (K mole s^2)
 *
 *  Define the atmosphere.  The sea level density of 1.225 kg/m^3 is
 *  derived from the above.
 *
 *  Hydrostatic constant = g M / R = 34.1632 x 10^-3 K / m
 *                                 = 34.1632 K / km
 */

/* ------------------------------------------------------------------------ */

/*
 *  The routine atmosphere() below is a translation from FORTRAN 90 of the
 *  subroutine Atmosphere() written by Ralph Carmichael and distributed via
 *  as Public Domain Aeronautical Software (pdas.com).
 */

static const float rearth = 6356.766E3;          /* Radius of the Earth (m) */
static const float rearth_km = 6356.766;        /* Radius of the Earth (km) */
static const float g = 9.80665; /* Acceleration due gravity at 0 km (m/s^2) */
static const float M = 28.96443;    /* Mean molecular weight of air (g/mol) */
static const float R = 8.31432;        /* Specific gas constant (J / K mol) */

#define NTAB 8

static void
atmosphere(float alt, float *sigma, float *delta, float *theta)
{
     int i;
     float gMR;                               /* Hydrostatic constant (K/km) */
     float delta_, theta_;                            /* Temporary variables */
     float h;                                  /* Geopotential altitude (km) */
     float tgrad, tbase; /* Temperature gradient and base temp of this layer */
     float tlocal;                                  /* Local temperature (K) */
     float deltah;                        /* Height above base of this layer */
     static const float htab[NTAB] = {            /* Atmospheric layers (km) */
	  0.0, 11.0, 20.0, 32.0, 47.0, 51.0, 71.0, 84.852 };
     static const float ttab[NTAB] = {     /* 15C @ 0km, temp @ layer bottom */
	  288.15, 216.65, 216.65, 228.65, 270.65, 270.65, 214.65, 186.946};
     static const float ptab[NTAB]= {       /* Pressure ratio @ layer bottom */
	  1.0, 2.233611E-1, 5.403295E-2, 8.5666784E-3, 1.0945601E-3,
	  6.6063531E-4, 3.9046834E-5, 3.68501E-6};
     static const float gtab[NTAB] = {     /* Temp. gradient (degreee K/ km) */
	  -6.5, 0.0, 1.0, 2.8, 0.0, -2.8, -2.0, 0.0};

     /*
      *  Hydrostatic constant
      *  Note that we actually need to divide M by 1000 g / kg AND THEN
      *  multiply by 1 km / 1000 m to convert to K / km.  Since the net
      *  effect is to multiply by 1000 / 1000 = 1, we omit those conversions.
      */
     gMR = g * M / R;

     /*
      *  Convert geometric to geopotential altitude.  Since we want our
      *  answer in km and not m, we need t
      */
     h = atmos_geopotential_alt_km(alt);

     /*
      *  Determine which layer this geopotential altitude corresponds to
      *  Allow geopotential altitudes > 84.852 km (htab[NTAB-1])
      */
     for (i = 0; i < NTAB - 1; i++)
	  if (h >= htab[i])
	       break;

     /*
      *  Temperature gradient for this layer
      */
     tgrad = gtab[i];

     /*
      *  Temperature at layer's base assuming 15 C = 288.15K at 0 km
      */
     tbase = ttab[i];

     /*
      *  Height above layer's base
      */
     deltah = h - htab[i];

     /*
      *  Temperature at this geopotential altitude
      */
     tlocal = tbase + tgrad * deltah;

     /*
      *  Temperature ratio
      */
     theta_ = tlocal / ttab[0];
     if (theta)
	  *theta = theta_;

     /*
      *  Pressure ratio
      */
     if (tgrad == 0.0)
	  delta_ = ptab[i] * expf(-gMR * deltah / tbase);
     else
	  delta_ = ptab[i] * powf(tbase / tlocal, gMR / tgrad);
     if (delta)
	  *delta = delta_;

     /*
      *  Density ratio
      */
     if (sigma)
	  *sigma = delta_ / theta_;
}

#undef NTAB

/* ------------------------------------------------------------------------ */

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

float
atmos_geopotential_alt(float Z)
{
     return(Z * rearth / (Z + rearth));
}


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

float
atmos_geopotential_alt_km(float Z)
{
     return(Z * rearth_km / (Z + rearth_km));
}


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

float
atmos_press_adjust2a(float Z2, float Z1, float T1)
{
     float H1, H2, T2;
     static const float L = -6.5; /* Temp. gradient (K/km), good to 11,019 m */

     H1 = atmos_geopotential_alt(Z1);
     H2 = atmos_geopotential_alt(Z2);
     return(atmos_press_adjust2b(H2, H1, T1));
}


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

float
atmos_press_adjust2b(float H2, float H1, float T1)
{
     float T2;
     static const float L = -6.5; /* Temp. gradient (K/km), good to 11,019 m */

     T1 += 273.15; /* Convert to Kelvin */
     T2 = T1 + (L / 1000.0) * (H2 - H1);

     /*
      *  Note M is actually in the wrong units and needs to be
      *  multiplied by 1 kg / 1000 g.  However, L is also in the
      *  wrong units and needs to be multiplied by 1000 m / km.
      *  So, the convert to proper units we need to multiply by
      *  1000 m kg / 1000 g km = 1 m kg / g km.  Since that is unity,
      *  we omit that in the calculation below.
      */
     return(powf(T2 / T1,  -g * M  / (L * R))); 
}


/*
 *  float correct(float Td, float Z)
 *
 *    Given the station dew point Td in degrees Celsius and the station's
 *    geometric altitude Z in meters, determine the vapor pressure correction
 *    C from table 48 A -- Correction for Humidity C, used in determining
 *    t_mv when reducing pressure, Smithsonian Meteorological Tables,
 *    Robert J. List, Sixth Revised Edition, Smithsonian Institution Press,
 *    Washington DC, 1966.
 *
 *    Bilinear interpolation is used to arrive at a correction from the
 *    table.  The table data is a regular, rectangular grid.
 *
 *  Call arguments:
 *
 *    float Td
 *      Dew point in degrees Celsius.  Used for input only.
 *
 *    float Z
 *      Geometric altitude in meters.  Used for input only.
 *
 *  Return value:
 *
 *    Correction factor in degrees Celsius.
 */

#define NALT  6
#define NDEW 30

static float
correct(float Td, float Z)
{
     float a00, a01, a10, a11, correction, d_fraction, z_fraction;
     int d_index, z_index;
     static const float alt_incr = 500.0;  /* meters, geometric altitude  */
     static const float alts[NALT] = {0.0, 500.0, 1000.0, 1500.0, 2000.0, 2500.0};
     static const float dew_incr = 2.0;    /* degrees Celsius */
     static const float dews[NDEW] = {
        -28.0, -26.0, -24.0, -22.0, -20.0, -18.0, -16.0, -14.0, -12.0, -10.0,
         -8.0,  -6.0,  -4.0,  -2.0,   0.0,   2.0,   4.0,   6.0,   8.0,  10.0,
         12.0,  14.0,  16.0,  18.0,  20.0,  22.0,  24.0,  26.0,  28.0,  30.0};
     static const float corrections[NALT][NDEW] = {
/* alt=   0 m, dew-points = -28 C, -26 C, ..., 30 C */
	  0.1, 0.1, 0.1, 0.1, 0.1,   0.1, 0.2, 0.2, 0.2, 0.3,
	  0.3, 0.4, 0.5, 0.6, 0.7,   0.8, 0.9, 1.0, 1.2, 1.3,
	  1.5, 1.7, 1.9, 2.2, 2.5,   2.8, 3.2, 3.6, 4.1, 4.6,
/* alt= 500 m, dew-points = -28 C, -26 C, ..., 30 C */
	  0.1, 0.1, 0.1, 0.1, 0.1,   0.2, 0.2, 0.2, 0.3, 0.3,
	  0.4, 0.4, 0.5, 0.6, 0.7,   0.8, 1.0, 1.1, 1.3, 1.5,
	  1.7, 1.9, 2.2, 2.5, 2.8,   3.2, 3.6, 4.0, 4.6, 5.1,
/* alt=1000 m, dew-points = -28 C, -26 C, ..., 30 C */
	  0.1, 0.1, 0.1, 0.1, 0.1,   0.2, 0.2, 0.2, 0.3, 0.4,
	  0.4, 0.5, 0.6, 0.7, 0.8,   1.0, 1.1, 1.3, 1.5, 1.7,
	  1.9, 2.2, 2.5, 2.8, 3.2,   3.6, 4.0, 4.6, 5.1, 5.8,
/* alt=1500 m, dew-points = -28 C, -26 C, ..., 30 C */
	  0.1, 0.1, 0.1, 0.1, 0.2,   0.2, 0.2, 0.3, 0.3, 0.4,
	  0.5, 0.6, 0.7, 0.8, 0.9,   1.1, 1.2, 1.4, 1.6, 1.9,
	  2.1, 2.4, 2.8, 3.1, 3.6,   4.0, 4.6, 5.1, 5.8, 6.5,
/* alt=2000 m, dew-points = -28 C, -26 C, ..., 30 C */
	  0.1, 0.1, 0.1, 0.1, 0.2,   0.2, 0.3, 0.3, 0.4, 0.5,
	  0.5, 0.6, 0.8, 0.9, 1.1,   1.2, 1.4, 1.6, 1.8, 2.1,
	  2.4, 2.7, 3.1, 3.5, 4.0,   4.5, 5.1, 5.8, 6.5, 7.3,
/* alt=2500 m, dew-points = -28 C, -26 C, ..., 30 C */
	  0.1, 0.1, 0.1, 0.2, 0.2,   0.2, 0.3, 0.4, 0.4, 0.5,
	  0.6, 0.7, 0.9, 1.0, 1.2,   1.4, 1.6, 1.8, 2.1, 2.4,
	  2.7, 3.1, 3.5, 4.0, 4.5,   5.1, 5.8, 6.5, 7.3, 8.2};

     /*
      *  For purposes of interpolation, determine where the geometric
      *  altitude and dew point land on our grid of correction.
      */
     if (dews[0] <= Td && Td < dews[NDEW - 1])
     {
	  d_index    = (int)((Td - dews[0]) / dew_incr);
	  d_fraction = (Td - dews[d_index]) / dew_incr;
     }
     else if (dews[0] > Td)
     {
	  d_index    = 0;
	  d_fraction = (Td - dews[0]) / dew_incr;
     }
     else if (Td == dews[NDEW - 1])
     {
	  d_index    = NDEW - 2;
	  d_fraction = 1.0;
     }
     else /* dew_hi < Td */
     {
	  d_index    = NDEW - 2;
	  d_fraction = (Td - dews[NDEW - 1]) / dew_incr;
     }

     if (alts[0] <= Z && Z < alts[NALT - 1])
     {
	  z_index    = (int)((Z - alts[0]) / alt_incr);
	  z_fraction = (Z - alts[z_index]) / alt_incr;
     }
     else if (alts[0] > Z)
     {
	  z_index    = 0;
	  z_fraction = (Z - alts[0]) / alt_incr;
     }
     else if (Z == alts[NALT - 1])
     {
	  z_index    = NALT - 2;
	  z_fraction = 1.0;
     }
     else /* alt_hi < Z */
     {
	  z_index    = NALT - 2;
	  z_fraction = (Z - alts[NALT - 1]) / alt_incr;
     }

     /*
      *  Bilinear interpolation
      *
      *      h3 (0,1)            h4 (1,1)
      *              +----------+
      *              |      |   |
      *              |------+---|   h = a00 + a10 * x + a01 * y + a11 * x * y
      *              |   x  |   |            a00 = h1
      *              |      | y |            a10 = h2 - h1
      *              +----------+            a01 = h3 - h1
      *      h1 (0,0)            h2 (1,0)    a11 = h1 - h2 - h3 + h4
      */
     a00 = corrections[z_index+0][d_index+0];
     a10 = corrections[z_index+1][d_index+0] - a00;
     a01 = corrections[z_index+0][d_index+1] - a00;
     a11 = -a10 - corrections[z_index+0][d_index+1]
	  + corrections[z_index+1][d_index+1];

     correction = a00 + z_fraction * a10 + d_fraction * a01 +
	  a11 * z_fraction * d_fraction;
     return((correction >= 0.0) ? correction : 0.0);
}

#undef NALTS
#undef NDEWS

/*
 *  float atmos_press_adjust1a(float Z2, float Z1, float T1, float RH1)
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
 *      Relative humidity at the station.  Supply a value < 0.0 to indicate
 *      that a vapor pressure correction should be omitted.  Used for input
 *      only.
 *
 *  Return value:
 *
 *    Pressure adjustment ratio, R.
 */

float
atmos_press_adjust(float Z2, float Z1, float T1, float RH1)
{
     float C, L, Tmv, Hd;

     /*
      *  Hd = difference in geopotential altitudes (meters)
      */
     Hd = atmos_geopotential_alt(Z1) - atmos_geopotential_alt(Z2);

     /*
      *  L = lapse rate correction with total temperature difference
      *      being Hd / 200.0 and temperature at midpoint thus being
      *      Hd / 200.0 / 2 = Hd / 400.0
      */
     L = Hd / 400.0;

     /*
      *  Vapor pressure correction.  Tables use dew point rather than
      *  vapor pressure.... so we compute the dew point.
      */
     if (RH1 >= 0.0)
	  C = correct(dewpoint(RH1, T1), Z1);

     /*
      *  So, our mean temperature is thus
      */
     Tmv = T1 + L + C + 273.15;

     /*
      *  log10(p2 / p1) = Hd / (67.442 * Tmv)
      *
      *  We here return the ratio p2/p1 = 10^(Hd / (67.442 * Tmv))
      */
     return(powf(10.0, Hd / (67.442 * Tmv)));
}

#if defined(__TEST__)

#include <stdio.h>

main()
{
     /*
      * Oregon Scientfic: 871 mb @ 0 m ->1014 mb @ 1280 m
      *
      *    Inside 0:  17.3C, 39% RH
      *    Inside 1:  16.4C, 38% RH
      *    Outside 3: 14.7C, 39% RH
      */
     float r;
     atmosphere(1.280, 0, &r, 0);
     printf("%f\n", 871*100.0/r);
     printf("Outside:  p = %f (%f)\n",
	    871.0 * atmos_press_adjust2a(0.0, 1280.0, 14.7),
	    871.0 * atmos_press_adjust1a(0.0, 1280.0, 14.7, 39.0));

     printf("Inside 0: p = %f (%f)\n",
	    871.0 * atmos_press_adjust2a(0.0, 1280.0, 17.3),
	    871.0 * atmos_press_adjust1a(0.0, 1280.0, 17.3, 38.0));
   
     printf("Inside 1: p = %f (%f)\n",
	    871.0 * atmos_press_adjust2a(0.0, 1280.0, 16.4),
	    871.0 * atmos_press_adjust1a(0.0, 1280.0, 16.4, 39.0));
}

#endif
