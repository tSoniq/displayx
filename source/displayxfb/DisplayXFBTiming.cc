/** @file   DisplayXFBTiming.cc
 *  @brief  Class used to provide timing services in the kernel.
 *
 *  Copyright (c) 2011 tSoniq. All rights reserved.
 */

#include "DisplayXFBTiming.h"
#include <kern/clock.h>

/** Start the timer.
 *
 *  @param  period      The tick interval, in us. Must be non-zero.
 */
void DisplayXFBTiming::start(uint32_t period)
{
    uint64_t nsecs = ((uint64_t)period) * 1000;         // Convert period to ns
    nanoseconds_to_absolutetime(nsecs, &m_period);      // Convert period to abstime units
    clock_get_uptime(&m_nextTick);                      // Get current uptime
    m_nextTick += m_period;                             // Time of next tick
}


/** Update the timer.
 *
 *  @param  ticks           Returns the number of elapsed ticks since @e start() or the last @e update() call.
 *  @param  timeToNextTick  Returns the number of microseconds until the next tick.
 */
void DisplayXFBTiming::update(uint64_t& ticks, uint32_t& timeToNextTick)
{
    uint64_t now = 0;
    clock_get_uptime(&now);

    if (now >= m_nextTick)
    {
        // Passed the current tick
        if (m_period != 0)
        {
            ticks = ((now - m_nextTick) / m_period) + 1;
            m_nextTick += (m_period * ticks);
        }
        else
        {
            // Paranoid handling to avoid division by zero kernel crash if the timer is misconfigured
            ticks = 0;
            m_nextTick = now;
        }
    }
    else
    {
        // Still got time to go.
        ticks = 0;
        if ((m_nextTick - now) > m_period)
        {
            // The system clock has gone backwards (should never happen - this is an uptime measure!)
            m_nextTick = now + 1;
        }
    }

    uint64_t nsecs = 0;
    absolutetime_to_nanoseconds(m_nextTick - now, &nsecs);
    timeToNextTick = (uint32_t)(nsecs / 1000);
}
