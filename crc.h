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
#if !defined(__CRC_H__)

#define __CRC_H__

#if defined(__cplusplus)
extern "C" {
#endif

/*
 *  int crc16(int crc, int ch)
 *
 *    Compute the CRC-16 of the byte ch using the short crc as the seed.
 *    The result of the computation is the return value.
 *
 *    For example, to compute the CRC-16 of the bytes 0x01 0x02 0x03,
 *    one would.
 *
 *      int crc = 0;
 *      crc = crc16(crc, 0x01);
 *      crc = crc16(crc, 0x02);
 *      crc = crc16(crc, 0x03);
 *
 *    Note that the CRC-16 has two useful properties. For n > 0 bytes
 *    data[1], ..., data[n-1] compute the CRC-16 over the data as
 *    follows.
 *
 *      crc = 0;
 *      for (i = 0; i < n; i++)
 *          crc = crc16(crc, data[i]);
 *
 *    Then, the two properties are
 *
 *       0x0000 == crc16(crc16(crc, 0xff & crc), 0xff & (crc >> 8));
 *       0xb001 == crc16(crc16(crc, 0xff & ~crc), 0xff & (~crc >> 8)));
 *
 *    These properties allow us to compare an expected CRC-16 or it's
 *    one's complement with the received data by computing the CRC-16 over
 *    the received data AND the expected CRC-16 [or 1's complement] and then
 *    seeing if the result is 0x0000 [or 0xb001].
 *
 *
 *  Call arguments:
 *
 *    int crc
 *      The 16-bit seed to combine with the low 8 bits of ch.  Used
 *      for input only.
 *
 *    int ch
 *      The byte to compute the CRC of.  Only the low 8 bits are used.
 *      Used for input only.
 *
 *  Return values:
 *
 *     16 bit CRC computation.
 */

int crc16(int crc, int ch);

/*
 *  unsigned char crc8(unsigned char crc, unsigned char ch)
 *
 *    Compute the 8-bit DOW CRC of the byte ch using the byte crc
 *    as the seed.  The result of the computation is the return value.
 *
 *    For example, to compute the DOW CRC of the bytes 0x01 0x02 0x03,
 *    one would use the following code:
 *
 *      unsigned char crc = 0x00;
 *      crc = crc8(crc, 0x01);
 *      crc = crc8(crc, 0x02);
 *      crc = crc8(crc, 0x03);
 *
 *    Note that the DOW CRC has two useful properties. For n > 0 bytes
 *    data[1], ..., data[n-1] compute the DOW CRC over the data as
 *    follows.
 *
 *      crc = 0x00;
 *      for (i = 0; i < n; i++)
 *          crc = crc8(crc, data[i]);
 *
 *    Then, the two properties are
 *
 *      0x00 == crc8(crc, crc);
 *      0x35 == crc8(crc, ~crc);
 *
 *    These properties allow us to compare an expected DOW CRC or it's
 *    one's complement with the received data by computing the CRC over
 *    the received data AND the expected CRC [or 1's complement] and then
 *    seeing if the result is 0x00 [or 0x35].
 *
 *  Call arguments:
 *
 *    unsigned char crc
 *      The seed to combine with ch.  Used for input only.
 *
 *    unsigned char ch
 *      The byte to compute the CRC of.  Used for input only.
 *
 *  Return values:
 *
 *     8 bit CRC computation.
 */

unsigned char crc8(unsigned char crc, unsigned char ch);

#if defined(__cplusplus)
}
#endif

#endif /* !defined(__CRC_H__) */
