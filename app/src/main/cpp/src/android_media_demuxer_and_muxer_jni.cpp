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
#include <libavutil/parseutils.h>

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
    int initialized;

    int inputs_done;
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

int transcode_step(InputStream ist[2],OutputStream ost[2],AVFormatContext *ifmt_ctx_a,AVFormatContext *ifmt_ctx_v,AVFormatContext *ofmt_ctx){
    int64_t opts_min = INT64_MAX;
    OutputStream *ost_min = NULL;
    int ret,i,source_index;
    AVPacket pkt;
    /*
     * choose_output
     */
    for (i = 0; i < 2; i++){
        source_index = i;
        int64_t opts = ost[i].st->cur_dts == AV_NOPTS_VALUE ? INT64_MIN :
                       av_rescale_q(ost[i].st->cur_dts, ost[i].st->time_base,
                                    AV_TIME_BASE_Q);
        if (ost[i].st->cur_dts == AV_NOPTS_VALUE)
            J4A_ALOGE("cur_dts is invalid (this is harmless if it occurs once at the start per stream)\n");

        if (!ost[i].initialized && !ost[i].inputs_done){
            ost_min = &ost[i];
            break;
        }


        if (!ost->finished && opts < opts_min) {
            opts_min = opts;
            ost_min  = &ost[i];
        }
    }

    if(!ost_min){
        return 0;
    }

    if(source_index ==0){
        ret = av_read_frame(ifmt_ctx_a, &pkt);
        output_packet_filter(ost[source_index].st,ost[source_index].bsf_extradata_updated,ost[source_index].bsf_ctx,&pkt);


    }else {
        ret = av_read_frame(ifmt_ctx_v, &pkt);
    }

    if (ret == AVERROR(EAGAIN)) {
        return 0;
    }

    pkt.pts = av_rescale_q_rnd(pkt.pts, ist[source_index].st->time_base, ost[source_index].st->time_base, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
    pkt.dts = av_rescale_q_rnd(pkt.dts, ist[source_index].st->time_base, ost[source_index].st->time_base, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
    pkt.duration = av_rescale_q(pkt.duration, ist[source_index].st->time_base, ost[source_index].st->time_base);
    pkt.pos = -1;
    if ((ret = av_interleaved_write_frame(ofmt_ctx, &pkt)) < 0) {
        J4A_ALOGE( "Error muxing packet\n");
        return -1;
    }

    return 0;
}
#define IST_INDEX_AUDIO 0
#define IST_INDEX_VIDEO 1

JNIEXPORT jint JNICALL Java_ican_ytx_com_andoridmediademuxerandmuxer_MediaUtils_muxer
        (JNIEnv *env, jobject obj, jstring inputVideoFile, jstring inputAudioFile, jstring outputMediaFile)
{
    uint8_t error[128];
    InputStream ist[2];
    OutputStream ost[2];
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
            audioindex_a = i;
            AVStream *st = ifmt_ctx_a->streams[i];
            AVCodecParameters *par = st->codecpar;
            ist[IST_INDEX_AUDIO].st = st;
            ist[IST_INDEX_AUDIO].dec = avcodec_find_decoder(st->codecpar->codec_id);
            ist[IST_INDEX_AUDIO].dec_ctx = avcodec_alloc_context3(ist[IST_INDEX_AUDIO].dec);
            avcodec_parameters_to_context(ist[IST_INDEX_AUDIO].dec_ctx, par);
            ist[IST_INDEX_AUDIO].dec_ctx->channel_layout = (uint64_t) av_get_default_channel_layout(ist[IST_INDEX_AUDIO].dec_ctx->channels);

            ret = avcodec_parameters_from_context(par, ist[IST_INDEX_AUDIO].dec_ctx);
            if (ret < 0) {
                J4A_ALOGE( "Error initializing the decoder context.\n");
                return -1;
            }

        }
    }

    for(i = 0; i < ifmt_ctx_v->nb_streams; i++){
        if(ifmt_ctx_v->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO){
            videoindex_v = i;
            AVStream *st = ifmt_ctx_v->streams[i];
            AVCodecParameters *par = st->codecpar;
            ist[IST_INDEX_VIDEO].st = st;
            ist[IST_INDEX_VIDEO].dec = avcodec_find_decoder(st->codecpar->codec_id);
            ist[IST_INDEX_VIDEO].dec_ctx = avcodec_alloc_context3(ist[IST_INDEX_VIDEO].dec);
            avcodec_parameters_to_context(ist[IST_INDEX_VIDEO].dec_ctx, par);
         //   ist[IST_INDEX_VIDEO].dec_ctx->channel_layout = (uint64_t) av_get_default_channel_layout(ist[IST_INDEX_VIDEO].dec_ctx->channels);



            ist[IST_INDEX_VIDEO].dec_ctx->framerate = st->avg_frame_rate;

            if (av_parse_video_rate(&ist[IST_INDEX_VIDEO].framerate,
                                    "20") < 0) {
                return -1;
            }

            ret = avcodec_parameters_from_context(par, ist[IST_INDEX_VIDEO].dec_ctx);
            if (ret < 0) {
                J4A_ALOGE( "Error initializing the decoder context.\n");
                return -1;
            }

        }
    }

    {
        //new_audio_stream
        const AVBitStreamFilter *filter;
        AVStream *st = avformat_new_stream(ofmt_ctx, NULL);
        ost[IST_INDEX_AUDIO].st  = st;
        st->codecpar->codec_type = AVMEDIA_TYPE_AUDIO;
        ost[IST_INDEX_AUDIO].enc = NULL;
        ost[IST_INDEX_AUDIO].enc_ctx = avcodec_alloc_context3(ost[IST_INDEX_AUDIO].enc);
        ost[IST_INDEX_AUDIO].enc_ctx->codec_type = AVMEDIA_TYPE_AUDIO;
        ost[IST_INDEX_AUDIO].ref_par = avcodec_parameters_alloc();
        filter = av_bsf_get_by_name("aac_adtstoasc");
        ret = av_bsf_alloc(filter, &ost[IST_INDEX_AUDIO].bsf_ctx);
        J4A_ALOGD("sizeof(*ost.bsf_extradata_updated)=%d",sizeof(*ost[IST_INDEX_AUDIO].bsf_extradata_updated));
        ost[IST_INDEX_AUDIO].bsf_extradata_updated = (uint8_t *) av_mallocz_array(1, sizeof(*ost[IST_INDEX_AUDIO].bsf_extradata_updated));
        ost[IST_INDEX_AUDIO].max_muxing_queue_size = 32;
        ost[IST_INDEX_AUDIO].max_muxing_queue_size *= sizeof(AVPacket);

        if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
            ost[IST_INDEX_AUDIO].enc_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

        ost[IST_INDEX_AUDIO].muxing_queue = av_fifo_alloc(8 * sizeof(AVPacket));

        ist[IST_INDEX_AUDIO].next_pts = AV_NOPTS_VALUE;
        ist[IST_INDEX_AUDIO].next_dts = AV_NOPTS_VALUE;
    }


    {
        //new_video_stream
        AVStream *st = avformat_new_stream(ofmt_ctx, NULL);
        ost[IST_INDEX_VIDEO].st  = st;
        st->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
        ost[IST_INDEX_VIDEO].enc = NULL;
        ost[IST_INDEX_VIDEO].enc_ctx = avcodec_alloc_context3(ost[IST_INDEX_VIDEO].enc);
        ost[IST_INDEX_VIDEO].enc_ctx->codec_type = AVMEDIA_TYPE_VIDEO;
        ost[IST_INDEX_VIDEO].ref_par = avcodec_parameters_alloc();

        ost[IST_INDEX_VIDEO].max_muxing_queue_size = 32;
        ost[IST_INDEX_VIDEO].max_muxing_queue_size *= sizeof(AVPacket);

        if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
            ost[IST_INDEX_VIDEO].enc_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

        ost[IST_INDEX_VIDEO].muxing_queue = av_fifo_alloc(8 * sizeof(AVPacket));

        ist[IST_INDEX_VIDEO].next_pts = AV_NOPTS_VALUE;
        ist[IST_INDEX_VIDEO].next_dts = AV_NOPTS_VALUE;
    }

    {
        //streamcopy audio_stream
        AVCodecParameters *par_dst = ost[IST_INDEX_AUDIO].st->codecpar;
        AVCodecParameters *par_src = ost[IST_INDEX_AUDIO].ref_par;
        AVRational sar;
        ret = avcodec_parameters_to_context(ost[IST_INDEX_AUDIO].enc_ctx, ist[IST_INDEX_AUDIO].st->codecpar);
        avcodec_parameters_from_context(par_src, ost[IST_INDEX_AUDIO].enc_ctx);
        ret = avcodec_parameters_copy(par_dst, par_src);
        par_dst->codec_tag = 0;
        if (!ost[IST_INDEX_AUDIO].frame_rate.num){
            ost[IST_INDEX_AUDIO].frame_rate = ist[IST_INDEX_AUDIO].framerate;
        }
        ost[IST_INDEX_AUDIO].st->avg_frame_rate = ost[IST_INDEX_AUDIO].frame_rate;
        // copy timebase while removing common factors
        if (ost[IST_INDEX_AUDIO].st->time_base.num <= 0 || ost[IST_INDEX_AUDIO].st->time_base.den <= 0)
            ost[IST_INDEX_AUDIO].st->time_base = av_add_q(av_stream_get_codec_timebase(ost[IST_INDEX_AUDIO].st), (AVRational){0, 1});

        if (ost[IST_INDEX_AUDIO].st->duration <= 0 && ist[IST_INDEX_AUDIO].st->duration > 0)
            ost[IST_INDEX_AUDIO].st->duration = av_rescale_q(ist[IST_INDEX_AUDIO].st->duration, ist[IST_INDEX_AUDIO].st->time_base, ost[IST_INDEX_AUDIO].st->time_base);

        // copy disposition
        ost[IST_INDEX_AUDIO].st->disposition = ist[IST_INDEX_AUDIO].st->disposition;
        ost[IST_INDEX_AUDIO].parser = av_parser_init(par_dst->codec_id);
        ost[IST_INDEX_AUDIO].parser_avctx = avcodec_alloc_context3(NULL);
        if((par_dst->block_align == 1 || par_dst->block_align == 1152 || par_dst->block_align == 576) && par_dst->codec_id == AV_CODEC_ID_MP3)
            par_dst->block_align= 0;
        if(par_dst->codec_id == AV_CODEC_ID_AC3)
            par_dst->block_align= 0;

        ost[IST_INDEX_AUDIO].mux_timebase = ist[IST_INDEX_AUDIO].st->time_base;
        ret = avcodec_parameters_to_context(ost[IST_INDEX_AUDIO].parser_avctx, ost[IST_INDEX_AUDIO].st->codecpar);
    }

    {
        //streamcopy video_stream
        AVCodecParameters *par_dst = ost[IST_INDEX_VIDEO].st->codecpar;
        AVCodecParameters *par_src = ost[IST_INDEX_VIDEO].ref_par;
        AVRational sar;
        ret = avcodec_parameters_to_context(ost[IST_INDEX_VIDEO].enc_ctx, ist[IST_INDEX_VIDEO].st->codecpar);
        avcodec_parameters_from_context(par_src, ost[IST_INDEX_VIDEO].enc_ctx);
        ret = avcodec_parameters_copy(par_dst, par_src);
        par_dst->codec_tag = 0;
        if (!ost[IST_INDEX_VIDEO].frame_rate.num){
            ost[IST_INDEX_VIDEO].frame_rate = ist[IST_INDEX_VIDEO].framerate;
        }
        ost[IST_INDEX_VIDEO].st->avg_frame_rate = ost[IST_INDEX_VIDEO].frame_rate;
        // copy timebase while removing common factors
        if (ost[IST_INDEX_VIDEO].st->time_base.num <= 0 || ost[IST_INDEX_VIDEO].st->time_base.den <= 0)
            ost[IST_INDEX_VIDEO].st->time_base = av_add_q(av_stream_get_codec_timebase(ost[IST_INDEX_VIDEO].st), (AVRational){0, 1});

        if (ost[IST_INDEX_VIDEO].st->duration <= 0 && ist[IST_INDEX_VIDEO].st->duration > 0)
            ost[IST_INDEX_VIDEO].st->duration = av_rescale_q(ist[IST_INDEX_VIDEO].st->duration, ist[IST_INDEX_VIDEO].st->time_base, ost[IST_INDEX_VIDEO].st->time_base);

        // copy disposition
        ost[IST_INDEX_VIDEO].st->disposition = ist[IST_INDEX_VIDEO].st->disposition;
        ost[IST_INDEX_VIDEO].parser = av_parser_init(par_dst->codec_id);
        ost[IST_INDEX_VIDEO].parser_avctx = avcodec_alloc_context3(NULL);

        if (ist[IST_INDEX_VIDEO].st->sample_aspect_ratio.num)
            sar = ist[IST_INDEX_VIDEO].st->sample_aspect_ratio;
        else
            sar = par_src->sample_aspect_ratio;
        ost[IST_INDEX_VIDEO].st->sample_aspect_ratio = par_dst->sample_aspect_ratio = sar;
        ost[IST_INDEX_VIDEO].st->avg_frame_rate = ist[IST_INDEX_VIDEO].st->avg_frame_rate;
        ost[IST_INDEX_VIDEO].st->r_frame_rate = ist[IST_INDEX_VIDEO].st->r_frame_rate;
        ost[IST_INDEX_VIDEO].mux_timebase = ist[IST_INDEX_VIDEO].st->time_base;
        ret = avcodec_parameters_to_context(ost[IST_INDEX_VIDEO].parser_avctx, ost[IST_INDEX_VIDEO].st->codecpar);
    }

    {
        AVBSFContext *ctx;
        ctx = ost[IST_INDEX_AUDIO].bsf_ctx;
        ret = avcodec_parameters_copy(ctx->par_in, ost[IST_INDEX_AUDIO].st->codecpar);
        ctx->time_base_in = ost[IST_INDEX_AUDIO].st->time_base;
        ret = av_bsf_init(ctx);
        ret = avcodec_parameters_copy(ost[IST_INDEX_AUDIO].st->codecpar, ctx->par_out);
        ost[IST_INDEX_AUDIO].st->time_base = ctx->time_base_out;
    }

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

    J4A_ALOGD("==========Output Information ==========\n");
    av_dump_format(ofmt_ctx, 0, out_filename, 1);
    J4A_ALOGD("======================================\n");

    while (true) {
        int64_t duration;
        AVRational time_base_tmp;
        AVPacket pkt;
        int64_t pkt_dts;
        int source_index = 0;
        time_base_tmp.den=1;
        time_base_tmp.num=1;
//        if((ost[0].finished && ost[1].finished)|| (ofmt_ctx->pb && avio_tell(ofmt_ctx->pb) >=UINT64_MAX)){
//            break;
//        }


        if(av_compare_ts(cur_pts_v,ist[1].st->time_base,cur_pts_a,ist[0].st->time_base) <= 0){
            source_index = 1;
            if(av_read_frame(ifmt_ctx_v, &pkt) >= 0){
                if(pkt.pts==AV_NOPTS_VALUE){
                    //Write PTS
                    AVRational time_base1 = ist[source_index].st->time_base;
                    //Duration between 2 frames (us)
                    int64_t calc_duration = (double)AV_TIME_BASE/av_q2d(ist[source_index].st->r_frame_rate);
                    //Parameters
                    pkt.pts=(double)(frame_index*calc_duration)/(double)(av_q2d(time_base1)*AV_TIME_BASE);
                    pkt.dts=pkt.pts;
                    pkt.duration=(double)calc_duration/(double)(av_q2d(time_base1)*AV_TIME_BASE);
                    frame_index++;
                }
                cur_pts_v = pkt.pts;
            }else{
                break;
            }
        }else{
            source_index = 0;
            if(av_read_frame(ifmt_ctx_a, &pkt) >= 0){
                if(pkt.pts==AV_NOPTS_VALUE){
                    //Write PTS
                    AVRational time_base1 = ist[source_index].st->time_base;
                    //Duration between 2 frames (us)
                    int64_t calc_duration=(double)AV_TIME_BASE/av_q2d(ist[source_index].st->r_frame_rate);
                    //Parameters
                    pkt.pts=(double)(frame_index*calc_duration)/(double)(av_q2d(time_base1)*AV_TIME_BASE);
                    pkt.dts=pkt.pts;
                    pkt.duration=(double)calc_duration/(double)(av_q2d(time_base1)*AV_TIME_BASE);
                    frame_index++;
                }

                cur_pts_a = pkt.pts;
                output_packet_filter(ost[source_index].st,ost[source_index].bsf_extradata_updated,ost[source_index].bsf_ctx,&pkt);
            }else{
                break;
            }
        }

        pkt.pts = av_rescale_q_rnd(pkt.pts, ist[source_index].st->time_base, ost[source_index].st->time_base, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
        pkt.dts = av_rescale_q_rnd(pkt.dts, ist[source_index].st->time_base, ost[source_index].st->time_base, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
        pkt.duration = av_rescale_q(pkt.duration, ist[source_index].st->time_base, ost[source_index].st->time_base);
        pkt.pos = -1;
        pkt.stream_index = source_index;
        if ((ret = av_interleaved_write_frame(ofmt_ctx, &pkt)) < 0) {
            J4A_ALOGE( "Error muxing packet\n");
            break;
        }
        av_free_packet(&pkt);

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
