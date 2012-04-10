#include <sys/mman.h>
#include <sys/stat.h>        /* For mode constants */
#include <fcntl.h>           /* For O_* constants */
#include <stdlib.h>

#ifndef __SIGNAL_DEFS__
#define __SIGNAL_DEFS__

// use sigqueue to queue these
#define SIG_NEW_FRAME     SIGRTMIN+4
#define SIG_NEW_PACKET    SIGRTMIN+5
#define SIG_CHECKSUM_ERR  SIGRTMIN+6
#define SIG_FLOW_ON       SIGRTMIN+7
#define SIG_NEW_CLIENT    SIGRTMIN+8

// use create_timer to create these timers
#define SIG_TIMER_ELAPSED SIGRTMIN+9

// shared memory stuff to work with signal attachments
// TODO: fix random so doesn't have null char issue
#define SHM_GRAB_NEW(type, vname, id) \
type* vname; \
int id; \
{ \
  id = (int)'/'; \
  id |= (( 0x00000100 ) | ((int)random()) << 8) & 0x00ffff00; \
  int fd = shm_open((char*)&id, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR | S_IROTH | S_IWOTH); \
  if (fd < 0) POST_ERR("SHARED_MEM: unable to get fd: " << strerror(errno)); \
  ftruncate(fd, sizeof(type)); \
  vname = (type*)mmap(NULL, sizeof(type), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0); \
  if (vname == MAP_FAILED) POST_ERR("SHARED_MEM: unable to map memory: " << strerror(errno)); \
  close(fd); \
}

#define SHM_GRAB(type, vname, id) \
type* vname; \
{ \
  int fd = shm_open((char*)&id, O_RDONLY, S_IRUSR | S_IWUSR | S_IROTH | S_IWOTH); \
  if (fd < 0) POST_ERR("SHARED_MEM: unable to get fd: " << strerror(errno)); \
  ftruncate(fd, sizeof(type)); \
  vname = (type*)mmap(NULL, sizeof(type), PROT_READ, MAP_SHARED, fd, 0); \
  if (vname == MAP_FAILED) POST_ERR("SHARED_MEM: unable to map memory: " << strerror(errno)); \
  close(fd); \
}

#define SHM_RELEASE(type, vname) munmap(vname, sizeof(type));

#define SHM_DESTROY(id) \
{ \
  shm_unlink((char*)&id); \
}

#endif
