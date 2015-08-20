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

#if !defined(__DEVICE_H__)

#define __DEVICE_H__

#include <time.h>
#include <sys/time.h>
#include "debug.h"
#include "os.h"
#include "owire_devices.h"
#include "utils.h"  /* for timestr */
#include "xml_const.h"

#if defined(__cplusplus)
extern "C" {
#endif

/*
 *  Length in bytes of a hex-encoded 1-Wire ROM id
 */
#if !defined(OWIRE_ID_LEN)
#define OWIRE_ID_LEN 16
#endif

/*
 *  device_ignore_t
 *
 *  Linked list of glob-style patterns for matching 1-Wire device
 *  ROM ids.  Intent being to have a list of patterns for filtering
 *  lists of ROM ids (e.g., which devices to ignore).
 *
 *  device_ignore_t *next
 *    Next element in the linked list.
 *
 *  size_t plen
 *    Length in bytes of the pattern string contained in the "pat" field.
 *    Length does not include any NUL terminator.
 *
 *  char pat[DEV_IGNORE_PAT_LEN_MAX]
 *    NUL terminated glob-style pattern to match against.  Code will convert
 *    the string to uppercase so as to effect case-insensitive matches
 *    against canonicalized ROM ids.  (ROM ids are canonicalized by
 *    conversion to uppercase.)  Patterns may use
 *
 *         * -- Match zero or more characters
 *         ? -- Match exactly one character
 *     [x-y] -- Match characters in the range x through y, inclusive
 *    [!x-y] -- Match characters outside of the inclusive range x through y
 *        \x -- Interpret x literally (e.g., to match a "*" character,
 *                specify "\*").
 *
 *    Note that [...] constructs may contain multiple ranges or explicit
 *    characters (e.g., "[a-bA-Z0-9]" to match alphanumeric characters;
 *    "[aeiou]" to match vowels, "[!aeiou]" to match consonants, etc.).
 *
 *    See glob.c for further details.
 */

#define DEV_IGNORE_PAT_LEN_MAX 64

typedef struct device_ignore_s {
     struct device_ignore_s *next;                        /* Next pattern   */
     size_t                  plen;                        /* Pattern length */
     char                    pat[DEV_IGNORE_PAT_LEN_MAX]; /* Pattern        */
} device_ignore_t;


/*
 *  device_group_t
 *
 *  Linked list of devices which together constitute a meaningful group of
 *  devices.
 *
 *    size_t ref
 *      Reference number.  Used at configuration time to spot related devices
 *      and automatically group them together.
 *
 *    size_t nlen
 *      Length in bytes of the group name.  This length does not include any
 *      NUL terminator.
 *
 *    char name[DEV_GNAME_LEN]
 *      Name of the device group.  The name does not need to be unique.
 *      The string does not need to be NUL terminated.
 *
 *    device_group_t *next, *prev
 *      Forward and backward list pointers.  The backward pointer is kept
 *      to facilitate locating the head of the group list given a pointer
 *      to any device in the group.
 */

#define DEV_GNAME_LEN 64 /* Max group name length; Nice to be >= OPT_NAM_LEN */

typedef struct dev_group_s {
     size_t           ref;                  /* Reference number             */
     size_t           nlen;                 /* Group name length            */
     char             name[DEV_GNAME_LEN];  /* Group name                   */
     struct device_s *next;                 /* Next device in the group     */
     struct device_s *prev;                 /* Previous device in the group */
} device_group_t;


/*
 *  Maximum number of measurements to store in memory.  Value chosen to
 *  represent an entire 24 hour day's worth of samples taken every minute.
 */

#define NPAST (60 * 24)

/*
 *  Maximum number of distinct measurements per device (e.g., temperature and
 *  pressure would represent two measurements).
 */

#define NVALS 4

/*
 *  A missing data measurement is indicated by storing (time_t)-1 as the
 *  measurement time stamp.
 */

#define DEV_MISSING_TVALUE ((time_t)-1)

/*
 *  Maximum number of running average periods per device (e.g., 10 minutes and
 *  60 minutes would be two distinct periods).
 */

#define NPERS 4

/*
 *  device_period_array_t
 *
 *  An array in which to store running average period sizes, in seconds.
 *  The values are stored such that period[n] >= period[n+1].  That way,
 *  if period[0] == 0 we know that no periods are stored in the array.
 */

typedef int device_period_array_t[NPERS];


/*
 *  Generic device flags
 */

#define DEV_FLAGS_IGNORE      0x00000001 /* Ignore this device               */
#define DEV_FLAGS_INITIALIZED 0x00000002 /* Device is initialized            */
#define DEV_FLAGS_ISSUB       0x00000004 /* Device is a subdevice of another */
#define DEV_FLAGS_OUTSIDE     0x00000008 /* Device reports outdoors measure  */
#define DEV_FLAGS_END         0x80000000 /* End of device list/array         */


/*
 *  device_loc_t
 *
 *  This structure is used to store per-device configuration information.
 *  The information stored in a device_loc_t structure is merged into a
 *  device_t structure by dev_info_merge().
 *
 *  Note that these structures are allocated on the heap.  The allocated
 *  size will be ssize = sizeof(device_loc_t) + dlen.  The extra dlen bytes
 *  at the end of the structure provides storage for a NUL terminated
 *  location/description string.
 */

#define MAXHINT 32

typedef struct device_loc_s {
     struct device_loc_s   *next;     /* Next element                        */
     size_t                 ssize;    /* Allocated structure length          */
     char                   romid[OWIRE_ID_LEN+1]; /* Device's ROM id        */
     device_period_array_t  periods;  /* Running average periods (seconds)   */
     int                    sleeze;   /* State/context used by config reader */
     device_group_t         group1;   /* Config-based device grouping        */
     size_t                 slen;     /* Length of specific data             */
     char                  *spec;     /* Device specific data                */
     unsigned int           flags;
     size_t                 hlen;     /* Driver hint length, in bytes        */
     char                   hint[MAXHINT+1]; /* Driver hint                  */
     float                  gain;     /* Correction gain                     */
     float                  offset;   /* Correction offset                   */
     size_t                 dlen;     /* Description length, in bytes        */
     char                   desc[1];  /* Device description or location      */
} device_loc_t;


/*
 *  device_averages_t
 *
 *  avg[i][j] is the running average for the trailing period[j] seconds
 *  of the device_data_t value val[i] (0 <= i < NVALS, 0 <= j < NPERS).
 *  That is, avg[i][j] is the average of the device_data_t data val[i]
 *  from the current time until period[j] seconds in the past.  The computed
 *  running average is an integrated average of the form
 *
 *    avg[i][j] =
 *
 *       k = n_past
 *           ----
 *           \             1
 *           /             - (time[k+1] - time[k]) * (val[i][k+1] - val[i][k])
 *           ----          2
 *      k = n_current - 1
 *      ----------------------------------------------------------------------
 *                    time[n_current] - time[n_past]
 *
 *   where
 *
 *   0 <= i < NVALS, 0 <= j < NPERS
 *
 *   n_current, time[], and val[][] are the fields of the same name from
 *      device_data_t, and
 *
 *   n_past satisfies
 *
 *    (n_past % NPAST) < n_current AND
 *    time[n_past] + period[j] = time[n_current]
 *
 *  int period[NPERS]
 *    Array of averaging periods in seconds.  The array is sorted such that
 *    period[j] >= period[j+1].  Consequently, if period[0] == 0, then no
 *    running averages are to be computed.
 *
 *  int period_approx[NPERS]
 *    period_approx[j] = 0.95 * period[j].
 *
 *  int range_exists[NPERS]
 *    For a given j in [0,NPERS), as long as two samples within the time
 *    period period[j] exist, the average avg[i][j] can be computed for all
 *    i in [0,NVALS).  However, that average will not be a "true" running
 *    average covering the entire time period period[j].  When range_exists[j]
 *    is non-zero, then there is data covering the entire period
 *    period_approx[j] (= 0.95*period[j]).
 *
 *  float avg[NVALS][NPERS]
 *    Running averages.  See the description above.
 */

typedef struct {
     device_period_array_t period;
     device_period_array_t period_approx;
     device_period_array_t range_exists;
     float                 avg[NVALS][NPERS];
} averages_t;


/*
 *  hi_lo_t
 *
 *  Measurement extrema are stored in a hi_lo_t structure.  The dev_read()
 *  routine updates this structure each time a successful device read
 *  occurrs.  The routine dev_hi_lo_reset() may be used to initialize
 *  this structure.
 */

typedef struct {
     float   min[NVALS];    /* min(dev->data[i][j], 0 <= j < NPAST) = min[i] */
     float   max[NVALS];    /* max(dev->data[i][j], 0 <= j < NPAST) = max[i] */
     time_t  tmin[NVALS];   /* tmin[i] is the time stamp for min[i]          */
     time_t  tmax[NVALS];   /* tmax[i] is the time stamp for max[i]          */
     timestr tmin_str[NVALS]; /* Storage for a HH:MM representations of tmin */
     timestr tmax_str[NVALS]; /* and tmax.  Used by the data output routines */
} hi_lo_t;


/*
 *  device_data_t
 *
 *  Device measurement information is stored in a device_data_t structure.
 *  The primary field in this structure is the val[][] field:
 *
 *     val[i][j] is the j'th measurement for the devices ith component
 *               where 0 <= j < NPAST and 0 <= i < NVALS.
 *  and
 *
 *     time[j] is the timestamp for the measurement val[][j]
 *
 *  Each time dev_read() is called, dev_read() will increment n_current.
 *  n_current is the "j" index running from 0 to NPAST-1 which should
 *  be used to determine where in time[] and val[][] to store the next
 *  set of measurements.  When the incremented value of n_current attains
 *  (or exceeds) NPAST, dev_read() "wraps" the index setting n_current = 0.
 *  As such, it is possible for n_previous > n_current.  Specifically, when
 *  n_previous = NPAST - 1 and n_current = 0.
 *
 *  The fld_used[] field indicates for which values of i, 0 <= i < NVAL,
 *  the val[i][] values are meaningful.  If fld_used[i] is non-zero, then
 *  val[i][] is used; otherwise, it is ignored.
 *
 *  The running averages and extrema are stored in the avgs, today, and
 *  yesterday fields.  They are automatically generated by dev_read()
 *  after a successful measurement.  Individual device drivers do not
 *  and should not attempt to compute those values.
 *
 *  NOTE: if a dev_read() call fails on a device, time[n_current] will be
 *  set to DEV_MISSING_TVALUE so as to indicate a "missing value".
 *
 *  For example, a temperature compensated humidty sensor might have
 *
 *     val[0][] = Temperature readings (i = 0)
 *     val[1][] = Humidity readings (i = 1)
 *     fld_used[0]   = DEV_FLD_USED
 *     fld_dtype[0]  = DEV_DTYPE_TEMP
 *     fld_units[0]  = DEV_UNIT_C
 *     fld_format[0] = "%0.1f"
 *
 *     fld_used[1]   = DEV_FLD_USED
 *     fld_dtype[1]  = DEV_DTYPE_RH
 *     fld_units[1]  = DEV_UNIT_RH
 *     fld_format[1] = "%0.f"
 *
 *     fld_used[2,...] = 0
 */

#define DEV_FLD_USED           1
#define DEV_FLD_USED_NORECORD -1

typedef struct device_data_s {
     float        val[NVALS][NPAST+1];  /* Device measurements/conversions   */
     time_t       time[NPAST+1];        /* Timestamps for each measurement   */
     size_t       n_current;            /* Index of current measurement      */
     size_t       n_previous;           /* Index of previous measurement     */
     hi_lo_t      today;                /* Today's extrema                   */
     hi_lo_t      yesterday;            /* Yesterday's extrema               */
     averages_t   avgs;                 /* Running averages                  */
     int          fld_dtype[NVALS];     /* DEV_DTYPE_ of val[i][]            */
     int          fld_used[NVALS];      /* val[i][] used if fld_used[i] != 0 */
     int          fld_units[NVALS];     /* DEV_UNIT_ of val[i][]             */
     const char  *fld_format[NVALS];    /* printf() format for val[i][]      */
     struct device_press_adj_s *pcor;   /* Pressure correction to sea level  */
} device_data_t;

/*
 *  device_t
 *
 *  For each device on the 1-Wire bus, a device_t structure is allocated.
 *  This structure provides storage for the device's ROM id, generic status
 *  flags, serialization mutices, collected measurements, device driver
 *  tables, device driver specific data, and grouping information.
 *
 *  Where possible, use the dev_ macros and subroutines to access individual
 *  fields in a device_t structure.
 */

typedef struct device_s {
     char                       romid[OWIRE_ID_LEN+1]; /* Hex string ROM id  */
     unsigned char              fcode;         /* Family code = romid[15]    */
     unsigned int               flags;         /* DEV_FLAGS_ flags           */
     float                      gain;          /* Correction gain            */
     float                      offset;        /* Correction offset          */
     struct timeval             lastcmd;       /* Time of last command       */
     os_pthread_mutex_t         mutex;         /* To serialize access        */
     device_data_t              data;          /* Device measurements        */
     struct device_dispatch_s  *driver;        /* Driver table               */
     void                      *private;       /* Device specific data       */
     size_t                     dlen;          /* Description length         */
     char                      *desc;          /* Device description         */
     size_t                     slen;          /* Length of spec             */
     char                      *spec;          /* Dev. specific config data  */
     device_group_t             group1;        /* Config-based grouping      */
     device_group_t             group2;        /* Device-based grouping      */
} device_t;


/*
 *  device_press_adj_t
 *
 *  Data used to adjust a barometer reading to mean sea level
 */

typedef struct device_press_adj_s {
     size_t     ssize;       /* Allocated structure size                     */
     float      alt_station; /* Geometrical alt. of the press. sensor (m)    */
     float      alt_adjust;  /* Geometrical alt. to adjust pressure to (m)   */
     size_t     fld_spare;  /* Spare field to use for the corrected pressure */
     size_t     fld_spare2; /* Spare field to use for alt. corrected pressure*/
     size_t     fld_press;  /* Station pressure field                        */
     size_t     ntemp;      /* Count of outside temperature values to average*/
     size_t     nrh;        /* Count of outside humidity values to average   */
     size_t    *temp_flds;  /* temp_devs[i]->data.val[temp_flds[i]] is temp  */
     size_t    *rh_flds;    /* rh_devs[i]->data.val[rh_flds[i]] is humidity  */
     device_t **temp_devs;  /* Outside temperature devices                   */
     device_t **rh_devs;    /* Outside humidity devices                      */
} device_press_adj_t;


/*
 *  Prototypes for the various routines which together form a device driver
 */

struct ha7net_s;

typedef int device_proc_drv_init_t(void);
typedef int device_proc_drv_done_t(void);
typedef int device_proc_init_t(struct ha7net_s *ctx, device_t *device,
  device_t *devices);
typedef int device_proc_done_t(struct ha7net_s *ctx, device_t *device,
  device_t *devices);
typedef int device_proc_read_t(struct ha7net_s *ctx, device_t *device,
  unsigned int flags);
typedef void device_proc_out_t(void *out_ctx, const char *fmt, ...);
typedef int device_proc_show_t(struct ha7net_s *ctx, device_t *device,
  unsigned int flags, device_proc_out_t *out, void *out_ctx);

/*
 *  device_dispatch_t
 *
 *  A device driver dispatch table.  For each possible family code,
 *  one or more device_dispatch_t structures exist describing the
 *  available device drivers for that family code.  As such, there's
 *  a minimum of 256 device_dispatch_t structures.  For bogus family
 *  code values (e.g., 0x00), a default device driver is supplied.
 *  That device driver always returns an error when its various routines
 *  are invoked.
 *
 *  Note that this structure allows for linked lists via the next field.
 *  This is how more than one device driver may be associated with a single
 *  family code value.  dev_init() will walk the list calling each drv_init()
 *  routine until:
 *
 *    ERR_OK is returned in which case that set of drivers is considered to
 *       be the correct set and the devices is flagged DEV_FLAGS_INITIALIZED,
 *
 *    ERR_EOM is returned in which case that set of drivers is considered to
 *       be a mismatch and the next set of drivers is tried, or
 *
 *    Some other error is returned in which case dev_init() stops the list
 *       walk and gives up looking for a driver for that specific device.
 *       A warning is issued and the device is flagged DEV_FLAGS_IGNORE.
 */

typedef struct device_dispatch_s {
     struct device_dispatch_s *next;     /* Next driver for this family code */
     unsigned char             fcode;    /* Family code                      */
     const char               *name;     /* Driver name                      */
     size_t                    name_len; /* Length of the driver name        */
     device_proc_drv_init_t   *drv_init; /* Init routine for these drivers   */
     device_proc_drv_done_t   *drv_done; /* De-init routine for these drivers*/
     device_proc_init_t       *init;     /* Per-device init routine          */
     device_proc_done_t       *done;     /* Per-device de-init routine       */
     device_proc_read_t       *read;     /* Perform a measurement/conversion */
     device_proc_show_t       *show;     /* Show device specific information */
} device_dispatch_t;


/*
 *  Initialize the device driver library.  It's only necessary to call
 *  this routine when device drivers will be used (e.g., calls to read
 *  measurements from devices).
 */

int dev_lib_init(void);

/*
 *  Shut down the device driver library.
 */

void dev_lib_done(void);

/*
 *  Set the debug level and output procedure for the dev_ library
 */

void dev_debug_set(debug_proc_t *proc, void *ctx, int flags);


/*
 *  Return a pointer to the linked list of device drivers associated with
 *  the supplied family code, "fc".  For family codes lacking any device
 *  drivers, the default driver table will be returned.  Those drivers
 *  return ERR_NO when used.
 */

device_dispatch_t *dev_driver_get(unsigned char fc, const char *hint,
  size_t hlen);


/*
 *  Return a pointer to a dynamically allocated and initialized array of
 *  devices_t structures.  The array will have ndevices + 1 entries with
 *  the final entry satisfying dev_flag_test(, DEV_FLAGS_END).  The array
 *  must be released with dev_array_free() so as to ensure that all allocated
 *  memory is properly released.
 */

device_t *dev_array(size_t ndevices);

/*
 *  Free a device_t array allocated by dev_array().
 */

void dev_array_free(device_t *devices);

/*
 *  Walk an array of device_t structures, initializing each device not
 *  marked DEV_FLAGS_IGNORE or DEV_FLAGS_INITIALIZED.  Devices merely
 *  marked DEV_FLAGS_ISSUB will be initialized. This may lead to spurious
 *  warnings such as with a AAG TAI 8570 Pressure Sensor: if the DS2406
 *  with the distinguishing TMEX file is found first, then the secondary
 *  DS2406 can first be marked ignore.  However, if the secondary DS2406
 *  appears first in the search results and is attempted first, then
 *  a spurious warning may arise indicating that no appropriate driver
 *  was located for the device.  (In this case, all is resolved when
 *  the DS2406 with TMEX file is subsequently initialized.)
 */

int dev_list_init(struct ha7net_s *ctx, device_t *device);


/*
 *  Walk an array of device_t structures, de-initializing each device
 *  which is marked DEV_FLAGS_INITIALIZED.
 */

int dev_list_done(struct ha7net_s *ctx, device_t *device);


/*
 *  Disassociate a group of devices.
 */
void dev_ungroup(device_t *dev);

/*
 *  Group together a list of devices
 */
int dev_group(const char *gname, device_t *dev, ...);

/*
 *  Get the first device in a group of devices
 */
device_t *dev_group_get(device_t *dev);

/*
 *  Get the next device from a group of devices
 */
#define dev_group_next(dev) ((dev) ? (dev)->group2.next : NULL)


/*
 *  Invoke a device's initialization procedure, if any
 */
device_proc_init_t dev_init;

/*
 *  Invoke a device's de-initialization procedure, if any
 */
device_proc_done_t dev_done;

/*
 *  Perform a measurement / conversion
 */
device_proc_read_t dev_read;

/*
 *  Compute belated statistics: normally done by dev_read()
 */
int dev_stats(device_t *dev, int fld_start, int fld_end, size_t fld_ignore1,
  size_t fld_ignore2);

/*
 *  Macros to set device flags, clear device flags, and test device flags
 *  *** These macros assume that dev != NULL ***
 */

#define dev_flag_set(dev,flag)   (dev)->flags |= (flag)
#define dev_flag_clear(dev,flag) (dev)->flags &= ~(flag)
#define dev_flag_test(dev,flag) ((dev)->flags & (flag))

/*
 *  Macros to serialize access to device_t fields
 *  *** These macros assume that dev != NULL ***
 */

#define dev_lock(dev)          os_pthread_mutex_lock(&(dev)->mutex)
#define dev_unlock(dev)        os_pthread_mutex_unlock(&(dev)->mutex)

/*
 *  Macros to access various device_t fields.  Please use these macros
 *  as it allows changes to the device_t structure.
 *
 *  *** These macros assume that dev != NULL ***
 */

#define dev_romid(dev)         ((dev)->romid)
#define dev_fcode(dev)         ((dev)->fcode)
#define dev_private(dev)       ((dev)->private)
#define dev_private_set(dev,p) (dev)->private = (void *)(p)
#define dev_pcor(dev)          ((dev)->data.pcor)
#define dev_pcor_set(dev,p)    (dev)->data.pcor = p
#define dev_driver(dev)        ((dev)->driver)
#define dev_dlen(dev)          ((dev)->dlen)
#define dev_desc(dev)          ((dev)->desc)
#define dev_desc_drv(dev)      (((dev)->driver) ? (dev)->driver->name : "")
#define dev_dlen_drv(dev)      (((dev)->driver) ? (dev)->driver->name_len : 0)
#define dev_gref(dev)          ((dev)->group1.ref)

/*
 *  Move today's extrema (dev->data.today) to yesterday's extrema and
 *  then reset today's extrema to intial values (e.g., -1.0E28 for the
 *  maxima and +1.0E28 for the minima).  This routine is primarily called
 *  by a thread which awakens once a day at 00:00 (midnight).
 */

void dev_hi_lo_reset(device_t *devs);

/*
 *  Utility routine to apply driver hints to the device list in an attempt
 *  to locate the most apt driver for a specific ROM ID.  This is useful
 *  when having to distinguish between otherwise indistinguishable hardware
 *  such as the AAG TAI 8540 Humidity Sensor and the HBI H3-R1-K Humidity
 *  Sensor.  Both use DS18S20 and DS2480(Z) devices and cannot be told apart.
 *  However, one uses a Honeywell HIH-3600 series humidity sensor while the
 *  other uses a HIH-4000 series sensor.  The two sensors have different
 *  temperature corrections and thus need slightly different computations
 *  to compute the adjusted RH.
 */

int dev_info_hints(device_t *devices, size_t ndevices,
  const device_loc_t *linfo);


/*
 *  Utility routine to merge disparate pieces of information into
 *  an array of device_t structures.
 */

int dev_info_merge(device_t *devices, size_t ndevices, int apply_hints,
  device_period_array_t periods, const device_loc_t *linfo,
  const device_ignore_t *ilist);


/*
 *  Add pressure correction data to a device which measures barometric pressure
 */

int dev_pcor_add(device_t *device, device_t *devices, int altitude);


/*
 *  Perform a pressure correction
 */

int dev_pcor_adjust(device_t *device, int period);


/*
 *  Convert a 1-Wire device id (ROM id) to a cannonical form (e.g.,
 *  all upper case).  It is important that ROM ids be cannonicalized
 *  for purposes of comparing ROM ids.
 */

void dev_romid_cannonical(char *dst, size_t dsrmaxlen, const char *src,
  size_t srclen);


/*
 *  Return a pointer to a NUL terminated, static string description of a
 *  a DEV_DTYPE_ constant (e.g., "precipitation" for DEV_DTYPE_RAIN).  The
 *  values of these strings are given by the compile-time configuration file
 *  xml_const.conf which is transformed into a C header file named
 *  xml_const.h by the make_includes.c program.
 */

const char *dev_dtypedescstr(int dtype);


/*
 *  Return a pointer to a NUL terminated, static string representation of
 *  a DEV_DTYPE_ constant (e.g., "rain" for DEV_DTYPE_RAIN).  The values
 *  of these strings are given by the compile-time configuration file
 *  xml_const.conf which is transformed into a C header file named
 *  xml_const.h by the make_includes.c program.
 */

const char *dev_dtypestr(int dtype);


/*
 *  Return a pointer to a NUL terminated, static string representation of
 *  a DEV_UNIT_ constant (e.g., "kPa" for DEV_UNIT_KPA).  The values
 *  of these strings are given by the compile-time configuration file
 *  xml_const.conf which is transformed into a C header file named
 *  xml_const.h by the make_includes.c program.
 */

const char *dev_unitstr(int units);

/*
 *  Return a pointer to a NUL terminated, static string describing the device
 *  family associated with the supplied device family code, "fc".  For
 *  unrecognized device codes, the string "Unknown family code 0xnn" is
 *  returned.  See owire_devices_private.h.
 */

const char *dev_strfcode(unsigned char fc);

/*
 *  Same as dev_strfcode() but will set the integer pointed at by the
 *  "unknown" argument to a non-zero value when the supplied family code "fc"
 *  is not recognized and otherwise set it to zero.
 */

const char *dev_strfcodeu(unsigned char fc, int *unknown);


#if !defined(__DEV_SKIP_DEBUG)

/*
 *  The following routines are intended for use by device drivers
 */

extern int dev_dotrace;

extern void dev_debug(const char *fmt, ...);
extern void dev_trace(const char *fmt, ...);
extern void dev_detail(const char *fmt, ...);

#endif

#if defined(DRIVER)
#undef DRIVER
#endif

#if defined(DECLARE)
#undef DECLARE
#endif

#define DRIVER(a1,a2,a3,a4,a5,a6,a7,a8)
#define DECLARE(a1,a2) \
     a1 a2;

#if defined(__cplusplus)
}
#endif

#endif /* !defined(__DEV_H__) */
