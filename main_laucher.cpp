#include "physical_layer.h"
#include "data_link_layer.h"

int main(int argc, char* argv[])
{
  pid_t dll_pid;
  pid_t my_pid = getpid();

  dll_pid = fork();

  if (dll_pid == 0) // we are the child
  {
    init_data_link_layer(argv[1][0] == '1', my_pid);
  }

}
