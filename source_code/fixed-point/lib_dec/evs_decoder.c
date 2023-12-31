#ifdef _WIN32
#include <windows.h>
#define sleep_sec(x) Sleep((x) * 1000)
#else
#include <unistd.h>
#define sleep_sec(x) sleep(x)
#endif

#include "evs_decoder.h"

/************************************************************
Usage : EVS_dec.exe[Options] Fs bitstream_file output_file

	Mandatory parameters :
		-------------------- -
			Fs : Output sampling rate in kHz(8, 16, 32 or 48)
			bitstream_file : Input bitstream filename or RTP packet filename(in VOIP mode)
			output_file : Output speech filename

			Options :
		--------
			- VOIP : VOIP mode,
			-Tracefile TF : Generate trace file named TF,
			-no_delay_cmp : Turn off delay compensation
			- fec_cfg_file : Optimal channel aware configuration computed by the JBM
			as described in Section 6.3.1 of TS26.448.The output is
			written into a.txt file.Each line contains the FER indicator
			(HI | LO) and optimal FEC offset.
			- mime : Mime input bitstream file format
			The decoder reads both TS26.445 Annex.2.6 and RFC4867 Mime Storage Format,
			the magic word in the mime input file is used to determine the format.
			default input bitstream file format is G.192
			- q : Quiet mode, no frame counter
			default is OFF
******************************************************************************************/

EvsDecoderContext* NewEvsDecoder(void)
{
    EvsDecoderContext  *dec;   
    if ( (dec = (EvsDecoderContext *) calloc( 1, sizeof(EvsDecoderContext) ) ) == NULL )
    {
        fprintf(stderr, "Can not allocate memory for EvsDecoderContext state structure\n");
        return NULL;
    }  

    return dec; 
}

int InitDecoder(EvsDecoderContext* dec,int sample,int bitRate,int isG192Format)
{
    char *strSample;
    int argc;
    if ( (dec->st_fx = (Decoder_State_fx *) calloc(1, sizeof(Decoder_State_fx) ) ) == NULL )
    {
        fprintf(stderr, "Can not allocate memory for Decoder_State_fx state structure\n");
        return -1;
    }

     if ( (dec->buf = (DecoderDataBuf *) calloc( 1, sizeof(DecoderDataBuf) ) ) == NULL )
    {
        fprintf(stderr, "Can not allocate memory for EncoderDataBuf state structure\n");
        return -1;
    }

    BASOP_init

    dec->f_stream = NULL;
    dec->f_synth = NULL;
    dec->jbmFECoffsetFileName = NULL;
    dec->jbmTraceFileName = NULL;
    dec->quietMode = 0;
    dec-> noDelayCmp = 0;
    dec->frame  = 0;

   
    if(sample == 8000){
        strSample = "8";
    }else if(sample == 16000){
        strSample = "16";
    }else if(sample == 32000){
        strSample = "32";
    }else if(sample == 48000){
        strSample = "48";
    }else{
        strSample = "8";
    }

    dec->st_fx->bit_stream_fx = dec->bit_stream;
    dec->st_fx->total_brate_fx = bitRate;

	if (isG192Format == 0)
	{
		char *argv[5];
		argv[0] = "EVS_dec";
		argv[1] = "-MIME";
		argv[2] = strSample;
		argv[3] = (char*)dec->f_stream;
		argv[4] = (char*)dec->f_synth;
	  argc = sizeof(argv) / sizeof(argv[0]);
		io_ini_dec_fx(argc, argv, &dec->f_stream, &dec->f_synth,
			&dec->quietMode,
			&dec->noDelayCmp,
			dec->st_fx,
#ifdef SUPPORT_JBM_TRACEFILE
			&dec->jbmTraceFileName,
#endif
			&dec->jbmFECoffsetFileName
		);
	}
	else 
	{
		char *argv[4];
		argv[0] = "EVS_dec";
		argv[1] = strSample;
		argv[2] = (char*)dec->f_stream;
		argv[3] = (char*)dec->f_synth;
	  argc = sizeof(argv) / sizeof(argv[0]);
		io_ini_dec_fx(argc, argv, &dec->f_stream, &dec->f_synth,
			&dec->quietMode,
			&dec->noDelayCmp,
			dec->st_fx,
#ifdef SUPPORT_JBM_TRACEFILE
			&dec->jbmTraceFileName,
#endif
			&dec->jbmFECoffsetFileName
		);
	}

    dec->st_fx->output_frame_fx = extract_l(Mult_32_16(dec->st_fx->output_Fs_fx , 0x0290));

    srand((unsigned int)time(0));

    reset_indices_dec_fx(dec->st_fx);
    
    init_decoder_fx(dec->st_fx);

     if( dec->noDelayCmp == 0)
      {
          /* calculate the compensation (decoded signal aligned with original signal) */
          /* the number of first output samples will be reduced by this amount */
          dec->dec_delay = NS2SA_fx2(dec->st_fx->output_Fs_fx, get_delay_fx(DEC, dec->st_fx->output_Fs_fx));
      }
      else
      {
          dec->dec_delay = 0;
      }

      dec->zero_pad = dec->dec_delay;

     /*------------------------------------------------------------------------------------------*
     * Loop for every packet (frame) of bitstream data
     * - Read the bitstream packet
     * - Run the decoder
     * - Write the synthesized signal into output file
     *------------------------------------------------------------------------------------------*/
     if (dec->quietMode == 0)
     {
         fprintf( stdout, "\n------ Running the decoder ------\n\n" );
         fprintf( stdout, "Decoder Frames processed:       \n" );
     }
     else {
         fprintf( stdout, "\n-- Start the decoder (quiet mode) --\n\n" );
     }
     BASOP_end_noprint;
     BASOP_init;
#if (WMOPS)
       Init_WMOPS_counter();
       Reset_WMOPS_counter();
       setFrameRate(48000, 960);
#endif

    return 0;
    
}

/*------------------------------------------------------------------------------------------*
    * Loop for every packet (frame) of bitstream data
    * - Read the bitstream packet
    * - Run the decoder
    * - Write the synthesized signal into output buf
    *------------------------------------------------------------------------------------------*/
int EvsStartDecoder(EvsDecoderContext *dec,char* data)
{
    Word16   output_frame;
    Word16   ret  = 0;

     if (dec == NULL && dec->st_fx  == NULL)
     {
         fprintf(stdout,"EvsStartDecoder dec is NULL\n");
         return -1;
     }

    /* output frame length */
    output_frame = dec->st_fx->output_frame_fx;
    dec->buf->size = output_frame;

    /*----- loop: decode-a-frame -----*/
	if (dec->st_fx->bitstreamformat == G192)
		ret = read_indices_fx_real(dec->st_fx, (UWord16*)data, 0);
	else
		ret = read_indices_mime_real(dec->st_fx, (UWord8*)data, 0);

    if (ret < 0){
		dec->buf->size = -1;
        return ret;
    }
        
#if (WMOPS)
            fwc();
            Reset_WMOPS_counter();
#endif

            SUB_WMOPS_INIT("evs_dec");
            /* run the main encoding routine */
            if (dec != NULL && dec->st_fx  != NULL)
            {
                if(sub(dec->st_fx->codec_mode, MODE1) == 0)
                {
                    if ( dec->st_fx->Opt_AMR_WB_fx )
                    {
                        amr_wb_dec_fx(dec->buf->data,dec->st_fx);
                    }
                    else
                    {
                       evs_dec_fx( dec->st_fx, dec->buf->data, FRAMEMODE_NORMAL);
                    }
                }
                else
               {
                    if(dec->st_fx->bfi_fx == 0)
                    {
                        evs_dec_fx( dec->st_fx, dec->buf->data, FRAMEMODE_NORMAL);
                    }
                    else /* conceal */
                    {
                       evs_dec_fx( dec->st_fx, dec->buf->data, FRAMEMODE_MISSING);
                    }
               }
            }


            END_SUB_WMOPS;

            /* increase the counter of initialization frames */
            if( dec != NULL && sub(dec->st_fx->ini_frame_fx,MAX_FRAME_COUNTER) < 0 )
            {
                if(dec->st_fx != NULL)
                dec->st_fx->ini_frame_fx = add(dec->st_fx->ini_frame_fx,1);
            }

             /* write the synthesized signal into output file */
             /* do final delay compensation */
              /*if( dec->dec_delay == 0 )
              {
                   fwrite(dec->buf->data, sizeof(Word16), output_frame, dec->f_synth );
              }
              else
               {
                    if ( sub(dec->dec_delay , output_frame) <= 0 )
                    {
                         fwrite(dec->buf->data +dec->dec_delay, sizeof(Word16), sub(output_frame , dec->dec_delay), dec->f_synth );
                         dec->dec_delay = 0;
                         move16();
                     }
                     else
                     {
                         dec->dec_delay = sub(dec->dec_delay, output_frame);
                     }
               }*/
            dec->frame++;
        
    return 0;
}

      
int StopDecoder(EvsDecoderContext *dec)
{
    if(dec == NULL){
       return -1;
    }

    sleep_sec(3);
    /*----- decode-a-frame-loop end -----*/
    fflush( stderr );
    if (dec->quietMode == 0)
    {
          fprintf( stdout, "\n\n" );
          fprintf( stdout, "EVS Decoding finished:       " );
    }
    else
    {
         fprintf(stdout,"EVS Decoding of %ld frames finished\n\n", dec->frame);
    }

    /* end of WMOPS counting */
#if (WMOPS)
     fwc();
     fprintf(stdout,"\nDecoder complexity\n");
     WMOPS_output(0);
     fprintf("\n");
#endif

    if(dec->f_synth){
     fwrite( dec->buf->data, sizeof(Word16), dec->zero_pad, dec->f_synth );
     fclose( dec->f_synth );
    }

     /* free memory etc. */
     if(dec->st_fx)
     {
        destroy_decoder( dec->st_fx );
     }

     if(dec->f_stream)  fclose( dec->f_stream );
     if(dec) {
       free(dec);
       dec = NULL;
     }

     fprintf( stdout, "EVS StopDecoder success\n\n" );
     fflush(stdout);
     fflush(stderr);

     return 0;
}

int UnitTestEvsDecoder(void)
{
   int n_samples = 0;
   char data[61];                              /* Input buffer */
   EvsDecoderContext *dec = NewEvsDecoder();

   FILE * f_input = fopen("./encoder.evs","rb");
   if(!f_input)
   {
        return -1;
   }

    InitDecoder(dec,8000,24400,0);
   
    fprintf( stdout, "\n------ Running the decoder ------\n\n" );
    while( (n_samples = (char)fread(data, sizeof(char), 61, f_input)) > 0 )
    {
       
        EvsStartDecoder(dec,data);
    }

   
    StopDecoder(dec);
    return 0;
}