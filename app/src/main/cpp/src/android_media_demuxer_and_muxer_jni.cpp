#include <jni.h>
#include <string>
#include <android/log.h>
#include <ican_ytx_com_MediaUtils.h>

#ifdef __cplusplus
extern "C"
{
#endif
#include <libavformat/avformat.h>
#include <libavutil/fifo.h>

typedef struct InputStream {
    AVStream *st;
    AVCodecContext *dec_ctx;
    AVCodec *dec;
    int64_t       next_dts;
    int64_t       dts;

    int64_t       next_pts;
    int64_t       pts;
    AVRational framerate;               /* framerate forced with -r */
} InputStream;


typedef struct OutputStream {
    AVStream *st;
    AVCodecContext *enc_ctx;
    AVCodecParameters *ref_par; /* associated input codec parameters with encoders options applied */
    AVCodec *enc;
    uint8_t *bsf_extradata_updated;
    AVBSFContext *bsf_ctx;
    int max_muxing_queue_size;
    AVFifoBuffer *muxing_queue;
    /* video only */
    AVRational frame_rate;
    AVCodecParserContext *parser;
    AVCodecContext       *parser_avctx;
    AVRational mux_timebase;
    int finished;
} OutputStream;

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


JNIEXPORT jint JNICALL Java_ican_ytx_com_andoridmediademuxerandmuxer_MediaUtils_demuxer
        (JNIEnv *env, jobject obj, jstring inputFile,jstring outputVideoFile,jstring outputAudioFile)
{

    return 0;
}

static void output_packet_filter(AVStream *out_stream,uint8_t *bsf_extradata_updated, AVBSFContext *bsf_ctx, AVPacket *pkt){
    int idx;
    int ret;
    ret = av_bsf_send_packet(bsf_ctx, pkt);
    if (ret < 0)
        goto finish;
    idx = 1;
    while (idx) {

        ret = av_bsf_receive_packet(bsf_ctx, pkt);
        if (ret == AVERROR(EAGAIN)) {
            ret = 0;
            idx--;
            continue;
        } else if (ret < 0){
            goto finish;
        }

        if (!(bsf_extradata_updated[idx - 1] & 1)) {
            ret = avcodec_parameters_copy(out_stream->codecpar, bsf_ctx->par_out);
            if (ret < 0)
                goto finish;
            bsf_extradata_updated[idx - 1] |= 1;
        }


    }

    finish:
    if (ret < 0 && ret != AVERROR_EOF) {
        J4A_ALOGE("Error applying bitstream filters to an output packet for stream");

    }
}

void log_callback(void* ptr, int level, const char* fmt,va_list vl)
{
    J4A_VLOGD(fmt,vl);
}

JNIEXPORT jint JNICALL Java_ican_ytx_com_andoridmediademuxerandmuxer_MediaUtils_muxer
        (JNIEnv *env, jobject obj, jstring inputVideoFile, jstring inputAudioFile, jstring outputMediaFile)
{
    uint8_t error[128];
    InputStream ist;
    OutputStream ost;
    AVOutputFormat *ofmt = NULL;

    AVFormatContext *ifmt_ctx_v = NULL, *ifmt_ctx_a = NULL,*ofmt_ctx = NULL;
    AVPacket pkt;
    int ret, i;
    int videoindex_v=-1,videoindex_out=-1;
    int audioindex_a=-1,audioindex_out=-1;
    int frame_index=0;
    int64_t cur_pts_v=0,cur_pts_a=0;

    const char *in_filename_v = env->GetStringUTFChars(inputVideoFile, NULL);

    const char *in_filename_a = env->GetStringUTFChars(inputAudioFile, NULL);

    const char *out_filename = env->GetStringUTFChars(outputMediaFile, NULL);//Output file URL
    memset(&ist,0, sizeof(InputStream));
    memset(&ost,0, sizeof(OutputStream));
    J4A_ALOGD("avcodec_configuration=%s",avcodec_configuration());
    av_register_all();
    av_log_set_callback(log_callback);
    //Input
    J4A_ALOGD( "in_filename_v=%s in_filename_a=%s",in_filename_v,in_filename_a);

    if ((ret = avformat_open_input(&ifmt_ctx_v, in_filename_v, 0, 0)) < 0) {
        av_strerror(ret, (char *) error, sizeof(error));
        J4A_ALOGD( "Could not open input file ret=%d error=%s",ret,error);
        goto end;
    }

    if ((ret = avformat_find_stream_info(ifmt_ctx_v, 0)) < 0) {
        J4A_ALOGD( "Failed to retrieve input stream information");
        goto end;
    }

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

    for (i = 0; i < ifmt_ctx_a->nb_streams; i++) {
        //Create output AVStream according to input AVStream
        if(ifmt_ctx_a->streams[i]->codecpar->codec_type==AVMEDIA_TYPE_AUDIO){
            AVStream *st = ifmt_ctx_a->streams[i];
            AVCodecParameters *par = st->codecpar;
            ist.st = st;
            ist.dec = avcodec_find_decoder(st->codecpar->codec_id);
            ist.dec_ctx = avcodec_alloc_context3(ist.dec);
            avcodec_parameters_to_context(ist.dec_ctx, par);
            ist.dec_ctx->channel_layout = (uint64_t) av_get_default_channel_layout(ist.dec_ctx->channels);

            ret = avcodec_parameters_from_context(par, ist.dec_ctx);
            if (ret < 0) {
                J4A_ALOGE( "Error initializing the decoder context.\n");
                return -1;
            }

        }
    }

    {
        const AVBitStreamFilter *filter;
        AVStream *st = avformat_new_stream(ofmt_ctx, NULL);
        ost.st  = st;
        st->codecpar->codec_type = AVMEDIA_TYPE_AUDIO;
        ost.enc = NULL;
        ost.enc_ctx = avcodec_alloc_context3(ost.enc);
        ost.enc_ctx->codec_type = AVMEDIA_TYPE_AUDIO;
        ost.ref_par = avcodec_parameters_alloc();
        filter = av_bsf_get_by_name("aac_adtstoasc");
        ret = av_bsf_alloc(filter, &ost.bsf_ctx);
        J4A_ALOGD("sizeof(*ost.bsf_extradata_updated)=%d",sizeof(*ost.bsf_extradata_updated));
        ost.bsf_extradata_updated = (uint8_t *) av_mallocz_array(1, sizeof(*ost.bsf_extradata_updated));
        ost.max_muxing_queue_size = 32;
        ost.max_muxing_queue_size *= sizeof(AVPacket);

        if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
            ost.enc_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

        ost.muxing_queue = av_fifo_alloc(8 * sizeof(AVPacket));
    }

    ist.next_pts = AV_NOPTS_VALUE;
    ist.next_dts = AV_NOPTS_VALUE;


    {
        //streamcopy
        AVCodecParameters *par_dst = ost.st->codecpar;
        AVCodecParameters *par_src = ost.ref_par;
        AVRational sar;
        ret = avcodec_parameters_to_context(ost.enc_ctx, ist.st->codecpar);
        avcodec_parameters_from_context(par_src, ost.enc_ctx);
        ret = avcodec_parameters_copy(par_dst, par_src);
        par_dst->codec_tag = 0;
        if (!ost.frame_rate.num){
            ost.frame_rate = ist.framerate;
        }
        ost.st->avg_frame_rate = ost.frame_rate;
        // copy timebase while removing common factors
        if (ost.st->time_base.num <= 0 || ost.st->time_base.den <= 0)
            ost.st->time_base = av_add_q(av_stream_get_codec_timebase(ost.st), (AVRational){0, 1});

        if (ost.st->duration <= 0 && ist.st->duration > 0)
            ost.st->duration = av_rescale_q(ist.st->duration, ist.st->time_base, ost.st->time_base);

        // copy disposition
        ost.st->disposition = ist.st->disposition;
        ost.parser = av_parser_init(par_dst->codec_id);
        ost.parser_avctx = avcodec_alloc_context3(NULL);
        if((par_dst->block_align == 1 || par_dst->block_align == 1152 || par_dst->block_align == 576) && par_dst->codec_id == AV_CODEC_ID_MP3)
            par_dst->block_align= 0;
        if(par_dst->codec_id == AV_CODEC_ID_AC3)
            par_dst->block_align= 0;

        ost.mux_timebase = ist.st->time_base;
        ret = avcodec_parameters_to_context(ost.parser_avctx, ost.st->codecpar);
    }

    {
        AVBSFContext *ctx;
        ctx = ost.bsf_ctx;
        ret = avcodec_parameters_copy(ctx->par_in, ost.st->codecpar);
        ctx->time_base_in = ost.st->time_base;
        ret = av_bsf_init(ctx);
        ret = avcodec_parameters_copy(ost.st->codecpar, ctx->par_out);
        ost.st->time_base = ctx->time_base_out;
    }

    //Open output file
    if (!(ofmt->flags & AVFMT_NOFILE)) {
        if (avio_open(&ofmt_ctx->pb, out_filename, AVIO_FLAG_WRITE) < 0) {
            J4A_ALOGD( "Could not open output file '%s'", out_filename);
            goto end;
        }
    }

    J4A_ALOGD("==========Output Information ==========\n");
    av_dump_format(ofmt_ctx, 0, out_filename, 1);
    J4A_ALOGD("======================================\n");
    {
        if (!av_fifo_size(ost.muxing_queue))
            ost.mux_timebase = ost.st->time_base;
    }
    //Write file header
    if (avformat_write_header(ofmt_ctx, NULL) < 0) {
        J4A_ALOGD( "Error occurred when opening output file\n");
        goto end;
    }

    while (true) {
        int64_t duration;
        AVRational time_base_tmp;
        AVPacket pkt;
        int64_t pkt_dts;
        time_base_tmp.den=1;
        time_base_tmp.num=1;
        if(ost.finished || (ofmt_ctx->pb && avio_tell(ofmt_ctx->pb) >=UINT64_MAX)){
            break;
        }

        ret = av_read_frame(ifmt_ctx_a, &pkt);
        if (ret == AVERROR(EAGAIN)) {
            continue;
        }
        if(ret < 0){
            if (ret != AVERROR_EOF) {
                 break;
            }
            break;
        }
        output_packet_filter(ost.st,ost.bsf_extradata_updated,ost.bsf_ctx,&pkt);

        pkt.pts = av_rescale_q_rnd(pkt.pts, ist.st->time_base, ost.st->time_base, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
        pkt.dts = av_rescale_q_rnd(pkt.dts, ist.st->time_base, ost.st->time_base, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
        pkt.duration = av_rescale_q(pkt.duration, ist.st->time_base, ost.st->time_base);
        pkt.pos = -1;
        if ((ret = av_interleaved_write_frame(ofmt_ctx, &pkt)) < 0) {
            J4A_ALOGE( "Error muxing packet\n");
            break;
        }
//        if (pkt.dts != AV_NOPTS_VALUE)
//            pkt.dts += av_rescale_q(0, AV_TIME_BASE_Q, ist.st->time_base);
//        if (pkt.pts != AV_NOPTS_VALUE)
//            pkt.pts += av_rescale_q(0, AV_TIME_BASE_Q, ist.st->time_base);
//        pkt_dts = av_rescale_q_rnd(pkt.dts, ist.st->time_base, AV_TIME_BASE_Q, (enum AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
//
//        duration = av_rescale_q(0, time_base_tmp, ist.st->time_base);
//        if (pkt.pts != AV_NOPTS_VALUE) {
//            pkt.pts += duration;
////            ist.max_pts = FFMAX(pkt.pts, ist.max_pts);
////            ist.min_pts = FFMIN(pkt.pts, ist.min_pts);
//        }
//
//        if (pkt.dts != AV_NOPTS_VALUE)
//            pkt.dts += duration;
//
//        pkt_dts = av_rescale_q_rnd(pkt.dts, ist.st->time_base, AV_TIME_BASE_Q, (enum AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
    }
    //Write file trailer
    av_write_trailer(ofmt_ctx);

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
