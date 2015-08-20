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
#if !defined(__HA7NET_H__)

#define __HA7NET_H__

#include <stdarg.h>
#include "err.h"
#include "debug.h"
#include "http.h"
#include "device.h"

#if defined(__cplusplus)
extern "C" {
#endif

/*
 *  Maximum number of pages which can be read at once from a device.
 *  This limit is a compile-time limit and owes itself to the size
 *  of the stack buffer which ha7net_readpages() declares for its call
 *  to ha7net_getstuff().
 */
#define HA7NET_MAX_RESULTS   1024

/*
 *  Maximum length in bytes we accept for a DNS host name.  While we could
 *  get an "official" value from the platform's header files, that's a bunch
 *  of header files we'd just assume not import, thank you.
 */
#define MAX_HOST_LEN  64

/*
 *  Maximum length in bytes of a lock ID for the 1Wire bus.
 */
#define MAX_LOCK_LEN  32

/*
 *  ha7net_t
 *  ha7net_ call context.
 */
typedef struct ha7net_s {

     /* TCP connection to the 1Wire bus master's HTTP server                */
     http_conn_t    hconn;

     /* Last HTTP Response                                                  */
     http_msg_t     hresp;
     int            hresp_dispose;

     /* Lock info for our lock on the 1Wire bus                             */
     char           lockid[MAX_LOCK_LEN + 1];  /* ID for lock on the bus    */
     size_t         lockid_len;                /* Lock id length, bytes     */

     /* Last device addressed since a bus reset */
     struct device_s *current_device;

     /* We retain the following information in case we need to re-establish */
     /* our connection to the 1Wire bus master's HTTP server                */
     unsigned short port;                  /* TCP port for HTTP connection  */
     unsigned int   tmo;                   /* I/O timeout, milliseconds     */
     char           host[MAX_HOST_LEN+1];  /* Bus master's DNS host name    */
     size_t         host_len;              /* Host name length, bytes       */
} ha7net_t;


typedef struct {
     int     algorithm;     /* HA7NET_CRC_NONE, _8, _16               */
     size_t  start_byte;    /* First byte                             */
     size_t  nbytes;        /* Number of bytes                        */
     int     repeat_every;  /* Repeat length, including CRC bytes     */
} ha7net_crc_t;

#define HA7NET_CRC_NONE 0
#define HA7NET_CRC_8    1
#define HA7NET_CRC_16   2

#define HA7NET_CRC8(s,l,r)         { (HA7NET_CRC_8), (s), (l), (r) }
#define HA7NET_CRC16(s,l,r)        { (HA7NET_CRC_16), (s), (l), (r) }
#define HA7NET_CRC16_EVERY_1(s,l)  HA7NET_CRC16(s,l,1)
#define HA7NET_CRC16_EVERY_8(s,l)  HA7NET_CRC16(s,l,8)
#define HA7NET_CRC16_EVERY_32(s,l) HA7NET_CRC16(s,l,32)

#define HA7NET_FLAGS_NORESEND  0x01
#define HA7NET_FLAGS_NORESET   0x02
#define HA7NET_FLAGS_NOSELECT  0x04
#define HA7NET_FLAGS_RELEASE   0x08
#define HA7NET_FLAGS_SELECT    0x10
#define HA7NET_FLAGS_POWERDOWN 0x20

int ha7net_lib_init(void);
void ha7net_lib_done(void);
void ha7net_debug_set(debug_proc_t *proc, void *ctx, int flags);

int ha7net_open(ha7net_t *ctx, const char *host, unsigned short port,
  unsigned int timeout,int flags);
void ha7net_close(ha7net_t *ctx, int flags);
void ha7net_done(ha7net_t *ctx, int flags);

int ha7net_getlock(ha7net_t *ctx);
int ha7net_releaselock(ha7net_t *ctx);

int ha7net_powerdownbus(ha7net_t *ctx, int flags);
int ha7net_resetbus(ha7net_t *ctx, int flags);
int ha7net_addressdevice(ha7net_t *ctx, device_t *dev, int flags);

int ha7net_search(ha7net_t *ctx, device_t **devices, size_t *ndevices,
  unsigned char family_code, int cond_state, int flags);
void ha7net_search_free(device_t *devices);

int ha7net_readpages(ha7net_t *ctx, device_t *dev, char **data, size_t *dlen,
  size_t start_page, size_t npages, int flags);
int ha7net_readpages_ex(ha7net_t *ctx, device_t *dev, unsigned char *data,
  size_t minlen, size_t start_page, size_t npages, int flags);
void ha7net_readpages_free(char *data);

#define HA7NET_WRITEBLOCK_MAX 32

int ha7net_writeblock(ha7net_t *ctx, device_t *dev, char **data, size_t *dlen,
  const char *cmd, int flags);
int ha7net_writeblock_ex(ha7net_t *ctx, device_t *dev, unsigned char *data,
  size_t minlen, const char *cmd, ha7net_crc_t *crc, int flags);
void ha7net_writeblock_free(char *data);

const char *ha7net_last_response(ha7net_t *ctx, size_t *len);

void ha7net_debug_set(debug_proc_t *proc, void *ctx, int flags);

#if defined(__cplusplus)
}
#endif

#endif /* !defined(__HA7NET_H__) */
