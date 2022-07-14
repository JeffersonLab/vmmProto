#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>
#include <cstring>
#include "fpga_io.h"

#define SCALERS_PRINT             0
#define EVENT_DATA_PRINT          1
#define EVENT_STAT_PRINT          1
#define EVENT_DATA_SAVE           1

#define EVENT_BUFFER_NWORDS       1024

static unsigned long long nwords_current = 0;
static unsigned long long nwords_total = 0;
static unsigned long long nevents_current = 0;
static unsigned long long nevents_total = 0;
static int chip_id = 0;
static int adc_charge[32];
static int adc_fine_time[32];
static int adc_fine_time_sum[32];
static int adc_coarse_time[32];
static int adc_hit[32];

void scaler_print()
{
/*
  unsigned int scalers[192];
  unsigned int pixels[192];
  int i, j;

  rich_get_scalers(scalers, 1);

  for(i=0;i<192;i++)
  {
    int asic=i/64;
    int maroc_ch=i%64;
    int pixel = asic*64+maroc_ch_to_pixel[maroc_ch];
    pixels[pixel] = scalers[i];
  }

  for(i=0;i<8;i++)
  {
    for(j=0;j<24;j++)
    {
      int pmt = j/8;
      int col = j%8;
      int row = i;
      printf("%7u",pixels[64*pmt+8*row+col]);

      if(col==7 && pmt<2)
        printf(" | ");
    }
    printf("\n");
  }
  printf("\n");

  fflush(stdout);
*/
}

void event_stat_print(double diff)
{
  printf("Average bytes/sec     = %lf\n", (double)(nwords_current*4L)/diff);
  printf("Total bytes received  = %lf\n", (double)(nwords_total*4L)/diff);
  printf("Average events/sec    = %lf\n", (double)nevents_current/diff);
  printf("Total events received = %lf\n", (double)nevents_total/diff);
  printf("\n");

  nwords_total+= nwords_current;
  nevents_total+= nevents_current;

  nwords_current = 0;
  nevents_current = 0;
}

FILE *gf;

int process_buf(int *buf, int nwords, FILE *f, int print)
{
  static int tag = 15;
  static int tag_idx = 0;
  char str[500];
  int word;
  int nevents = 0;

  while(nwords--)
  {
    word = *buf++;

    if(f) fprintf(f, "0x%08X", word);
    if(print) printf("0x%08X\n", word);

    if(word & 0x80000000)
    {
      tag = (word>>27) & 0xF;
      tag_idx = 0;
    }
    else
      tag_idx++;

    str[0] = NULL;
    switch(tag)
    {
/*
      case 0: // block header
        sprintf(str, " [BLOCKHEADER] SLOT=%d, BLOCKNUM=%d, BLOCKSIZE=%d\n", (word>>22)&0x1F, (word>>8)&0x3FF, (word>>0)&0xFF);
        break;

      case 1: // block trailer
        sprintf(str, " [BLOCKTRAILER] SLOT=%d, WORDCNT=%d\n", (word>>22)&0x1F, (word>>0)&0x3FFFFF);
        break;
*/
      case 1: // petiroc data
        if(tag_idx == 0)
          chip_id = word & 0x3;

        if( (chip_id==0) && (tag_idx>=1 && tag_idx<=32) )
        {
          adc_hit[tag_idx-1] = (word>>20) & 0x1;
          adc_charge[tag_idx-1] = (word>> 0) & 0x3FF;
          adc_fine_time[tag_idx-1] = (word>>10) & 0x3FF;
          adc_fine_time_sum[tag_idx-1]+= (word>>10) & 0x3FF;
          adc_coarse_time[tag_idx-1] = (word>>21) & 0x1FF;
        }
        break;

      case 2: // event header
        nevents++;
/*
        sprintf(str, " [EVENTHEADER] TRIGGERNUM=%d, DEVID=%d\n", (word>>0)&0x3FFFFF, (word>>22)&0x1F);
*/
        break;

      case 3: // trigger time
/*
        if(tag_idx == 0)
          sprintf(str, " [TIMESTAMP 0] TIME=%d\n", (word>>0)&0xFFFFFF);
        else if(tag_idx == 1)
          sprintf(str, " [TIMESTAMP 1] TIME=%d\n", (word>>0)&0xFFFFFF);
*/
        break;
/*
      case 8: // TDC hit
        sprintf(str, " [TDC HIT] EDGE=%d, CH=%d, TIME=%d\n", (word>>26)&0x1,(word>>16)&0xFF, (word>>0)&0xFFFF);
        break;

      case 9: // ADC Hit
        if(tag_idx == 0)
          sprintf(str, "[ADC DATA %d] MAROC_ID=%d, RESOLUTION=%dbit, HOLD1_DLY=%d, HOLD2_DLY=%d\n", tag_idx, (word>>0)&0x3, (word>>4)&0xF, (word>>8)&0xFF, (word>>16)&0xFF);
        else
          sprintf(str, "[ADC DATA %d] Ch%2d ADC=%4d, Ch%2d ADC=%4d\n", tag_idx, 2*(tag_idx-1)+0, (word>>0)&0xFFF,2*(tag_idx-1)+1, (word>>16)&0xFFF);

      case 14:  // data not valid
        sprintf(str, " [DNV]\n");
        break;

      case 15:  // filler word
        sprintf(str, " [FILLER]\n");
        break;

      default:  // unknown
        sprintf(str, " [UNKNOWN]\n");
        break;
*/
    }
    if(str[0])
    {
      if(f) fprintf(f, "%s", str);
      if(print) printf("%s", str);
    }
  }
  return nevents;
}


int main(int argc, char *argv[])
{
  int i;
  int event_buffer[EVENT_BUFFER_NWORDS];
  int event_buffer_len;
  char data_filename[200];

#if EVENT_DATA_SAVE
  printf("\n--- Enter output file name (.bin is appended): \n");
  scanf("%s", data_filename);
  strcat(data_filename, ".bin");
  printf("\nData output file name = %s\n\n", data_filename); 
  FILE *f_events = fopen(data_filename, "wb");
//  FILE *f_events = fopen("vmm_direct_data.bin", "wb");
#endif

  struct timeval t_last, t_cur;
  double diff;
  int val;
  int channels, adc6b, hit_value;
  int key_value = 0;
  int phase_data = 0;

  open_register_socket();

  fpga_init();    /* also reads BoardId */

  printf("--- read all registers ---\n");
  val = fpga_read32(&pFPGA_regs->Clk.Ctrl);
  printf("%s: Ctrl = 0x%08X\n", __func__, val);

  val = fpga_read32(&pFPGA_regs->Clk.Ctrl2);
  printf("%s: Ctrl2 = 0x%08X\n", __func__, val);

  val = fpga_read32(&pFPGA_regs->Clk.Latency);
  printf("%s: Latency = 0x%08X\n", __func__, val);

  val = fpga_read32(&pFPGA_regs->Clk.Width);
  printf("%s: Width = 0x%08X\n", __func__, val);

  val = fpga_read32(&pFPGA_regs->Clk.Hit);
  printf("%s: Hit = 0x%08X\n", __func__, val);

  val = fpga_read32(&pFPGA_regs->Clk.Delay);
  printf("%s: Delay = 0x%08X\n", __func__, val);

  val = fpga_read32(&pFPGA_regs->Clk.Aux);
  printf("%s: Aux = 0x%08X\n", __func__, val);
//
  printf("******** LATCH STATUS VALUES ********\n");
  fpga_write32(&pFPGA_regs->Clk.Ctrl, 0x10000000);	  // LATCH status values - write Ctrl(28) = 1

  val = fpga_read32(&pFPGA_regs->Clk.Status0);
  printf("%s: Status0 = 0x%08X\n", __func__, val);

  val = fpga_read32(&pFPGA_regs->Clk.Status1);
  printf("%s: Status1 = 0x%08X\n", __func__, val);

  val = fpga_read32(&pFPGA_regs->Clk.Status2);
  printf("%s: Status2 = 0x%08X\n", __func__, val);

  val = fpga_read32(&pFPGA_regs->Clk.Status3);
  printf("%s: Status3 = 0x%08X\n", __func__, val);
//
  val = fpga_read32(&pFPGA_regs->Clk.Phase);
  printf("%s: Phase = 0x%08X\n", __func__, val);

  val = fpga_read32(&pFPGA_regs->Clk.Phase_stat);
  printf("%s: Phase status = 0x%08X\n", __func__, val);

  printf("----------------------------------\n");

/* write values to set up soft data generation */
  printf("--- do soft bus reset ---\n");
  fpga_write32(&pFPGA_regs->Clk.Ctrl, 0x40000000);	  // soft bus reset - Ctrl(30) = 1

// --------------------------------------------------------------------------------------------------------------------------------

// printf("--- select internal clock, internal data, soft trigger ---\n");
//  fpga_write32(&pFPGA_regs->Clk.Ctrl2, 0x33);		  // select internal clock, internal data, soft trigger - Ctrl2(0) = 0x33  
//  val = fpga_read32(&pFPGA_regs->Clk.Ctrl2);              
//  printf("%s: Ctrl2 = 0x%08X\n", __func__, val);

// printf("--- select internal clock, internal data, self trigger ---\n");
//  fpga_write32(&pFPGA_regs->Clk.Ctrl2, 0x23);		  // select internal clock, internal data, soft trigger - Ctrl2(0) = 0x23  
//  val = fpga_read32(&pFPGA_regs->Clk.Ctrl2);              
//  printf("%s: Ctrl2 = 0x%08X\n", __func__, val);

  printf("--- select external clock, external data, self trigger ---\n");
  fpga_write32(&pFPGA_regs->Clk.Ctrl2, 0x20);		  // select external clock, external data, self trigger - Ctrl2(0) = 0x20  
  val = fpga_read32(&pFPGA_regs->Clk.Ctrl2);              
  printf("%s: Ctrl2 = 0x%08X\n", __func__, val);

//  printf("--- select internal clock, external data, self trigger ---\n");
//  fpga_write32(&pFPGA_regs->Clk.Ctrl2, 0x22);		  // select internal clock, external data, self trigger - Ctrl2(0) = 0x22  
//  val = fpga_read32(&pFPGA_regs->Clk.Ctrl2);              
//  printf("%s: Ctrl2 = 0x%08X\n", __func__, val);

//  printf("--- select external clock, external data, external trigger ---\n");
//  fpga_write32(&pFPGA_regs->Clk.Ctrl2, 0x00);		  // select external clock, external data, external trigger - Ctrl2(0) = 0x00  
//  val = fpga_read32(&pFPGA_regs->Clk.Ctrl2);              
//  printf("%s: Ctrl2 = 0x%08X\n", __func__, val);

//----------------------------------------------------------------------------------------------------------------------------------

  printf("--- do soft reset after clock change! ---\n");
  fpga_write32(&pFPGA_regs->Clk.Ctrl, 0x80000000);	  // soft reset after clock change - Ctrl(31) = 1

  printf("--- write trigger latency ---\n");
//  fpga_write32(&pFPGA_regs->Clk.Latency, 200);	  // write trigger latency (~200)
  fpga_write32(&pFPGA_regs->Clk.Latency, 7);		  // use short latency (~4) for self trigger mode
  val = fpga_read32(&pFPGA_regs->Clk.Latency);		  // 
  printf("%s: Latency = 0x%08X\n", __func__, val);

  printf("--- write trigger window width ---\n");
  fpga_write32(&pFPGA_regs->Clk.Width, 100);		  // write trigger window width 
  val = fpga_read32(&pFPGA_regs->Clk.Width);
  printf("%s: Width = 0x%08X\n", __func__, val);

// to capture soft hit soft trigger delay must satisfy: (latency-width) < delay < latency
  printf("--- write soft trigger delay ---\n");
//  fpga_write32(&pFPGA_regs->Clk.Delay, 150);		 // soft trigger mode: delay (~150) 
  fpga_write32(&pFPGA_regs->Clk.Delay, 0x00640001);	 // self trigger mode: trigger width (~width) and trigger delay (~1) 
  val = fpga_read32(&pFPGA_regs->Clk.Delay);             // use short delay (~1) for self_trigger mode
  printf("%s: Delay = 0x%08X\n", __func__, val);

  printf("--- load latency, trig delay value into memory ---\n");
  fpga_write32(&pFPGA_regs->Clk.Ctrl, 0x20000000);	  // load latency, trig delay
 
  getchar();
// phase adjust
  printf("\nEnter number of phase shift steps (0-4095) (18.5 ps/step) - ");
  scanf("%d", &key_value);
  phase_data = 0x80000000 | (0xFFF & key_value);
  printf("phase data = %08X (steps = %d)\n\n", phase_data, key_value);

  val = fpga_read32(&pFPGA_regs->Clk.Phase_stat);
  printf("%s: Phase status = 0x%08X\n", __func__, val);
  printf("--- write phase adjust register ---\n");
  fpga_write32(&pFPGA_regs->Clk.Phase, phase_data);	 // bits 11-0 are number of steps, bit 31 starts state machine 
  val = fpga_read32(&pFPGA_regs->Clk.Phase);
  printf("%s: Phase = 0x%08X\n", __func__, val);
  val = fpga_read32(&pFPGA_regs->Clk.Phase_stat);
  printf("%s: Phase status = 0x%08X\n", __func__, val);
  sleep(1);                                              // wait 
  val = fpga_read32(&pFPGA_regs->Clk.Phase_stat);
  printf("%s: Phase status = 0x%08X\n", __func__, val);

// read status registers
  printf("******** LATCH STATUS VALUES ********\n");
  fpga_write32(&pFPGA_regs->Clk.Ctrl, 0x10000000);	  // LATCH status values - write Ctrl(28) = 1

  printf("--- read STATUS registers ---\n");
  val = fpga_read32(&pFPGA_regs->Clk.Status0);
  printf("%s: Status0 = 0x%08X\n", __func__, val);

  val = fpga_read32(&pFPGA_regs->Clk.Status1);
  printf("%s: Status1 = 0x%08X\n", __func__, val);

  val = fpga_read32(&pFPGA_regs->Clk.Status2);
  printf("%s: Status2 = 0x%08X\n", __func__, val);

  val = fpga_read32(&pFPGA_regs->Clk.Status3);
  printf("%s: Status3 = 0x%08X\n", __func__, val);
//
  sleep(3);                                             // wait 

printf("--- GO! ---\n");
  fpga_write32(&pFPGA_regs->Clk.Ctrl, 1);	  	  // GO! 
  val = fpga_read32(&pFPGA_regs->Clk.Ctrl);
  printf("%s: Ctrl = 0x%08X\n", __func__, val);

// +++++++++++++++++++++++++++++++++++++++++++++++++++
//  printf("--- write soft hit data ---\n");		  // Hit(11-0) = channels, Hit(21-16) = adc6b
//  channels = 0x001;
//  adc6b = 0x0E;
//  hit_value = (adc6b << 16) | channels;
//  fpga_write32(&pFPGA_regs->Clk.Hit, hit_value);
//  val = fpga_read32(&pFPGA_regs->Clk.Hit);
//  printf("%s: Hit = 0x%08X\n", __func__, val);

//  printf("--- pulse soft hit data ---\n");		  // Hit(31) = 1
//  hit_value = 0x80000000 | (adc6b << 16) | channels;
//  fpga_write32(&pFPGA_regs->Clk.Hit, hit_value);
//  val = fpga_read32(&pFPGA_regs->Clk.Hit);
//  printf("%s: Hit = 0x%08X\n", __func__, val);

//  printf("--- write soft hit data ---\n");		  // Hit(11-0) = channels, Hit(21-16) = adc6b
//  channels = 0x002;
//  adc6b = 0x1E;
//  hit_value = (adc6b << 16) | channels;
//  fpga_write32(&pFPGA_regs->Clk.Hit, hit_value);
//  val = fpga_read32(&pFPGA_regs->Clk.Hit);
//  printf("%s: Hit = 0x%08X\n", __func__, val);

//  printf("--- pulse soft hit data ---\n");		  // Hit(31) = 1
//  hit_value = 0x80000000 | (adc6b << 16) | channels;
//  fpga_write32(&pFPGA_regs->Clk.Hit, hit_value);
//  val = fpga_read32(&pFPGA_regs->Clk.Hit);
//  printf("%s: Hit = 0x%08X\n", __func__, val);

//  printf("--- write soft hit data ---\n");		  // Hit(11-0) = channels, Hit(21-16) = adc6b
//  channels = 0x004;
//  adc6b = 0x2E;
//  hit_value = (adc6b << 16) | channels;
//  fpga_write32(&pFPGA_regs->Clk.Hit, hit_value);
//  val = fpga_read32(&pFPGA_regs->Clk.Hit);
//  printf("%s: Hit = 0x%08X\n", __func__, val);

//  printf("--- pulse soft hit data ---\n");		  // Hit(31) = 1
//  hit_value = 0x80000000 | (adc6b << 16) | channels;
//  fpga_write32(&pFPGA_regs->Clk.Hit, hit_value);
//  val = fpga_read32(&pFPGA_regs->Clk.Hit);
//  printf("%s: Hit = 0x%08X\n", __func__, val);

//  printf("--- write soft hit data ---\n");		  // Hit(11-0) = channels, Hit(21-16) = adc6b
//  channels = 0x004;
//  adc6b = 0x3E;
// hit_value = (adc6b << 16) | channels;
//  fpga_write32(&pFPGA_regs->Clk.Hit, hit_value);
//  val = fpga_read32(&pFPGA_regs->Clk.Hit);
//  printf("%s: Hit = 0x%08X\n", __func__, val);

//  printf("--- pulse soft hit data ---\n");		  // Hit(31) = 1
//  hit_value = 0x80000000 | (adc6b << 16) | channels;
//  fpga_write32(&pFPGA_regs->Clk.Hit, hit_value);
//  val = fpga_read32(&pFPGA_regs->Clk.Hit);
//  printf("%s: Hit = 0x%08X\n", __func__, val);

//  printf("--- write soft hit data ---\n");		  // Hit(11-0) = channels, Hit(21-16) = adc6b
//  channels = 0xAAA;
//  adc6b = 0x3A;
//  hit_value = (adc6b << 16) | channels;
//  fpga_write32(&pFPGA_regs->Clk.Hit, hit_value);
//  val = fpga_read32(&pFPGA_regs->Clk.Hit);
//  printf("%s: Hit = 0x%08X\n", __func__, val);

//  printf("--- pulse soft hit data ---\n");		  // Hit(31) = 1
//  hit_value = 0x80000000 | (adc6b << 16) | channels;
//  fpga_write32(&pFPGA_regs->Clk.Hit, hit_value);
//  val = fpga_read32(&pFPGA_regs->Clk.Hit);
//  printf("%s: Hit = 0x%08X\n", __func__, val);

//  printf("--- write soft hit data ---\n");		  // Hit(11-0) = channels, Hit(21-16) = adc6b
//  channels = 0xFFF;
//  adc6b = 0x1A;
//  hit_value = (adc6b << 16) | channels;
//  fpga_write32(&pFPGA_regs->Clk.Hit, hit_value);
//  val = fpga_read32(&pFPGA_regs->Clk.Hit);
//  printf("%s: Hit = 0x%08X\n", __func__, val);

//  printf("--- pulse soft hit data ---\n");		  // Hit(31) = 1
//  hit_value = 0x80000000 | (adc6b << 16) | channels;
//  fpga_write32(&pFPGA_regs->Clk.Hit, hit_value);
//  val = fpga_read32(&pFPGA_regs->Clk.Hit);
//  printf("%s: Hit = 0x%08X\n", __func__, val);

//  printf("--- write soft hit data ---\n");		  // Hit(11-0) = channels, Hit(21-16) = adc6b
//  channels = 0x101;
//  adc6b = 0xB;
//  hit_value = (adc6b << 16) | channels;
//  fpga_write32(&pFPGA_regs->Clk.Hit, hit_value);
//  val = fpga_read32(&pFPGA_regs->Clk.Hit);
//  printf("%s: Hit = 0x%08X\n", __func__, val);

//  printf("--- pulse soft hit data ---\n");		  // Hit(31) = 1
//  hit_value = 0x80000000 | (adc6b << 16) | channels;
//  fpga_write32(&pFPGA_regs->Clk.Hit, hit_value);
//  val = fpga_read32(&pFPGA_regs->Clk.Hit);
//  printf("%s: Hit = 0x%08X\n", __func__, val);

//  printf("--- write soft hit data ---\n");		  // Hit(11-0) = channels, Hit(21-16) = adc6b
//  channels = 0x010;
//  adc6b = 0x18;
//  hit_value = (adc6b << 16) | channels;
//  fpga_write32(&pFPGA_regs->Clk.Hit, hit_value);
//  val = fpga_read32(&pFPGA_regs->Clk.Hit);
//  printf("%s: Hit = 0x%08X\n", __func__, val);

//  printf("--- pulse soft hit data ---\n");		  // Hit(31) = 1
//  hit_value = 0x80000000 | (adc6b << 16) | channels;
//  fpga_write32(&pFPGA_regs->Clk.Hit, hit_value);
//  val = fpga_read32(&pFPGA_regs->Clk.Hit);
//  printf("%s: Hit = 0x%08X\n", __func__, val);

// +++++++++++++++++++++++++++++++++++++++++++++++++++

  sleep(1);
  open_event_socket();
  printf("Socket opened, press key to continue\n");
  getchar();
  getchar();
  getchar();
//  petiroc_adc_init(1);


  unsigned char hold_delay = 0;
  int cnt=0;
  gettimeofday(&t_last, NULL);

    channels = 1;	// start values
    adc6b = 0;

  while(1)
  {
    event_buffer_len = read_event_socket(event_buffer, EVENT_BUFFER_NWORDS);

    if(event_buffer_len > 0)
    {
#if EVENT_DATA_SAVE
      fwrite(event_buffer, event_buffer_len, sizeof(event_buffer[0]), f_events);
#endif
      nwords_current+= event_buffer_len;

      nevents_current += process_buf(event_buffer, event_buffer_len, NULL, EVENT_DATA_PRINT);
    }
    else
      usleep(1000);

    gettimeofday(&t_cur, NULL);
    diff = t_cur.tv_sec + 1.0E-6 * t_cur.tv_usec - t_last.tv_sec - 1.0E-6 * t_last.tv_usec;
    if(diff > 1.0)
    {
#if EVENT_DATA_SAVE
      fflush(f_events);
#endif  

#if EVENT_STAT_PRINT
      event_stat_print(diff);
#endif

#if SCALERS_PRINT
      scaler_print();
#endif
      gettimeofday(&t_last, NULL);
    }

// read status registers
//  printf("******** LATCH STATUS VALUES ********\n");
//  fpga_write32(&pFPGA_regs->Clk.Ctrl, 0x10000001);	  // LATCH status values - write Ctrl(28) = 1

//  printf("--- read STATUS registers ---\n");
//  val = fpga_read32(&pFPGA_regs->Clk.Status0);
// printf("%s: Status0 = 0x%08X\n", __func__, val);

//  val = fpga_read32(&pFPGA_regs->Clk.Status1);
//  printf("%s: Status1 = 0x%08X\n", __func__, val);

//  val = fpga_read32(&pFPGA_regs->Clk.Status2);
//  printf("%s: Status2 = 0x%08X\n", __func__, val);

//  val = fpga_read32(&pFPGA_regs->Clk.Status3);
//  printf("%s: Status3 = 0x%08X\n", __func__, val);

/* -------------------------------------------------------------
      hit_value = 0x80000000 | (adc6b << 16) | channels;
      fpga_write32(&pFPGA_regs->Clk.Hit, hit_value);
      val = fpga_read32(&pFPGA_regs->Clk.Hit);
      printf("%s: Hit = 0x%08X\n", __func__, val);    

      channels = channels << 1; // advance hit channel number (0-11)
      if( channels == 0x1000 )   // after all channels hit, reset to channel 1 and increment adc value 
      {
         channels = 1;
     	 adc6b = adc6b + 1;
      }

      if( adc6b == 64 )		// if adc at max value, reset to 0
         adc6b = 0; */
// -------------------------------------------------------------

  }
  close_register_socket();
  close_event_socket();

#if EVENT_DATA_SAVE
  fclose(f_events);
#endif  
  return 0;
}

