//*****************************************************************************
//
//! @file am_util_tap_detect.h
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
#ifndef AM_UTIL_TAP_DETECT_H
#define AM_UTIL_TAP_DETECT_H

#ifdef __cplusplus
extern "C"
{
#endif


//*****************************************************************************
//
// Enumerated return constants
//
//*****************************************************************************
typedef enum
{
    NO_TAP_DETECTED = 0,
    TAP_OCCURED,           // reports every tap
    TAP_DETECTED,          // only if a single tap is not part of double/triple
    DOUBLE_TAP_DETECTED,
    TRIPLE_TAP_DETECTED
}am_util_tap_detect_enum_t;


//*****************************************************************************
//
// Tap Detector Data Structure
//
//*****************************************************************************
typedef struct am_util_tap_detector
{
  float accX;
  float accY;
  float accZ;
  float prev_accX;
  float prev_accY;
  float prev_accZ;
  float SlopeThreshold;
  int   previous_peak_location;
  int   current_sample;
  int   sample_count;
  bool  start_flag;
  float sample_rate;
  //in terms of seconds
  float  peak_min_width_seconds;
  float  group_peak_max_threshold_seconds;
  //in terms of samples
  int  peak_min_width_samples;
  int  group_peak_max_threshold;
  //record the max peak found so you can adjust threshold in future
  float max_mag;
  float mag; //magnitude of the partial derivatives
  int   dist;
  //previous peak was double tap -> flag to sep[arate out double tap pairs
  int previous_tap_was_dbl_tap;

} am_util_tap_detect_t;

//*****************************************************************************
//
// External function definitions
//
//*****************************************************************************
void am_util_tap_detect_init(am_util_tap_detect_t * tap,
                             float dp_min_seconds,
                             float dp_max_seconds,
                             float srate,
                             float slope_thresh);
void am_util_tap_detect_print(am_util_tap_detect_t * tap);
am_util_tap_detect_enum_t am_util_tap_detect_process_sample
                       (am_util_tap_detect_t * tap,
                        short accX, short accY, short accZ);

#ifdef __cplusplus
}
#endif

#endif // AM_UTIL_TAP_DETECT_H


