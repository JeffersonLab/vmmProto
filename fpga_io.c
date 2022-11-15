/**
 * @copyright Copyright 2022, Jefferson Science Associates, LLC.
 *            Subject to the terms in the LICENSE file found in the
 *            top-level directory.
 *
 * @author    Ben Raydo
 *            braydo@jlab.org                   Jefferson Lab, MS-12B3
 *            Phone: (757) 269-7317             12000 Jefferson Ave.
 *            Fax:   (757) 269-5800             Newport News, VA 23606
 *
 * @author    Bryan Moffit
 *            moffit@jlab.org                   Jefferson Lab, MS-12B3
 *            Phone: (757) 269-5660             12000 Jefferson Ave.
 *            Fax:   (757) 269-5800             Newport News, VA 23606
 *
 *
 * @file      fpga_io.c
 * @brief     Library for socket communcation with FPGAs
 *
 */

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
  int32_t  active;
  int32_t  sockfd_reg;
  int32_t  sockfd_event;
  char     ip_addr[16];
  struct sockaddr_in sockaddr_reg;
  struct sockaddr_in sockaddr_event;
} FPGAIO;

#ifndef FPGAIO_MAX
#define FPGAIO_MAX 12
#endif

#ifndef FPGAIO_ACTIVE
#define FPGAIO_ACTIVE 0x4A4C4142
#endif

/* Array of initialized FPGAIOs */
FPGAIO pFPGAIO[FPGAIO_MAX];

/* Size of pFPGA array */
int32_t nFPGAIO = 0;

/* Keep this around for the original IO */
FPGA_regs *pFPGA_regs = (FPGA_regs *) 0x0;

int32_t fpga_io_debug = 1;
#define FPGAIO_DBG(format, ...) if(fpga_io_debug==1){{printf("%s: ",__func__); printf(format, ## __VA_ARGS__);}}
#define FPGAIO_ERR(format, ...) {fprintf(stderr, "%s: ERROR: ",__func__); fprintf(stderr, format, ## __VA_ARGS__);}

#define CHECKID {							\
    if((id != pFPGAIO[id].id) ||(pFPGAIO[id].active != FPGAIO_ACTIVE))	\
      FPGAIO_ERR("Invalid FPGA ID (%d)\n", id);				\
  }

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
  int32_t id = nFPGAIO+1;

  /* Check to be sure there's room in the array for a new one */
  if(id >= FPGAIO_MAX)
    {
      FPGAIO_ERR("MAX FPGAIO (%d) already allocated.\n", FPGAIO_MAX);
      return -1;
    }

  /* Populate the FPGAIO structure for this id */
  memset(&pFPGAIO[id], 0, sizeof(FPGAIO));
  memcpy(&pFPGAIO[id].ip_addr, ip, 16*sizeof(char));

  pFPGAIO[id].sockaddr_reg.sin_port = htons(reg_port);
  pFPGAIO[id].sockaddr_event.sin_port = htons(event_port);

  /* open register socket */
  rval = open_register_socket(id);
  if(rval != 0)
    {
      FPGAIO_ERR("opening register socket for FPGAIO %d at %s\n", id, ip);
      memset(&pFPGAIO[id], 0, sizeof(FPGAIO));
      return -1;
    }

  /* Connection active */
  pFPGAIO[id].id     = id;
  pFPGAIO[id].active = FPGAIO_ACTIVE;
  nFPGAIO++;

  if(fpga_io_debug == 1)
    {
      uint32_t val;
      val = fpga_read32(id, &pFPGA_regs->Clk.BoardId);
      FPGAIO_DBG("BoardId = 0x%08X\n", val);
    }

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

/**
 * @brief Write a 32bit value to an address for specified FPGA
 * @details Write a 32bit value to an address for specified FPGA
 * @param[in] id FPGA identifier
 * @param[out] addr FPGA address to write
 * @param[in] val value to write
 * @return 0 if successful, otherwise -1;
 */
int32_t
fpga_write32(int32_t id, void *addr, int val)
{
  write_struct ws;

  CHECKID;
  FPGAIO_DBG("(%d, 0x%08x, 0x%08x)\n", id, (uint32_t)(unsigned long)addr, val);

  ws.len = 16;
  ws.type = 4;
  ws.wrcnt = 1;
  ws.addr = (int) ((long) addr);
  ws.flags = 0;
  ws.vals[0] = val;
  write(pFPGAIO[id].sockfd_reg, &ws, sizeof(ws));

  FPGAIO_DBG(" done.\n");

  return 0;
}

/**
 * @brief Read a 32bit value from an address for specified FPGA
 * @details  Read a 32bit value from an address for specified FPGA
 * @param[in] id FPGA identifier
 * @param[out] addr FPGA address to read
 * @return 32bit value from FPGA, otherwise -1;
 */
uint32_t
fpga_read32(int32_t id, void *addr)
{
  read_struct rs;
  read_rsp_struct rs_rsp;
  int len;

  CHECKID;

  FPGAIO_DBG("(%d, 0x%08x)\n", id, (uint32_t)(unsigned long)addr);

  rs.len = 12;
  rs.type = 3;
  rs.rdcnt = 1;
  rs.addr = (int) ((long) addr);
  rs.flags = 0;
  write(pFPGAIO[id].sockfd_reg, &rs, sizeof(rs));

  len = read(pFPGAIO[id].sockfd_reg, &rs_rsp, sizeof(rs_rsp));
  if(len != sizeof(rs_rsp))
    FPGAIO_ERR("socket read failed\n");


  FPGAIO_DBG(" = 0x%08x\n", rs_rsp.data[0]);

  return rs_rsp.data[0];
}

/**
 * @brief Read 32bit vallues from a range of addresses for specified FPGA
 * @details Read 32bit vallues from a range of addresses for specified FPGA
 * @param[in] id FPGA identifier
 * @param[in] n Number of addresses to increments from addr
 * @param[out] addr Starting FPGA address
 * @param[out] buf Pointer to buffer where read values will be stored
 * @return 0 if successful, otherwise -1
 */
int32_t
fpga_read32_n(int32_t id, int32_t n, void *addr, uint32_t *buf)
{
  read_struct rs;
  read_rsp_struct rs_rsp;
  int32_t len, i;
  CHECKID;

  FPGAIO_DBG("(%d, %d, 0x%08x)\n", id, n, (uint32_t)(unsigned long)addr);

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
	FPGAIO_ERR("socket read failed\n");

      buf[i] = rs_rsp.data[0];

      FPGAIO_DBG(" buf[%d] = 0x%08x\n", i, rs_rsp.data[0]);
    }

  return 0;
}

/**
 * @brief Open socket to specified FPGA registers
 * @details Open socket to specified FPGA registers
 * @param[in] id FPGA idenitifier
 * @return 0 if successful, otherwise -1
 */
int32_t
open_register_socket(int32_t id)
{

  if(pFPGAIO[id].active == FPGAIO_ACTIVE)
    {
      FPGAIO_ERR("FPGAIO %d for %s already active\n",
		 id, pFPGAIO[id].ip_addr);
      return -1;
    }

  FPGAIO_DBG("open socket for FPGAIO %d at %s\n", id, pFPGAIO[id].ip_addr);

  if((pFPGAIO[id].sockfd_reg = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
      FPGAIO_ERR("Could not create socket \n");
      return -1;
    }

  pFPGAIO[id].sockaddr_reg.sin_family = AF_INET;

  if(inet_pton(AF_INET, pFPGAIO[id].ip_addr, &pFPGAIO[id].sockaddr_reg.sin_addr) <= 0)
    {
      FPGAIO_ERR("inet_pton error occured\n");
      return -1;
    }

  FPGAIO_DBG("connect\n");
  if(connect(pFPGAIO[id].sockfd_reg, (struct sockaddr *) &pFPGAIO[id].sockaddr_reg,
	     sizeof(struct sockaddr_in)) < 0)
    {
      FPGAIO_ERR("Connection Failed\n");
      return -1;
    }

  if(fpga_io_debug==1)
    {
      FPGAIO_DBG("write\n");
      /* Send endian test header */
      int n, val;
      val = 0x12345678;
      write(pFPGAIO[id].sockfd_reg, &val, 4);

      val = 0;
      n = read(pFPGAIO[id].sockfd_reg, &val, 4);
      FPGAIO_DBG(" n = %d, val = 0x%08x\n", n, val);
    }

  /* Set connection as active */
  pFPGAIO[id].active = FPGAIO_ACTIVE;


  return 0;
}

/**
 * @brief Close socket to specified FPGA registers
 * @details Close socket to specified FPGA registers
 * @param[in] id FPGA identifier
 * @return 0 if successful, otherwise -1
 */
int32_t
close_register_socket(int32_t id)
{
  CHECKID;

  FPGAIO_DBG(" close socket for FPGAIO %d at %s\n", id, pFPGAIO[id].ip_addr);

  if(pFPGAIO[id].sockfd_reg)
    {
      close(pFPGAIO[id].sockfd_reg);
      pFPGAIO[id].sockfd_reg = 0;
      pFPGAIO[id].active     = 0;
    }

  return 0;
}



/**
 * @brief Open socket to specified FPGA event data
 * @details Open socket to specified FPGA event data
 * @param[in] id FPGA identifier
 * @return 0 if successful, otherwise -1
 */
int32_t
open_event_socket(int32_t id)
{

  /* See if the event socket is already open */
  if(pFPGAIO[id].sockfd_event != 0)
    {
      FPGAIO_ERR("Socket for FPGAIO %d at %s already open\n", id, pFPGAIO[id].ip_addr);
      return -1;
    }

  FPGAIO_DBG("open socket for FPGAIO %d at %s\n", id, pFPGAIO[id].ip_addr);
  if((pFPGAIO[id].sockfd_event = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
      FPGAIO_ERR("Could not create socket \n");
      return -1;
    }

  pFPGAIO[id].sockaddr_event.sin_family = AF_INET;

  if(inet_pton(AF_INET, pFPGAIO[id].ip_addr, &pFPGAIO[id].sockaddr_event.sin_addr) <= 0)
    {
      FPGAIO_ERR("inet_pton error occured\n");
      return -1;
    }

  FPGAIO_DBG("connect\n");
  if(connect(pFPGAIO[id].sockfd_event, (struct sockaddr *) &pFPGAIO[id].sockaddr_event,
	     sizeof(struct sockaddr_in)) < 0)
    {
      FPGAIO_ERR("Connection Failed \n");
      return -1;
    }

  return 0;
}

/**
 * @brief Close socket to specified FPGA event data
 * @details Close socket to specified FPGA event data
 * @param[in] id FPGA identifier
 * @return 0 if successful, otherwise -1
 */
int32_t
close_event_socket(int32_t id)
{
  CHECKID;
  if(pFPGAIO[id].sockfd_event)
    {
      close(pFPGAIO[id].sockfd_event);
      pFPGAIO[id].sockfd_event = 0;
    }

  return 0;
}

/**
 * @brief Read from specified FPGA event data socket
 * @details Read from specified FPGA event data socket
 * @param[in] id FPGA identifier
 * @param[out] buf Pointer to buffer where event data will be stored
 * @param[in] nwords_max The max of nwords allowed to transfer to buf
 * @return Number of words added to buffer, if successful.  Otherwise -1
 */
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
