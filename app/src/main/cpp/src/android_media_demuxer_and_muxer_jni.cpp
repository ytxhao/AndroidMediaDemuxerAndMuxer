#include <jni.h>
#include <string>
#include <ican_ytx_com_MediaUtils.h>
#include "muxer_common.h"

/*
FIX: H.264 in some container format (FLV, MP4, MKV etc.) need
"h264_mp4toannexb" bitstream filter (BSF)
  *Add SPS,PPS in front of IDR frame
  *Add start code ("0,0,0,1") in front of NALU
H.264 in some container (MPEG2TS) don't need this BSF.
*/
//'1': Use H.264 Bitstream Filter
#define USE_H264BSF 0

/*
FIX:AAC in some container format (FLV, MP4, MKV etc.) need
"aac_adtstoasc" bitstream filter (BSF)
*/
//'1': Use AAC Bitstream Filter
#define USE_AACBSF 1


JNIEXPORT jint JNICALL Java_ican_ytx_com_andoridmediademuxerandmuxer_MediaUtils_demuxer
        (JNIEnv *env, jobject obj, jstring inputFile,jstring outputVideoFile,jstring outputAudioFile)
{

    AVOutputFormat *ofmt_a = NULL,*ofmt_v = NULL;
    //（Input AVFormatContext and Output AVFormatContext）
    AVFormatContext *ifmt_ctx = NULL, *ofmt_ctx_a = NULL, *ofmt_ctx_v = NULL;
    AVPacket pkt;
    int ret, i;
    int videoindex=-1,audioindex=-1;
    int frame_index=0;

    const char *in_filename  = env->GetStringUTFChars(inputFile, NULL);//Input file URL
    //char *in_filename  = "cuc_ieschool.mkv";
    const char *out_filename_v = env->GetStringUTFChars(outputVideoFile, NULL);//Output file URL
    //char *out_filename_a = "cuc_ieschool.mp3";
    const char *out_filename_a =  env->GetStringUTFChars(outputAudioFile, NULL);;
    AVBitStreamFilterContext* h264bsfc = NULL;
    av_register_all();
    //Input
    if ((ret = avformat_open_input(&ifmt_ctx, in_filename, 0, 0)) < 0) {
        J4A_ALOGD( "Could not open input file.");
        goto end;
    }
    if ((ret = avformat_find_stream_info(ifmt_ctx, 0)) < 0) {
        J4A_ALOGD( "Failed to retrieve input stream information");
        goto end;
    }

    //Output
    avformat_alloc_output_context2(&ofmt_ctx_v, NULL, NULL, out_filename_v);
    if (!ofmt_ctx_v) {
        J4A_ALOGD( "Could not create output context\n");
        ret = AVERROR_UNKNOWN;
        goto end;
    }
    ofmt_v = ofmt_ctx_v->oformat;

    avformat_alloc_output_context2(&ofmt_ctx_a, NULL, NULL, out_filename_a);
    if (!ofmt_ctx_a) {
        J4A_ALOGD( "Could not create output context\n");
        ret = AVERROR_UNKNOWN;
        goto end;
    }
    ofmt_a = ofmt_ctx_a->oformat;

    for (i = 0; i < ifmt_ctx->nb_streams; i++) {
        //Create output AVStream according to input AVStream
        AVFormatContext *ofmt_ctx;
        AVStream *in_stream = ifmt_ctx->streams[i];
        AVStream *out_stream = NULL;

        if(ifmt_ctx->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO){
            videoindex=i;
            out_stream=avformat_new_stream(ofmt_ctx_v, in_stream->codec->codec);
            ofmt_ctx=ofmt_ctx_v;
        }else if(ifmt_ctx->streams[i]->codec->codec_type==AVMEDIA_TYPE_AUDIO){
            audioindex=i;
            out_stream=avformat_new_stream(ofmt_ctx_a, in_stream->codec->codec);
            ofmt_ctx=ofmt_ctx_a;
        }else{
            break;
        }

        if (!out_stream) {
            J4A_ALOGD( "Failed allocating output stream\n");
            ret = AVERROR_UNKNOWN;
            goto end;
        }
        //Copy the settings of AVCodecContext
        if (avcodec_copy_context(out_stream->codec, in_stream->codec) < 0) {
            J4A_ALOGD( "Failed to copy context from input to output stream codec context\n");
            goto end;
        }
        out_stream->codec->codec_tag = 0;

        if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
            out_stream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
    }

    //Dump Format------------------
    J4A_ALOGD("\n==============Input Video=============\n");
    av_dump_format(ifmt_ctx, 0, in_filename, 0);
    J4A_ALOGD("\n==============Output Video============\n");
    av_dump_format(ofmt_ctx_v, 0, out_filename_v, 1);
    J4A_ALOGD("\n==============Output Audio============\n");
    av_dump_format(ofmt_ctx_a, 0, out_filename_a, 1);
    J4A_ALOGD("\n======================================\n");
    //Open output file
    if (!(ofmt_v->flags & AVFMT_NOFILE)) {
        if (avio_open(&ofmt_ctx_v->pb, out_filename_v, AVIO_FLAG_WRITE) < 0) {
            J4A_ALOGD( "Could not open output file '%s'", out_filename_v);
            goto end;
        }
    }

    if (!(ofmt_a->flags & AVFMT_NOFILE)) {
        if (avio_open(&ofmt_ctx_a->pb, out_filename_a, AVIO_FLAG_WRITE) < 0) {
            J4A_ALOGD( "Could not open output file '%s'", out_filename_a);
            goto end;
        }
    }

    //Write file header
    if (avformat_write_header(ofmt_ctx_v, NULL) < 0) {
        J4A_ALOGD( "Error occurred when opening video output file\n");
        goto end;
    }
    if (avformat_write_header(ofmt_ctx_a, NULL) < 0) {
        J4A_ALOGD( "Error occurred when opening audio output file\n");
        goto end;
    }

#if USE_H264BSF
    h264bsfc =  av_bitstream_filter_init("h264_mp4toannexb");
#endif

    while (1) {
        AVFormatContext *ofmt_ctx;
        AVStream *in_stream, *out_stream;
        //Get an AVPacket
        if (av_read_frame(ifmt_ctx, &pkt) < 0)
            break;
        in_stream  = ifmt_ctx->streams[pkt.stream_index];


        if(pkt.stream_index==videoindex){
            out_stream = ofmt_ctx_v->streams[0];
            ofmt_ctx=ofmt_ctx_v;
            J4A_ALOGD("Write Video Packet. size:%d\tpts:%lld\n",pkt.size,pkt.pts);
#if USE_H264BSF
            av_bitstream_filter_filter(h264bsfc, in_stream->codec, NULL, &pkt.data, &pkt.size, pkt.data, pkt.size, 0);
#endif
        }else if(pkt.stream_index==audioindex){
            out_stream = ofmt_ctx_a->streams[0];
            ofmt_ctx=ofmt_ctx_a;
            J4A_ALOGD("Write Audio Packet. size:%d\tpts:%lld\n",pkt.size,pkt.pts);
        }else{
            continue;
        }


        //Convert PTS/DTS
        pkt.pts = av_rescale_q_rnd(pkt.pts, in_stream->time_base, out_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
        pkt.dts = av_rescale_q_rnd(pkt.dts, in_stream->time_base, out_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
        pkt.duration = av_rescale_q(pkt.duration, in_stream->time_base, out_stream->time_base);
        pkt.pos = -1;
        pkt.stream_index=0;
        //Write
        if (av_interleaved_write_frame(ofmt_ctx, &pkt) < 0) {
            J4A_ALOGD( "Error muxing packet\n");
            break;
        }
        //J4A_ALOGD("Write %8d frames to output file\n",frame_index);
        av_free_packet(&pkt);
        frame_index++;
    }

#if USE_H264BSF
    av_bitstream_filter_close(h264bsfc);
#endif

    //Write file trailer
    av_write_trailer(ofmt_ctx_a);
    av_write_trailer(ofmt_ctx_v);
    end:
    avformat_close_input(&ifmt_ctx);
    /* close output */
    if (ofmt_ctx_a && !(ofmt_a->flags & AVFMT_NOFILE))
        avio_close(ofmt_ctx_a->pb);

    if (ofmt_ctx_v && !(ofmt_v->flags & AVFMT_NOFILE))
        avio_close(ofmt_ctx_v->pb);

    avformat_free_context(ofmt_ctx_a);
    avformat_free_context(ofmt_ctx_v);


    if (ret < 0 && ret != AVERROR_EOF) {
        J4A_ALOGD( "Error occurred.\n");
        return -1;
    }
    J4A_ALOGD( "Demuxer finish.\n");
    return 0;
}



void log_callback(void* ptr, int level, const char* fmt,va_list vl)
{
    J4A_VLOGD(fmt,vl);
}


JNIEXPORT jint JNICALL Java_ican_ytx_com_andoridmediademuxerandmuxer_MediaUtils_muxer
        (JNIEnv *env, jobject obj, jstring inputVideoFile, jstring inputAudioFile, jstring outputMediaFile)
{
    const char *in_filename_v = env->GetStringUTFChars(inputVideoFile, NULL);
    const char *in_filename_a = env->GetStringUTFChars(inputAudioFile, NULL);
    const char *out_filename = env->GetStringUTFChars(outputMediaFile, NULL);//Output file URL

    //av_log_set_callback(log_callback);
    avcodec_register_all();
    avfilter_register_all();
    av_register_all();
    avformat_network_init();
    ffmpeg_parse_options(1,in_filename_a,out_filename);




    J4A_ALOGD( "muxer finish.\n");
    return 0;
}
