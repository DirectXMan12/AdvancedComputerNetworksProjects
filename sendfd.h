#include <sys/socket.h>
#include <sys/un.h>
#include <sys/uio.h>
#include <unistd.h>
#include <errno.h>

#ifndef CMSG_ALIGN
#       ifdef __sun__
#               define CMSG_ALIGN _CMSG_DATA_ALIGN
#       else
#               define CMSG_ALIGN(len) (((len)+sizeof(long)-1) & ~(sizeof(long)-1))
#       endif
#endif

#ifndef CMSG_SPACE
#       define CMSG_SPACE(len) (CMSG_ALIGN(sizeof(struct cmsghdr))+CMSG_ALIGN(len))
#endif

#ifndef CMSG_LEN
#       define CMSG_LEN(len) (CMSG_ALIGN(sizeof(struct cmsghdr))+(len))
#endif

#ifndef __SEND_RECV_FD__
#define __SEND_RECV_FD__

// base sendfd and recvfd code courtesy of plan9port (Plan 9 from User Space)
int
sendfd(int s, int fd)
{
        char buf[1];
        struct iovec iov;
        struct msghdr msg;
        struct cmsghdr *cmsg;
        int n;
        char cms[CMSG_SPACE(sizeof(int))];
        
        buf[0] = 0;
        iov.iov_base = buf;
        iov.iov_len = 1;

        memset(&msg, 0, sizeof msg);
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;
        msg.msg_control = (caddr_t)cms;
        msg.msg_controllen = CMSG_LEN(sizeof(int));

        cmsg = CMSG_FIRSTHDR(&msg);
        cmsg->cmsg_len = CMSG_LEN(sizeof(int));
        cmsg->cmsg_level = SOL_SOCKET;
        cmsg->cmsg_type = SCM_RIGHTS;
        memmove(CMSG_DATA(cmsg), &fd, sizeof(int));

        if((n=sendmsg(s, &msg, 0)) != iov.iov_len)
                return -1;
        return 0;
}

int
recvfd(int s)
{
        int n;
        int fd;
        char buf[1];
        struct iovec iov;
        struct msghdr msg;
        struct cmsghdr *cmsg;
        char cms[CMSG_SPACE(sizeof(int))];

        iov.iov_base = buf;
        iov.iov_len = 1;

        memset(&msg, 0, sizeof msg);
        msg.msg_name = 0;
        msg.msg_namelen = 0;
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;

        msg.msg_control = (caddr_t)cms;
        msg.msg_controllen = sizeof cms;

        if((n=recvmsg(s, &msg, 0)) < 0)
                return -1;
        if(n == 0)
        {
          POST_ERR("FD_TRANSFER: unexpected EOF");
          return -1;
        }
        cmsg = CMSG_FIRSTHDR(&msg);
        memmove(&fd, CMSG_DATA(cmsg), sizeof(int));
        return fd;
}

void transferfdto(int fd, const char* filename)
{
  int sfd;
  struct sockaddr_un my_addr;
  socklen_t peer_addr_size;

  sfd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (sfd < 0)
  {
    POST_ERR("FD_TRANSFER: error getting domain socket");
    return;
  }

  memset(&my_addr, 0, sizeof(struct sockaddr_un));

  my_addr.sun_family = AF_UNIX;
  strcpy(my_addr.sun_path, filename);

  while(connect(sfd, (struct sockaddr *) &my_addr, sizeof(struct sockaddr_un)) < 0) {} // wait until we can connect

  if (sendfd(sfd, fd) < 0) POST_ERR("FD_TRANSFER: error sending file descriptor");
  close(sfd);
}

int transferfdfrom(const char* filename)
{
  int sfd, cfd;
  struct sockaddr_un my_addr, peer_addr;
  socklen_t peer_addr_size;

  sfd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (sfd < 0)
  {
    POST_ERR("FD_TRANSFER: error getting domain socket");
    return -1;
  }

  memset(&my_addr, 0, sizeof(struct sockaddr_un));

  my_addr.sun_family = AF_UNIX;
  strcpy(my_addr.sun_path, filename);

  if (bind(sfd, (struct sockaddr *) &my_addr, sizeof(struct sockaddr_un)) < 0)
  {
    POST_ERR("FD_TRANSFER: error binding to path '" << filename << "': " << strerror(errno));
    return -1;
  }

  if (listen(sfd, 1) < 0)
  {
    POST_ERR("FD_TRANSFER: error listening on domain socket...");
    return -1;
  }

  peer_addr_size = sizeof(struct sockaddr_un);
  cfd = accept(sfd, (struct sockaddr *) &peer_addr, &peer_addr_size);

  if (cfd < 0)
  {
    POST_ERR("FD_TRANSFER: issue accepting the connection...");
    return -1;
  }
  
  int res = recvfd(cfd);
  close(cfd);
  close(sfd);
  unlink(filename);

  return res;
}

#endif
