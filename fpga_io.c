#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include "fpga_io.h"


/* Local struct to keep track of several fpgas */
typedef struct
{
  int32_t  id;
  int32_t  sockfd_reg;
  int32_t  sockfd_event;
  unsigned char ip_addr[16];
  struct sockaddr_in sockaddr_reg;
  struct sockaddr_in sockaddr_event;
} FPGAIO;

#ifndef MAX_FPGAIO
#define MAX_FPGAIO 12
#endif

/* Array of initialized FPGAIOs */
FPGAIO pFPGAIO[MAX_FPGAIO];

/* Size of pFPGA array */
int32_t nFPGAIO = 0;

FPGA_regs *pFPGA_regs = (FPGA_regs *) 0x0;

/**
 * @details initialize the fpga_io library with it's ip address and ports
 * @param[in] ip IP address of FPGA
 * @param[in] reg_port Port for register access
 * @param[in] event_port Port for event data
 * @return If successful, initialization id [0,n].  Otherwise -1
 */
int32_t
fpga_init(const char ip[16], uint16_t reg_port, uint16_t event_port)
{
  int32_t rval = -1;
  int32_t id = 0;

  memset(&pFPGAIO[id], 0, sizeof(FPGAIO));
  memcpy(&pFPGAIO[id].ip_addr, ip, 16*sizeof(char));

  /* serv_addr->sin_port = htons(port); */
  pFPGAIO[id].sockaddr_reg.sin_port = htons(reg_port);
  pFPGAIO[id].sockaddr_event.sin_port = htons(event_port);
  /* open register socket */
  rval = open_register_socket(id);
  if(rval != 0)
    {
      printf("%s: Error opening register socket\n",
	     __func__);
      return -1;
    }

  uint32_t val;
  val = fpga_read32(id, &pFPGA_regs->Clk.BoardId);
  printf("%s: BoardId = 0x%08X\n", __func__, val);

  return rval;
}

typedef struct
{
  int len;
  int type;
  int wrcnt;
  int addr;
  int flags;
  int vals[1];
} write_struct;

typedef struct
{
  int len;
  int type;
  int rdcnt;
  int addr;
  int flags;
} read_struct;

typedef struct
{
  int len;
  int type;
  int rdcnt;
  int data[1];
} read_rsp_struct;

void
fpga_write32(int32_t id, void *addr, int val)
{
  write_struct ws;

//printf("%s(0x%08X,0x%08X)", __func__, (int)((long)addr), val);
//fflush(stdout);

  ws.len = 16;
  ws.type = 4;
  ws.wrcnt = 1;
  ws.addr = (int) ((long) addr);
  ws.flags = 0;
  ws.vals[0] = val;
  write(pFPGAIO[id].sockfd_reg, &ws, sizeof(ws));
//printf(" done.\n");
}

unsigned int
fpga_read32(int32_t id, void *addr)
{
  read_struct rs;
  read_rsp_struct rs_rsp;
  int len;

//printf("%s(0x%08X)=", __func__, (int)((long)addr));

  rs.len = 12;
  rs.type = 3;
  rs.rdcnt = 1;
  rs.addr = (int) ((long) addr);
  rs.flags = 0;
  write(pFPGAIO[id].sockfd_reg, &rs, sizeof(rs));

  len = read(pFPGAIO[id].sockfd_reg, &rs_rsp, sizeof(rs_rsp));
  if(len != sizeof(rs_rsp))
    printf("Error in %s: socket read failed...\n", __FUNCTION__);

//printf("0x%08X\n", rs_rsp.data[0]);

  return rs_rsp.data[0];
}

void
fpga_read32_n(int32_t id, int32_t n, void *addr, uint32_t *buf)
{
  read_struct rs;
  read_rsp_struct rs_rsp;
  int32_t len, i;

  for(i = 0; i < n; i++)
    {
      rs.len = 12;
      rs.type = 3;
      rs.rdcnt = 1;
      rs.addr = (int) ((long) addr);
      rs.flags = 0;
      write(pFPGAIO[id].sockfd_reg, &rs, sizeof(rs));
    }

  for(i = 0; i < n; i++)
    {
      len = read(pFPGAIO[id].sockfd_reg, &rs_rsp, sizeof(rs_rsp));
      if(len != sizeof(rs_rsp))
	printf("Error in %s: socket read failed...\n", __FUNCTION__);

      buf[i] = rs_rsp.data[0];
    }
}

int32_t
open_register_socket(int32_t id)
{
  printf("%s: open socket\n", __func__);

  if((pFPGAIO[id].sockfd_reg = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
      printf("\n Error : Could not create socket \n");
      return -1;
    }

  pFPGAIO[id].sockaddr_reg.sin_family = AF_INET;

  if(inet_pton(AF_INET, pFPGAIO[id].ip_addr, &pFPGAIO[id].sockaddr_reg.sin_addr) <= 0)
    {
      printf("\n inet_pton error occured\n");
      return -1;
    }

  printf("%s: connect\n", __func__);
  if(connect(pFPGAIO[id].sockfd_reg, (struct sockaddr *) &pFPGAIO[id].sockaddr_reg,
	     sizeof(struct sockaddr_in)) < 0)
    {
      printf("\n Error : Connect Failed \n");
      return -1;
    }

  printf("%s: write\n", __func__);
  /* Send endian test header */
  int n, val;
  val = 0x12345678;
  write(pFPGAIO[id].sockfd_reg, &val, 4);

  val = 0;
  n = read(pFPGAIO[id].sockfd_reg, &val, 4);
  printf("n = %d, val = 0x%08X\n", n, val);

  return 0;
}

void
close_register_socket(int32_t id)
{
  if(pFPGAIO[id].sockfd_reg)
    {
      close(pFPGAIO[id].sockfd_reg);
      pFPGAIO[id].sockfd_reg = 0;
    }
}



int32_t
open_event_socket(int32_t id)
{
  if((pFPGAIO[id].sockfd_event = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
      printf("\n Error : Could not create socket \n");
      return -1;
    }

  pFPGAIO[id].sockaddr_event.sin_family = AF_INET;
  /* serv_addr->sin_port = htons(port); */

  if(inet_pton(AF_INET, pFPGAIO[id].ip_addr, &pFPGAIO[id].sockaddr_event.sin_addr) <= 0)
    {
      printf("\n inet_pton error occured\n");
      return -1;
    }

  if(connect(pFPGAIO[id].sockfd_event, (struct sockaddr *) &pFPGAIO[id].sockaddr_event,
	     sizeof(struct sockaddr_in)) < 0)
    {
      printf("\n Error : Connect Failed \n");
      return -1;
    }

  return 0;
}

void
close_event_socket(int32_t id)
{
  if(pFPGAIO[id].sockfd_event)
    {
      close(pFPGAIO[id].sockfd_event);
      pFPGAIO[id].sockfd_event = 0;
    }
}

int32_t
read_event_socket(int32_t id, int *buf, int32_t nwords_max)
{
  int32_t nbytes, nwords;

  ioctl(pFPGAIO[id].sockfd_event, FIONREAD, &nbytes);
  if(ioctl < 0)
    return 0;

  nwords = nbytes / 4;
  if(nwords > nwords_max)
    nwords = nwords_max;

  if(nwords)
    read(pFPGAIO[id].sockfd_event, buf, nwords * 4);

  return nwords;
}
