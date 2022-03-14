//*****************************************************************************
//
//! @file am_util_tap_detect.c
//!
//! @brief Tap Gesture Detector
//!
//! These functions implement the tap detector utility
//
//*****************************************************************************

//*****************************************************************************
//
// Copyright (c) 2017, Ambiq Micro
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
// 
// 1. Redistributions of source code must retain the above copyright notice,
// this list of conditions and the following disclaimer.
// 
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
// 
// 3. Neither the name of the copyright holder nor the names of its
// contributors may be used to endorse or promote products derived from this
// software without specific prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
// This is part of revision v1.2.10-2-gea660ad-hotfix2 of the AmbiqSuite Development Package.
//
//*****************************************************************************

#include <stdint.h>
#include <stdbool.h>

#include <math.h>

#include <am_util_tap_detect.h>

#define USE_L2_NORM
//#define DEBUG_TAP_DETECTOR

//*****************************************************************************
//
//! @brief Initialize tap detector structure
//!
//! @param tap is a pointer to the tap detector structure
//! @param dp_min_seconds minimum time to detect double (or triple) tap
//! @param dp_max_seconds maximum time to detect double (or triple) tap
//! @param srate is the sample rate at which the accel runs typically 400 or 200
//! @param slope_thresh is the sensitivity setting for tap detection, typ 800
//!
//! This function initializes the tap detector structure and sets various
//! settings, e.g. min/max times for classifying single, double or triple taps.
//! In addition the structures tells the tap detector how long one sample is
//! in time. Finally, it specifies the sensitiviy of tap detection by setting
//! a minimimum slope threshold to signal tap detections.
//!
//! returns nothing
//
//*****************************************************************************
void
am_util_tap_detect_init(am_util_tap_detect_t * tap,
                        float dp_min_seconds,
                        float dp_max_seconds,
                        float srate,
                        float slope_thresh)
{

  tap->prev_accX = 0;
  tap->prev_accY = 0;
  tap->prev_accZ = 0;
  tap->sample_count = 0;
  tap->start_flag = true;
#ifndef USE_L2_NORM
  tap->SlopeThreshold = slope_thresh;
#else
  tap->SlopeThreshold = slope_thresh*slope_thresh;
#endif
  tap->previous_peak_location = -10000000;
  tap->sample_rate = srate;
  tap->previous_tap_was_dbl_tap = 0;
  tap->peak_min_width_seconds = dp_min_seconds;
  tap->group_peak_max_threshold_seconds = dp_max_seconds;
  //convert to samples
  tap->peak_min_width_samples = srate*tap->peak_min_width_seconds;
  tap->group_peak_max_threshold = srate*tap->group_peak_max_threshold_seconds;
  tap->max_mag = 0;

}


//*****************************************************************************
//
//! @brief Print the contents of the Tap Detector Structure
//!
//! @param tap is a pointer to the tap detector structure
//!
//! This function will print the contents of the tap detector structure if
//! needed for debug.
//!
//! returns nothing
//
//*****************************************************************************
#ifdef DEBUG_TAP_DETECTOR
void
am_util_tap_detect_print(am_util_tap_detect_t * tap)
{
    am_util_stdio_printf("Sampling Rate          = %d\n", (int)tap->sample_rate);
    am_util_stdio_printf("SlopeThreshold         = %d\n", (int)tap->SlopeThreshold);
    //am_util_stdio_printf("DoublePeak min seconds = %f\n",
    //                     tap->double_peak_min_threshold_seconds);
    //am_util_stdio_printf("DoublePeak max seconds = %f\n",
    //                     tap->double_peak_max_threshold_seconds);
    am_util_stdio_printf("DoublePeak min samples = %i\n",
                         tap->double_peak_min_threshold);
    am_util_stdio_printf("DoublePeak max samples = %i\n",
                         tap->double_peak_max_threshold);
    am_util_stdio_printf("Start Flag = %i\n", tap->start_flag);

    // new stuff below
    printf("Sampling Rate          = %f\n", tap->sample_rate);
    printf("SlopeThreshold         = %f\n", tap->SlopeThreshold);
    printf("DoublePeak min seconds = %f\n", tap->peak_min_width_seconds);
    printf("DoublePeak max seconds = %f\n", tap->group_peak_max_threshold_seconds);
    printf("DoublePeak min samples = %i\n", tap->peak_min_width_samples);
    printf("DoublePeak max samples = %i\n", tap->group_peak_max_threshold);
}

#endif


//*****************************************************************************
//
//! @brief Process One Sample (Triplet) Through the Tap Dector
//!
//! @param tap is a pointer to the tap detector structure
//! @param accX Accelerometer X axis value of a triplet
//! @param accY Accelerometer Y axis value of a triplet
//! @param accZ Accelerometer Z axis value of a triplet
//!
//! This function utilizes the tap detector structure in conjunction with sample
//! counting to establish all necessary timing.
//!
//! @return NO_TAP, TAP_OCCURED, TAP, DOUBLE, TRIPLE
//
//*****************************************************************************
am_util_tap_detect_enum_t
am_util_tap_detect_process_sample(am_util_tap_detect_t * tap,
                                  short accX, short accY, short accZ)
{

    am_util_tap_detect_enum_t out = NO_TAP_DETECTED;
    static int tap_occured = 0;

    //initialize the first previous sample
    if ( tap->start_flag )
    {
        tap->start_flag = false;
        tap->prev_accX = accX;
        tap->prev_accY = accY;
        tap->prev_accZ = accZ;
    }

    //Feature Extract ----------------------------------------------------
    //get the first derivative
    float axx = accX - tap->prev_accX;
    float ayy = accY - tap->prev_accY;
    float azz = accZ - tap->prev_accZ;

    //get the magnitude of the partial derivatives
    //NOTE: do not need the sqrt!!! This is a lot of cycles!
#ifndef USE_L2_NORM
    float mag_sample = sqrt((axx*axx) + (ayy*ayy) + (azz*azz));
#else
    float mag_sample = ((axx*axx) + (ayy*ayy) + (azz*azz));
#endif

    //PEAK DETECTION **********************************************************
    if ( mag_sample > tap->SlopeThreshold )
    {

        int distance = (tap->sample_count - tap->previous_peak_location);

        //OK detect a standard tape
        if ( distance > tap->peak_min_width_samples )   //should be min peak width
        {
            // returned only for first tap event, TAP, DOUBLE or TRIPLE over writes
            out = TAP_OCCURED;
            tap_occured++;
        }

        //record where this peak occured
        tap->previous_peak_location = tap->sample_count;

        //these are handy for debugging and tuning
        if ( mag_sample > tap->max_mag )
        {
            tap->max_mag = mag_sample;
        }
        tap->dist = distance;
    }

    //GROUPING CLASSIFICATION OF SINGLE, DOUBLE, AND TRIPLE TAPS
    //*******************************************************************************
    //These are grouping cases where we report a single, double, or triple tap
    // If a tap is within group_peak_max_threshold, than is forms a group of taps such as DOULBLE or TRIPLE
    // otherwise the tap is just a single tap
    int dist_classification = (tap->sample_count - tap->previous_peak_location);
    if ( (tap_occured == 1) &&  (dist_classification > tap->group_peak_max_threshold) )
    {
        out = TAP_DETECTED;
        tap_occured = 0;
    }
    if ( (tap_occured == 2) &&  (dist_classification > tap->group_peak_max_threshold) )
    {
        out = DOUBLE_TAP_DETECTED;
        tap_occured = 0;
    }
    if ( tap_occured == 3 )
    {
        out = TRIPLE_TAP_DETECTED;
        tap_occured = 0;
    }

    //*******************************************************************************

    tap->mag = mag_sample;

    //store for next sample --------------------------------------------------
    tap->prev_accX = accX;
    tap->prev_accY = accY;
    tap->prev_accZ = accZ;

    //sample_count keeps track of time!!!-------------------------------------
    tap->sample_count++;

    return out;
}
