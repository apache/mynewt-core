#ifndef NRF53_ERRATAS_H
#define NRF53_ERRATAS_H

/*

Copyright (c) 2010 - 2018, Nordic Semiconductor ASA All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

3. Neither the name of Nordic Semiconductor ASA nor the names of its
   contributors may be used to endorse or promote products derived from this
   software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY, AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL NORDIC SEMICONDUCTOR ASA OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

*/

#include <stdint.h>
#include <stdbool.h>
#include "compiler_abstraction.h"

static bool nrf53_errata_3(void) __UNUSED;
static bool nrf53_errata_4(void) __UNUSED;
static bool nrf53_errata_5(void) __UNUSED;
static bool nrf53_errata_6(void) __UNUSED;
static bool nrf53_errata_7(void) __UNUSED;
static bool nrf53_errata_8(void) __UNUSED;
static bool nrf53_errata_9(void) __UNUSED;
static bool nrf53_errata_10(void) __UNUSED;
static bool nrf53_errata_11(void) __UNUSED;
static bool nrf53_errata_12(void) __UNUSED;
static bool nrf53_errata_13(void) __UNUSED;
static bool nrf53_errata_14(void) __UNUSED;
static bool nrf53_errata_15(void) __UNUSED;
static bool nrf53_errata_16(void) __UNUSED;
static bool nrf53_errata_18(void) __UNUSED;
static bool nrf53_errata_19(void) __UNUSED;
static bool nrf53_errata_20(void) __UNUSED;
static bool nrf53_errata_21(void) __UNUSED;
static bool nrf53_errata_22(void) __UNUSED;
static bool nrf53_errata_23(void) __UNUSED;
static bool nrf53_errata_26(void) __UNUSED;
static bool nrf53_errata_27(void) __UNUSED;
static bool nrf53_errata_28(void) __UNUSED;
static bool nrf53_errata_29(void) __UNUSED;
static bool nrf53_errata_30(void) __UNUSED;
static bool nrf53_errata_32(void) __UNUSED;
static bool nrf53_errata_33(void) __UNUSED;
static bool nrf53_errata_37(void) __UNUSED;
static bool nrf53_errata_42(void) __UNUSED;
static bool nrf53_errata_43(void) __UNUSED;
static bool nrf53_errata_44(void) __UNUSED;
static bool nrf53_errata_45(void) __UNUSED;
static bool nrf53_errata_46(void) __UNUSED;
static bool nrf53_errata_47(void) __UNUSED;
static bool nrf53_errata_49(void) __UNUSED;
static bool nrf53_errata_50(void) __UNUSED;
static bool nrf53_errata_51(void) __UNUSED;
static bool nrf53_errata_53(void) __UNUSED;
static bool nrf53_errata_54(void) __UNUSED;
static bool nrf53_errata_55(void) __UNUSED;
static bool nrf53_errata_57(void) __UNUSED;
static bool nrf53_errata_58(void) __UNUSED;
static bool nrf53_errata_59(void) __UNUSED;
static bool nrf53_errata_62(void) __UNUSED;
static bool nrf53_errata_64(void) __UNUSED;
static bool nrf53_errata_65(void) __UNUSED;
static bool nrf53_errata_69(void) __UNUSED;
static bool nrf53_errata_72(void) __UNUSED;
static bool nrf53_errata_73(void) __UNUSED;
static bool nrf53_errata_74(void) __UNUSED;
static bool nrf53_errata_79(void) __UNUSED;
static bool nrf53_errata_80(void) __UNUSED;
static bool nrf53_errata_81(void) __UNUSED;
static bool nrf53_errata_82(void) __UNUSED;
static bool nrf53_errata_83(void) __UNUSED;
static bool nrf53_errata_84(void) __UNUSED;

static bool nrf53_errata_3(void)
{
    #ifndef NRF53_SERIES
        return false;
    #else
        #if defined (NRF5340_XXAA_APPLICATION) || defined (DEVELOP_IN_NRF5340_APPLICATION)
            uint32_t var1 = *(uint32_t *)0x00FF0130ul;
            uint32_t var2 = *(uint32_t *)0x00FF0134ul;
        #endif
        #if defined (NRF5340_XXAA_APPLICATION) || defined (DEVELOP_IN_NRF5340_APPLICATION)
            if (var1 == 0x07)
            {
                switch(var2)
                {
                    case 0x02ul:
                        return true;
                    case 0x03ul:
                        return true;
                }
            }
        #endif
        return false;
    #endif
}

static bool nrf53_errata_4(void)
{
    #ifndef NRF53_SERIES
        return false;
    #else
        #if defined (NRF5340_XXAA_APPLICATION) || defined (DEVELOP_IN_NRF5340_APPLICATION)
            uint32_t var1 = *(uint32_t *)0x00FF0130ul;
            uint32_t var2 = *(uint32_t *)0x00FF0134ul;
        #endif
        #if defined (NRF5340_XXAA_APPLICATION) || defined (DEVELOP_IN_NRF5340_APPLICATION)
            if (var1 == 0x07)
            {
                switch(var2)
                {
                    case 0x02ul:
                        return true;
                    case 0x03ul:
                        return false;
                }
            }
        #endif
        return false;
    #endif
}

static bool nrf53_errata_5(void)
{
    #ifndef NRF53_SERIES
        return false;
    #else
        #if defined (NRF5340_XXAA_APPLICATION) || defined (DEVELOP_IN_NRF5340_APPLICATION)
            uint32_t var1 = *(uint32_t *)0x00FF0130ul;
            uint32_t var2 = *(uint32_t *)0x00FF0134ul;
        #endif
        #if defined (NRF5340_XXAA_APPLICATION) || defined (DEVELOP_IN_NRF5340_APPLICATION)
            if (var1 == 0x07)
            {
                switch(var2)
                {
                    case 0x02ul:
                        return true;
                    case 0x03ul:
                        return false;
                }
            }
        #endif
        return false;
    #endif
}

static bool nrf53_errata_6(void)
{
    #ifndef NRF53_SERIES
        return false;
    #else
        #if defined (NRF5340_XXAA_NETWORK) || defined (DEVELOP_IN_NRF5340_NETWORK)
            uint32_t var1 = *(uint32_t *)0x01FF0130ul;
            uint32_t var2 = *(uint32_t *)0x01FF0134ul;
        #endif
        #if defined (NRF5340_XXAA_NETWORK) || defined (DEVELOP_IN_NRF5340_NETWORK)
            if (var1 == 0x07)
            {
                switch(var2)
                {
                    case 0x02ul:
                        return true;
                    case 0x03ul:
                        return false;
                }
            }
        #endif
        return false;
    #endif
}

static bool nrf53_errata_7(void)
{
    #ifndef NRF53_SERIES
        return false;
    #else
        #if defined (NRF5340_XXAA_APPLICATION) || defined (DEVELOP_IN_NRF5340_APPLICATION)
            uint32_t var1 = *(uint32_t *)0x00FF0130ul;
            uint32_t var2 = *(uint32_t *)0x00FF0134ul;
        #endif
        #if defined (NRF5340_XXAA_APPLICATION) || defined (DEVELOP_IN_NRF5340_APPLICATION)
            if (var1 == 0x07)
            {
                switch(var2)
                {
                    case 0x02ul:
                        return true;
                    case 0x03ul:
                        return false;
                }
            }
        #endif
        return false;
    #endif
}

static bool nrf53_errata_8(void)
{
    #ifndef NRF53_SERIES
        return false;
    #else
        #if defined (NRF5340_XXAA_APPLICATION) || defined (DEVELOP_IN_NRF5340_APPLICATION)
            uint32_t var1 = *(uint32_t *)0x00FF0130ul;
            uint32_t var2 = *(uint32_t *)0x00FF0134ul;
        #endif
        #if defined (NRF5340_XXAA_APPLICATION) || defined (DEVELOP_IN_NRF5340_APPLICATION)
            if (var1 == 0x07)
            {
                switch(var2)
                {
                    case 0x02ul:
                        return true;
                    case 0x03ul:
                        return false;
                }
            }
        #endif
        return false;
    #endif
}

static bool nrf53_errata_9(void)
{
    #ifndef NRF53_SERIES
        return false;
    #else
        #if defined (NRF5340_XXAA_APPLICATION) || defined (DEVELOP_IN_NRF5340_APPLICATION)
            uint32_t var1 = *(uint32_t *)0x00FF0130ul;
            uint32_t var2 = *(uint32_t *)0x00FF0134ul;
        #endif
        #if defined (NRF5340_XXAA_APPLICATION) || defined (DEVELOP_IN_NRF5340_APPLICATION)
            if (var1 == 0x07)
            {
                switch(var2)
                {
                    case 0x02ul:
                        return true;
                    case 0x03ul:
                        return false;
                }
            }
        #endif
        return false;
    #endif
}

static bool nrf53_errata_10(void)
{
    #ifndef NRF53_SERIES
        return false;
    #else
        #if defined (NRF5340_XXAA_NETWORK) || defined (DEVELOP_IN_NRF5340_NETWORK)
            uint32_t var1 = *(uint32_t *)0x01FF0130ul;
            uint32_t var2 = *(uint32_t *)0x01FF0134ul;
        #endif
        #if defined (NRF5340_XXAA_NETWORK) || defined (DEVELOP_IN_NRF5340_NETWORK)
            if (var1 == 0x07)
            {
                switch(var2)
                {
                    case 0x02ul:
                        return true;
                    case 0x03ul:
                        return false;
                }
            }
        #endif
        return false;
    #endif
}

static bool nrf53_errata_11(void)
{
    #ifndef NRF53_SERIES
        return false;
    #else
        #if defined (NRF5340_XXAA_NETWORK) || defined (DEVELOP_IN_NRF5340_NETWORK)
            uint32_t var1 = *(uint32_t *)0x01FF0130ul;
            uint32_t var2 = *(uint32_t *)0x01FF0134ul;
        #endif
        #if defined (NRF5340_XXAA_NETWORK) || defined (DEVELOP_IN_NRF5340_NETWORK)
            if (var1 == 0x07)
            {
                switch(var2)
                {
                    case 0x02ul:
                        return true;
                    case 0x03ul:
                        return false;
                }
            }
        #endif
        return false;
    #endif
}

static bool nrf53_errata_12(void)
{
    #ifndef NRF53_SERIES
        return false;
    #else
        #if defined (NRF5340_XXAA_APPLICATION) || defined (DEVELOP_IN_NRF5340_APPLICATION)
            uint32_t var1 = *(uint32_t *)0x00FF0130ul;
            uint32_t var2 = *(uint32_t *)0x00FF0134ul;
        #endif
        #if defined (NRF5340_XXAA_APPLICATION) || defined (DEVELOP_IN_NRF5340_APPLICATION)
            if (var1 == 0x07)
            {
                switch(var2)
                {
                    case 0x02ul:
                        return true;
                    case 0x03ul:
                        return true;
                }
            }
        #endif
        return false;
    #endif
}

static bool nrf53_errata_13(void)
{
    #ifndef NRF53_SERIES
        return false;
    #else
        #if defined (NRF5340_XXAA_APPLICATION) || defined (DEVELOP_IN_NRF5340_APPLICATION)\
         || defined (NRF5340_XXAA_NETWORK) || defined (DEVELOP_IN_NRF5340_NETWORK)
            #if defined(NRF_APPLICATION)
                uint32_t var1 = *(uint32_t *)0x00FF0130ul;
                uint32_t var2 = *(uint32_t *)0x00FF0134ul;
            #elif defined(NRF_NETWORK)
                uint32_t var1 = *(uint32_t *)0x01FF0130ul;
                uint32_t var2 = *(uint32_t *)0x01FF0134ul;
            #endif
        #endif
        #if defined (NRF5340_XXAA_APPLICATION) || defined (DEVELOP_IN_NRF5340_APPLICATION)\
         || defined (NRF5340_XXAA_NETWORK) || defined (DEVELOP_IN_NRF5340_NETWORK)
            if (var1 == 0x07)
            {
                switch(var2)
                {
                    case 0x02ul:
                        return true;
                    case 0x03ul:
                        return false;
                }
            }
        #endif
        return false;
    #endif
}

static bool nrf53_errata_14(void)
{
    #ifndef NRF53_SERIES
        return false;
    #else
        #if defined (NRF5340_XXAA_NETWORK) || defined (DEVELOP_IN_NRF5340_NETWORK)
            uint32_t var1 = *(uint32_t *)0x01FF0130ul;
            uint32_t var2 = *(uint32_t *)0x01FF0134ul;
        #endif
        #if defined (NRF5340_XXAA_NETWORK) || defined (DEVELOP_IN_NRF5340_NETWORK)
            if (var1 == 0x07)
            {
                switch(var2)
                {
                    case 0x02ul:
                        return true;
                    case 0x03ul:
                        return false;
                }
            }
        #endif
        return false;
    #endif
}

static bool nrf53_errata_15(void)
{
    #ifndef NRF53_SERIES
        return false;
    #else
        #if defined (NRF5340_XXAA_APPLICATION) || defined (DEVELOP_IN_NRF5340_APPLICATION)\
         || defined (NRF5340_XXAA_NETWORK) || defined (DEVELOP_IN_NRF5340_NETWORK)
            #if defined(NRF_APPLICATION)
                uint32_t var1 = *(uint32_t *)0x00FF0130ul;
                uint32_t var2 = *(uint32_t *)0x00FF0134ul;
            #elif defined(NRF_NETWORK)
                uint32_t var1 = *(uint32_t *)0x01FF0130ul;
                uint32_t var2 = *(uint32_t *)0x01FF0134ul;
            #endif
        #endif
        #if defined (NRF5340_XXAA_APPLICATION) || defined (DEVELOP_IN_NRF5340_APPLICATION)\
         || defined (NRF5340_XXAA_NETWORK) || defined (DEVELOP_IN_NRF5340_NETWORK)
            if (var1 == 0x07)
            {
                switch(var2)
                {
                    case 0x02ul:
                        return true;
                    case 0x03ul:
                        return false;
                }
            }
        #endif
        return false;
    #endif
}

static bool nrf53_errata_16(void)
{
    #ifndef NRF53_SERIES
        return false;
    #else
        #if defined (NRF5340_XXAA_NETWORK) || defined (DEVELOP_IN_NRF5340_NETWORK)
            uint32_t var1 = *(uint32_t *)0x01FF0130ul;
            uint32_t var2 = *(uint32_t *)0x01FF0134ul;
        #endif
        #if defined (NRF5340_XXAA_NETWORK) || defined (DEVELOP_IN_NRF5340_NETWORK)
            if (var1 == 0x07)
            {
                switch(var2)
                {
                    case 0x02ul:
                        return true;
                    case 0x03ul:
                        return false;
                }
            }
        #endif
        return false;
    #endif
}

static bool nrf53_errata_18(void)
{
    #ifndef NRF53_SERIES
        return false;
    #else
        #if defined (NRF5340_XXAA_APPLICATION) || defined (DEVELOP_IN_NRF5340_APPLICATION)
            uint32_t var1 = *(uint32_t *)0x00FF0130ul;
            uint32_t var2 = *(uint32_t *)0x00FF0134ul;
        #endif
        #if defined (NRF5340_XXAA_APPLICATION) || defined (DEVELOP_IN_NRF5340_APPLICATION)
            if (var1 == 0x07)
            {
                switch(var2)
                {
                    case 0x02ul:
                        return true;
                    case 0x03ul:
                        return false;
                }
            }
        #endif
        return false;
    #endif
}

static bool nrf53_errata_19(void)
{
    #ifndef NRF53_SERIES
        return false;
    #else
        #if defined (NRF5340_XXAA_APPLICATION) || defined (DEVELOP_IN_NRF5340_APPLICATION)
            uint32_t var1 = *(uint32_t *)0x00FF0130ul;
            uint32_t var2 = *(uint32_t *)0x00FF0134ul;
        #endif
        #if defined (NRF5340_XXAA_APPLICATION) || defined (DEVELOP_IN_NRF5340_APPLICATION)
            if (var1 == 0x07)
            {
                switch(var2)
                {
                    case 0x02ul:
                        return true;
                    case 0x03ul:
                        return false;
                }
            }
        #endif
        return false;
    #endif
}

static bool nrf53_errata_20(void)
{
    #ifndef NRF53_SERIES
        return false;
    #else
        #if defined (NRF5340_XXAA_APPLICATION) || defined (DEVELOP_IN_NRF5340_APPLICATION)\
         || defined (NRF5340_XXAA_NETWORK) || defined (DEVELOP_IN_NRF5340_NETWORK)
            #if defined(NRF_APPLICATION)
                uint32_t var1 = *(uint32_t *)0x00FF0130ul;
                uint32_t var2 = *(uint32_t *)0x00FF0134ul;
            #elif defined(NRF_NETWORK)
                uint32_t var1 = *(uint32_t *)0x01FF0130ul;
                uint32_t var2 = *(uint32_t *)0x01FF0134ul;
            #endif
        #endif
        #if defined (NRF5340_XXAA_APPLICATION) || defined (DEVELOP_IN_NRF5340_APPLICATION)\
         || defined (NRF5340_XXAA_NETWORK) || defined (DEVELOP_IN_NRF5340_NETWORK)
            if (var1 == 0x07)
            {
                switch(var2)
                {
                    case 0x02ul:
                        return true;
                    case 0x03ul:
                        return false;
                }
            }
        #endif
        return false;
    #endif
}

static bool nrf53_errata_21(void)
{
    #ifndef NRF53_SERIES
        return false;
    #else
        #if defined (NRF5340_XXAA_APPLICATION) || defined (DEVELOP_IN_NRF5340_APPLICATION)\
         || defined (NRF5340_XXAA_NETWORK) || defined (DEVELOP_IN_NRF5340_NETWORK)
            #if defined(NRF_APPLICATION)
                uint32_t var1 = *(uint32_t *)0x00FF0130ul;
                uint32_t var2 = *(uint32_t *)0x00FF0134ul;
            #elif defined(NRF_NETWORK)
                uint32_t var1 = *(uint32_t *)0x01FF0130ul;
                uint32_t var2 = *(uint32_t *)0x01FF0134ul;
            #endif
        #endif
        #if defined (NRF5340_XXAA_APPLICATION) || defined (DEVELOP_IN_NRF5340_APPLICATION)\
         || defined (NRF5340_XXAA_NETWORK) || defined (DEVELOP_IN_NRF5340_NETWORK)
            if (var1 == 0x07)
            {
                switch(var2)
                {
                    case 0x02ul:
                        return true;
                    case 0x03ul:
                        return true;
                }
            }
        #endif
        return false;
    #endif
}

static bool nrf53_errata_22(void)
{
    #ifndef NRF53_SERIES
        return false;
    #else
        #if defined (NRF5340_XXAA_APPLICATION) || defined (DEVELOP_IN_NRF5340_APPLICATION)
            uint32_t var1 = *(uint32_t *)0x00FF0130ul;
            uint32_t var2 = *(uint32_t *)0x00FF0134ul;
        #endif
        #if defined (NRF5340_XXAA_APPLICATION) || defined (DEVELOP_IN_NRF5340_APPLICATION)
            if (var1 == 0x07)
            {
                switch(var2)
                {
                    case 0x02ul:
                        return true;
                    case 0x03ul:
                        return false;
                }
            }
        #endif
        return false;
    #endif
}

static bool nrf53_errata_23(void)
{
    #ifndef NRF53_SERIES
        return false;
    #else
        #if defined (NRF5340_XXAA_APPLICATION) || defined (DEVELOP_IN_NRF5340_APPLICATION)
            uint32_t var1 = *(uint32_t *)0x00FF0130ul;
            uint32_t var2 = *(uint32_t *)0x00FF0134ul;
        #endif
        #if defined (NRF5340_XXAA_APPLICATION) || defined (DEVELOP_IN_NRF5340_APPLICATION)
            if (var1 == 0x07)
            {
                switch(var2)
                {
                    case 0x02ul:
                        return true;
                    case 0x03ul:
                        return false;
                }
            }
        #endif
        return false;
    #endif
}

static bool nrf53_errata_26(void)
{
    #ifndef NRF53_SERIES
        return false;
    #else
        #if defined (NRF5340_XXAA_APPLICATION) || defined (DEVELOP_IN_NRF5340_APPLICATION)\
         || defined (NRF5340_XXAA_NETWORK) || defined (DEVELOP_IN_NRF5340_NETWORK)
            #if defined(NRF_APPLICATION)
                uint32_t var1 = *(uint32_t *)0x00FF0130ul;
                uint32_t var2 = *(uint32_t *)0x00FF0134ul;
            #elif defined(NRF_NETWORK)
                uint32_t var1 = *(uint32_t *)0x01FF0130ul;
                uint32_t var2 = *(uint32_t *)0x01FF0134ul;
            #endif
        #endif
        #if defined (NRF5340_XXAA_APPLICATION) || defined (DEVELOP_IN_NRF5340_APPLICATION)\
         || defined (NRF5340_XXAA_NETWORK) || defined (DEVELOP_IN_NRF5340_NETWORK)
            if (var1 == 0x07)
            {
                switch(var2)
                {
                    case 0x02ul:
                        return true;
                    case 0x03ul:
                        return false;
                }
            }
        #endif
        return false;
    #endif
}

static bool nrf53_errata_27(void)
{
    #ifndef NRF53_SERIES
        return false;
    #else
        #if defined (NRF5340_XXAA_APPLICATION) || defined (DEVELOP_IN_NRF5340_APPLICATION)\
         || defined (NRF5340_XXAA_NETWORK) || defined (DEVELOP_IN_NRF5340_NETWORK)
            #if defined(NRF_APPLICATION)
                uint32_t var1 = *(uint32_t *)0x00FF0130ul;
                uint32_t var2 = *(uint32_t *)0x00FF0134ul;
            #elif defined(NRF_NETWORK)
                uint32_t var1 = *(uint32_t *)0x01FF0130ul;
                uint32_t var2 = *(uint32_t *)0x01FF0134ul;
            #endif
        #endif
        #if defined (NRF5340_XXAA_APPLICATION) || defined (DEVELOP_IN_NRF5340_APPLICATION)\
         || defined (NRF5340_XXAA_NETWORK) || defined (DEVELOP_IN_NRF5340_NETWORK)
            if (var1 == 0x07)
            {
                switch(var2)
                {
                    case 0x02ul:
                        return true;
                    case 0x03ul:
                        return false;
                }
            }
        #endif
        return false;
    #endif
}

static bool nrf53_errata_28(void)
{
    #ifndef NRF53_SERIES
        return false;
    #else
        #if defined (NRF5340_XXAA_APPLICATION) || defined (DEVELOP_IN_NRF5340_APPLICATION)\
         || defined (NRF5340_XXAA_NETWORK) || defined (DEVELOP_IN_NRF5340_NETWORK)
            #if defined(NRF_APPLICATION)
                uint32_t var1 = *(uint32_t *)0x00FF0130ul;
                uint32_t var2 = *(uint32_t *)0x00FF0134ul;
            #elif defined(NRF_NETWORK)
                uint32_t var1 = *(uint32_t *)0x01FF0130ul;
                uint32_t var2 = *(uint32_t *)0x01FF0134ul;
            #endif
        #endif
        #if defined (NRF5340_XXAA_APPLICATION) || defined (DEVELOP_IN_NRF5340_APPLICATION)\
         || defined (NRF5340_XXAA_NETWORK) || defined (DEVELOP_IN_NRF5340_NETWORK)
            if (var1 == 0x07)
            {
                switch(var2)
                {
                    case 0x02ul:
                        return true;
                    case 0x03ul:
                        return false;
                }
            }
        #endif
        return false;
    #endif
}

static bool nrf53_errata_29(void)
{
    #ifndef NRF53_SERIES
        return false;
    #else
        #if defined (NRF5340_XXAA_NETWORK) || defined (DEVELOP_IN_NRF5340_NETWORK)
            uint32_t var1 = *(uint32_t *)0x01FF0130ul;
            uint32_t var2 = *(uint32_t *)0x01FF0134ul;
        #endif
        #if defined (NRF5340_XXAA_NETWORK) || defined (DEVELOP_IN_NRF5340_NETWORK)
            if (var1 == 0x07)
            {
                switch(var2)
                {
                    case 0x02ul:
                        return true;
                    case 0x03ul:
                        return false;
                }
            }
        #endif
        return false;
    #endif
}

static bool nrf53_errata_30(void)
{
    #ifndef NRF53_SERIES
        return false;
    #else
        #if defined (NRF5340_XXAA_NETWORK) || defined (DEVELOP_IN_NRF5340_NETWORK)
            uint32_t var1 = *(uint32_t *)0x01FF0130ul;
            uint32_t var2 = *(uint32_t *)0x01FF0134ul;
        #endif
        #if defined (NRF5340_XXAA_NETWORK) || defined (DEVELOP_IN_NRF5340_NETWORK)
            if (var1 == 0x07)
            {
                switch(var2)
                {
                    case 0x02ul:
                        return true;
                    case 0x03ul:
                        return false;
                }
            }
        #endif
        return false;
    #endif
}

static bool nrf53_errata_32(void)
{
    #ifndef NRF53_SERIES
        return false;
    #else
        #if defined (NRF5340_XXAA_NETWORK) || defined (DEVELOP_IN_NRF5340_NETWORK)
            uint32_t var1 = *(uint32_t *)0x01FF0130ul;
            uint32_t var2 = *(uint32_t *)0x01FF0134ul;
        #endif
        #if defined (NRF5340_XXAA_NETWORK) || defined (DEVELOP_IN_NRF5340_NETWORK)
            if (var1 == 0x07)
            {
                switch(var2)
                {
                    case 0x02ul:
                        return true;
                    case 0x03ul:
                        return false;
                }
            }
        #endif
        return false;
    #endif
}

static bool nrf53_errata_33(void)
{
    #ifndef NRF53_SERIES
        return false;
    #else
        #if defined (NRF5340_XXAA_APPLICATION) || defined (DEVELOP_IN_NRF5340_APPLICATION)
            uint32_t var1 = *(uint32_t *)0x00FF0130ul;
            uint32_t var2 = *(uint32_t *)0x00FF0134ul;
        #endif
        #if defined (NRF5340_XXAA_APPLICATION) || defined (DEVELOP_IN_NRF5340_APPLICATION)
            if (var1 == 0x07)
            {
                switch(var2)
                {
                    case 0x02ul:
                        return true;
                    case 0x03ul:
                        return false;
                }
            }
        #endif
        return false;
    #endif
}

static bool nrf53_errata_37(void)
{
    #ifndef NRF53_SERIES
        return false;
    #else
        #if defined (NRF5340_XXAA_APPLICATION) || defined (DEVELOP_IN_NRF5340_APPLICATION)\
         || defined (NRF5340_XXAA_NETWORK) || defined (DEVELOP_IN_NRF5340_NETWORK)
            #if defined(NRF_APPLICATION)
                uint32_t var1 = *(uint32_t *)0x00FF0130ul;
                uint32_t var2 = *(uint32_t *)0x00FF0134ul;
            #elif defined(NRF_NETWORK)
                uint32_t var1 = *(uint32_t *)0x01FF0130ul;
                uint32_t var2 = *(uint32_t *)0x01FF0134ul;
            #endif
        #endif
        #if defined (NRF5340_XXAA_APPLICATION) || defined (DEVELOP_IN_NRF5340_APPLICATION)\
         || defined (NRF5340_XXAA_NETWORK) || defined (DEVELOP_IN_NRF5340_NETWORK)
            if (var1 == 0x07)
            {
                switch(var2)
                {
                    case 0x02ul:
                        return true;
                    case 0x03ul:
                        return false;
                }
            }
        #endif
        return false;
    #endif
}

static bool nrf53_errata_42(void)
{
    #ifndef NRF53_SERIES
        return false;
    #else
        #if defined (NRF5340_XXAA_APPLICATION) || defined (DEVELOP_IN_NRF5340_APPLICATION)
            uint32_t var1 = *(uint32_t *)0x00FF0130ul;
            uint32_t var2 = *(uint32_t *)0x00FF0134ul;
        #endif
        #if defined (NRF5340_XXAA_APPLICATION) || defined (DEVELOP_IN_NRF5340_APPLICATION)
            if (var1 == 0x07)
            {
                switch(var2)
                {
                    case 0x02ul:
                        return true;
                    case 0x03ul:
                        return false;
                }
            }
        #endif
        return false;
    #endif
}

static bool nrf53_errata_43(void)
{
    #ifndef NRF53_SERIES
        return false;
    #else
        #if defined (NRF5340_XXAA_APPLICATION) || defined (DEVELOP_IN_NRF5340_APPLICATION)
            uint32_t var1 = *(uint32_t *)0x00FF0130ul;
            uint32_t var2 = *(uint32_t *)0x00FF0134ul;
        #endif
        #if defined (NRF5340_XXAA_APPLICATION) || defined (DEVELOP_IN_NRF5340_APPLICATION)
            if (var1 == 0x07)
            {
                switch(var2)
                {
                    case 0x02ul:
                        return true;
                    case 0x03ul:
                        return true;
                }
            }
        #endif
        return false;
    #endif
}

static bool nrf53_errata_44(void)
{
    #ifndef NRF53_SERIES
        return false;
    #else
        #if defined (NRF5340_XXAA_APPLICATION) || defined (DEVELOP_IN_NRF5340_APPLICATION)\
         || defined (NRF5340_XXAA_NETWORK) || defined (DEVELOP_IN_NRF5340_NETWORK)
            #if defined(NRF_APPLICATION)
                uint32_t var1 = *(uint32_t *)0x00FF0130ul;
                uint32_t var2 = *(uint32_t *)0x00FF0134ul;
            #elif defined(NRF_NETWORK)
                uint32_t var1 = *(uint32_t *)0x01FF0130ul;
                uint32_t var2 = *(uint32_t *)0x01FF0134ul;
            #endif
        #endif
        #if defined (NRF5340_XXAA_APPLICATION) || defined (DEVELOP_IN_NRF5340_APPLICATION)\
         || defined (NRF5340_XXAA_NETWORK) || defined (DEVELOP_IN_NRF5340_NETWORK)
            if (var1 == 0x07)
            {
                switch(var2)
                {
                    case 0x02ul:
                        return true;
                    case 0x03ul:
                        return false;
                }
            }
        #endif
        return false;
    #endif
}

static bool nrf53_errata_45(void)
{
    #ifndef NRF53_SERIES
        return false;
    #else
        #if defined (NRF5340_XXAA_APPLICATION) || defined (DEVELOP_IN_NRF5340_APPLICATION)
            uint32_t var1 = *(uint32_t *)0x00FF0130ul;
            uint32_t var2 = *(uint32_t *)0x00FF0134ul;
        #endif
        #if defined (NRF5340_XXAA_APPLICATION) || defined (DEVELOP_IN_NRF5340_APPLICATION)
            if (var1 == 0x07)
            {
                switch(var2)
                {
                    case 0x02ul:
                        return true;
                    case 0x03ul:
                        return true;
                }
            }
        #endif
        return false;
    #endif
}

static bool nrf53_errata_46(void)
{
    #ifndef NRF53_SERIES
        return false;
    #else
        #if defined (NRF5340_XXAA_APPLICATION) || defined (DEVELOP_IN_NRF5340_APPLICATION)
            uint32_t var1 = *(uint32_t *)0x00FF0130ul;
            uint32_t var2 = *(uint32_t *)0x00FF0134ul;
        #endif
        #if defined (NRF5340_XXAA_APPLICATION) || defined (DEVELOP_IN_NRF5340_APPLICATION)
            if (var1 == 0x07)
            {
                switch(var2)
                {
                    case 0x02ul:
                        return true;
                    case 0x03ul:
                        return false;
                }
            }
        #endif
        return false;
    #endif
}

static bool nrf53_errata_47(void)
{
    #ifndef NRF53_SERIES
        return false;
    #else
        #if defined (NRF5340_XXAA_APPLICATION) || defined (DEVELOP_IN_NRF5340_APPLICATION)\
         || defined (NRF5340_XXAA_NETWORK) || defined (DEVELOP_IN_NRF5340_NETWORK)
            #if defined(NRF_APPLICATION)
                uint32_t var1 = *(uint32_t *)0x00FF0130ul;
                uint32_t var2 = *(uint32_t *)0x00FF0134ul;
            #elif defined(NRF_NETWORK)
                uint32_t var1 = *(uint32_t *)0x01FF0130ul;
                uint32_t var2 = *(uint32_t *)0x01FF0134ul;
            #endif
        #endif
        #if defined (NRF5340_XXAA_APPLICATION) || defined (DEVELOP_IN_NRF5340_APPLICATION)\
         || defined (NRF5340_XXAA_NETWORK) || defined (DEVELOP_IN_NRF5340_NETWORK)
            if (var1 == 0x07)
            {
                switch(var2)
                {
                    case 0x02ul:
                        return true;
                    case 0x03ul:
                        return true;
                }
            }
        #endif
        return false;
    #endif
}

static bool nrf53_errata_49(void)
{
    #ifndef NRF53_SERIES
        return false;
    #else
        #if defined (NRF5340_XXAA_APPLICATION) || defined (DEVELOP_IN_NRF5340_APPLICATION)\
         || defined (NRF5340_XXAA_NETWORK) || defined (DEVELOP_IN_NRF5340_NETWORK)
            #if defined(NRF_APPLICATION)
                uint32_t var1 = *(uint32_t *)0x00FF0130ul;
                uint32_t var2 = *(uint32_t *)0x00FF0134ul;
            #elif defined(NRF_NETWORK)
                uint32_t var1 = *(uint32_t *)0x01FF0130ul;
                uint32_t var2 = *(uint32_t *)0x01FF0134ul;
            #endif
        #endif
        #if defined (NRF5340_XXAA_APPLICATION) || defined (DEVELOP_IN_NRF5340_APPLICATION)\
         || defined (NRF5340_XXAA_NETWORK) || defined (DEVELOP_IN_NRF5340_NETWORK)
            if (var1 == 0x07)
            {
                switch(var2)
                {
                    case 0x02ul:
                        return true;
                    case 0x03ul:
                        return false;
                }
            }
        #endif
        return false;
    #endif
}

static bool nrf53_errata_50(void)
{
    #ifndef NRF53_SERIES
        return false;
    #else
        #if defined (NRF5340_XXAA_APPLICATION) || defined (DEVELOP_IN_NRF5340_APPLICATION)
            uint32_t var1 = *(uint32_t *)0x00FF0130ul;
            uint32_t var2 = *(uint32_t *)0x00FF0134ul;
        #endif
        #if defined (NRF5340_XXAA_APPLICATION) || defined (DEVELOP_IN_NRF5340_APPLICATION)
            if (var1 == 0x07)
            {
                switch(var2)
                {
                    case 0x02ul:
                        return true;
                    case 0x03ul:
                        return false;
                }
            }
        #endif
        return false;
    #endif
}

static bool nrf53_errata_51(void)
{
    #ifndef NRF53_SERIES
        return false;
    #else
        #if defined (NRF5340_XXAA_APPLICATION) || defined (DEVELOP_IN_NRF5340_APPLICATION)
            uint32_t var1 = *(uint32_t *)0x00FF0130ul;
            uint32_t var2 = *(uint32_t *)0x00FF0134ul;
        #endif
        #if defined (NRF5340_XXAA_APPLICATION) || defined (DEVELOP_IN_NRF5340_APPLICATION)
            if (var1 == 0x07)
            {
                switch(var2)
                {
                    case 0x02ul:
                        return true;
                    case 0x03ul:
                        return false;
                }
            }
        #endif
        return false;
    #endif
}

static bool nrf53_errata_53(void)
{
    #ifndef NRF53_SERIES
        return false;
    #else
        #if defined (NRF5340_XXAA_APPLICATION) || defined (DEVELOP_IN_NRF5340_APPLICATION)
            uint32_t var1 = *(uint32_t *)0x00FF0130ul;
            uint32_t var2 = *(uint32_t *)0x00FF0134ul;
        #endif
        #if defined (NRF5340_XXAA_APPLICATION) || defined (DEVELOP_IN_NRF5340_APPLICATION)
            if (var1 == 0x07)
            {
                switch(var2)
                {
                    case 0x02ul:
                        return true;
                    case 0x03ul:
                        return false;
                }
            }
        #endif
        return false;
    #endif
}

static bool nrf53_errata_54(void)
{
    #ifndef NRF53_SERIES
        return false;
    #else
        #if defined (NRF5340_XXAA_NETWORK) || defined (DEVELOP_IN_NRF5340_NETWORK)
            uint32_t var1 = *(uint32_t *)0x01FF0130ul;
            uint32_t var2 = *(uint32_t *)0x01FF0134ul;
        #endif
        #if defined (NRF5340_XXAA_NETWORK) || defined (DEVELOP_IN_NRF5340_NETWORK)
            if (var1 == 0x07)
            {
                switch(var2)
                {
                    case 0x02ul:
                        return true;
                    case 0x03ul:
                        return false;
                }
            }
        #endif
        return false;
    #endif
}

static bool nrf53_errata_55(void)
{
    #ifndef NRF53_SERIES
        return false;
    #else
        #if defined (NRF5340_XXAA_APPLICATION) || defined (DEVELOP_IN_NRF5340_APPLICATION)\
         || defined (NRF5340_XXAA_NETWORK) || defined (DEVELOP_IN_NRF5340_NETWORK)
            #if defined(NRF_APPLICATION)
                uint32_t var1 = *(uint32_t *)0x00FF0130ul;
                uint32_t var2 = *(uint32_t *)0x00FF0134ul;
            #elif defined(NRF_NETWORK)
                uint32_t var1 = *(uint32_t *)0x01FF0130ul;
                uint32_t var2 = *(uint32_t *)0x01FF0134ul;
            #endif
        #endif
        #if defined (NRF5340_XXAA_APPLICATION) || defined (DEVELOP_IN_NRF5340_APPLICATION)\
         || defined (NRF5340_XXAA_NETWORK) || defined (DEVELOP_IN_NRF5340_NETWORK)
            if (var1 == 0x07)
            {
                switch(var2)
                {
                    case 0x02ul:
                        return true;
                    case 0x03ul:
                        return true;
                }
            }
        #endif
        return false;
    #endif
}

static bool nrf53_errata_57(void)
{
    #ifndef NRF53_SERIES
        return false;
    #else
        #if defined (NRF5340_XXAA_APPLICATION) || defined (DEVELOP_IN_NRF5340_APPLICATION)
            uint32_t var1 = *(uint32_t *)0x00FF0130ul;
            uint32_t var2 = *(uint32_t *)0x00FF0134ul;
        #endif
        #if defined (NRF5340_XXAA_APPLICATION) || defined (DEVELOP_IN_NRF5340_APPLICATION)
            if (var1 == 0x07)
            {
                switch(var2)
                {
                    case 0x02ul:
                        return true;
                    case 0x03ul:
                        return false;
                }
            }
        #endif
        return false;
    #endif
}

static bool nrf53_errata_58(void)
{
    #ifndef NRF53_SERIES
        return false;
    #else
        #if defined (NRF5340_XXAA_APPLICATION) || defined (DEVELOP_IN_NRF5340_APPLICATION)
            uint32_t var1 = *(uint32_t *)0x00FF0130ul;
            uint32_t var2 = *(uint32_t *)0x00FF0134ul;
        #endif
        #if defined (NRF5340_XXAA_APPLICATION) || defined (DEVELOP_IN_NRF5340_APPLICATION)
            if (var1 == 0x07)
            {
                switch(var2)
                {
                    case 0x02ul:
                        return true;
                    case 0x03ul:
                        return false;
                }
            }
        #endif
        return false;
    #endif
}

static bool nrf53_errata_59(void)
{
    #ifndef NRF53_SERIES
        return false;
    #else
        #if defined (NRF5340_XXAA_APPLICATION) || defined (DEVELOP_IN_NRF5340_APPLICATION)
            uint32_t var1 = *(uint32_t *)0x00FF0130ul;
            uint32_t var2 = *(uint32_t *)0x00FF0134ul;
        #endif
        #if defined (NRF5340_XXAA_APPLICATION) || defined (DEVELOP_IN_NRF5340_APPLICATION)
            if (var1 == 0x07)
            {
                switch(var2)
                {
                    case 0x02ul:
                        return true;
                    case 0x03ul:
                        return false;
                }
            }
        #endif
        return false;
    #endif
}

static bool nrf53_errata_62(void)
{
    #ifndef NRF53_SERIES
        return false;
    #else
        #if defined (NRF5340_XXAA_APPLICATION) || defined (DEVELOP_IN_NRF5340_APPLICATION)\
         || defined (NRF5340_XXAA_NETWORK) || defined (DEVELOP_IN_NRF5340_NETWORK)
            #if defined(NRF_APPLICATION)
                uint32_t var1 = *(uint32_t *)0x00FF0130ul;
                uint32_t var2 = *(uint32_t *)0x00FF0134ul;
            #elif defined(NRF_NETWORK)
                uint32_t var1 = *(uint32_t *)0x01FF0130ul;
                uint32_t var2 = *(uint32_t *)0x01FF0134ul;
            #endif
        #endif
        #if defined (NRF5340_XXAA_APPLICATION) || defined (DEVELOP_IN_NRF5340_APPLICATION)\
         || defined (NRF5340_XXAA_NETWORK) || defined (DEVELOP_IN_NRF5340_NETWORK)
            if (var1 == 0x07)
            {
                switch(var2)
                {
                    case 0x02ul:
                        return true;
                    case 0x03ul:
                        return false;
                }
            }
        #endif
        return false;
    #endif
}

static bool nrf53_errata_64(void)
{
    #ifndef NRF53_SERIES
        return false;
    #else
        #if defined (NRF5340_XXAA_APPLICATION) || defined (DEVELOP_IN_NRF5340_APPLICATION)\
         || defined (NRF5340_XXAA_NETWORK) || defined (DEVELOP_IN_NRF5340_NETWORK)
            #if defined(NRF_APPLICATION)
                uint32_t var1 = *(uint32_t *)0x00FF0130ul;
                uint32_t var2 = *(uint32_t *)0x00FF0134ul;
            #elif defined(NRF_NETWORK)
                uint32_t var1 = *(uint32_t *)0x01FF0130ul;
                uint32_t var2 = *(uint32_t *)0x01FF0134ul;
            #endif
        #endif
        #if defined (NRF5340_XXAA_APPLICATION) || defined (DEVELOP_IN_NRF5340_APPLICATION)\
         || defined (NRF5340_XXAA_NETWORK) || defined (DEVELOP_IN_NRF5340_NETWORK)
            if (var1 == 0x07)
            {
                switch(var2)
                {
                    case 0x02ul:
                        return true;
                    case 0x03ul:
                        return false;
                }
            }
        #endif
        return false;
    #endif
}

static bool nrf53_errata_65(void)
{
    #ifndef NRF53_SERIES
        return false;
    #else
        #if defined (NRF5340_XXAA_APPLICATION) || defined (DEVELOP_IN_NRF5340_APPLICATION)
            uint32_t var1 = *(uint32_t *)0x00FF0130ul;
            uint32_t var2 = *(uint32_t *)0x00FF0134ul;
        #endif
        #if defined (NRF5340_XXAA_APPLICATION) || defined (DEVELOP_IN_NRF5340_APPLICATION)
            if (var1 == 0x07)
            {
                switch(var2)
                {
                    case 0x02ul:
                        return true;
                    case 0x03ul:
                        return true;
                }
            }
        #endif
        return false;
    #endif
}

static bool nrf53_errata_69(void)
{
    #ifndef NRF53_SERIES
        return false;
    #else
        #if defined (NRF5340_XXAA_APPLICATION) || defined (DEVELOP_IN_NRF5340_APPLICATION)
            uint32_t var1 = *(uint32_t *)0x00FF0130ul;
            uint32_t var2 = *(uint32_t *)0x00FF0134ul;
        #endif
        #if defined (NRF5340_XXAA_APPLICATION) || defined (DEVELOP_IN_NRF5340_APPLICATION)
            if (var1 == 0x07)
            {
                switch(var2)
                {
                    case 0x02ul:
                        return true;
                    case 0x03ul:
                        return false;
                }
            }
        #endif
        return false;
    #endif
}

static bool nrf53_errata_72(void)
{
    #ifndef NRF53_SERIES
        return false;
    #else
        #if defined (NRF5340_XXAA_APPLICATION) || defined (DEVELOP_IN_NRF5340_APPLICATION)
            uint32_t var1 = *(uint32_t *)0x00FF0130ul;
            uint32_t var2 = *(uint32_t *)0x00FF0134ul;
        #endif
        #if defined (NRF5340_XXAA_APPLICATION) || defined (DEVELOP_IN_NRF5340_APPLICATION)
            if (var1 == 0x07)
            {
                switch(var2)
                {
                    case 0x02ul:
                        return true;
                    case 0x03ul:
                        return false;
                }
            }
        #endif
        return false;
    #endif
}

static bool nrf53_errata_73(void)
{
    #ifndef NRF53_SERIES
        return false;
    #else
        #if defined (NRF5340_XXAA_APPLICATION) || defined (DEVELOP_IN_NRF5340_APPLICATION)\
         || defined (NRF5340_XXAA_NETWORK) || defined (DEVELOP_IN_NRF5340_NETWORK)
            #if defined(NRF_APPLICATION)
                uint32_t var1 = *(uint32_t *)0x00FF0130ul;
                uint32_t var2 = *(uint32_t *)0x00FF0134ul;
            #elif defined(NRF_NETWORK)
                uint32_t var1 = *(uint32_t *)0x01FF0130ul;
                uint32_t var2 = *(uint32_t *)0x01FF0134ul;
            #endif
        #endif
        #if defined (NRF5340_XXAA_APPLICATION) || defined (DEVELOP_IN_NRF5340_APPLICATION)\
         || defined (NRF5340_XXAA_NETWORK) || defined (DEVELOP_IN_NRF5340_NETWORK)
            if (var1 == 0x07)
            {
                switch(var2)
                {
                    case 0x02ul:
                        return true;
                    case 0x03ul:
                        return false;
                }
            }
        #endif
        return false;
    #endif
}

static bool nrf53_errata_74(void)
{
    #ifndef NRF53_SERIES
        return false;
    #else
        #if defined (NRF5340_XXAA_APPLICATION) || defined (DEVELOP_IN_NRF5340_APPLICATION)\
         || defined (NRF5340_XXAA_NETWORK) || defined (DEVELOP_IN_NRF5340_NETWORK)
            #if defined(NRF_APPLICATION)
                uint32_t var1 = *(uint32_t *)0x00FF0130ul;
                uint32_t var2 = *(uint32_t *)0x00FF0134ul;
            #elif defined(NRF_NETWORK)
                uint32_t var1 = *(uint32_t *)0x01FF0130ul;
                uint32_t var2 = *(uint32_t *)0x01FF0134ul;
            #endif
        #endif
        #if defined (NRF5340_XXAA_APPLICATION) || defined (DEVELOP_IN_NRF5340_APPLICATION)\
         || defined (NRF5340_XXAA_NETWORK) || defined (DEVELOP_IN_NRF5340_NETWORK)
            if (var1 == 0x07)
            {
                switch(var2)
                {
                    case 0x02ul:
                        return true;
                    case 0x03ul:
                        return false;
                }
            }
        #endif
        return false;
    #endif
}

static bool nrf53_errata_79(void)
{
    #ifndef NRF53_SERIES
        return false;
    #else
        #if defined (NRF5340_XXAA_APPLICATION) || defined (DEVELOP_IN_NRF5340_APPLICATION)
            uint32_t var1 = *(uint32_t *)0x00FF0130ul;
            uint32_t var2 = *(uint32_t *)0x00FF0134ul;
        #endif
        #if defined (NRF5340_XXAA_APPLICATION) || defined (DEVELOP_IN_NRF5340_APPLICATION)
            if (var1 == 0x07)
            {
                switch(var2)
                {
                    case 0x02ul:
                        return true;
                    case 0x03ul:
                        return false;
                }
            }
        #endif
        return false;
    #endif
}

static bool nrf53_errata_80(void)
{
    #ifndef NRF53_SERIES
        return false;
    #else
        #if defined (NRF5340_XXAA_APPLICATION) || defined (DEVELOP_IN_NRF5340_APPLICATION)
            uint32_t var1 = *(uint32_t *)0x00FF0130ul;
            uint32_t var2 = *(uint32_t *)0x00FF0134ul;
        #endif
        #if defined (NRF5340_XXAA_APPLICATION) || defined (DEVELOP_IN_NRF5340_APPLICATION)
            if (var1 == 0x07)
            {
                switch(var2)
                {
                    case 0x02ul:
                        return true;
                    case 0x03ul:
                        return false;
                }
            }
        #endif
        return false;
    #endif
}

static bool nrf53_errata_81(void)
{
    #ifndef NRF53_SERIES
        return false;
    #else
        #if defined (NRF5340_XXAA_APPLICATION) || defined (DEVELOP_IN_NRF5340_APPLICATION)
            uint32_t var1 = *(uint32_t *)0x00FF0130ul;
            uint32_t var2 = *(uint32_t *)0x00FF0134ul;
        #endif
        #if defined (NRF5340_XXAA_APPLICATION) || defined (DEVELOP_IN_NRF5340_APPLICATION)
            if (var1 == 0x07)
            {
                switch(var2)
                {
                    case 0x02ul:
                        return true;
                    case 0x03ul:
                        return false;
                }
            }
        #endif
        return false;
    #endif
}

static bool nrf53_errata_82(void)
{
    #ifndef NRF53_SERIES
        return false;
    #else
        #if defined (NRF5340_XXAA_APPLICATION) || defined (DEVELOP_IN_NRF5340_APPLICATION)
            uint32_t var1 = *(uint32_t *)0x00FF0130ul;
            uint32_t var2 = *(uint32_t *)0x00FF0134ul;
        #endif
        #if defined (NRF5340_XXAA_APPLICATION) || defined (DEVELOP_IN_NRF5340_APPLICATION)
            if (var1 == 0x07)
            {
                switch(var2)
                {
                    case 0x02ul:
                        return true;
                    case 0x03ul:
                        return false;
                }
            }
        #endif
        return false;
    #endif
}

static bool nrf53_errata_83(void)
{
    #ifndef NRF53_SERIES
        return false;
    #else
        #if defined (NRF5340_XXAA_APPLICATION) || defined (DEVELOP_IN_NRF5340_APPLICATION)
            uint32_t var1 = *(uint32_t *)0x00FF0130ul;
            uint32_t var2 = *(uint32_t *)0x00FF0134ul;
        #endif
        #if defined (NRF5340_XXAA_APPLICATION) || defined (DEVELOP_IN_NRF5340_APPLICATION)
            if (var1 == 0x07)
            {
                switch(var2)
                {
                    case 0x02ul:
                        return true;
                    case 0x03ul:
                        return false;
                }
            }
        #endif
        return false;
    #endif
}

static bool nrf53_errata_84(void)
{
    #ifndef NRF53_SERIES
        return false;
    #else
        #if defined (NRF5340_XXAA_APPLICATION) || defined (DEVELOP_IN_NRF5340_APPLICATION)
            uint32_t var1 = *(uint32_t *)0x00FF0130ul;
            uint32_t var2 = *(uint32_t *)0x00FF0134ul;
        #endif
        #if defined (NRF5340_XXAA_APPLICATION) || defined (DEVELOP_IN_NRF5340_APPLICATION)
            if (var1 == 0x07)
            {
                switch(var2)
                {
                    case 0x02ul:
                        return true;
                    case 0x03ul:
                        return false;
                }
            }
        #endif
        return false;
    #endif
}

#endif /* NRF53_ERRATAS_H */
