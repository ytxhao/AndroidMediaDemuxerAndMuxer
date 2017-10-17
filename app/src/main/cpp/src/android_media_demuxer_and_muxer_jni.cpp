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


typedef struct InputStream {
    int file_index;
    AVStream *st;
    int discard;             /* true if stream data should be discarded */
    int user_set_discard;
    int decoding_needed;     /* non zero if the packets must be decoded in 'raw_fifo', see DECODING_FOR_* */
#define DECODING_FOR_OST    1
#define DECODING_FOR_FILTER 2

    AVCodecContext *dec_ctx;
    AVCodec *dec;
    AVFrame *decoded_frame;
    AVFrame *filter_frame; /* a ref of decoded_frame, to be sent to filters */

    int64_t       start;     /* time when read started */
    /* predicted dts of the next packet read for this stream or (when there are
     * several frames in a packet) of the next frame in current packet (in AV_TIME_BASE units) */
    int64_t       next_dts;
    int64_t       dts;       ///< dts of the last packet read for this stream (in AV_TIME_BASE units)

    int64_t       next_pts;  ///< synthetic pts for the next decode frame (in AV_TIME_BASE units)
    int64_t       pts;       ///< current pts of the decoded frame  (in AV_TIME_BASE units)
    int           wrap_correction_done;

    int64_t filter_in_rescale_delta_last;

    int64_t min_pts; /* pts with the smallest value in a current stream */
    int64_t max_pts; /* pts with the higher value in a current stream */

    // when forcing constant input framerate through -r,
    // this contains the pts that will be given to the next decoded frame
    int64_t cfr_next_pts;

    int64_t nb_samples; /* number of samples in the last decoded audio frame before looping */

    double ts_scale;
    int saw_first_ts;
    AVDictionary *decoder_opts;
    AVRational framerate;               /* framerate forced with -r */
    int top_field_first;
    int guess_layout_max;

    int autorotate;

    int fix_sub_duration;
    struct { /* previous decoded subtitle and related variables */
        int got_output;
        int ret;
        AVSubtitle subtitle;
    } prev_sub;

    struct sub2video {
        int64_t last_pts;
        int64_t end_pts;
        AVFrame *frame;
        int w, h;
    } sub2video;

    int dr1;

    /* decoded data from this stream goes into all those filters
     * currently video and audio only */

    int        nb_filters;

    int reinit_filters;

    /* hwaccel options */

    char  *hwaccel_device;
    enum AVPixelFormat hwaccel_output_format;

    /* hwaccel context */

    void  *hwaccel_ctx;
    void (*hwaccel_uninit)(AVCodecContext *s);
    int  (*hwaccel_get_buffer)(AVCodecContext *s, AVFrame *frame, int flags);
    int  (*hwaccel_retrieve_data)(AVCodecContext *s, AVFrame *frame);
    enum AVPixelFormat hwaccel_pix_fmt;
    enum AVPixelFormat hwaccel_retrieved_pix_fmt;
    AVBufferRef *hw_frames_ctx;

    /* stats */
    // combined size of all the packets read
    uint64_t data_size;
    /* number of packets successfully read for this stream */
    uint64_t nb_packets;
    // number of frames/samples retrieved from the decoder
    uint64_t frames_decoded;
    uint64_t samples_decoded;

    int64_t *dts_buffer;
    int nb_dts_buffer;

    int got_output;
} InputStream;


typedef struct OutputStream {
    int file_index;          /* file index */
    int index;               /* stream index in the output file */
    int source_index;        /* InputStream index */
    AVStream *st;            /* stream in the output file */
    int encoding_needed;     /* true if encoding needed for this stream */
    int frame_number;
    /* input pts and corresponding output pts
       for A/V sync */
    struct InputStream *sync_ist; /* input stream to sync against */
    int64_t sync_opts;       /* output frame counter, could be changed to some true timestamp */ // FIXME look at frame_number
    /* pts of the first frame encoded for this stream, used for limiting
     * recording time */
    int64_t first_pts;
    /* dts of the last packet sent to the muxer */
    int64_t last_mux_dts;
    // the timebase of the packets sent to the muxer
    AVRational mux_timebase;

    int                    nb_bitstream_filters;
    uint8_t                  *bsf_extradata_updated;
    AVBSFContext            **bsf_ctx;

    AVCodecContext *enc_ctx;
    AVCodecParameters *ref_par; /* associated input codec parameters with encoders options applied */
    AVCodec *enc;
    int64_t max_frames;
    AVFrame *filtered_frame;
    AVFrame *last_frame;
    int last_dropped;
    int last_nb0_frames[3];

    void  *hwaccel_ctx;

    /* video only */
    AVRational frame_rate;
    int is_cfr;
    int force_fps;
    int top_field_first;
    int rotate_overridden;
    double rotate_override_value;

    AVRational frame_aspect_ratio;

    /* forced key frames */
    int64_t *forced_kf_pts;
    int forced_kf_count;
    int forced_kf_index;
    char *forced_keyframes;

    /* audio only */
    int *audio_channels_map;             /* list of the channels id to pick from the source stream */
    int audio_channels_mapped;           /* number of channels in audio_channels_map */

    char *logfile_prefix;
    FILE *logfile;


    char *avfilter;
    char *filters;         ///< filtergraph associated to the -filter option
    char *filters_script;  ///< filtergraph script associated to the -filter_script option

    AVDictionary *encoder_opts;
    AVDictionary *sws_dict;
    AVDictionary *swr_opts;
    AVDictionary *resample_opts;
    char *apad;

    int unavailable;                     /* true if the steram is unavailable (possibly temporarily) */
    int stream_copy;

    // init_output_stream() has been called for this stream
    // The encoder and the bitstream filters have been initialized and the stream
    // parameters are set in the AVStream.
    int initialized;

    int inputs_done;

    const char *attachment_filename;
    int copy_initial_nonkeyframes;
    int copy_prior_start;
    char *disposition;

    int keep_pix_fmt;

    AVCodecParserContext *parser;
    AVCodecContext       *parser_avctx;

    /* stats */
    // combined size of all the packets written
    uint64_t data_size;
    // number of packets send to the muxer
    uint64_t packets_written;
    // number of frames/samples sent to the encoder
    uint64_t frames_encoded;
    uint64_t samples_encoded;

    /* packet quality factor */
    int quality;

    int max_muxing_queue_size;


    /* packet picture type */
    int pict_type;

    /* frame encode sum of squared error values */
    int64_t error[4];
} OutputStream;



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
InputStream *input_streams = NULL;
OutputStream *output_streams = NULL;
int check_stream_specifier(AVFormatContext *s, AVStream *st, const char *spec)
{
    int ret = avformat_match_stream_specifier(s, st, spec);
    if (ret < 0)
        av_log(s, AV_LOG_ERROR, "Invalid stream specifier: %s.\n", spec);
    return ret;
}

//#define MATCH_PER_STREAM_OPT(name, type, outvar, fmtctx, st)\
//{\
//    int i, ret;\
//    for (i = 0; i < o->nb_ ## name; i++) {\
//        char *spec = o->name[i].specifier;\
//        if ((ret = check_stream_specifier(fmtctx, st, spec)) > 0)\
//            outvar = o->name[i].u.type;\
//        else if (ret < 0)\
//            exit_program(1);\
//    }\
//}

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

    int scan_all_pmts_set = 0;
    AVDictionary *format_opts;
    AVDictionary **opts;
    AVFormatContext *ic;
    av_register_all();
    av_log_set_callback(log_callback);
    //Input
    J4A_ALOGD("avcodec_configuration=%s",avcodec_configuration());
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
//    if ((ret = avformat_open_input(&ifmt_ctx_a, in_filename_a, 0, 0)) < 0) {
//        J4A_ALOGD( "Could not open input file ");
//        goto end;
//    }
//    if ((ret = avformat_find_stream_info(ifmt_ctx_a, 0)) < 0) {
//        J4A_ALOGD( "Failed to retrieve input stream information");
//        goto end;
//    }
    J4A_ALOGD("===========Input Information==========\n");
    av_dump_format(ifmt_ctx_v, 0, in_filename_v, 0);
   // av_dump_format(ifmt_ctx_a, 0, in_filename_a, 0);
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
        AVCodecContext *dest;
        const AVCodecContext *src;
        if(ifmt_ctx_v->streams[i]->codecpar->codec_type==AVMEDIA_TYPE_VIDEO){
            AVStream *in_stream = ifmt_ctx_v->streams[i];

            AVCodec *pCodecV = avcodec_find_decoder(AV_CODEC_ID_H264);
            AVCodecContext *dec_ctx = avcodec_alloc_context3(pCodecV);
            AVStream *out_stream = avformat_new_stream(ofmt_ctx, pCodecV);
           // avcodec_copy_context(dec_ctx, in_stream->codec);

            dest = dec_ctx;
            src = in_stream->codec;
#define alloc_and_copy_or_fail(type, obj, size, pad) \
    if (src->obj && size > 0) { \
        dest->obj = (type)av_malloc(size + pad); \
        if (!dest->obj) \
            J4A_ALOGD("alloc_and_copy_or_fail fail"); \
        memcpy(dest->obj, src->obj, size); \
        if (pad) \
            memset(((uint8_t *) dest->obj) + size, 0, pad); \
    }
            alloc_and_copy_or_fail(uint8_t*,extradata,src->extradata_size,AV_INPUT_BUFFER_PADDING_SIZE);

            dest->extradata_size  = src->extradata_size;
          //  alloc_and_copy_or_fail(uint16_t*,intra_matrix, 64 * sizeof(int16_t), 0);
          //  alloc_and_copy_or_fail(uint16_t*,inter_matrix, 64 * sizeof(int16_t), 0);
            //  alloc_and_copy_or_fail(int,rc_override,  src->rc_override_count * sizeof(*src->rc_override), 0);
           // alloc_and_copy_or_fail(uint8_t*,subtitle_header, src->subtitle_header_size, 1);


            if (avcodec_parameters_from_context(out_stream->codecpar,dec_ctx) < 0) {
                J4A_ALOGD( "Failed to audio copy context from input to output stream codec context\n");
            }

            AVCodecParameters *par = out_stream->codecpar;
            par->width = in_stream->codecpar->width;
            par->height = in_stream->codecpar->height;
            dec_ctx->time_base = (AVRational){ 1, 25 };
            out_stream->r_frame_rate = (AVRational){ 25, 1 };
            videoindex_v=i;

            videoindex_out=out_stream->index;


        }
    }
    scan_all_pmts_set = 0;
    ic = avformat_alloc_context();
    ic->flags |= AVFMT_FLAG_NONBLOCK;
    ifmt_ctx_a = ic;
//    av_dict_set(&format_opts, "scan_all_pmts", "1", AV_DICT_DONT_OVERWRITE);
//    scan_all_pmts_set = 1;
//    if (scan_all_pmts_set)
//        av_dict_set(&format_opts, "scan_all_pmts", NULL, AV_DICT_MATCH_CASE);

    if ((ret = avformat_open_input(&ic, in_filename_a, 0, 0)) < 0) {
        J4A_ALOGD( "Could not open input file ");
        goto end;
    }


    opts = NULL;


    if ((ret = avformat_find_stream_info(ic, opts)) < 0) {
        J4A_ALOGD( "Failed to retrieve input stream information");
        goto end;
    }


    for (i = 0; i < ic->nb_streams; i++) {
        audioindex_a =0;
        AVStream *st = ic->streams[i];
        AVCodecParameters *par = st->codecpar;
        InputStream *ist = (InputStream *) av_mallocz(sizeof(*ist));
        char *framerate = NULL, *hwaccel = NULL, *hwaccel_device = NULL;
        char *hwaccel_output_format = NULL;
        char *codec_tag = NULL;
        char *next;
        char *discard_str = NULL;
        const AVClass *cc = avcodec_get_class();
       // const AVOption *discard_opt = av_opt_find(&cc, "skip_frame", NULL, 0, 0);
        input_streams = ist;
        ist->st = st;
        ist->file_index = 1;
        ist->discard = 1;
        st->discard  = AVDISCARD_ALL;
        ist->nb_samples = 0;
        ist->min_pts = INT64_MAX;
        ist->max_pts = INT64_MIN;

        ist->ts_scale = 1.0;
        ist->dec = avcodec_find_decoder(st->codecpar->codec_id);
        ist->decoder_opts = NULL;
        ist->reinit_filters = -1;
        ist->user_set_discard = AVDISCARD_NONE;
        ist->filter_in_rescale_delta_last = AV_NOPTS_VALUE;
        ist->dec_ctx = avcodec_alloc_context3(ist->dec);
        avcodec_parameters_to_context(ist->dec_ctx, par);
        ist->guess_layout_max = INT_MAX; //2
        ist->dec_ctx->channel_layout = av_get_default_channel_layout(ist->dec_ctx->channels);
        //MATCH_PER_STREAM_OPT(guess_layout_max, i, ist->guess_layout_max, ic, st);
        avcodec_parameters_from_context(par, ist->dec_ctx);
    }
//oc == ofmt_ctx
    {
        AVStream *st = avformat_new_stream(ofmt_ctx, NULL);
        OutputStream *ost;
        ost = (OutputStream *) av_mallocz(sizeof(*ost));
        output_streams = ost;
        audioindex_out = st->index;
        ost->file_index = 0;
        ost->index = 0;
        ost->st = st;
        st->codecpar->codec_type = AVMEDIA_TYPE_AUDIO;
        ost->enc_ctx = avcodec_alloc_context3(NULL);
        ost->ref_par = avcodec_parameters_alloc();
        ost->max_frames = INT64_MAX;
        ost->copy_prior_start = -1;
        const AVBitStreamFilter *filter;
        filter = av_bsf_get_by_name("aac_adtstoasc");
        ost->bsf_ctx = (AVBSFContext **) av_realloc_array(ost->bsf_ctx,
                                                          ost->nb_bitstream_filters + 1,
                                                          sizeof(*ost->bsf_ctx));
        av_bsf_alloc(filter, &ost->bsf_ctx[ost->nb_bitstream_filters]);
        ost->nb_bitstream_filters++;
        ost->bsf_extradata_updated = (uint8_t *) av_mallocz_array(ost->nb_bitstream_filters,
                                                                  sizeof(*ost->bsf_extradata_updated));

        ost->max_muxing_queue_size = 128;

        if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
            ost->enc_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

        av_dict_set(&ofmt_ctx->metadata, "creation_time", NULL, 0);
    }

    {
        AVBSFContext *ctx;
        OutputStream *ost = output_streams;
        InputStream *ist = input_streams;
        AVCodecParameters *par_dst = ost->st->codecpar;
        AVCodecParameters *par_src = ost->ref_par;
        avcodec_parameters_to_context(ost->enc_ctx, ist->st->codecpar);
        avcodec_parameters_from_context(par_src, ost->enc_ctx);
        avcodec_parameters_copy(par_dst, par_src);
        par_dst->codec_tag = 0;
        ost->frame_rate = ist->framerate;;
        ost->st->avg_frame_rate = ost->frame_rate;
        // copy timebase while removing common factors
        if (ost->st->time_base.num <= 0 || ost->st->time_base.den <= 0)
            ost->st->time_base = av_add_q(av_stream_get_codec_timebase(ost->st), (AVRational){0, 1});

        if (ost->st->duration <= 0 && ist->st->duration > 0)
            ost->st->duration = av_rescale_q(ist->st->duration, ist->st->time_base, ost->st->time_base);

        // copy disposition
        ost->st->disposition = ist->st->disposition;
        ost->parser = av_parser_init(par_dst->codec_id);
        ost->parser_avctx = avcodec_alloc_context3(NULL);
        ost->mux_timebase = ist->st->time_base;
        avcodec_parameters_to_context(ost->parser_avctx, ost->st->codecpar);
        ctx = ost->bsf_ctx[0];
        avcodec_parameters_copy(ctx->par_in, ost->st->codecpar);
        ctx->time_base_in = ost->st->time_base;
        av_bsf_init(ctx);
        avcodec_parameters_copy(ost->st->codecpar, ctx->par_out);
        ost->st->time_base = ctx->time_base_out;
    }

    /*
    for (i = 0; i < ifmt_ctx_a->nb_streams; i++) {
        //Create output AVStream according to input AVStream
        if(ifmt_ctx_a->streams[i]->codecpar->codec_type==AVMEDIA_TYPE_AUDIO){
            AVStream *in_stream = ifmt_ctx_a->streams[i];
            AVStream *out_stream = avformat_new_stream(ofmt_ctx, in_stream->codec->codec);
            //AVCodecContext *dec_ctx = avcodec_alloc_context3(avcodec_find_decoder(in_stream->codec->codec_id));
            avcodec_copy_context(out_stream->codec, in_stream->codec);
            audioindex_a=i;

            audioindex_out=out_stream->index;
            out_stream->codecpar->sample_rate = 1600;
            out_stream->time_base =(AVRational){ 1, 1600 };

            pCodecCtx->codec_type = dec_ctx->codec_type;
            pCodecCtx->codec_id = dec_ctx->codec_id;
            pCodecCtx->codec_tag = dec_ctx->codec_tag;
            pCodecCtx->bit_rate       = dec_ctx->bit_rate;
            pCodecCtx->rc_max_rate    = dec_ctx->rc_max_rate;
            pCodecCtx->rc_buffer_size = dec_ctx->rc_buffer_size;
            pCodecCtx->field_order    = dec_ctx->field_order;
            pCodecCtx->extradata_size= dec_ctx->extradata_size;
            pCodecCtx->bits_per_coded_sample  = dec_ctx->bits_per_coded_sample;
          //  pCodecCtx->time_base = in_stream->time_base;

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
                //st->codecpar->frame_size = 1024;

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
            AVCodecParameters *par = in_stream->codecpar;
            par->sample_rate = dec_ctx->sample_rate;
            out_stream->time_base = (AVRational){ 1, dec_ctx->sample_rate };
            out_stream->codecpar->frame_size = 1024;
            J4A_ALOGD("dec_ctx->sample_rate = %d",dec_ctx->sample_rate);
            J4A_ALOGD("pCodecCtx->sample_rate = %d",dec_ctx->sample_rate);
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

            av_dict_copy(&out_stream->metadata, in_stream->metadata, AV_DICT_DONT_OVERWRITE);

            out_stream->codecpar->codec_tag = 0;
            if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
                out_stream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;

            break;
        }
    }
*/
//    for(int i=0;i<ofmt_ctx->nb_streams;i++){
//        AVStream *st = ofmt_ctx->streams[i];
//        AVCodecParameters *par = st->codecpar;
//        if(par->codec_type == AVMEDIA_TYPE_VIDEO){
//            par->width = width;
//            par->height = height;
//            st->r_frame_rate = (AVRational){ 20, 1 };
//        }else if(par->codec_type == AVMEDIA_TYPE_AUDIO){
//
//        }
//    }
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
   // aacbsfc =  av_bitstream_filter_init("aac_adtstoasc");
#endif

    while (1) {
        AVFormatContext *ifmt_ctx;
        int stream_index=0;
        AVStream *in_stream, *out_stream;

        //Get an AVPacket
        if(false && av_compare_ts(cur_pts_v,ifmt_ctx_v->streams[videoindex_v]->time_base,cur_pts_a,ifmt_ctx_a->streams[audioindex_a]->time_base) <= 0){
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
      //  av_bitstream_filter_filter(aacbsfc, out_stream->codec, NULL, &pkt.data, &pkt.size, pkt.data, pkt.size, 0);
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
