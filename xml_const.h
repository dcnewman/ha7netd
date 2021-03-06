/*
 *  Copyright (c) 2005, Daniel C. Newman <dan.newman@mtbaldy.us>
 *  All rights reserved.
 *  See the file COPYRIGHT for further information.
 */

/*
 * *** DO NOT EDIT THIS FILE ***
 * *** This file was automatically generated by make_includes.c
 * *** from the source file make_includes.conf
 */

#if !defined(__XML_CONST_H__)

#define __XML_CONST_H__

#if defined(__cplusplus)
extern "C" {
#endif

#define DEV_MISSING_VALUE '*'

#define DEV_DTYPE_FIRST     0
#define DEV_DTYPE_UNKNOWN   0
#define DEV_DTYPE_DEWP      1  /* Dew point                    */
#define DEV_DTYPE_LENG      2  /* Length                       */
#define DEV_DTYPE_PRES      3  /* Atmospheric pressure, station altitude */
#define DEV_DTYPE_PRSL      4  /* Atmospheric pressure, mean sea level (12 hour averaging) */
#define DEV_DTYPE_PRSL0     5  /* Atmospheric pressure, mean sea level (no averaging) */
#define DEV_DTYPE_RAIN      6  /* Precipitation                */
#define DEV_DTYPE_RH        7  /* Relative humidity            */
#define DEV_DTYPE_TEMP      8  /* Temperature                  */
#define DEV_DTYPE_WDIR      9  /* Wind direction               */
#define DEV_DTYPE_WVEL     10  /* Wind velocity                */
#define DEV_DTYPE_LAST     10

#if defined(__BUILD_DNAMES__)

static struct {
     int         dtype;
     const char *dname;
     const char *descr;
} build_dnames[] = {
     { DEV_DTYPE_UNKNOWN,  "",      "" },
     { DEV_DTYPE_DEWP,     "dewp",  "Dew point" },
     { DEV_DTYPE_LENG,     "leng",  "Length" },
     { DEV_DTYPE_PRES,     "pres",  "Atmospheric pressure, station altitude" },
     { DEV_DTYPE_PRSL,     "prsl",  "Atmospheric pressure, mean sea level (12 hour averaging)" },
     { DEV_DTYPE_PRSL0,    "prsl0",  "Atmospheric pressure, mean sea level (no averaging)" },
     { DEV_DTYPE_RAIN,     "rain",  "Precipitation" },
     { DEV_DTYPE_RH,       "rh",    "Relative humidity" },
     { DEV_DTYPE_TEMP,     "temp",  "Temperature" },
     { DEV_DTYPE_WDIR,     "wdir",  "Wind direction" },
     { DEV_DTYPE_WVEL,     "wvel",  "Wind velocity" },
     { -1, 0 }
};

#endif /* if defined(__BUILD_DNAMES__) */

#define DEV_UNIT_UNKNOWN    0
#define DEV_UNIT_S          1  /* seconds                      */
#define DEV_UNIT_MIN        2  /* minutes                      */
#define DEV_UNIT_HR         3  /* hours                        */
#define DEV_UNIT_D          4  /* days                         */
#define DEV_UNIT_M          5  /* meters                       */
#define DEV_UNIT_MM         6  /* millimeters                  */
#define DEV_UNIT_CM         7  /* centimeters                  */
#define DEV_UNIT_KM         8  /* kilometers                   */
#define DEV_UNIT_FT         9  /* feet                         */
#define DEV_UNIT_MI        10  /* miles                        */
#define DEV_UNIT_IN        11  /* inches                       */
#define DEV_UNIT_C         12  /* Celsius                      */
#define DEV_UNIT_K         13  /* Kelvin                       */
#define DEV_UNIT_F         14  /* Fahrenheit                   */
#define DEV_UNIT_KPH       15  /* kilometers per hour          */
#define DEV_UNIT_MPH       16  /* miles per hour               */
#define DEV_UNIT_ATM       17  /* Atmospheres                  */
#define DEV_UNIT_PA        18  /* Pascals                      */
#define DEV_UNIT_HPA       19  /* hecto Pascals                */
#define DEV_UNIT_KPA       20  /* kilo Pascals                 */
#define DEV_UNIT_MBAR      21  /* millibar                     */
#define DEV_UNIT_MB        22  /* millibar                     */
#define DEV_UNIT_MMHG      23  /* millimeters Mercury          */
#define DEV_UNIT_TORR      24  /* Torricellis                  */
#define DEV_UNIT_INHG      25  /* inches Mercury               */
#define DEV_UNIT_AT        26  /* technical atmospheres        */
#define DEV_UNIT_RH        27  /* Relative Humidity            */
#define DEV_UNIT_WF        28  /* Wind is blowing from         */
#define DEV_UNIT_WT        29  /* Wind is blowing towards      */
#define DEV_UNIT_LAST      29

#if defined(__BUILD_UNITS__)

static struct {
     int         atype;
     const char *abbrev;
} build_units[] = {
     { DEV_UNIT_UNKNOWN,  ""     },
     { DEV_UNIT_S,        "s"    },
     { DEV_UNIT_MIN,      "min"  },
     { DEV_UNIT_HR,       "hr"   },
     { DEV_UNIT_D,        "d"    },
     { DEV_UNIT_M,        "m"    },
     { DEV_UNIT_MM,       "mm"   },
     { DEV_UNIT_CM,       "cm"   },
     { DEV_UNIT_KM,       "km"   },
     { DEV_UNIT_FT,       "ft"   },
     { DEV_UNIT_MI,       "mi"   },
     { DEV_UNIT_IN,       "in"   },
     { DEV_UNIT_C,        "C"    },
     { DEV_UNIT_K,        "K"    },
     { DEV_UNIT_F,        "F"    },
     { DEV_UNIT_KPH,      "kph"  },
     { DEV_UNIT_MPH,      "mph"  },
     { DEV_UNIT_ATM,      "atm"  },
     { DEV_UNIT_PA,       "Pa"   },
     { DEV_UNIT_HPA,      "hPa"  },
     { DEV_UNIT_KPA,      "kPa"  },
     { DEV_UNIT_MBAR,     "mbar" },
     { DEV_UNIT_MB,       "mb"   },
     { DEV_UNIT_MMHG,     "mmHg" },
     { DEV_UNIT_TORR,     "Torr" },
     { DEV_UNIT_INHG,     "inHg" },
     { DEV_UNIT_AT,       "at"   },
     { DEV_UNIT_RH,       "%"    },
     { DEV_UNIT_WF,       "wf"   },
     { DEV_UNIT_WT,       "wt"   },
     { -1, 0 }
};

#endif /* if defined(__BUILD_UNITS__) */

#if defined(__cplusplus)
}
#endif

#endif /* !defined(__XML_CONST_H__) */
