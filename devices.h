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
/*
 *  At compile time, each device driver is loaded into the master
 *  device driver table via this header file.  device.c includes
 *  this header file multiple times, each time changing the definition
 *  of the DRIVER() and DECLARE() macros.
 *
 *  Each device driver should supply a header file which
 *
 *    1. Declares each driver procedure via the DECLARE() macro
 *
 *          DRIVER(procedure-type, procedure-name)
 *
 *       where "procedure-type" is one of the five types
 *
 *          device_proc_drv_init_t
 *          device_proc_drv_done_t
 *          device_proc_init_t
 *          device_proc_done_t
 *          device_proc_read_t
 *
 *       declared in device.h, and "procedure-name" is the name of a
 *       global C procedure provided by the device driver code and of
 *       the type "procedure-type".  Note that a device driver only
 *       needs to provide a procedure of type device_proc_read_t.  All
 *       other procedures are optional.
 *
 *    2. Provides device driver information via the DRIVER() macro:
 *
 *         DRIVER(name,fcode,drvinit,drvdone,devint,devdon,devread)
 *
 *       where
 *
 *         const char *name
 *           Brief name to associate with the driver.
 *
 *         unsigned char fcode
 *           Dallas Semiconductor 1-Wire family code associated with
 *           the device.  If the device driver supports more than one
 *           family code, then provide a DRIVER() statement for each
 *           family code.
 *
 *         device_proc_drv_init_t drvinit
 *         device_proc_drv_done_t drvdone
 *         device_proc_init_t devinit
 *         device_proc_done_t devdone
 *         device_proc_read_t devread
 *           The global device driver procedures provided by the driver.
 *           Only devread must be provided.  All other procedures may be
 *           omitted by supplying the value NULL.
 *
 */

#include "device_ds18s20.h"
#include "device_eds_aprobe.h"
#include "device_tai_8540.h"
#include "device_tai_8570.h"
#include "device_hbi_h3r1.h"
