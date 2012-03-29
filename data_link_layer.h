#include <signal.h>
#include <unistd.h>

#ifndef __DATA_LINK_LAYER__
#define __DATA_LINK_LAYER__

void handle_signals(int signum, siginfo_t* info, void* context);
void init_data_link_layer(bool is_server);

#endif
