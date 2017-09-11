#include <jni.h>
#include <string>
#include <android/log.h>
#include <ican_ytx_com_MediaUtils.h>

#ifdef __cplusplus
extern "C"
{
#endif
#include <libavformat/avformat.h>

void log_callback(void* ptr, int level, const char* fmt,va_list vl);

#ifdef __cplusplus
};
#endif


#define J4A_LOG_TAG "J4A"
#define J4A_ALOGV(...)  __android_log_print(ANDROID_LOG_VERBOSE,    J4A_LOG_TAG, __VA_ARGS__)
#define J4A_ALOGD(...)  __android_log_print(ANDROID_LOG_DEBUG,      J4A_LOG_TAG, __VA_ARGS__)
#define J4A_ALOGI(...)  __android_log_print(ANDROID_LOG_INFO,       J4A_LOG_TAG, __VA_ARGS__)
#define J4A_ALOGW(...)  __android_log_print(ANDROID_LOG_WARN,       J4A_LOG_TAG, __VA_ARGS__)
#define J4A_ALOGE(...)  __android_log_print(ANDROID_LOG_ERROR,      J4A_LOG_TAG, __VA_ARGS__)


#define J4A_VLOGV(...)  __android_log_vprint(ANDROID_LOG_VERBOSE,   J4A_LOG_TAG, __VA_ARGS__)
#define J4A_VLOGD(...)  __android_log_vprint(ANDROID_LOG_DEBUG,     J4A_LOG_TAG, __VA_ARGS__)
#define J4A_VLOGI(...)  __android_log_vprint(ANDROID_LOG_INFO,      J4A_LOG_TAG, __VA_ARGS__)
#define J4A_VLOGW(...)  __android_log_vprint(ANDROID_LOG_WARN,      J4A_LOG_TAG, __VA_ARGS__)
#define J4A_VLOGE(...)  __android_log_vprint(ANDROID_LOG_ERROR,     J4A_LOG_TAG, __VA_ARGS__)
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
    uint8_t error[128];

    AVOutputFormat *ofmt = NULL;
    //Input AVFormatContext and Output AVFormatContext
    AVFormatContext *ifmt_ctx_v = NULL, *ifmt_ctx_a = NULL,*ofmt_ctx = NULL;
    AVPacket pkt;
    int ret, i;
    int videoindex_v=-1,videoindex_out=-1;
    int audioindex_a=-1,audioindex_out=-1;
    int frame_index=0;
    int64_t cur_pts_v=0,cur_pts_a=0;
    AVBitStreamFilterContext* h264bsfc = NULL;
    AVBitStreamFilterContext* aacbsfc = NULL;
    //const char *in_filename_v = "cuc_ieschool.ts";//Input file URL
    const char *in_filename_v = env->GetStringUTFChars(inputVideoFile, NULL);
    //const char *in_filename_a = "cuc_ieschool.mp3";
    //const char *in_filename_a = "gowest.m4a";
    //const char *in_filename_a = "gowest.aac";
    const char *in_filename_a = env->GetStringUTFChars(inputAudioFile, NULL);

    const char *out_filename = env->GetStringUTFChars(outputMediaFile, NULL);//Output file URL
    av_register_all();
    av_log_set_callback(log_callback);
    //Input
    J4A_ALOGD( "in_filename_v=%s in_filename_a=%s",in_filename_v,in_filename_a);
 //   J4A_ALOGD("ytxhao test r_frame_rate.num=%d,r_frame_rate.den=%d",ifmt_ctx_v->streams[0]->r_frame_rate.num,ifmt_ctx_v->streams[0]->r_frame_rate.den);
 //   J4A_ALOGD("ytxhao test video time_base.num=%d,time_base.den=%d",ifmt_ctx_v->streams[0]->time_base.num,ifmt_ctx_v->streams[0]->time_base.den);
    if ((ret = avformat_open_input(&ifmt_ctx_v, in_filename_v, 0, 0)) < 0) {
        av_strerror(ret, (char *) error, sizeof(error));
        J4A_ALOGD( "Could not open input file ret=%d error=%s",ret,error);
        goto end;
    }
    J4A_ALOGD("ytxhao test r_frame_rate.num=%d,r_frame_rate.den=%d",ifmt_ctx_v->streams[0]->r_frame_rate.num,ifmt_ctx_v->streams[0]->r_frame_rate.den);
    J4A_ALOGD("ytxhao test video time_base.num=%d,time_base.den=%d",ifmt_ctx_v->streams[0]->time_base.num,ifmt_ctx_v->streams[0]->time_base.den);
    if ((ret = avformat_find_stream_info(ifmt_ctx_v, 0)) < 0) {
        J4A_ALOGD( "Failed to retrieve input stream information");
        goto end;
    }
    J4A_ALOGD("ytxhao test r_frame_rate.num=%d,r_frame_rate.den=%d",ifmt_ctx_v->streams[0]->r_frame_rate.num,ifmt_ctx_v->streams[0]->r_frame_rate.den);
    J4A_ALOGD("ytxhao test video time_base.num=%d,time_base.den=%d",ifmt_ctx_v->streams[0]->time_base.num,ifmt_ctx_v->streams[0]->time_base.den);
    if ((ret = avformat_open_input(&ifmt_ctx_a, in_filename_a, 0, 0)) < 0) {
        J4A_ALOGD( "Could not open input file ");
        goto end;
    }
    if ((ret = avformat_find_stream_info(ifmt_ctx_a, 0)) < 0) {
        J4A_ALOGD( "Failed to retrieve input stream information");
        goto end;
    }
    J4A_ALOGD("===========Input Information==========\n");
    av_dump_format(ifmt_ctx_v, 0, in_filename_v, 0);
    av_dump_format(ifmt_ctx_a, 0, in_filename_a, 0);
    J4A_ALOGD("======================================\n");
    //Output
    avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, out_filename);
    if (!ofmt_ctx) {
        J4A_ALOGD( "Could not create output context\n");
        ret = AVERROR_UNKNOWN;
        goto end;
    }
    ofmt = ofmt_ctx->oformat;

    for (i = 0; i < ifmt_ctx_v->nb_streams; i++) {
        //Create output AVStream according to input AVStream
        if(ifmt_ctx_v->streams[i]->codecpar->codec_type==AVMEDIA_TYPE_VIDEO){
            AVStream *in_stream = ifmt_ctx_v->streams[i];

            AVStream *out_stream = avformat_new_stream(ofmt_ctx, NULL);
            AVCodecContext *dec_ctx = avcodec_alloc_context3(avcodec_find_decoder(in_stream->codec->codec_id));
            avcodec_copy_context(dec_ctx, in_stream->codec);
            videoindex_v=i;
            if (!out_stream) {
                J4A_ALOGD( "Failed allocating output stream\n");
                ret = AVERROR_UNKNOWN;
                goto end;
            }
            AVCodecContext *pCodecCtx = out_stream->codec;
            videoindex_out=out_stream->index;


            pCodecCtx->codec_type = dec_ctx->codec_type;
            pCodecCtx->codec_id = dec_ctx->codec_id;
            pCodecCtx->codec_tag = dec_ctx->codec_tag;
            pCodecCtx->bit_rate       = dec_ctx->bit_rate;
            pCodecCtx->rc_max_rate    = dec_ctx->rc_max_rate;
            pCodecCtx->rc_buffer_size = dec_ctx->rc_buffer_size;
            pCodecCtx->field_order    = dec_ctx->field_order;
            pCodecCtx->extradata_size= dec_ctx->extradata_size;
            pCodecCtx->bits_per_coded_sample  = dec_ctx->bits_per_coded_sample;
            pCodecCtx->time_base = in_stream->time_base;

            out_stream->disposition = in_stream->disposition ;

            pCodecCtx->bits_per_raw_sample = dec_ctx->bits_per_raw_sample;
            //复制AVCodecContext的设置（Copy the settings of AVCodecContext）

            if(in_stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO){
                pCodecCtx->channel_layout     = in_stream->codecpar->channel_layout;
                pCodecCtx->sample_rate        = in_stream->codecpar->sample_rate;
                pCodecCtx->channels           = in_stream->codecpar->channels;
                pCodecCtx->frame_size         = in_stream->codecpar->frame_size;
                pCodecCtx->audio_service_type = in_stream->codec->audio_service_type;
                pCodecCtx->block_align        = in_stream->codecpar->block_align;
                pCodecCtx->initial_padding    = in_stream->codec->delay;
                pCodecCtx->profile            = in_stream->codecpar->profile;
            }else{
                pCodecCtx->pix_fmt            = dec_ctx->pix_fmt;
                pCodecCtx->colorspace         = dec_ctx->colorspace;
                pCodecCtx->color_range        = in_stream->codecpar->color_range;
                pCodecCtx->color_primaries    = in_stream->codecpar->color_primaries;
                pCodecCtx->color_trc          = in_stream->codecpar->color_trc;
                pCodecCtx->width              = in_stream->codecpar->width;
                pCodecCtx->height             = in_stream->codecpar->height;
                pCodecCtx->has_b_frames       = in_stream->codec->has_b_frames;
            }
            out_stream->avg_frame_rate = in_stream->avg_frame_rate;
            out_stream->r_frame_rate = in_stream->r_frame_rate;
            //ret = avcodec_parameters_from_context(out_stream->codecpar,pCodecCtx);

            uint64_t extra_size;
            extra_size = (uint64_t)dec_ctx->extradata_size + AV_INPUT_BUFFER_PADDING_SIZE;
            if(dec_ctx->extradata_size){
                pCodecCtx->extradata  = (uint8_t *) av_mallocz(extra_size);
                if (!pCodecCtx->extradata) {
                    J4A_ALOGD("pCodecCtx->extradata is null");
                }
                memcpy(pCodecCtx->extradata, dec_ctx->extradata, dec_ctx->extradata_size);
            }
            J4A_ALOGD("extradata_size=%d extradata=%#x codec_type=%d",pCodecCtx->extradata_size,pCodecCtx->extradata,pCodecCtx->codec_type);
            for(int i=0;i<pCodecCtx->extradata_size;i++){
                J4A_ALOGD("extradata[%d]=%#x",i,pCodecCtx->extradata[i]);
            }
            out_stream->codecpar->codec_tag = 0;
            if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
                pCodecCtx->flags |= CODEC_FLAG_GLOBAL_HEADER;
            break;
        }
    }

    for (i = 0; i < ifmt_ctx_a->nb_streams; i++) {
        //Create output AVStream according to input AVStream
        if(ifmt_ctx_a->streams[i]->codecpar->codec_type==AVMEDIA_TYPE_AUDIO){
            AVStream *in_stream = ifmt_ctx_a->streams[i];
            AVStream *out_stream = avformat_new_stream(ofmt_ctx, in_stream->codec->codec);
            AVCodecContext *dec_ctx = avcodec_alloc_context3(avcodec_find_decoder(in_stream->codec->codec_id));
            avcodec_copy_context(dec_ctx, in_stream->codec);
            audioindex_a=i;
            if (!out_stream) {
                J4A_ALOGD( "Failed allocating output stream\n");
                ret = AVERROR_UNKNOWN;
                goto end;
            }
            AVCodecContext *pCodecCtx = out_stream->codec;
            audioindex_out=out_stream->index;

            pCodecCtx->codec_type = dec_ctx->codec_type;
            pCodecCtx->codec_id = dec_ctx->codec_id;
            pCodecCtx->codec_tag = dec_ctx->codec_tag;
            pCodecCtx->bit_rate       = dec_ctx->bit_rate;
            pCodecCtx->rc_max_rate    = dec_ctx->rc_max_rate;
            pCodecCtx->rc_buffer_size = dec_ctx->rc_buffer_size;
            pCodecCtx->field_order    = dec_ctx->field_order;
            pCodecCtx->extradata_size= dec_ctx->extradata_size;
            pCodecCtx->bits_per_coded_sample  = dec_ctx->bits_per_coded_sample;
            pCodecCtx->time_base = in_stream->time_base;

            out_stream->disposition = in_stream->disposition ;

            pCodecCtx->bits_per_raw_sample = dec_ctx->bits_per_raw_sample;
            //复制AVCodecContext的设置（Copy the settings of AVCodecContext）

            if(in_stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO){
                pCodecCtx->channel_layout     = dec_ctx->channel_layout;
                pCodecCtx->sample_rate        = dec_ctx->sample_rate;
                pCodecCtx->channels           = dec_ctx->channels;
                pCodecCtx->frame_size         = dec_ctx->frame_size;
                pCodecCtx->audio_service_type = dec_ctx->audio_service_type;
                pCodecCtx->block_align        = dec_ctx->block_align;
                pCodecCtx->initial_padding    = dec_ctx->delay;
                pCodecCtx->profile            = dec_ctx->profile;
            }else{
                pCodecCtx->pix_fmt            = dec_ctx->pix_fmt;
                pCodecCtx->colorspace         = dec_ctx->colorspace;
                pCodecCtx->color_range        = dec_ctx->color_range;
                pCodecCtx->color_primaries    = dec_ctx->color_primaries;
                pCodecCtx->color_trc          = dec_ctx->color_trc;
                pCodecCtx->width              = dec_ctx->width;
                pCodecCtx->height             = dec_ctx->height;
                pCodecCtx->has_b_frames       = dec_ctx->has_b_frames;
            }
            out_stream->avg_frame_rate = in_stream->avg_frame_rate;
            out_stream->r_frame_rate = in_stream->r_frame_rate;
            //ret = avcodec_parameters_from_context(out_stream->codecpar,pCodecCtx);

            uint64_t extra_size;
            extra_size = (uint64_t)dec_ctx->extradata_size + AV_INPUT_BUFFER_PADDING_SIZE;
            if(dec_ctx->extradata_size){
                pCodecCtx->extradata  = (uint8_t *) av_mallocz(extra_size);
                if (!pCodecCtx->extradata) {
                    J4A_ALOGD("pCodecCtx->extradata is null");
                }
                memcpy(pCodecCtx->extradata, dec_ctx->extradata, dec_ctx->extradata_size);
            }
            J4A_ALOGD("extradata_size=%d extradata=%#x codec_type=%d",pCodecCtx->extradata_size,pCodecCtx->extradata,pCodecCtx->codec_type);
            for(int i=0;i<out_stream->codecpar->extradata_size;i++){
                J4A_ALOGD("extradata[%d]=%#x",i,out_stream->codecpar->extradata[i]);
            }
            J4A_ALOGD("extradata_size=%d extradata=%#x codec_type=%d",dec_ctx->extradata_size,dec_ctx->extradata,dec_ctx->codec_type);
            out_stream->codecpar->codec_tag = 0;
            if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
                pCodecCtx->flags |= CODEC_FLAG_GLOBAL_HEADER;

            break;
        }
    }

    J4A_ALOGD("==========Output Information 0==========\n");
    av_dump_format(ofmt_ctx, 0, out_filename, 1);

    J4A_ALOGD("yuhaoo test pix_fmt=%d,w=%d,h=%d",ofmt_ctx->streams[videoindex_v]->codec->pix_fmt,ofmt_ctx->streams[videoindex_v]->codec->width,ofmt_ctx->streams[videoindex_v]->codec->height);
    J4A_ALOGD("======================================\n");
    //Open output file
    if (!(ofmt->flags & AVFMT_NOFILE)) {
        if (avio_open(&ofmt_ctx->pb, out_filename, AVIO_FLAG_WRITE) < 0) {
            J4A_ALOGD( "Could not open output file '%s'", out_filename);
            goto end;
        }
    }
    //Write file header
    if (avformat_write_header(ofmt_ctx, NULL) < 0) {
        J4A_ALOGD( "Error occurred when opening output file\n");
        goto end;
    }

    J4A_ALOGD("==========Output Information 1==========\n");
    av_dump_format(ofmt_ctx, 0, out_filename, 1);

    J4A_ALOGD("yuhaoo test pix_fmt=%d,w=%d,h=%d",ofmt_ctx->streams[videoindex_v]->codec->pix_fmt,ofmt_ctx->streams[videoindex_v]->codec->width,ofmt_ctx->streams[videoindex_v]->codec->height);
    J4A_ALOGD("======================================\n");
    //FIX
#if USE_H264BSF
     h264bsfc =  av_bitstream_filter_init("h264_mp4toannexb");
#endif
#if USE_AACBSF
    aacbsfc =  av_bitstream_filter_init("aac_adtstoasc");
#endif

    while (1) {
        AVFormatContext *ifmt_ctx;
        int stream_index=0;
        AVStream *in_stream, *out_stream;

        //Get an AVPacket
        if(av_compare_ts(cur_pts_v,ifmt_ctx_v->streams[videoindex_v]->time_base,cur_pts_a,ifmt_ctx_a->streams[audioindex_a]->time_base) <= 0){
            ifmt_ctx=ifmt_ctx_v;
            stream_index=videoindex_out;

            if(av_read_frame(ifmt_ctx, &pkt) >= 0){
                do{
                    in_stream  = ifmt_ctx->streams[pkt.stream_index];
                    out_stream = ofmt_ctx->streams[stream_index];

                    if(pkt.stream_index==videoindex_v){
                        //FIX：No PTS (Example: Raw H.264)
                        //Simple Write PTS
                        if(pkt.pts==AV_NOPTS_VALUE){
                            //Write PTS
                            AVRational time_base1=in_stream->time_base;
                            AVRational time_base2=out_stream->time_base;
                            //Duration between 2 frames (us)
                        //    J4A_ALOGD("time_base1.num=%d,time_base1.den=%d",time_base1.num,time_base1.den);
                        //    J4A_ALOGD("time_base2.num=%d,time_base2.den=%d",time_base2.num,time_base2.den);
                        //    J4A_ALOGD("r_frame_rate.num=%d,r_frame_rate.den=%d",in_stream->r_frame_rate.num,in_stream->r_frame_rate.den);
                            int64_t calc_duration=(double)AV_TIME_BASE/av_q2d(in_stream->r_frame_rate);
                            //Parameters
                            pkt.pts=(double)(frame_index*calc_duration)/(double)(av_q2d(time_base1)*AV_TIME_BASE);
                            pkt.dts=pkt.pts;
                            pkt.duration=(double)calc_duration/(double)(av_q2d(time_base1)*AV_TIME_BASE);
                            frame_index++;
                          //  J4A_ALOGD("Packet video . size:%5d\tpts:%lld\n",pkt.size,pkt.pts);
                        }

                        cur_pts_v=pkt.pts;
                        break;
                    }
                }while(av_read_frame(ifmt_ctx, &pkt) >= 0);
            }else{
                break;
            }
        }else{
            ifmt_ctx=ifmt_ctx_a;
            stream_index=audioindex_out;
            if(av_read_frame(ifmt_ctx, &pkt) >= 0){
                do{
                    in_stream  = ifmt_ctx->streams[pkt.stream_index];
                    out_stream = ofmt_ctx->streams[stream_index];

                    if(pkt.stream_index==audioindex_a){

                        //FIX：No PTS
                        //Simple Write PTS
                        if(pkt.pts==AV_NOPTS_VALUE){
                            //Write PTS
                            AVRational time_base1=in_stream->time_base;
                            //Duration between 2 frames (us)
                            int64_t calc_duration=(double)AV_TIME_BASE/av_q2d(in_stream->r_frame_rate);
                            //Parameters
                            pkt.pts=(double)(frame_index*calc_duration)/(double)(av_q2d(time_base1)*AV_TIME_BASE);
                            pkt.dts=pkt.pts;
                            pkt.duration=(double)calc_duration/(double)(av_q2d(time_base1)*AV_TIME_BASE);
                            frame_index++;


                        }
                        cur_pts_a=pkt.pts;
                      //  J4A_ALOGD("Write 1 Packet audio in_stream->codecpar->frame_size=%d pkt size=%d r_frame_rate.num=%d  r_frame_rate.den=%d pkt.pts=%lld",
                      //            in_stream->codecpar->frame_size,pkt.size,in_stream->r_frame_rate.num,in_stream->r_frame_rate.den,pkt.pts);
                        break;
                    }
                }while(av_read_frame(ifmt_ctx, &pkt) >= 0);
            }else{
                break;
            }

        }

        //FIX:Bitstream Filter
#if USE_H264BSF
        av_bitstream_filter_filter(h264bsfc, in_stream->codec, NULL, &pkt.data, &pkt.size, pkt.data, pkt.size, 0);
#endif


#if USE_AACBSF
        av_bitstream_filter_filter(aacbsfc, out_stream->codec, NULL, &pkt.data, &pkt.size, pkt.data, pkt.size, 0);
#endif


        //Convert PTS/DTS
        pkt.pts = av_rescale_q_rnd(pkt.pts, in_stream->time_base, out_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
        pkt.dts = av_rescale_q_rnd(pkt.dts, in_stream->time_base, out_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
        pkt.duration = av_rescale_q(pkt.duration, in_stream->time_base, out_stream->time_base);
        pkt.pos = -1;
        pkt.stream_index=stream_index;

        if( pkt.stream_index == audioindex_out){
//            J4A_ALOGD("Write 1 Packet in_stream->time_base.num=%d in_stream->time_base.den=%d",in_stream->time_base.num,in_stream->time_base.den);
//            J4A_ALOGD("Write 1 Packet out_stream->time_base.num=%d out_stream->time_base.den=%d",out_stream->time_base.num,out_stream->time_base.den);
//            J4A_ALOGD("Write 1 Packet stream_index=%d size:%5d\tpts:%lld\n",stream_index,pkt.size,pkt.pts);
        }

        //Write
        if (av_interleaved_write_frame(ofmt_ctx, &pkt) < 0) {
            J4A_ALOGD( "Error muxing packet\n");
            break;
        }
        av_free_packet(&pkt);

    }
    //Write file trailer
    av_write_trailer(ofmt_ctx);

#if USE_H264BSF
    av_bitstream_filter_close(h264bsfc);
#endif
#if USE_AACBSF
    av_bitstream_filter_close(aacbsfc);
#endif

    end:
    avformat_close_input(&ifmt_ctx_v);
    avformat_close_input(&ifmt_ctx_a);
    /* close output */
    if (ofmt_ctx && !(ofmt->flags & AVFMT_NOFILE))
        avio_close(ofmt_ctx->pb);
    avformat_free_context(ofmt_ctx);
    if (ret < 0 && ret != AVERROR_EOF) {
        J4A_ALOGD( "Error occurred.\n");
        return -1;
    }

    J4A_ALOGD( "muxer finish.\n");
    return 0;
}
