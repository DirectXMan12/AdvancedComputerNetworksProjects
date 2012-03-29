#ifndef __SIGNAL_DEFS__
#define __SIGNAL_DEFS__

// use sigqueue to queue these
#define SIG_NEW_FRAME SIGRTMIN+4
#define SIG_NEW_PACKET SIGRTMIN+5

// use create_timer to create these timers
#define SIG_TIMER_ELAPSED SIGRTMIN+6

#endif

