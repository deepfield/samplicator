/*
 rawsend.c

 Date Created: Tue Jan 18 12:13:31 2000
 Author:       Simon Leinen  <simon@limmat.switch.ch>

 Send a UDP datagram to a given destination address, but make it look
 as if it came from a given source address.
 */

#include "config.h"

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <sys/types.h>
#include <string.h>
#if STDC_HEADERS
# define bzero(b,n) memset(b,0,n)
#else
# include <strings.h>
# ifndef HAVE_MEMCPY
#  define memcpy(d, s, n) bcopy ((s), (d), (n))
# endif
#endif
#ifdef HAVE_NETINET_IN_SYSTM_H
#include <netinet/in_systm.h>
#endif
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <sys/uio.h>

/* make uh_... slot names available under Linux */
#define __FAVOR_BSD 1

#include <netinet/udp.h>

#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "rawsend.h"

#define MAX_IP_DATAGRAM_SIZE 65535

#define DEFAULT_TTL 64

static unsigned ip_header_checksum (const void * header);

static unsigned
ip_header_checksum (const void * header)
{
  unsigned long csum = 0;
  unsigned size = ((struct ip *) header)->ip_hl;
  uint16_t *h = (uint16_t *) header;
  unsigned k;
  for (k = 0; k < size; ++k)
    {
      csum ^= h[2*k];
      csum ^= h[2*k+1];
    }
  return ~csum;
}

int
raw_send_from_to (s, msg, msglen, saddr, daddr)
     int s;
     const void * msg;
     size_t msglen;
     struct sockaddr_in *saddr;
     struct sockaddr_in *daddr;
{
  int length;
  int flags = 0;
  int sockerr;
  int sockerr_size = sizeof sockerr;
  struct sockaddr_in dest_a;
  struct ip ih;
  struct udphdr uh;

#ifdef HAVE_SYS_UIO_H
  struct msghdr mh;
  struct iovec iov[3];
#else /* not HAVE_SYS_UIO_H */
  static char *msgbuf = 0;
  static size_t msgbuflen = 0;
  static size_t next_alloc_size = 1;
#endif /* not HAVE_SYS_UIO_H */

  uh.uh_sport = saddr->sin_port;
  uh.uh_dport = daddr->sin_port;
  uh.uh_ulen = htons (msglen + sizeof uh);
  uh.uh_sum = 0;

  length = msglen + sizeof uh + sizeof ih;
#ifndef HAVE_SYS_UIO_H
  if (length > msgbuflen)
    {
      if (length > MAX_IP_DATAGRAM_SIZE)
	{
	  return -1;
	}
      if (msgbuf != (char *) 0)
	free (msgbuf);
      while (next_alloc_size < length)
	next_alloc_size *= 2;
      if ((msgbuf = malloc (next_alloc_size)) == (char *) 0)
	{
	  fprintf (stderr, "Out of memory!\n");
	  return -1;
	}
      msgbuflen = next_alloc_size;
      next_alloc_size *= 2;
    }
#endif /* not HAVE_SYS_UIO_H */
  ih.ip_hl = (sizeof ih+3)/4;
  ih.ip_v = 4;
  ih.ip_tos = 0;
  ih.ip_len = length;
  ih.ip_id = htons (0);
  ih.ip_off = htons (0);
  ih.ip_ttl = DEFAULT_TTL;
  ih.ip_p = 17;
  ih.ip_sum = htons (0);
  ih.ip_src.s_addr = saddr->sin_addr.s_addr;
  ih.ip_dst.s_addr = daddr->sin_addr.s_addr;
  ih.ip_sum = htons (ip_header_checksum (&ih));

  dest_a.sin_family = AF_INET;
  dest_a.sin_port = IPPROTO_UDP;
  dest_a.sin_addr.s_addr = htonl (0x7f000001);

#ifdef HAVE_SYS_UIO_H
  iov[0].iov_base = &ih;
  iov[0].iov_len = sizeof ih;
  iov[1].iov_base = &uh;
  iov[1].iov_len = sizeof uh;
  iov[2].iov_base = (char *) msg;
  iov[2].iov_len = msglen;
  mh.msg_name = (struct sockaddr *)&dest_a;
  mh.msg_namelen = sizeof dest_a;
  mh.msg_iov = iov;
  mh.msg_iovlen = 3;
  mh.msg_control = 0;
  mh.msg_controllen = 0;

  if (sendmsg (s, &mh, 0) == -1)
#else /* not HAVE_SYS_UIO_H */
  memcpy (msgbuf+sizeof ih+sizeof uh, msg, msglen);
  memcpy (msgbuf+sizeof ih, & uh, sizeof uh);
  memcpy (msgbuf, & ih, sizeof ih);

  if (sendto (s, msgbuf, length, flags,
	      (struct sockaddr *)&dest_a, sizeof dest_a) == -1)
#endif /* not HAVE_SYS_UIO_H */
    {
      if (getsockopt (s, SOL_SOCKET, SO_ERROR, &sockerr, &sockerr_size) == 0)
	{
	  fprintf (stderr, "socket error: %d\n", sockerr);
	  fprintf (stderr, "socket: %s\n",
		   strerror (errno));
	  exit (1);
	}
    }
}

extern int
make_raw_udp_socket ()
{
  return socket (PF_INET, SOCK_RAW, IPPROTO_RAW);
}