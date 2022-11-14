#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "fpga_io.h"

int
main(int argc, char *argv[])
{

  fpga_init("129.57.29.143", 6102, 6102);			/* also reads BoardId */

  close_register_socket(0);


  return 0;
}

/*
  Local Variables:
  compile-command: "make -k vmmProtoTest "
  End:
*/
