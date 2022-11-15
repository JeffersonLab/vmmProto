/**
 * @copyright Copyright 2022, Jefferson Science Associates, LLC.
 *            Subject to the terms in the LICENSE file found in the
 *            top-level directory.
 *
 * @author    Bryan Moffit
 *            moffit@jlab.org                   Jefferson Lab, MS-12B3
 *            Phone: (757) 269-5660             12000 Jefferson Ave.
 *            Fax:   (757) 269-5800             Newport News, VA 23606
 *
 *
 * @file      vmmPrototypeStatus.c
 * @brief     program to test fpga_io from the vmmProto library
 *
 */


#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "fpga_io.h"

char progName[256];

void
usage()
{
  printf("Usage:\n");
  printf("   %s   <ip_addr>  <reg_port>\n",
	 progName);

}

int
main(int argc, char *argv[])
{
  char ip_addr[16];
  uint16_t reg_port;

  strncpy(progName, argv[0], 256);

  if(argc != 3)
    {
      usage();
      return -1;
    }

  strncpy(ip_addr, argv[1], 16);
  reg_port = atoi(argv[2]);

  fpga_setdebug(0);

  int32_t fpga_id = fpga_init(ip_addr, reg_port, 6102);
  if(fpga_id < 0)
    {
      usage();
      return -1;
    }

  uint32_t val = fpga_read32(fpga_id, &pFPGA_regs->Clk.BoardId);
  printf("   BoardId = 0x%08X\n", val);


  close_register_socket(fpga_id);


  return 0;
}
/*
  Local Variables:
  compile-command: "make -k vmmPrototypeStatus "
  End:
*/
