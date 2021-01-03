 #include <stdlib.h>
 #include <stdio.h>
 #include <string.h>

 #ifdef HAVE_AV_CONFIG_H
 #undef HAVE_AV_CONFIG_H
 #endif

 #include "libavcodec/avcodec.h"
 #include "libavutil/mathematics.h"

 #define INBUF_SIZE 4096
 #define AUDIO_INBUF_SIZE 20480
 #define AUDIO_REFILL_THRESH 4096

 /*
  * Audio encoding example
  */
 static void audio_encode_example(const char* filename)
 {
	     AVCodec * codec;
	     AVCodecContext * c = NULL;
	     int frame_size, i, j, out_size, outbuf_size;
	     FILE * f;
	     short* samples;
	     float t, tincr;
	     uint8_t * outbuf;
	
		  printf("Audio encoding\n");
	
		     /* find the MP2 encoder */
		  codec = avcodec_find_encoder(CODEC_ID_MP2);
	     if (!codec) {
		         fprintf(stderr, "codec not found\n");
		         exit(1);
		
	}
	
		     c = avcodec_alloc_context();
	
		     /* put sample parameters */
		     c->bit_rate = 64000;
			c->sample_rate = 44100;
			c->channels = 2;
	
		     /* open it */
		     if (avcodec_open(c, codec) < 0) {
		         fprintf(stderr, "could not open codec\n");
		         exit(1);
	}
	
		 /* the codec gives us the frame size, in samples */
		 frame_size = c->frame_size;
	     samples = malloc(frame_size * 2 * c->channels);
	     outbuf_size = 10000;
	     outbuf = malloc(outbuf_size);
	
		 f = fopen(filename, "wb");
	     if (!f) {
		         fprintf(stderr, "could not open %s\n", filename);
		         exit(1);
	}
	
		     /* encode a single tone sound */
		     t = 0;
	     tincr = 2 * M_PI * 440.0 / c->sample_rate;
	     for (i = 0; i < 200; i++) {
			for (j = 0; j < frame_size; j++) {
			     samples[2 * j] = (int)(sin(t) * 10000);
			      samples[2 * j + 1] = samples[2 * j];
			        t += tincr;
			
		}
		         /* encode the samples */
			         out_size = avcodec_encode_audio(c, outbuf, outbuf_size, samples);
		        fwrite(outbuf, 1, out_size, f);
		
	}
	     fclose(f);
	     free(outbuf);
	     free(samples);
	
		     avcodec_close(c);
	     av_free(c);
	 }

 /*
  * Audio decoding.
  */
 static void audio_decode_example(const char* outfilename, const char* filename)
 {
	     AVCodec* decCodec;
	     AVCodecContext * decCodecCtx = NULL;
	     int out_size, len;
	     FILE * f, * outfile;
	     uint8_t * outbuf;
	     uint8_t inbuf[AUDIO_INBUF_SIZE + FF_INPUT_BUFFER_PADDING_SIZE];
	     AVPacket avpkt;
		 AVFormatContext *ifmt_ctx;
		 av_init_packet(&avpkt);
		
		 if ((ret = avformat_open_input(&ifmt_ctx, filename, NULL, NULL)) < 0) {
			 av_log(NULL, AV_LOG_ERROR, "Cannot open input file\n");
			 return ret;
		 }

		 /* find the mpeg audio decoder */
		 codec = avcodec_find_decoder(ifmt_ctx.);
	     if (!codec)
		{
			fprintf(stderr, "codec not found\n");
		    exit(1);
		 }
	
		     c = avcodec_alloc_context();
	
		     /* open it */
		     if (avcodec_open(c, codec) < 0) {
		         fprintf(stderr, "could not open codec\n");
		         exit(1);
	
	}
	
		     outbuf = malloc(AVCODEC_MAX_AUDIO_FRAME_SIZE);
	
		     f = fopen(filename, "rb");
	     if (!f) {
		         fprintf(stderr, "could not open %s\n", filename);
		         exit(1);
		
		}
	     outfile = fopen(outfilename, "wb");
	     if (!outfile) {
		         av_free(c);
		         exit(1);
		 }
	
		     /* decode until eof */
		      avpkt.data = inbuf;
			avpkt.size = fread(inbuf, 1, AUDIO_INBUF_SIZE, f);
	
		     while (avpkt.size > 0) 
			 {
		         out_size = AVCODEC_MAX_AUDIO_FRAME_SIZE;
		         len = avcodec_decode_audio3(c, (short*)outbuf, &out_size, &avpkt);
		         if (len < 0) 
				 {
					fprintf(stderr, "Error while decoding\n");
					exit(1);
				 }
		         if (out_size > 0) 
				 {
					/* if a frame has been decoded, output it */
				    fwrite(outbuf, 1, out_size, outfile);
		         }
		         avpkt.size -= len;
		         avpkt.data += len;
		         if (avpkt.size < AUDIO_REFILL_THRESH) {
			             /* Refill the input buffer, to avoid trying to decode
			              * incomplete frames. Instead of this, one could also use
			              * a parser, or use a proper container format through
			              * libavformat. */
				             memmove(inbuf, avpkt.data, avpkt.size);
			             avpkt.data = inbuf;
			             len = fread(avpkt.data + avpkt.size, 1,
				                         AUDIO_INBUF_SIZE - avpkt.size, f);
			             if (len > 0)
				                 avpkt.size += len;
			
		}
		
	}
	
		     fclose(outfile);
	     fclose(f);
	     free(outbuf);
	
		     avcodec_close(c);
	     av_free(c);
	 }

 /*
  * Video encoding example
  */
 static void video_encode_example(const char* filename)
 {
	     AVCodec * codec;
	     AVCodecContext * c = NULL;
	     int i, out_size, size, x, y, outbuf_size;
	     FILE * f;
	     AVFrame * picture;
	     uint8_t* outbuf, * picture_buf;
	
		     printf("Video encoding\n");
	
		     /* find the mpeg1 video encoder */
		     codec = avcodec_find_encoder(CODEC_ID_MPEG1VIDEO);
	     if (!codec) {
		         fprintf(stderr, "codec not found\n");
		         exit(1);
		
	}
	
		     c = avcodec_alloc_context();
	     picture = avcodec_alloc_frame();
	
		     /* put sample parameters */
		     c->bit_rate = 400000;
	     /* resolution must be a multiple of two */
		     c->width = 352;
	     c->height = 288;
	     /* frames per second */
		     c->time_base = (AVRational){ 1,25 };
	     c->gop_size = 10; /* emit one intra frame every ten frames */
	     c->max_b_frames = 1;
	     c->pix_fmt = PIX_FMT_YUV420P;
	
		     /* open it */
		     if (avcodec_open(c, codec) < 0) {
		         fprintf(stderr, "could not open codec\n");
		         exit(1);
		
	}
	
		     f = fopen(filename, "wb");
	     if (!f) {
		         fprintf(stderr, "could not open %s\n", filename);
		         exit(1);
		
	}
	
		     /* alloc image and output buffer */
		 outbuf_size = 100000;
	     outbuf = malloc(outbuf_size);
	     size = c->width * c->height;
	     picture_buf = malloc((size * 3) / 2); /* size for YUV 420 */
	
		      picture->data[0] = picture_buf;
	     picture->data[1] = picture->data[0] + size;
	     picture->data[2] = picture->data[1] + size / 4;
	     picture->linesize[0] = c->width;
	     picture->linesize[1] = c->width / 2;
	     picture->linesize[2] = c->width / 2;
	
		/* encode 1 second of video */
		     for (i = 0; i < 25; i++) {
		         fflush(stdout);
		         /* prepare a dummy image */
			         /* Y */
			         for (y = 0; y < c->height; y++) {
			             for (x = 0; x < c->width; x++) {
				                 picture->data[0][y * picture->linesize[0] + x] = x + y + i * 3;
				
			}
			
		}
		
			         /* Cb and Cr */
			         for (y = 0; y < c->height / 2; y++) {
			             for (x = 0; x < c->width / 2; x++) {
				                 picture->data[1][y * picture->linesize[1] + x] = 128 + y + i * 2;
				                 picture->data[2][y * picture->linesize[2] + x] = 64 + x + i * 5;
				
			}
			
		}
		
			         /* encode the image */
			          out_size = avcodec_encode_video(c, outbuf, outbuf_size, picture);
		         printf("encoding frame %3d (size=%5d)\n", i, out_size);
		         fwrite(outbuf, 1, out_size, f);
		
	}
	
		     /* get the delayed frames */
		     for (; out_size; i++) {
		         fflush(stdout);
		
			          out_size = avcodec_encode_video(c, outbuf, outbuf_size, NULL);
		         printf("write frame %3d (size=%5d)\n", i, out_size);
		         fwrite(outbuf, 1, out_size, f);
		
	}
	
		     /* add sequence end code to have a real mpeg file */
		      outbuf[0] = 0x00;
	     outbuf[1] = 0x00;
	     outbuf[2] = 0x01;
	     outbuf[3] = 0xb7;
	     fwrite(outbuf, 1, 4, f);
	     fclose(f);
	     free(picture_buf);
	     free(outbuf);
	
		     avcodec_close(c);
	     av_free(c);
	     av_free(picture);
	     printf("\n");
	 }
00307
00308 /*
00309  * Video decoding example
00310  */
00311
00312 static void pgm_save(unsigned char* buf, int wrap, int xsize, int ysize,
	00313                      char* filename)
	00314 {
	00315     FILE * f;
	00316     int i;
	00317
		00318     f = fopen(filename, "w");
	00319     fprintf(f, "P5\n%d %d\n%d\n", xsize, ysize, 255);
	00320     for (i = 0; i < ysize; i++)
		00321         fwrite(buf + i * wrap, 1, xsize, f);
	00322     fclose(f);
	00323 }
00324
00325 static void video_decode_example(const char* outfilename, const char* filename)
00326 {
	00327     AVCodec * codec;
	00328     AVCodecContext* c = NULL;
	00329     int frame, got_picture, len;
	00330     FILE * f;
	00331     AVFrame * picture;
	00332     uint8_t inbuf[INBUF_SIZE + FF_INPUT_BUFFER_PADDING_SIZE];
	00333     char buf[1024];
	00334     AVPacket avpkt;
	00335
		00336     av_init_packet(&avpkt);
	00337
		00338     /* set end of buffer to 0 (this ensures that no overreading happens for damaged mpeg streams) */
		00339     memset(inbuf + INBUF_SIZE, 0, FF_INPUT_BUFFER_PADDING_SIZE);
	00340
		00341     printf("Video decoding\n");
	00342
		00343     /* find the mpeg1 video decoder */
		00344     codec = avcodec_find_decoder(CODEC_ID_MPEG1VIDEO);
	00345     if (!codec) {
		00346         fprintf(stderr, "codec not found\n");
		00347         exit(1);
		00348
	}
	00349
		00350     c = avcodec_alloc_context();
	00351     picture = avcodec_alloc_frame();
	00352
		00353     if (codec->capabilities & CODEC_CAP_TRUNCATED)
		00354         c->flags |= CODEC_FLAG_TRUNCATED; /* we do not send complete frames */
	00355
		00356     /* For some codecs, such as msmpeg4 and mpeg4, width and height
		00357        MUST be initialized there because this information is not
		00358        available in the bitstream. */
		00359
		00360     /* open it */
		00361     if (avcodec_open(c, codec) < 0) {
		00362         fprintf(stderr, "could not open codec\n");
		00363         exit(1);
		00364
	}
	00365
		00366     /* the codec gives us the frame size, in samples */
		00367
		00368     f = fopen(filename, "rb");
	00369     if (!f) {
		00370         fprintf(stderr, "could not open %s\n", filename);
		00371         exit(1);
		00372
	}
	00373
		00374     frame = 0;
	00375     for (;;) {
		00376         avpkt.size = fread(inbuf, 1, INBUF_SIZE, f);
		00377         if (avpkt.size == 0)
			00378             break;
		00379
			00380         /* NOTE1: some codecs are stream based (mpegvideo, mpegaudio)
			00381            and this is the only method to use them because you cannot
			00382            know the compressed data size before analysing it.
			00383
			00384            BUT some other codecs (msmpeg4, mpeg4) are inherently frame
			00385            based, so you must call them with all the data for one
			00386            frame exactly. You must also initialize 'width' and
			00387            'height' before initializing them. */
			00388
			00389         /* NOTE2: some codecs allow the raw parameters (frame size,
			00390            sample rate) to be changed at any frame. We handle this, so
			00391            you should also take care of it */
			00392
			00393         /* here, we use a stream based decoder (mpeg1video), so we
			00394            feed decoder and see if it could decode a frame */
			00395         avpkt.data = inbuf;
		00396         while (avpkt.size > 0) {
			00397             len = avcodec_decode_video2(c, picture, &got_picture, &avpkt);
			00398             if (len < 0) {
				00399                 fprintf(stderr, "Error while decoding frame %d\n", frame);
				00400                 exit(1);
				00401
			}
			00402             if (got_picture) {
				00403                 printf("saving frame %3d\n", frame);
				00404                 fflush(stdout);
				00405
					00406                 /* the picture is allocated by the decoder. no need to
					00407                    free it */
					00408                 snprintf(buf, sizeof(buf), outfilename, frame);
				00409                 pgm_save(picture->data[0], picture->linesize[0],
					00410                          c->width, c->height, buf);
				00411                 frame++;
				00412
			}
			00413             avpkt.size -= len;
			00414             avpkt.data += len;
			00415
		}
		00416
	}
	00417
		00418     /* some codecs, such as MPEG, transmit the I and P frame with a
		00419        latency of one frame. You must do the following to have a
		00420        chance to get the last frame of the video */
		00421     avpkt.data = NULL;
	00422     avpkt.size = 0;
	00423     len = avcodec_decode_video2(c, picture, &got_picture, &avpkt);
	00424     if (got_picture) {
		00425         printf("saving last frame %3d\n", frame);
		00426         fflush(stdout);
		00427
			00428         /* the picture is allocated by the decoder. no need to
			00429            free it */
			00430         snprintf(buf, sizeof(buf), outfilename, frame);
		00431         pgm_save(picture->data[0], picture->linesize[0],
			00432                  c->width, c->height, buf);
		00433         frame++;
		00434
	}
	00435
		00436     fclose(f);
	00437
		00438     avcodec_close(c);
	00439     av_free(c);
	00440     av_free(picture);
	00441     printf("\n");
	00442 }
00443
00444 int main(int argc, char** argv)
00445 {
	00446     const char* filename;
	00447
		00448     /* must be called before using avcodec lib */
		00449     avcodec_init();
	00450
		00451     /* register all the codecs */
		00452     avcodec_register_all();
	00453
		00454     if (argc <= 1) {
		00455         audio_encode_example("/tmp/test.mp2");
		00456         audio_decode_example("/tmp/test.sw", "/tmp/test.mp2");
		00457
			00458         video_encode_example("/tmp/test.mpg");
		00459         filename = "/tmp/test.mpg";
		00460
	}
		else {
		00461         filename = argv[1];
		00462
	}
	00463
		00464     //    audio_decode_example("/tmp/test.sw", filename);
		00465     video_decode_example("/tmp/test%d.pgm", filename);
	00466
		00467     return 0;
	00468 }