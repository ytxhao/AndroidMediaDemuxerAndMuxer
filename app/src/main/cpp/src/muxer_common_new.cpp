//
// Created by Administrator on 2017/10/18.
//
#include "muxer_common_new.h"


template<typename T>
T *grow_array(T *array, int elem_size, int *size, int new_size)
{

    if (new_size >= INT_MAX / elem_size) {
        J4A_ALOGD("Array too big.\n");
        return NULL;
    }
    if (*size < new_size) {
        uint8_t *tmp = (uint8_t *) av_realloc_array((void *)array, new_size, elem_size);
        if (!tmp) {
            J4A_ALOGD("Could not alloc buffer.\n");
            return NULL;
        }
        memset(tmp + *size*elem_size, 0, (new_size-*size) * elem_size);
        *size = new_size;
        return (T *)tmp;
    }
    return array;
}

template<typename I, typename O>
O type_transform(I in,O out){
    return (O)in;
};

void print_error(const char *filename, int err)
{
    char errbuf[128];
    const char *errbuf_ptr = errbuf;

    if (av_strerror(err, errbuf, sizeof(errbuf)) < 0)
        errbuf_ptr = strerror(AVUNERROR(err));
    J4A_ALOGE( "%s: %s\n", filename, errbuf_ptr);
}

int guess_input_channel_layout(InputStream *ist)
{
    AVCodecContext *dec = ist->dec_ctx;

    if (!dec->channel_layout) {
        char layout_name[256];

        if (dec->channels > ist->guess_layout_max)
            return 0;
        dec->channel_layout = av_get_default_channel_layout(dec->channels);
        if (!dec->channel_layout)
            return 0;
        av_get_channel_layout_string(layout_name, sizeof(layout_name),
                                     dec->channels, dec->channel_layout);
        J4A_ALOGD("Guessed Channel Layout for Input Stream "
                          "#%d.%d : %s\n", ist->file_index, ist->st->index, layout_name);
    }
    return 1;
}

int add_input_streams(FFContext *mFFContext,AVFormatContext *ic)
{
    int i, ret;

    for (i = 0; i < ic->nb_streams; i++) {
        AVStream *st = ic->streams[i];
        AVCodecParameters *par = st->codecpar;
        InputStream *ist = (InputStream *) av_mallocz(sizeof(*ist));
        if (!ist)
            return -1;

        GROW_ARRAY(mFFContext->input_streams, mFFContext->nb_input_streams);
        mFFContext->input_streams[mFFContext->nb_input_streams - 1] = ist;

        ist->st = st;
        ist->file_index = mFFContext->nb_input_files;
        ist->discard = 1;
        st->discard  = AVDISCARD_ALL;
        ist->nb_samples = 0;
        ist->min_pts = INT64_MAX;
        ist->max_pts = INT64_MIN;

        ist->ts_scale = 1.0;

        ist->autorotate = 1;


        ist->dec = avcodec_find_decoder(st->codecpar->codec_id);
        ist->decoder_opts = NULL;

        ist->reinit_filters = -1;

        ist->user_set_discard = AVDISCARD_NONE;

        ist->filter_in_rescale_delta_last = AV_NOPTS_VALUE;

        ist->dec_ctx = avcodec_alloc_context3(ist->dec);


        ret = avcodec_parameters_to_context(ist->dec_ctx, par);
        if (ret < 0) {
            J4A_ALOGE("Error initializing the decoder context.\n");
            return -1;
        }

        switch (par->codec_type) {
            case AVMEDIA_TYPE_VIDEO:
                if(!ist->dec)
                    ist->dec = avcodec_find_decoder(par->codec_id);
#if FF_API_EMU_EDGE
                if (av_codec_get_lowres(st->codec)) {
                    av_codec_set_lowres(ist->dec_ctx, av_codec_get_lowres(st->codec));
                    ist->dec_ctx->width  = st->codec->width;
                    ist->dec_ctx->height = st->codec->height;
                    ist->dec_ctx->coded_width  = st->codec->coded_width;
                    ist->dec_ctx->coded_height = st->codec->coded_height;
                    ist->dec_ctx->flags |= CODEC_FLAG_EMU_EDGE;
                }
#endif
                ist->dec_ctx->framerate = st->avg_frame_rate;

                if (av_parse_video_rate(&ist->framerate,
                                        "20") < 0) {
                    return -1;
                }

                ist->top_field_first = -1;


                break;
            case AVMEDIA_TYPE_AUDIO:
                ist->guess_layout_max = INT_MAX;
                guess_input_channel_layout(ist);
                break;
            case AVMEDIA_TYPE_DATA:
            case AVMEDIA_TYPE_SUBTITLE: {
                break;
            }
            case AVMEDIA_TYPE_ATTACHMENT:
            case AVMEDIA_TYPE_UNKNOWN:
                break;
            default:
                abort();
        }

        ret = avcodec_parameters_from_context(par, ist->dec_ctx);
        if (ret < 0) {
            J4A_ALOGE( "Error initializing the decoder context.\n");
            return -1;
        }
    }
    return 0;
}
int open_input_file(FFContext *mFFContext,const char *filename){

    InputFile *f;
    AVFormatContext *ic;
    //AVDictionary *format_opts = NULL;
    int scan_all_pmts_set = 0;
    int err, i, ret;
    ic = avformat_alloc_context();
    if (!ic) {
        print_error(filename, AVERROR(ENOMEM));
        return -1;
    }

//    if (!av_dict_get(format_opts, "scan_all_pmts", NULL, AV_DICT_MATCH_CASE)) {
//        av_dict_set(&format_opts, "scan_all_pmts", "1", AV_DICT_DONT_OVERWRITE);
//        scan_all_pmts_set = 1;
//    }

    err = avformat_open_input(&ic, filename, NULL, NULL);
    if (err < 0) {
        print_error(filename, err);
        if (err == AVERROR_PROTOCOL_NOT_FOUND)
            J4A_ALOGE( "Did you mean file:%s?\n", filename);
        return -1;
    }

    ret = avformat_find_stream_info(ic, NULL);
    if (ret < 0) {
        J4A_ALOGE("%s: could not find codec parameters\n", filename);
        if (ic->nb_streams == 0) {
            avformat_close_input(&ic);
            return -1;
        }
    }

    add_input_streams(mFFContext,ic);


    av_dump_format(ic, mFFContext->nb_input_files, filename, 0);
    GROW_ARRAY(mFFContext->input_files, mFFContext->nb_input_files);

    f = (InputFile *) av_mallocz(sizeof(*f));
    if (!f)
        return -1;
    mFFContext->input_files[mFFContext->nb_input_files - 1] = f;


    f->ctx        = ic;
    f->ist_index  =  mFFContext->nb_input_files - ic->nb_streams;
    f->start_time = AV_NOPTS_VALUE;
    f->recording_time = INT64_MAX;
    f->input_ts_offset = 0;
    f->ts_offset  = 0;
    f->nb_streams = ic->nb_streams;
    f->rate_emu   = 0;
    f->accurate_seek = 1;
    f->loop = 0;
    f->duration = 0;
    f->time_base = (AVRational){ 1, 1 };

    return 0;
}


void init_opts(FFContext *mFFContext)
{
    av_dict_set(&mFFContext->sws_dict, "flags", "bicubic", 0);
}


OutputStream *new_output_stream(FFContext *mFFContext,AVFormatContext *oc, enum AVMediaType type, int source_index)
{
    OutputStream *ost;
    AVStream *st = avformat_new_stream(oc, NULL);
    int idx      = oc->nb_streams - 1, ret = 0;
    //const char *bsfs = NULL;
    uint8_t *bsfs = NULL,*time_base = NULL;
    uint8_t *codec_tag = NULL;
    char *next;
    double qscale = -1;
    int i;

    if (!st) {
        J4A_ALOGE("Could not alloc stream.\n");
        return NULL;
    }


    GROW_ARRAY(mFFContext->output_streams, mFFContext->nb_output_streams);
    if (!(ost = (OutputStream *) av_mallocz(sizeof(*ost)))){
        return NULL;
    }

    mFFContext->output_streams[mFFContext->nb_output_streams - 1] = ost;

    ost->file_index = mFFContext->nb_output_files - 1;
    ost->index      = idx;
    ost->st         = st;
    st->codecpar->codec_type = type;
    ost->stream_copy = 1;
    ost->encoding_needed = !ost->stream_copy;
    ost->enc = NULL;


    ost->enc_ctx = avcodec_alloc_context3(ost->enc);
    if (!ost->enc_ctx) {
        J4A_ALOGE("Error allocating the encoding context.\n");
        return NULL;
    }
    ost->enc_ctx->codec_type = type;

    ost->ref_par = avcodec_parameters_alloc();
    if (!ost->ref_par) {
        J4A_ALOGE( "Error allocating the encoding parameters.\n");
        return NULL;
    }

    ost->encoder_opts = NULL;

    ost->max_frames = INT64_MAX;

    ost->copy_prior_start = -1;

    if(type == AVMEDIA_TYPE_AUDIO){
        bsfs = (uint8_t *) calloc(strlen("aac_adtstoasc") + 1, sizeof(uint8_t));
        sprintf((char *) bsfs, "aac_adtstoasc");
    }

    while (bsfs && *bsfs) {
        const AVBitStreamFilter *filter;
        char *bsf, *bsf_options_str, *bsf_name;

        bsf = av_get_token((const char **) &bsfs, ",");
        if (!bsf)
            return NULL;
        bsf_name = av_strtok(bsf, "=", &bsf_options_str);
        if (!bsf_name)
            return NULL;

        filter = av_bsf_get_by_name(bsf_name);
        if (!filter) {
            J4A_ALOGE( "Unknown bitstream filter %s\n", bsf_name);
            return NULL;
        }

        ost->bsf_ctx = (AVBSFContext **) av_realloc_array(ost->bsf_ctx,
                                                          ost->nb_bitstream_filters + 1,
                                                          sizeof(*ost->bsf_ctx));
        if (!ost->bsf_ctx)
            return NULL;

        ret = av_bsf_alloc(filter, &ost->bsf_ctx[ost->nb_bitstream_filters]);
        if (ret < 0) {
            J4A_ALOGE( "Error allocating a bitstream filter context\n");
            return NULL;
        }

        ost->nb_bitstream_filters++;

        if (bsf_options_str && filter->priv_class) {
            const AVOption *opt = av_opt_next(ost->bsf_ctx[ost->nb_bitstream_filters-1]->priv_data, NULL);
            const char * shorthand[2] = {NULL};

            if (opt)
                shorthand[0] = opt->name;

            ret = av_opt_set_from_string(ost->bsf_ctx[ost->nb_bitstream_filters-1]->priv_data, bsf_options_str, shorthand, "=", ":");
            if (ret < 0) {
                J4A_ALOGE("Error parsing options for bitstream filter %s\n", bsf_name);
                return NULL;
            }
        }
        av_freep(&bsf);

        if (*bsfs)
            bsfs++;
    }


    if (ost->nb_bitstream_filters) {
        ost->bsf_extradata_updated = (uint8_t *) av_mallocz_array(ost->nb_bitstream_filters, sizeof(*ost->bsf_extradata_updated));
        if (!ost->bsf_extradata_updated) {
            av_log(NULL, AV_LOG_FATAL, "Bitstream filter memory allocation failed\n");
            return NULL;
        }
    }

    ost->disposition = av_strdup(ost->disposition);
    ost->max_muxing_queue_size = 32;
    ost->max_muxing_queue_size *= sizeof(AVPacket);

    if (oc->oformat->flags & AVFMT_GLOBALHEADER)
        ost->enc_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    av_dict_copy(&ost->sws_dict, mFFContext->sws_dict, 0);

    if (ost->enc && av_get_exact_bits_per_sample(ost->enc->id) == 24)
        av_dict_set(&ost->swr_opts, "output_sample_bits", "24", 0);

    av_dict_copy(&ost->resample_opts, NULL, 0);

    ost->source_index = source_index;
    if (source_index >= 0) {
        ost->sync_ist = mFFContext->input_streams[source_index];
        mFFContext->input_streams[source_index]->discard = 0;
        mFFContext->input_streams[source_index]->st->discard = (AVDiscard) mFFContext->input_streams[source_index]->user_set_discard;
    }
    ost->last_mux_dts = AV_NOPTS_VALUE;

    ost->muxing_queue = av_fifo_alloc(8 * sizeof(AVPacket));
    if (!ost->muxing_queue)
        return NULL;

    return ost;
}

OutputStream *new_video_stream(FFContext *mFFContext,AVFormatContext *oc, int source_index)
{
    AVStream *st;
    OutputStream *ost;
    AVCodecContext *video_enc;
    int ret = 0;
    char *frame_rate = NULL, *frame_aspect_ratio = NULL;

    ost = new_output_stream(mFFContext,oc, AVMEDIA_TYPE_VIDEO, source_index);
    st  = ost->st;
    video_enc = ost->enc_ctx;


    ost->copy_initial_nonkeyframes = 0;


    return ost;
}


OutputStream *new_audio_stream(FFContext *mFFContext,AVFormatContext *oc, int source_index)
{
    int n;
    int ret = 0;
    AVStream *st;
    OutputStream *ost;
    AVCodecContext *audio_enc;

    ost = new_output_stream(mFFContext,oc, AVMEDIA_TYPE_AUDIO, source_index);
    st  = ost->st;

    audio_enc = ost->enc_ctx;
    audio_enc->codec_type = AVMEDIA_TYPE_AUDIO;


    return ost;
}

int open_output_file(FFContext *mFFContext,const char *filename){

    AVFormatContext *oc;
    int i, j, err;
    AVOutputFormat *file_oformat;
    OutputFile *of;
    OutputStream *ost;
    InputStream  *ist;
    AVDictionary *unused_opts = NULL;
    AVDictionaryEntry *e = NULL;
    int format_flags = 0;
    int idx = 0;


    GROW_ARRAY(mFFContext->output_files, mFFContext->nb_output_files);
    of = (OutputFile *) av_mallocz(sizeof(*of));
    if (!of){
        return -1;
    }

    mFFContext->output_files[mFFContext->nb_output_files - 1] = of;

    of->ost_index      =  mFFContext->nb_output_streams;
    of->recording_time = INT64_MAX;
    of->start_time     = AV_NOPTS_VALUE;
    of->limit_filesize = UINT64_MAX;
    of->shortest       = 0;


    err = avformat_alloc_output_context2(&oc, NULL, NULL, filename);
    if (!oc) {
        print_error(filename, err);
        return -1;
    }

    of->ctx = oc;

    file_oformat = oc->oformat;


    if (av_guess_codec(oc->oformat, NULL, filename, NULL, AVMEDIA_TYPE_VIDEO) != AV_CODEC_ID_NONE) {
        int area = 0, idx = -1;
        int qcr = avformat_query_codec(oc->oformat, oc->oformat->video_codec, 0);
        for (i = 0; i < mFFContext->nb_input_streams; i++) {
            int new_area;
            ist = mFFContext->input_streams[i];
            new_area = ist->st->codecpar->width * ist->st->codecpar->height + 100000000*!!ist->st->codec_info_nb_frames;
            if((qcr!=MKTAG('A', 'P', 'I', 'C')) && (ist->st->disposition & AV_DISPOSITION_ATTACHED_PIC))
                new_area = 1;
            if (ist->st->codecpar->codec_type == AVMEDIA_TYPE_VIDEO &&
                new_area > area) {
                if((qcr==MKTAG('A', 'P', 'I', 'C')) && !(ist->st->disposition & AV_DISPOSITION_ATTACHED_PIC))
                    continue;
                area = new_area;
                idx = i;
            }
        }
        if (idx >= 0) {
            new_video_stream(mFFContext,oc, idx);
            J4A_ALOGD("ost test video idx=%d nb_output_streams=%d",idx,mFFContext->nb_output_streams);
        }
    }


    if (av_guess_codec(oc->oformat, NULL, filename, NULL, AVMEDIA_TYPE_AUDIO) != AV_CODEC_ID_NONE) {
        int best_score = 0, idx = -1;
        for (i = 0; i < mFFContext->nb_input_streams; i++) {
            int score;
            ist = mFFContext->input_streams[i];
            score = ist->st->codecpar->channels + 100000000*!!ist->st->codec_info_nb_frames;
            if (ist->st->codecpar->codec_type == AVMEDIA_TYPE_AUDIO &&
                score > best_score) {
                best_score = score;
                idx = i;
            }
        }
        if (idx >= 0) {
            new_audio_stream(mFFContext,oc, idx);
            J4A_ALOGD("ost test audio idx=%d nb_output_streams=%d",idx,mFFContext->nb_output_streams);
        }
    }

    if (!oc->nb_streams && !(oc->oformat->flags & AVFMT_NOSTREAMS)) {
        av_dump_format(oc, mFFContext->nb_output_files - 1, oc->filename, 1);
        J4A_ALOGE("Output file #%d does not contain any stream\n", mFFContext->nb_output_files - 1);
        return -1;
    }

    if (oc->oformat->flags & AVFMT_NEEDNUMBER) {
        if (!av_filename_number_test(oc->filename)) {
            print_error(oc->filename, AVERROR(EINVAL));
            return -1;
        }
    }


    if (!(oc->oformat->flags & AVFMT_NOFILE)) {
        if ((err = avio_open(&oc->pb, filename, AVIO_FLAG_WRITE)) < 0) {
            print_error(filename, err);
            return -1;
        }
    }


    oc->max_delay = (int)(0.7 * AV_TIME_BASE);

    av_dict_set(&oc->metadata, "creation_time", NULL, 0);


    for (i = of->ost_index; i < mFFContext->nb_output_streams; i++) {
        InputStream *ist;
        J4A_ALOGD("output_streams[i]->source_index=%d", mFFContext->output_streams[i]->source_index);
        if (mFFContext->output_streams[i]->source_index < 0){
            continue;
        }

        ist = mFFContext->input_streams[mFFContext->output_streams[i]->source_index];
        J4A_ALOGD("ist->dec->type=%d", ist->dec->type);
        av_dict_copy(&mFFContext->output_streams[i]->st->metadata, ist->st->metadata, AV_DICT_DONT_OVERWRITE);
    }

    return 0;
}


void free_FFContext(FFContext *mFFContext){

    if(mFFContext){
        int i=0,j=0;

        for(i=0;i<mFFContext->nb_input_streams;i++){
            J4A_ALOGD("free_FFContext mFFContext->nb_input_streams=%d",mFFContext->nb_input_streams);
            InputStream *ist = mFFContext->input_streams[i];
            av_frame_free(&ist->decoded_frame);
            av_frame_free(&ist->filter_frame);
            av_dict_free(&ist->decoder_opts);
            avsubtitle_free(&ist->prev_sub.subtitle);
            av_frame_free(&ist->sub2video.frame);
            av_freep(&ist->dts_buffer);

            avcodec_free_context(&ist->dec_ctx);

            av_freep(&mFFContext->input_streams[i]);

        }

        for(i=0;i<mFFContext->nb_input_files;i++){
            J4A_ALOGD("free_FFContext mFFContext->input_files=%#x nb_input_files=%d",mFFContext->input_files,mFFContext->nb_input_files);
            if(mFFContext->input_files[i]->ctx){
                avformat_close_input(&mFFContext->input_files[i]->ctx);
            }
            av_freep(&mFFContext->input_files[i]);
        }

// free output memory
        for(i=0;i<mFFContext->nb_output_files;i++){
            J4A_ALOGD("free_FFContext mFFContext->output_files=%#x nb_output_files=%d",mFFContext->output_files,mFFContext->nb_output_files);
            OutputFile *of = mFFContext->output_files[i];
            AVFormatContext *s;
            if (!of){ continue; }
            s = of->ctx;
            if (s && s->oformat && !(s->oformat->flags & AVFMT_NOFILE)){
                avio_closep(&s->pb);
            }

            avformat_free_context(s);
            av_freep(&mFFContext->output_files[i]);
        }


        for (i = 0; i < mFFContext->nb_output_streams; i++) {
            J4A_ALOGD("free_FFContext mFFContext->nb_output_streams=%d",mFFContext->nb_output_streams);
            OutputStream *ost = mFFContext->output_streams[i];

            if (!ost){ continue; }

            for (j = 0; j < ost->nb_bitstream_filters; j++){
                av_bsf_free(&ost->bsf_ctx[j]);
            }

            av_freep(&ost->bsf_ctx);
            av_freep(&ost->bsf_extradata_updated);

            av_frame_free(&ost->filtered_frame);
            av_frame_free(&ost->last_frame);
            av_dict_free(&ost->encoder_opts);

            av_parser_close(ost->parser);
            avcodec_free_context(&ost->parser_avctx);

            av_freep(&ost->forced_keyframes);
            av_expr_free(ost->forced_keyframes_pexpr);
            av_freep(&ost->avfilter);
            av_freep(&ost->logfile_prefix);

            av_freep(&ost->audio_channels_map);
            ost->audio_channels_mapped = 0;

            av_dict_free(&ost->sws_dict);

            avcodec_free_context(&ost->enc_ctx);
            avcodec_parameters_free(&ost->ref_par);

            if (ost->muxing_queue) {
                while (av_fifo_size(ost->muxing_queue)) {
                    AVPacket pkt;
                    av_fifo_generic_read(ost->muxing_queue, &pkt, sizeof(pkt), NULL);
                    av_packet_unref(&pkt);
                }
                av_fifo_freep(&ost->muxing_queue);
            }

            av_freep(&mFFContext->output_streams[i]);
        }

        av_dict_free(&mFFContext->sws_dict);

        J4A_ALOGD("test 001 mFFContext->input_streams=%#x",mFFContext->input_streams);
        av_freep(&mFFContext->input_streams);
        J4A_ALOGD("test 002 mFFContext->input_streams=%#x",mFFContext->input_streams);
        av_freep(&mFFContext->input_files);
        av_freep(&mFFContext->output_streams);
        av_freep(&mFFContext->output_files);
        J4A_ALOGD("test 003 mFFContext=%#x",mFFContext);
        av_freep(&mFFContext);
        J4A_ALOGD("test 004 mFFContext=%#x",mFFContext);

    }

}

void* ffmpeg_parse(void* arg)
{
    FFContext *mFFContext = (FFContext *) malloc(sizeof(FFContext));
    memset(mFFContext,0, sizeof(FFContext));
    hls2mp4_context_t* context= (hls2mp4_context_t*)arg;
    J4A_ALOGD("avcodec_configuration=%s",avcodec_configuration());
    avcodec_register_all();
    avfilter_register_all();
    av_register_all();
    avformat_network_init();

    init_opts(mFFContext);
    J4A_ALOGD("context->in_filename_a=%s",context->in_filename_a);
    J4A_ALOGD("context->in_filename_v=%s",context->in_filename_v);
    if(context->in_filename_a){
        open_input_file(mFFContext,context->in_filename_a);
    }

    if(context->in_filename_v){
        open_input_file(mFFContext,context->in_filename_v);
    }

    open_output_file(mFFContext,context->out_filename);

    free_FFContext(mFFContext);

    if(context->in_filename_v) {
        free(context->in_filename_v);
        context->in_filename_v = NULL;
    }
    if(context->in_filename_a) {
        free(context->in_filename_a);
        context->in_filename_a = NULL;
    }
    if(context->out_filename) {
        free(context->out_filename);
        context->out_filename = NULL;
    }

    return NULL;
}
