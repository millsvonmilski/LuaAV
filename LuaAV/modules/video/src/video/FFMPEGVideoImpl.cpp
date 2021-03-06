#include "FFMPEGVideoImpl.h"
#include <stdint.h>

namespace muro {

FFMPEGVideoImpl :: FFMPEGVideoImpl()
:	VideoImpl(VideoImpl::FFMPEG),
	mAVformatCtx(NULL),
	mAVstream(NULL),
	mAVcodecCtx(NULL),
	mStreamIdx(-1),
	mAVframe(NULL),
	mAVswsCtx(NULL)
{
	ffmpeg::initialize();
}

FFMPEGVideoImpl :: ~FFMPEGVideoImpl() {

}

void FFMPEGVideoImpl :: destroy() {
	if(mAVformatCtx) {
		close();
	}
}

MuroError FFMPEGVideoImpl :: open(const char *filename) {
	MuroError err = MURO_ERROR_NONE;

	// destroy previously open file if necessary
	destroy();

	// Open video file (last 3 params do autodetect of (AVInputFormat *fmt,int buf_size, AVFormatParameters *ap)
	if(av_open_input_file(&mAVformatCtx, filename, NULL, 0, NULL) != 0) {
		return muro::setError(MURO_ERROR_GENERIC, "FFMPEGVideoImpl.open: unable to open file");
	}

	// Retrieve stream information
	if(av_find_stream_info(mAVformatCtx) < 0) {
		return muro::setError(MURO_ERROR_GENERIC, "FFMPEGVideoImpl.open: couldn't find stream information");
	}

	printInfo(filename);

	return err;
}

MuroError FFMPEGVideoImpl :: close() {
	MuroError err = MURO_ERROR_NONE;

	if(mAVformatCtx) {
		mStreamIdx = -1;

		av_free((void *)mAVframe);
		mAVframe = NULL;

		avcodec_close(mAVcodecCtx);
		mAVcodecCtx = NULL;

		mAVstream = NULL;
		av_close_input_file(mAVformatCtx);
		mAVformatCtx = NULL;
		
		mRGBFrame.free();
	}

	return err;
}

MuroError FFMPEGVideoImpl :: save(const char *filename) {
	MuroError err = MURO_ERROR_NONE;
	return err;
}

MuroError FFMPEGVideoImpl :: play() {
	MuroError err = MURO_ERROR_NONE;
	int vstream = -1;

	// Find the first video stream
	for(unsigned int i=0; i < mAVformatCtx->nb_streams; i++) {
		if(mAVformatCtx->streams[i]->codec->codec_type == CODEC_TYPE_VIDEO) {
			vstream = i;
			break;
		}
	}

	// check to see if we got a valid video stream
	if(vstream == -1) {
		return muro::setError(MURO_ERROR_GENERIC, "FFMPEGVideoImpl.play: couldn't find video stream in file");
	}
	mStreamIdx = vstream;

	// Get a pointer to the codec context for the video stream
	mAVcodecCtx = mAVformatCtx->streams[vstream]->codec;
	mAVstream = mAVformatCtx->streams[vstream];

	err = openCodec();
	if(err) return err;

	// allocate buffers for reading video data
	// video frame (will hold raw input codec data)
	mAVframe = avcodec_alloc_frame();
	mRGBFrame.alloc(PIX_FMT_RGB24, mAVcodecCtx->width, mAVcodecCtx->height);
	
	// conversion between raw and RGB frame
	mAVswsCtx = sws_getContext(mAVcodecCtx->width, mAVcodecCtx->height, mAVcodecCtx->pix_fmt,
					mAVcodecCtx->width, mAVcodecCtx->height, PIX_FMT_RGB24,
					SWS_FAST_BILINEAR, NULL, NULL, NULL);

	return err;
}

MuroError FFMPEGVideoImpl :: nextFrame(Matrix *mat) {
	MuroError err = MURO_ERROR_NONE;

	err = nextFrame();
	if(err) return err;


	int ret = sws_scale(mAVswsCtx,
						mAVframe->data, mAVframe->linesize,
						0, mAVcodecCtx->height,
						mRGBFrame.frame->data, mRGBFrame.frame->linesize);
	if(ret != 0) {
		//error
	}

	mat->adapt(3, Matrix::UCHAR, mAVcodecCtx->width, mAVcodecCtx->height);

	int irowsize = mRGBFrame.frame->linesize[0];
	int orowsize; mat->getRowSize(orowsize);
	for(int j=0; j < mat->header.dim[1]; j++) {
		Matrix::uchar *iptr = (Matrix::uchar *)(mRGBFrame.frame->data[0] + j*irowsize);
		Matrix::uchar *optr = mat->data.ptr + j*orowsize;

		for(int i=0; i < mat->header.dim[0]; i++) {
			*optr++ = *iptr++;
			*optr++ = *iptr++;
			*optr++ = *iptr++;
		}
	}

	return err;
}

MuroError FFMPEGVideoImpl :: openCodec() {
	MuroError err = MURO_ERROR_NONE;
	AVCodec *codec = NULL;

	if(!mAVformatCtx || !mAVcodecCtx) return MURO_ERROR_GENERIC;

	// Find the decoder for the video stream
	codec = avcodec_find_decoder(mAVcodecCtx->codec_id);
	if(codec == NULL) {
		return muro::setError(MURO_ERROR_GENERIC, "FFMPEGVideoImpl.play: couldn't find codec for video stream");
	}

	// Inform the codec that we can handle truncated bitstreams -- i.e.,
	// bitstreams where frame boundaries can fall in the middle of packets
//	if(codec->capabilities & CODEC_CAP_TRUNCATED)
//		mAVcodecCtx->flags |= CODEC_FLAG_TRUNCATED;

	// Open codec (couple it to the codec context)
	if(avcodec_open(mAVcodecCtx, codec) < 0) {
		return muro::setError(MURO_ERROR_GENERIC, "FFMPEGVideoImpl.play: couldn't open codec for video stream");
	}

	// sanity check for wrong frame rates that seem to be generated by some codecs
	//if(mAVcodecCtx->time_base.num > 1000 && mAVcodecCtx->time_base.den == 1) {
		//pCodecCtx->frame_rate_base=1000;
	//	return muro::setError(MURO_ERROR_GENERIC, "FFMPEGVideoImpl.play: video codec is producing invalid frame rate base values");
	//}

	return err;
}


/*
 if(pkt.pts != AV_NOPTS_VALUE){
        dp->pts=pkt.pts * av_q2d(priv->avfc->streams[id]->time_base);
        priv->last_pts= dp->pts * AV_TIME_BASE;
        if(pkt.convergence_duration)
            dp->endpts = dp->pts + pkt.convergence_duration * av_q2d(priv->avfc->streams[id]->time_base);
*/

/*
> As you can see, the AVPacket.dts field never ends up being 1 value shy
> of what the AVStream duration field reports.  When I'm watching the
> video decode, I can see pretty clearly that the last frame decoded is
> the 2nd to last at which point av_read_frame returns -1.  Is this
> expected behavior?  I'm using a build of libavcodec/format from a
> checkout that's a few hours old.

Repeating what i've seen mentioned earlier on the list, the problem is
that the codec must hold onto a picture in case the next B frame needs to
reference it.  When you run out of packets, you can get this last picture
by passing an empty packet (length 0) to avcodec_decode_video.
*/
MuroError FFMPEGVideoImpl :: nextFrame() {
	MuroError err = MURO_ERROR_NONE;

//	if(!mAVformatCtx || !mAVcodecCtx || !mAVframe || mStreamIdx == -1) return MURO_ERROR_GENERIC;

	int frameFinished = 0;
	bool done = true;
	double pts;
	AVPacket packet;

	while(av_read_frame(mAVformatCtx, &packet) >= 0) {
		pts = 0.;

		// Is this a packet from the video stream?
		if(packet.stream_index == mStreamIdx) {
			// Decode video frame
			avcodec_decode_video(mAVcodecCtx, mAVframe, &frameFinished, packet.data, packet.size);

			if(packet.dts != AV_NOPTS_VALUE) {
				pts = packet.dts;
			} else {
				pts = 0;
			}
			pts *= av_q2d(mAVstream->time_base);
//			printf("pts: %f  dur: %f %lld   cur: %lld\n", pts, ((double)mAVstream->duration)*av_q2d(mAVstream->time_base),
//					mAVstream->duration, packet.dts);
			// Did we get a video frame?
			if(frameFinished) {
//				printf("---------------------------\n");
				// Free the packet that was allocated by av_read_frame
				done = false;
				av_free_packet(&packet);
				break;
			}
		}

		// Free the packet that was allocated by av_read_frame
		av_free_packet(&packet);
	}

	// no frames left, start at beginning
	if(done) {
		if(av_seek_frame(mAVformatCtx, mStreamIdx, mAVformatCtx->start_time, 0) < 0) {
			return muro::setError(MURO_ERROR_GENERIC, "FFMPEGVideoImpl.nextFrame: error looping video");
		}
		return nextFrame();
	}

	return err;
}


void FFMPEGVideoImpl :: printInfo(const char *filename) {
	if(mAVformatCtx) {
		AVFormatContext *ic = mAVformatCtx;
		int index = 0;
		const char *url = filename;
		int is_output = false;
		int i;

		printf("%s #%d, %s, %s '%s':\n",
				is_output ? "Output" : "Input",
				index,
				is_output ? ic->oformat->name : ic->iformat->name,
				is_output ? "to" : "from", url);

		if(!is_output) {
		   printf("  Duration: ");
			if(ic->duration != AV_NOPTS_VALUE) {
				int hours, mins, secs, us;
				secs = ic->duration / AV_TIME_BASE;
				us = ic->duration % AV_TIME_BASE;
				mins = secs / 60;
				secs %= 60;
				hours = mins / 60;
				mins %= 60;
				printf("%02d:%02d:%02d.%02d", hours, mins, secs,
					   (100 * us) / AV_TIME_BASE);
			} else {
				printf("N/A");
			}
			if (ic->start_time != AV_NOPTS_VALUE) {
				int secs, us;
				printf(", start: ");
				secs = ic->start_time / AV_TIME_BASE;
				us = ic->start_time % AV_TIME_BASE;
				printf("%d.%06d",
					   secs, (int)av_rescale(us, 1000000, AV_TIME_BASE));
			}
			printf(", bitrate: ");
			if (ic->bit_rate) {
				printf("%d kb/s", ic->bit_rate / 1000);
			} else {
				printf("N/A");
			}
			printf("\n");
		}
		/*
		Apparently this isn't in the Ubuntu FFMPEG package yet
		if(ic->nb_programs) {
			int j, k;
			for(j=0; j<ic->nb_programs; j++) {
				printf("  Program %d %s\n", ic->programs[j]->id,
					   ic->programs[j]->name ? ic->programs[j]->name : "");
				for(k=0; k<ic->programs[j]->nb_stream_indexes; k++)
					printStreamFormat(ic, ic->programs[j]->stream_index[k], index, is_output);
			 }
		} else*/
		for(i=0;i<ic->nb_streams;i++)
			printStreamFormat(ic, i, index, is_output);
	}
}


void FFMPEGVideoImpl :: printStreamFormat(AVFormatContext *ic, int i, int index, int is_output) {
    char buf[256];
    int flags = (is_output ? ic->oformat->flags : ic->iformat->flags);
    AVStream *st = ic->streams[i];
    int g = ff_gcd(st->time_base.num, st->time_base.den);
    avcodec_string(buf, sizeof(buf), st->codec, is_output);
    printf("    Stream #%d.%d", index, i);
    /* the pid is an important information, so we display it */
    /* XXX: add a generic system */
    if (flags & AVFMT_SHOW_IDS)
		printf("[0x%x]", st->id);
    if (strlen(st->language) > 0)
		printf("(%s)", st->language);
	printf(", %d/%d", st->time_base.num/g, st->time_base.den/g);
	printf(": %s", buf);
    if(st->codec->codec_type == CODEC_TYPE_VIDEO){
        if(st->r_frame_rate.den && st->r_frame_rate.num)
            printf(", %5.2f tb(r)", av_q2d(st->r_frame_rate));
/*      else if(st->time_base.den && st->time_base.num)
            av_log(NULL, AV_LOG_INFO, ", %5.2f tb(m)", 1/av_q2d(st->time_base));*/
        else
			printf(", %5.2f tb(c)", 1/av_q2d(st->codec->time_base));
    }
	printf("\n");
}

}	// muro::
