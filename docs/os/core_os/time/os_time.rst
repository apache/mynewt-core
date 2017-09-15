OS\_Time
========

The system time for the Mynewt OS.

Description
-----------

The Mynewt OS contains an incrementing time that drives the OS scheduler
and time delays. The time is a fixed size (e.g. 32 bits) and will
eventually wrap back to zero. The time to wrap from zero back to zero is
called the **OS time epoch**.

The frequency of the OS time tick is specified in the
architecture-specific OS code ``os_arch.h`` and is named
``OS_TICKS_PER_SEC``.

The Mynewt OS also provides APIs for setting and retrieving the
wallclock time (also known as local time or time-of-day in other
operating systems).

Data Structures
---------------

Time is stored in Mynewt as an ``os_time_t`` value.

Wallclock time is represented using the ``struct os_timeval`` and
``struct os_timezone`` tuple.

``struct os_timeval`` represents the number of seconds elapsed since
00:00:00 Jan 1, 1970 UTC.

.. raw:: html

   <pre>
   struct os_timeval {
       int64_t tv_sec;  /* seconds since Jan 1 1970 UTC */
       int32_t tv_usec; /* fractional seconds */
   };

   struct os_timeval tv = { 1457400000, 0 };  /* 01:20:00 Mar 8 2016 UTC */
   </pre>

``struct os_timezone`` is used to specify the offset of local time from
UTC and whether daylight savings is in effect. Note that
``tz_minuteswest`` is a positive number if the local time is *behind*
UTC and a negative number if the local time is *ahead* of UTC.

.. raw:: html

   <pre>
   struct os_timezone {
       int16_t tz_minuteswest;
       int16_t tz_dsttime;
   };

   /* Pacific Standard Time is 08:00 hours west of UTC */
   struct os_timezone PST = { 480, 0 };
   struct os_timezone PDT = { 480, 1 };

   /* Indian Standard Time is 05:30 hours east of UTC */
   struct os_timezone IST = { -330, 0 };
   </pre>

API
-----------------

.. doxygengroup:: OSTime
    :content-only:

List of Macros
--------------

Several macros help with the evalution of times with respect to each
other.

-  ``OS_TIME_TICK_LT(t1,t2)`` -- evaluates to true if t1 is before t2 in
   time.
-  ``OS_TIME_TICK_GT(t1,t2)`` -- evaluates to true if t1 is after t2 in
   time
-  ``OS_TIME_TICK_GEQ(t1,t2)`` -- evaluates to true if t1 is on or after
   t2 in time.

NOTE: For all of these macros the calculations are done modulo
'os\_time\_t'.

Ensure that comparison of OS time always uses the macros above (to
compensate for the possible wrap of OS time).

The following macros help adding or subtracting time when represented as
``struct os_timeval``. All parameters to the following macros are
pointers to ``struct os_timeval``.

-  ``os_timeradd(tvp, uvp, vvp)`` -- Add ``uvp`` to ``tvp`` and store
   result in ``vvp``.
-  ``os_timersub(tvp, uvp, vvp)`` -- Subtract ``uvp`` from ``tvp`` and
   store result in ``vvp``.

Special Notes
-------------

Its important to understand how quickly the time wraps especially when
doing time comparison using the macros above (or by any other means).

For example, if a tick is 1 millisecond and ``os_time_t`` is 32-bits the
OS time will wrap back to zero in about 49.7 days or stated another way,
the OS time epoch is 49.7 days.

If two times are more than 1/2 the OS time epoch apart, any time
comparison will be incorrect. Ensure at design time that comparisons
will not occur between times that are more than half the OS time epoch.
