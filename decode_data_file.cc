#include <stdio.h>
#include <stdlib.h>

#define NWORDS 10000
#define I_PRINT 1

struct data_struct {
                     unsigned int complete;       // '1' = event complete 
                     unsigned int evt_num;
                     unsigned int build_id;
                     unsigned int time_1;
                     unsigned int time_2;
                     unsigned int num_hits;      // number of valid hits in arrays that follow
                     unsigned int hit_chan[50];
                     unsigned int hit_time[50];
                     unsigned int hit_adc[50];
                     unsigned int word_count;
                   };

struct data_struct event_data;


unsigned decode_data(unsigned int data)
{
	unsigned int data_type = 0;	
	unsigned int builder_id = 0;	
	unsigned int event_number = 0;	
	unsigned int trigger_time_1 = 0;	
	unsigned int trigger_time_2 = 0;	
	unsigned int channel = 127;	
	unsigned int hit_time = 255;	
	unsigned int adc_6b = 0x3F;	
	unsigned int event_word_count = 1023;
	
        static unsigned int hits = 0;
        int i = 0;
        unsigned int complete;

	complete = 0;

	data_type = (data & 0xF0000000) >> 28;
	switch( data_type )
	{
	        case 8:		/* EVENT HEADER */
                        // initialize event data structure
                        event_data.time_1 = 0;
                        event_data.time_2 = 0;
                        event_data.num_hits = 0;
                        for(i = 0; i < NWORDS; i++)
                        {
                            event_data.hit_chan[i] = 9999;
                            event_data.hit_time[i] = 9999;
                            event_data.hit_adc[i]  = 9999;
                        }
                        event_data.word_count = 0;
                        event_data.complete = complete;


 	        	event_number = (data & 0x00FFFFFF);
                        builder_id = (data & 0x07000000) >> 24;
                        event_data.evt_num = event_number;
                        event_data.build_id = builder_id;
	                if( I_PRINT ) 
	                    printf("\n%8X - EVENT HEADER (id = %d) - event number = %d\n", data, builder_id, event_number);
	            	break;

	        case 4:		/* TRIGGER TIME 1 */
	                trigger_time_1 = (data & 0x03FFFFFF);
                        event_data.time_1 = trigger_time_1;
	                if( I_PRINT ) 
	                    printf("%8X - TRIGGER TIME 1 - time = %LX\n", data, trigger_time_1);
	            	break;

	        case 5:		/* TRIGGER TIME 2 */
	                trigger_time_2 = (data & 0x00FFFFFF);
                        event_data.time_2 = trigger_time_2;
	                if( I_PRINT ) 
	                    printf("%8X - TRIGGER TIME 2 - time = %LX\n", data, trigger_time_2);
	            	break;

	        case 0:		/* HIT DATA */
	        	channel = (data & 0x001FC000) >> 14;
	                hit_time =  (data & 0x00003FC0) >> 6;
	                adc_6b =  (data & 0x000003F);
                        event_data.hit_chan[hits] = channel;
                        event_data.hit_time[hits] = hit_time;
                        event_data.hit_adc[hits] = adc_6b;
                        hits = hits + 1;
                        event_data.num_hits = hits;
 	                if( I_PRINT ) 
	                    printf("%8X - HIT DATA - channel = %d  time = %d  adc = %d\n", 
	                    		data, channel, hit_time, adc_6b);
			break;

	        case 10:	/* EVENT TRAILER */
	        	event_word_count = (data & 0x000003FF);
                        builder_id = (data & 0x07000000) >> 24;
                        event_data.word_count = event_word_count;
                        complete = 1;	        // set flag for complete event
                        event_data.complete = complete;
	                if( I_PRINT ) 
	                    printf("%8X - EVENT TRAILER (id = %d) - event word count = %d\n", data, builder_id, event_word_count);
	            	break;

// -------------------------------------------------------------------------------------------------

	        case 1:		/* UNDEFINED TYPE */
	            if( I_PRINT ) 
	                printf("%8X - UNDEFINED TYPE = %d\n", data, data_type);
	            break;
	            
	        case 2:		/* UNDEFINED TYPE */
	            if( I_PRINT ) 
	                printf("%8X - UNDEFINED TYPE = %d\n", data, data_type);
	            break;
	            
	        case 3:		/* UNDEFINED TYPE */
	            if( I_PRINT ) 
	                printf("%8X - UNDEFINED TYPE = %d\n", data, data_type);
	            break;
	            
	        case 6:		/* UNDEFINED TYPE */
	            if( I_PRINT ) 
	                printf("%8X - UNDEFINED TYPE = %d\n", data, data_type);
	            break;
	            
	        case 7:		/* UNDEFINED TYPE */
	            if( I_PRINT ) 
	                printf("%8X - UNDEFINED TYPE = %d\n", data, data_type);
	            break;
	            
	        case 9:		/* UNDEFINED TYPE */
	            if( I_PRINT ) 
	                printf("%8X - UNDEFINED TYPE = %d\n", data, data_type);
	            break;
	            
	        case 11:	/* UNDEFINED TYPE */
	            if( I_PRINT ) 
	                printf("%8X - UNDEFINED TYPE = %d\n", data, data_type);
	            break;
	            
	        case 12:	/* UNDEFINED TYPE */
	            if( I_PRINT ) 
	                printf("%8X - UNDEFINED TYPE = %d\n", data, data_type);
	            break;
	            
	        case 13:	/* UNDEFINED TYPE */
	            if( I_PRINT ) 
	                printf("%8X - UNDEFINED TYPE = %d\n", data, data_type);
	            break;
	            
	        case 14:	/* UNDEFINED TYPE */
	            if( I_PRINT ) 
	                printf("%8X - UNDEFINED TYPE = %d\n", data, data_type);
	            break;
	            
	        case 15:	/* UNDEFINED TYPE */
	            if( I_PRINT ) 
	                printf("%8X - UNDEFINED TYPE = %d\n", data, data_type);
	            break;

	            
// -------------------------------------------------------------------------------------------------

	}

	return complete;		   

}        

int main(int argc, char *argv[])
{
  int i;
  int value;
  int len = 0;
  unsigned int complete = 0;
  FILE *f_events = fopen("vmm_direct_data.bin", "rb");


  if(f_events == NULL)
  {
      printf("ERROR opening file\n");
      exit(0);
  }

  for(i = 0; i < NWORDS; i++)
  {
      len = fread(&value, sizeof(int), 1, f_events);
      if( len == 0 )
          printf("nread = %d  len = %d  word = %08X\n", i, len, value);
      else
          complete = decode_data(unsigned(value));
  } 

  fclose(f_events);
  return 0;
}




