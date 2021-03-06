/*
 * Copyright (c) 2016, SICS, Swedish ICT AB.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/**
 * \file
 *         Network timer implementation.
 * \author
 *         Niclas Finne <nfi@sics.se>
 *         Joakim Eriksson <joakime@sics.se>
 */

#include "sys/ntimer.h"
#include "lib/list.h"

#define DEBUG 0
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif

#ifndef NULL
#define NULL 0
#endif

LIST(timer_list);
static uint8_t is_initialized;
/*---------------------------------------------------------------------------*/
static void
add_timer(ntimer_t *timer)
{
  ntimer_t *n, *l, *p;

  if(!is_initialized) {
    /* The ntimer system has not yet been initialized */
    ntimer_init();
  }

  PRINTF("ntimer: adding timer %p at %lu\n", timer,
         (unsigned long)timer->expiration_time);

  p = list_head(timer_list);

  /* Make sure the timer is not already added to the timer list */
  list_remove(timer_list, timer);

  for(l = NULL, n = list_head(timer_list); n != NULL; l = n, n = n->next) {
    if(timer->expiration_time < n->expiration_time) {
      list_insert(timer_list, l, timer);
      timer = NULL;
      break;
    }
  }

  if(timer != NULL) {
    list_insert(timer_list, l, timer);
  }

  if(p != list_head(timer_list)) {
    /* The next timer to expire has changed so we need to notify the driver */
    NTIMER_DRIVER.update();
  }
}
/*---------------------------------------------------------------------------*/
void
ntimer_stop(ntimer_t *timer)
{
  PRINTF("ntimer: stopping timer %p\n", timer);

  /* Mark timer as expired right now */
  timer->expiration_time = ntimer_uptime();

  list_remove(timer_list, timer);
}
/*---------------------------------------------------------------------------*/
void
ntimer_set(ntimer_t *timer, uint64_t time)
{
  timer->expiration_time = ntimer_uptime() + time;
  add_timer(timer);
}
/*---------------------------------------------------------------------------*/
void
ntimer_reset(ntimer_t *timer, uint64_t time)
{
  timer->expiration_time += time;
  add_timer(timer);
}
/*---------------------------------------------------------------------------*/
uint64_t
ntimer_time_to_next_expiration(void)
{
  uint64_t now;
  ntimer_t *next;

  next = list_head(timer_list);
  if(next == NULL) {
    /* No pending timers - return a time in the future */
    return 60000;
  }

  now = ntimer_uptime();
  if(now < next->expiration_time) {
    return next->expiration_time - now;
  }
  /* The next timer should already have expired */
  return 0;
}
/*---------------------------------------------------------------------------*/
int
ntimer_run(void)
{
  uint64_t now;
  ntimer_t *next;

  /* Always get the current time because it might trigger clock updates */
  now = ntimer_uptime();

  next = list_head(timer_list);
  if(next == NULL) {
    /* No pending timers */
    return 0;
  }

  if(next->expiration_time <= now) {
    PRINTF("ntimer: timer %p expired at %lu\n", next,
           (unsigned long)now);

    /* This timer should expire now */
    list_remove(timer_list, next);

    if(next->callback) {
      next->callback(next);
    }

    /* The next timer has changed */
    NTIMER_DRIVER.update();

    /* Check if there is another pending timer */
    next = list_head(timer_list);
    if(next != NULL && next->expiration_time <= ntimer_uptime()) {
      /* Need to run again */
      return 1;
    }
  }

  return 0;
}
/*---------------------------------------------------------------------------*/
void
ntimer_init(void)
{
  if(is_initialized) {
    return;
  }
  is_initialized = 1;
  list_init(timer_list);
  if(NTIMER_DRIVER.init) {
    NTIMER_DRIVER.init();
  }
}
/*---------------------------------------------------------------------------*/
