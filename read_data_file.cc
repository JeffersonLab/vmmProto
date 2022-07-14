#include <stdio.h>
#include <stdlib.h>

#define EVENT_BUFFER_NWORDS 1024

int main(int argc, char *argv[])
{
  int i;
  int len = 0;
  int event_buffer[EVENT_BUFFER_NWORDS];
  FILE *f_events = fopen("vmm_direct_data.bin", "rb");

  if(f_events == NULL)
  {
      printf("ERROR opening file\n");
      exit(0);
  }

  len = fread(event_buffer, sizeof(event_buffer), 1, f_events);
  printf("len = %d\n", len); 

// print first 100 values
  for(i = 0; i < 100; i++)
      printf("%08X\n", event_buffer[i]); 

  fclose(f_events);
  return 0;
}

