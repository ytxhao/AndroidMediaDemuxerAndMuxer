//
// Created by Administrator on 2017/10/18.
//

#ifndef ANDROIDMEDIADEMUXERANDMUXER_MUXER_COMMON_H
#define ANDROIDMEDIADEMUXERANDMUXER_MUXER_COMMON_H
#ifdef __cplusplus
extern "C"
{
#endif
#include <libavformat/avformat.h>
#include <libavfilter/avfilter.h>
#include <libavutil/avutil.h>
#include <libavutil/fifo.h>
#include <libavutil/eval.h>
#include <libavutil/avstring.h>
#include "libavutil/opt.h"
#include <libavutil/time.h>
#include <libavutil/intreadwrite.h>
void log_callback(void* ptr, int level, const char* fmt,va_list vl);

#ifdef __cplusplus
};
#endif
#include <android/log.h>
#include "stdatomic.h"
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


typedef struct SpecifierOpt {
    char *specifier;    /**< stream/chapter/program/... specifier */
    union {
        uint8_t *str;
        int        i;
        int64_t  i64;
        float      f;
        double   dbl;
    } u;
} SpecifierOpt;

typedef struct OptionDef {
    const char *name;
    int flags;
#define HAS_ARG    0x0001
#define OPT_BOOL   0x0002
#define OPT_EXPERT 0x0004
#define OPT_STRING 0x0008
#define OPT_VIDEO  0x0010
#define OPT_AUDIO  0x0020
#define OPT_INT    0x0080
#define OPT_FLOAT  0x0100
#define OPT_SUBTITLE 0x0200
#define OPT_INT64  0x0400
#define OPT_EXIT   0x0800
#define OPT_DATA   0x1000
#define OPT_PERFILE  0x2000     /* the option is per-file (currently ffmpeg-only).
                                   implied by OPT_OFFSET or OPT_SPEC */
#define OPT_OFFSET 0x4000       /* option is specified as an offset in a passed optctx */
#define OPT_SPEC   0x8000       /* option is to be stored in an array of SpecifierOpt.
                                   Implies OPT_OFFSET. Next element after the offset is
                                   an int containing element count in the array. */
#define OPT_TIME  0x10000
#define OPT_DOUBLE 0x20000
#define OPT_INPUT  0x40000
#define OPT_OUTPUT 0x80000
    union {
        void *dst_ptr;
        int (*func_arg)(void *, const char *, const char *);
        size_t off;
    } u;
    const char *help;
    const char *argname;
} OptionDef;

typedef struct OptionGroupDef {
    /**< group name */
    const char *name;
    /**
     * Option to be used as group separator. Can be NULL for groups which
     * are terminated by a non-option argument (e.g. ffmpeg output files)
     */
    const char *sep;
    /**
     * Option flags that must be set on each option that is
     * applied to this group
     */
    int flags;
} OptionGroupDef;

typedef struct Option {
    const OptionDef  *opt;
    const char       *key;
    const char       *val;
} Option;

typedef struct OptionGroup {
    const OptionGroupDef *group_def;
    const char *arg;

    Option *opts;
    int  nb_opts;

    AVDictionary *codec_opts;
    AVDictionary *format_opts;
    AVDictionary *resample_opts;
    AVDictionary *sws_dict;
    AVDictionary *swr_opts;
} OptionGroup;

typedef struct OptionGroupList {
    const OptionGroupDef *group_def;

    OptionGroup *groups;
    int       nb_groups;
} OptionGroupList;

typedef struct OptionParseContext {
    OptionGroup global_opts;

    OptionGroupList *groups;
    int           nb_groups;

    /* parsing state */
    OptionGroup cur_group;
} OptionParseContext;

/* select an input stream for an output stream */
typedef struct StreamMap {
    int disabled;           /* 1 is this mapping is disabled by a negative map */
    int file_index;
    int stream_index;
    int sync_file_index;
    int sync_stream_index;
    char *linklabel;       /* name of an output link, for mapping lavfi outputs */
} StreamMap;

typedef struct {
    int  file_idx,  stream_idx,  channel_idx; // input
    int ofile_idx, ostream_idx;               // output
} AudioChannelMap;

typedef struct OptionsContext {
    OptionGroup *g;

    /* input/output options */
    int64_t start_time;
    int64_t start_time_eof;
    int seek_timestamp;
    const char *format;

    SpecifierOpt *codec_names;
    int        nb_codec_names;
    SpecifierOpt *audio_channels;
    int        nb_audio_channels;
    SpecifierOpt *audio_sample_rate;
    int        nb_audio_sample_rate;
    SpecifierOpt *frame_rates;
    int        nb_frame_rates;
    SpecifierOpt *frame_sizes;
    int        nb_frame_sizes;
    SpecifierOpt *frame_pix_fmts;
    int        nb_frame_pix_fmts;

    /* input options */
    int64_t input_ts_offset;
    int loop;
    int rate_emu;
    int accurate_seek;
    int thread_queue_size;

    SpecifierOpt *ts_scale;
    int        nb_ts_scale;
    SpecifierOpt *dump_attachment;
    int        nb_dump_attachment;
    SpecifierOpt *hwaccels;
    int        nb_hwaccels;
    SpecifierOpt *hwaccel_devices;
    int        nb_hwaccel_devices;
    SpecifierOpt *hwaccel_output_formats;
    int        nb_hwaccel_output_formats;
    SpecifierOpt *autorotate;
    int        nb_autorotate;

    /* output options */
    StreamMap *stream_maps;
    int     nb_stream_maps;
    AudioChannelMap *audio_channel_maps; /* one info entry per -map_channel */
    int           nb_audio_channel_maps; /* number of (valid) -map_channel settings */
    int metadata_global_manual;
    int metadata_streams_manual;
    int metadata_chapters_manual;
    const char **attachments;
    int       nb_attachments;

    int chapters_input_file;

    int64_t recording_time;
    int64_t stop_time;
    uint64_t limit_filesize;
    float mux_preload;
    float mux_max_delay;
    int shortest;

    int video_disable;
    int audio_disable;
    int subtitle_disable;
    int data_disable;

    /* indexed by output file stream index */
    int   *streamid_map;
    int nb_streamid_map;

    SpecifierOpt *metadata;
    int        nb_metadata;
    SpecifierOpt *max_frames;
    int        nb_max_frames;
    SpecifierOpt *bitstream_filters;
    int        nb_bitstream_filters;
    SpecifierOpt *codec_tags;
    int        nb_codec_tags;
    SpecifierOpt *sample_fmts;
    int        nb_sample_fmts;
    SpecifierOpt *qscale;
    int        nb_qscale;
    SpecifierOpt *forced_key_frames;
    int        nb_forced_key_frames;
    SpecifierOpt *force_fps;
    int        nb_force_fps;
    SpecifierOpt *frame_aspect_ratios;
    int        nb_frame_aspect_ratios;
    SpecifierOpt *rc_overrides;
    int        nb_rc_overrides;
    SpecifierOpt *intra_matrices;
    int        nb_intra_matrices;
    SpecifierOpt *inter_matrices;
    int        nb_inter_matrices;
    SpecifierOpt *chroma_intra_matrices;
    int        nb_chroma_intra_matrices;
    SpecifierOpt *top_field_first;
    int        nb_top_field_first;
    SpecifierOpt *metadata_map;
    int        nb_metadata_map;
    SpecifierOpt *presets;
    int        nb_presets;
    SpecifierOpt *copy_initial_nonkeyframes;
    int        nb_copy_initial_nonkeyframes;
    SpecifierOpt *copy_prior_start;
    int        nb_copy_prior_start;
    SpecifierOpt *filters;
    int        nb_filters;
    SpecifierOpt *filter_scripts;
    int        nb_filter_scripts;
    SpecifierOpt *reinit_filters;
    int        nb_reinit_filters;
    SpecifierOpt *fix_sub_duration;
    int        nb_fix_sub_duration;
    SpecifierOpt *canvas_sizes;
    int        nb_canvas_sizes;
    SpecifierOpt *pass;
    int        nb_pass;
    SpecifierOpt *passlogfiles;
    int        nb_passlogfiles;
    SpecifierOpt *max_muxing_queue_size;
    int        nb_max_muxing_queue_size;
    SpecifierOpt *guess_layout_max;
    int        nb_guess_layout_max;
    SpecifierOpt *apad;
    int        nb_apad;
    SpecifierOpt *discard;
    int        nb_discard;
    SpecifierOpt *disposition;
    int        nb_disposition;
    SpecifierOpt *program;
    int        nb_program;
    SpecifierOpt *time_bases;
    int        nb_time_bases;
} OptionsContext;




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
        AVFifoBuffer *sub_queue;    ///< queue of AVSubtitle* before filter init
        AVFrame *frame;
        int w, h;
    } sub2video;

    int dr1;

    /* decoded data from this stream goes into all those filters
     * currently video and audio only */
    //InputFilter **filters;
    int        nb_filters;

    int reinit_filters;

    /* hwaccel options */
//    enum HWAccelID hwaccel_id;
//    char  *hwaccel_device;
//    enum AVPixelFormat hwaccel_output_format;

    /* hwaccel context */
//    enum HWAccelID active_hwaccel_id;
//    void  *hwaccel_ctx;
//    void (*hwaccel_uninit)(AVCodecContext *s);
//    int  (*hwaccel_get_buffer)(AVCodecContext *s, AVFrame *frame, int flags);
//    int  (*hwaccel_retrieve_data)(AVCodecContext *s, AVFrame *frame);
//    enum AVPixelFormat hwaccel_pix_fmt;
//    enum AVPixelFormat hwaccel_retrieved_pix_fmt;
//    AVBufferRef *hw_frames_ctx;

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


typedef struct InputFile {
    AVFormatContext *ctx;
    int eof_reached;      /* true if eof reached */
    int eagain;           /* true if last read attempt returned EAGAIN */
    int ist_index;        /* index of first stream in input_streams */
    int loop;             /* set number of times input stream should be looped */
    int64_t duration;     /* actual duration of the longest stream in a file
                             at the moment when looping happens */
    AVRational time_base; /* time base of the duration */
    int64_t input_ts_offset;

    int64_t ts_offset;
    int64_t last_ts;
    int64_t start_time;   /* user-specified start time in AV_TIME_BASE or AV_NOPTS_VALUE */
    int seek_timestamp;
    int64_t recording_time;
    int nb_streams;       /* number of stream that ffmpeg is aware of; may be different
                             from ctx.nb_streams if new streams appear during av_read_frame() */
    int nb_streams_warn;  /* number of streams that the user was warned of */
    int rate_emu;
    int accurate_seek;

} InputFile;


enum forced_keyframes_const {
    FKF_N,
    FKF_N_FORCED,
    FKF_PREV_FORCED_N,
    FKF_PREV_FORCED_T,
    FKF_T,
    FKF_NB
};

#define ABORT_ON_FLAG_EMPTY_OUTPUT (1 <<  0)

extern const char *const forced_keyframes_const_names[];

typedef enum {
    ENCODER_FINISHED = 1,
    MUXER_FINISHED = 2,
} OSTFinished ;

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
    AVExpr *forced_keyframes_pexpr;
    double forced_keyframes_expr_const_values[FKF_NB];

    /* audio only */
    int *audio_channels_map;             /* list of the channels id to pick from the source stream */
    int audio_channels_mapped;           /* number of channels in audio_channels_map */

    char *logfile_prefix;
    FILE *logfile;

    //OutputFilter *filter;
    char *avfilter;
    char *filters;         ///< filtergraph associated to the -filter option
    char *filters_script;  ///< filtergraph script associated to the -filter_script option

    AVDictionary *encoder_opts;
    AVDictionary *sws_dict;
    AVDictionary *swr_opts;
    AVDictionary *resample_opts;
    char *apad;
    int finished;        /* no more packets should be written for this stream */
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

    /* the packets are buffered here until the muxer is ready to be initialized */
    AVFifoBuffer *muxing_queue;

    /* packet picture type */
    int pict_type;

    /* frame encode sum of squared error values */
    int64_t error[4];
} OutputStream;

typedef struct OutputFile {
    AVFormatContext *ctx;
    AVDictionary *opts;
    int ost_index;       /* index of the first stream in output_streams */
    int64_t recording_time;  ///< desired length of the resulting file in microseconds == AV_TIME_BASE units
    int64_t start_time;      ///< start time in microseconds == AV_TIME_BASE units
    uint64_t limit_filesize; /* filesize limit expressed in bytes */

    int shortest;

    int header_written;
} OutputFile;

//InputStream **input_streams;
//int        nb_input_streams;
//InputFile   **input_files;
//int        nb_input_files;
//
//OutputStream **output_streams;
//int         nb_output_streams;
//OutputFile   **output_files;
//int         nb_output_files;
extern const OptionDef options[];
//int opt_audio_codec(void *optctx, const char *opt, const char *arg);
//int opt_video_codec(void *optctx, const char *opt, const char *arg);
//int opt_old2new(void *optctx, const char *opt, const char *arg);



template<typename T>
T *grow_array(T *array, int elem_size, int *size, int new_size);

template<typename I, typename O>
O type_transform(I in,O out);

#define media_type_string av_get_media_type_string

#define GROW_ARRAY(array, nb_elems)\
    array = grow_array(array, sizeof(*array), &nb_elems, nb_elems + 1)


#define MATCH_PER_STREAM_OPT(name, type, outvar, fmtctx, st, outret)\
{\
    int i, ret;\
    for (i = 0; i < o->nb_ ## name; i++) {\
        char *spec = o->name[i].specifier;\
        if ((ret = check_stream_specifier(fmtctx, st, spec)) > 0)\
            outvar = type_transform(o->name[i].u.type,outvar);\
        else if (ret < 0)\
            outret = -1;\
    }\
}

int ffmpeg_parse_options(int argc, const char *input_file_name, const char *output_file_name);

#endif //ANDROIDMEDIADEMUXERANDMUXER_MUXER_COMMON_H
