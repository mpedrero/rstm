/**
 *  Copyright (C) 2011
 *  University of Rochester Department of Computer Science
 *    and
 *  Lehigh University Department of Computer Science and Engineering
 *
 * License: Modified BSD
 *          Please see the file LICENSE.RSTM for licensing information
 */

/**
 *  Sandboxing means that we admit the possibility that a zombie thread may
 *  fall into and infinite loop. This may either have a loop control that
 *  depends on an stm_read, where the stm_read hits a value that we stm_wrote
 *  as a zombie, or it may be a loop that has no stm instrumentation at all.
 *
 *  One, simple way to deal with this is to validate during every epoch (or
 *  adaptively every N epochs if validations take too much of an epoch). This
 *  isn't really an option because the OS doesn't provide any hooks for us to
 *  use for this purpose.
 *
 *  An alternative that we explore here is to make sure that every thread gets
 *  a timer interrupt occasionally (SIGALRM), at which time it can
 *  validate. Unfortunately, we can't register for thread-directed SIGINTs, the
 *  best that we can do is to have SIGINT code that pings threads with
 *  pthread_kill when it it called.
 *
 *  A "normal" application might be able to have a single thread deal with all
 *  of its SIGALRMs, this has some nice advantages, but in our library we don't
 *  think that we can do this if the library doesn't expect it since we want to
 *  be completely transparent here. (Imagine a user SIGALRM hander running and
 *  saying, "Wait, this isn't a thread that I pthread_created").
 */

#include <unistd.h>
#include <sys/time.h>
#include "common/interposition.hpp"
#include "../policies/policies.hpp"     // curr_policy
#include "../algs/algs.hpp"             // stms[]
#include "sandboxing.hpp"
#include "timer.hpp"

using stm::pad_word_t;
using stm::MAX_THREADS;
using stm::lazy_load_symbol;

extern "C" {

static unsigned int
call_alarm(unsigned int seconds)
{
    static unsigned int (*palarm)(unsigned int) = NULL;
    lazy_load_symbol(palarm, "alarm");
    return palarm(seconds);
}

unsigned int
alarm(unsigned int seconds)
{
    return call_alarm(seconds);
}

static int
call_getitimer(int which, struct itimerval *curr_value)
{
    static int (*pgetitimer)(int, struct itimerval*) = NULL;
    lazy_load_symbol(pgetitimer, "getitimer");
    return pgetitimer(which, curr_value);
}

int
getitimer(int which, struct itimerval *curr_value)
{
    return call_getitimer(which, curr_value);
}

static int
call_setitimer(int which, const struct itimerval* new_, struct itimerval* old)
{
    typedef struct itimerval itimerval_t;
    static int (*psetitimer)(int, const itimerval_t*, itimerval_t*) = NULL;
    lazy_load_symbol(psetitimer, "setitimer");
    return psetitimer(which, new_, old);
}

int
setitimer(int which, const struct itimerval* new_, struct itimerval* old)
{
    return call_setitimer(which, new_, old);
}
}

static pad_word_t prev_trans[MAX_THREADS] = {{0}};

bool
stm::sandbox::demultiplex_timer(int sig, siginfo_t* info, void* ctx)
{
    // // Cheat and install the timer here.
    // // special handling for timers because libstm uses them
    // if (sig == SIGALRM && stms[curr_policy.ALG_ID].sandbox_signals) {
    //     timer_validate();
    //     return false;
    // }

    fprintf(stderr, "sandboxing: got a timer I can't handle yet\n");
    return false;
}