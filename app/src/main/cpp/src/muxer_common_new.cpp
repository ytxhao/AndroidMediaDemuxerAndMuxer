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

        J4A_ALOGD("bsf_name=%s",bsf_name);
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

int init_input_stream(FFContext *mFFContext,int ist_index, char *error, int error_len)
{
    int ret;
    InputStream *ist = mFFContext->input_streams[ist_index];

    ist->next_pts = AV_NOPTS_VALUE;
    ist->next_dts = AV_NOPTS_VALUE;

    return 0;
}

InputStream *get_input_stream(FFContext *mFFContext,OutputStream *ost)
{
    if (ost->source_index >= 0)
        return mFFContext->input_streams[ost->source_index];
    return NULL;
}

int init_output_stream_streamcopy(FFContext *mFFContext,OutputStream *ost)
{
    OutputFile *of = mFFContext->output_files[ost->file_index];
    InputStream *ist = get_input_stream(mFFContext,ost);
    AVCodecParameters *par_dst = ost->st->codecpar;
    AVCodecParameters *par_src = ost->ref_par;
    AVRational sar;
    int i, ret;
    uint32_t codec_tag = par_dst->codec_tag;

    //av_assert0(ist && !ost->filter);

    ret = avcodec_parameters_to_context(ost->enc_ctx, ist->st->codecpar);
    if (ret >= 0)
        ret = av_opt_set_dict(ost->enc_ctx, &ost->encoder_opts);
    if (ret < 0) {
        J4A_ALOGE("Error setting up codec context options.\n");
        return ret;
    }
    avcodec_parameters_from_context(par_src, ost->enc_ctx);

    if (!codec_tag) {
        unsigned int codec_tag_tmp;
        if (!of->ctx->oformat->codec_tag ||
            av_codec_get_id (of->ctx->oformat->codec_tag, par_src->codec_tag) == par_src->codec_id ||
            !av_codec_get_tag2(of->ctx->oformat->codec_tag, par_src->codec_id, &codec_tag_tmp))
            codec_tag = par_src->codec_tag;
    }

    ret = avcodec_parameters_copy(par_dst, par_src);
    if (ret < 0)
        return ret;

    par_dst->codec_tag = codec_tag;

    if (!ost->frame_rate.num)
        ost->frame_rate = ist->framerate;
    ost->st->avg_frame_rate = ost->frame_rate;

//    ret = avformat_transfer_internal_stream_timing_info(of->ctx->oformat, ost->st, ist->st, copy_tb);
//    if (ret < 0)
//        return ret;

    // copy timebase while removing common factors
    if (ost->st->time_base.num <= 0 || ost->st->time_base.den <= 0)
        ost->st->time_base = av_add_q(av_stream_get_codec_timebase(ost->st), (AVRational){0, 1});

    // copy estimated duration as a hint to the muxer
    if (ost->st->duration <= 0 && ist->st->duration > 0)
        ost->st->duration = av_rescale_q(ist->st->duration, ist->st->time_base, ost->st->time_base);

    // copy disposition
    ost->st->disposition = ist->st->disposition;

    if (ist->st->nb_side_data) {
        ost->st->side_data = (AVPacketSideData *) av_realloc_array(NULL, ist->st->nb_side_data,
                                                                   sizeof(*ist->st->side_data));
        if (!ost->st->side_data)
            return AVERROR(ENOMEM);

        ost->st->nb_side_data = 0;
        for (i = 0; i < ist->st->nb_side_data; i++) {
            const AVPacketSideData *sd_src = &ist->st->side_data[i];
            AVPacketSideData *sd_dst = &ost->st->side_data[ost->st->nb_side_data];

            sd_dst->data = (uint8_t *) av_malloc(sd_src->size);
            if (!sd_dst->data)
                return AVERROR(ENOMEM);
            memcpy(sd_dst->data, sd_src->data, sd_src->size);
            sd_dst->size = sd_src->size;
            sd_dst->type = sd_src->type;
            ost->st->nb_side_data++;
        }
    }


    ost->parser = av_parser_init(par_dst->codec_id);
    ost->parser_avctx = avcodec_alloc_context3(NULL);
    if (!ost->parser_avctx)
        return AVERROR(ENOMEM);

    switch (par_dst->codec_type) {
        case AVMEDIA_TYPE_AUDIO:
            if (mFFContext->audio_volume != 256) {
                J4A_ALOGE("-acodec copy and -vol are incompatible (frames are not decoded)\n");
                return -1;
            }
            if((par_dst->block_align == 1 || par_dst->block_align == 1152 || par_dst->block_align == 576) && par_dst->codec_id == AV_CODEC_ID_MP3)
                par_dst->block_align= 0;
            if(par_dst->codec_id == AV_CODEC_ID_AC3)
                par_dst->block_align= 0;
            break;
        case AVMEDIA_TYPE_VIDEO:
            if (ost->frame_aspect_ratio.num) { // overridden by the -aspect cli option
                sar =
                        av_mul_q(ost->frame_aspect_ratio,
                                 (AVRational){ par_dst->height, par_dst->width });
                av_log(NULL, AV_LOG_WARNING, "Overriding aspect ratio "
                        "with stream copy may produce invalid files\n");
            }
            else if (ist->st->sample_aspect_ratio.num)
                sar = ist->st->sample_aspect_ratio;
            else
                sar = par_src->sample_aspect_ratio;
            ost->st->sample_aspect_ratio = par_dst->sample_aspect_ratio = sar;
            ost->st->avg_frame_rate = ist->st->avg_frame_rate;
            ost->st->r_frame_rate = ist->st->r_frame_rate;
            break;
    }

    ost->mux_timebase = ist->st->time_base;

    return 0;
}

static int init_output_bsfs(OutputStream *ost)
{
    AVBSFContext *ctx;
    int i, ret;

    if (!ost->nb_bitstream_filters)
        return 0;

    for (i = 0; i < ost->nb_bitstream_filters; i++) {
        ctx = ost->bsf_ctx[i];

        ret = avcodec_parameters_copy(ctx->par_in,
                                      i ? ost->bsf_ctx[i - 1]->par_out : ost->st->codecpar);
        if (ret < 0)
            return ret;

        ctx->time_base_in = i ? ost->bsf_ctx[i - 1]->time_base_out : ost->st->time_base;

        ret = av_bsf_init(ctx);
        if (ret < 0) {
            J4A_ALOGE("Error initializing bitstream filter: %s\n",
                      ost->bsf_ctx[i]->filter->name);
            return ret;
        }
    }

    ctx = ost->bsf_ctx[ost->nb_bitstream_filters - 1];
    ret = avcodec_parameters_copy(ost->st->codecpar, ctx->par_out);
    if (ret < 0)
        return ret;

    ost->st->time_base = ctx->time_base_out;

    return 0;
}


void close_all_output_streams(FFContext *mFFContext,OutputStream *ost, int this_stream, OSTFinished others)
{
    int i;
    for (i = 0; i < mFFContext->nb_output_streams; i++) {
        OutputStream *ost2 = mFFContext->output_streams[i];
        ost2->finished |= ost == ost2 ? this_stream : others;
    }
}

int write_packet(FFContext *mFFContext,OutputFile *of, AVPacket *pkt, OutputStream *ost, int unqueue)
{
    AVFormatContext *s = of->ctx;
    AVStream *st = ost->st;
    int ret;

    if (!(st->codecpar->codec_type == AVMEDIA_TYPE_VIDEO && ost->encoding_needed) && !unqueue) {
        if (ost->frame_number >= ost->max_frames) {
            av_packet_unref(pkt);
            return 0;
        }
        ost->frame_number++;
    }

    if (!of->header_written) {
        AVPacket tmp_pkt = {0};
        /* the muxer is not initialized yet, buffer the packet */
        if (!av_fifo_space(ost->muxing_queue)) {
            int new_size = FFMIN(2 * av_fifo_size(ost->muxing_queue),
                                 ost->max_muxing_queue_size);
            if (new_size <= av_fifo_size(ost->muxing_queue)) {
                J4A_ALOGE(
                        "Too many packets buffered for output stream %d:%d.\n",
                        ost->file_index, ost->st->index);
                return -1;
            }
            ret = av_fifo_realloc2(ost->muxing_queue, new_size);
            if (ret < 0)
                return -1;
        }
        ret = av_packet_ref(&tmp_pkt, pkt);
        if (ret < 0)
            return -1;
        av_fifo_generic_write(ost->muxing_queue, &tmp_pkt, sizeof(tmp_pkt), NULL);
        av_packet_unref(pkt);
        return 0;
    }


    if (st->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
        int i;
        uint8_t *sd = av_packet_get_side_data(pkt, AV_PKT_DATA_QUALITY_STATS,
                                              NULL);
        ost->quality = sd ? AV_RL32(sd) : -1;
        ost->pict_type = sd ? sd[4] : AV_PICTURE_TYPE_NONE;

        for (i = 0; i<FF_ARRAY_ELEMS(ost->error); i++) {
            if (sd && i < sd[5])
                ost->error[i] = AV_RL64(sd + 8 + 8*i);
            else
                ost->error[i] = -1;
        }

        if (ost->frame_rate.num && ost->is_cfr) {
            if (pkt->duration > 0)
                J4A_ALOGD( "Overriding packet duration by frame rate, this should not happen\n");
            pkt->duration = av_rescale_q(1, av_inv_q(ost->frame_rate),
                                         ost->mux_timebase);
        }
    }

    av_packet_rescale_ts(pkt, ost->mux_timebase, ost->st->time_base);

    if (!(s->oformat->flags & AVFMT_NOTIMESTAMPS)) {
        if (pkt->dts != AV_NOPTS_VALUE &&
            pkt->pts != AV_NOPTS_VALUE &&
            pkt->dts > pkt->pts) {
            J4A_ALOGD("Invalid DTS: %"PRId64" PTS: %"PRId64" in output stream %d:%d, replacing by guess\n",
                      pkt->dts, pkt->pts,
                      ost->file_index, ost->st->index);
            pkt->pts =
            pkt->dts = pkt->pts + pkt->dts + ost->last_mux_dts + 1
                       - FFMIN3(pkt->pts, pkt->dts, ost->last_mux_dts + 1)
                       - FFMAX3(pkt->pts, pkt->dts, ost->last_mux_dts + 1);
        }
        if ((st->codecpar->codec_type == AVMEDIA_TYPE_AUDIO || st->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) &&
            pkt->dts != AV_NOPTS_VALUE &&
            !(st->codecpar->codec_id == AV_CODEC_ID_VP9 && ost->stream_copy) &&
            ost->last_mux_dts != AV_NOPTS_VALUE) {
            int64_t max = ost->last_mux_dts + !(s->oformat->flags & AVFMT_TS_NONSTRICT);
            if (pkt->dts < max) {
                int loglevel = max - pkt->dts > 2 || st->codecpar->codec_type == AVMEDIA_TYPE_VIDEO ? AV_LOG_WARNING : AV_LOG_DEBUG;
                J4A_ALOGD( "Non-monotonous DTS in output stream "
                                   "%d:%d; previous: %"PRId64", current: %"PRId64"; ",
                           ost->file_index, ost->st->index, ost->last_mux_dts, pkt->dts);

                J4A_ALOGD( "changing to %"PRId64". This may result "
                        "in incorrect timestamps in the output file.\n",
                           max);
                if (pkt->pts >= pkt->dts)
                    pkt->pts = FFMAX(pkt->pts, max);
                pkt->dts = max;
            }
        }
    }
    ost->last_mux_dts = pkt->dts;

    ost->data_size += pkt->size;
    ost->packets_written++;

    pkt->stream_index = ost->index;


    ret = av_interleaved_write_frame(s, pkt);
    if (ret < 0) {
        print_error("av_interleaved_write_frame()", ret);

        close_all_output_streams(mFFContext,ost, MUXER_FINISHED | ENCODER_FINISHED, ENCODER_FINISHED);
    }
    av_packet_unref(pkt);
    return 0;
}

int check_init_output_file(FFContext *mFFContext,OutputFile *of, int file_index)
{
    int ret, i;

    for (i = 0; i < of->ctx->nb_streams; i++) {
        OutputStream *ost = mFFContext->output_streams[of->ost_index + i];
        if (!ost->initialized)
            return 0;
    }


    ret = avformat_write_header(of->ctx, NULL);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR,
               "Could not write header for output file #%d "
                       "(incorrect codec parameters ?): %s\n",
               file_index, av_err2str(ret));
        return ret;
    }

    of->header_written = 1;

    av_dump_format(of->ctx, file_index, of->ctx->filename, 1);

    /* flush the muxing queues */
    for (i = 0; i < of->ctx->nb_streams; i++) {
        OutputStream *ost = mFFContext->output_streams[of->ost_index + i];

        /* try to improve muxing time_base (only possible if nothing has been written yet) */
        if (!av_fifo_size(ost->muxing_queue))
            ost->mux_timebase = ost->st->time_base;

        while (av_fifo_size(ost->muxing_queue)) {
            AVPacket pkt;
            av_fifo_generic_read(ost->muxing_queue, &pkt, sizeof(pkt), NULL);
            write_packet(mFFContext,of, &pkt, ost, 1);
        }
    }

    return 0;
}

int init_output_stream(FFContext *mFFContext,OutputStream *ost, char *error, int error_len)
{
    int ret = 0;

    if (ost->stream_copy) {
        ret = init_output_stream_streamcopy(mFFContext,ost);
        if (ret < 0)
            return ret;

        /*
         * FIXME: will the codec context used by the parser during streamcopy
         * This should go away with the new parser API.
         */
        ret = avcodec_parameters_to_context(ost->parser_avctx, ost->st->codecpar);
        if (ret < 0)
            return ret;
    }


    /* initialize bitstream filters for the output stream
     * needs to be done here, because the codec id for streamcopy is not
     * known until now */
    ret = init_output_bsfs(ost);
    if (ret < 0)
        return ret;

    ost->initialized = 1;

    ret = check_init_output_file(mFFContext,mFFContext->output_files[ost->file_index], ost->file_index);
    if (ret < 0)
        return ret;

    return ret;
}

int transcode_init(FFContext *mFFContext){
    int ret = 0, i, j, k;
    AVFormatContext *oc;
    OutputStream *ost;
    InputStream *ist;
    char error[1024] = {0};

    for (i = 0; i < mFFContext->nb_input_streams; i++) {
        if ((ret = init_input_stream(mFFContext,i, error, sizeof(error))) < 0) {
            for (i = 0; i < mFFContext->nb_output_streams; i++) {
                ost = mFFContext->output_streams[i];
                avcodec_close(ost->enc_ctx);
            }
        }
    }

    for (i = 0; i < mFFContext->nb_output_streams; i++) {

        ret = init_output_stream(mFFContext,mFFContext->output_streams[i], error, sizeof(error));
        if (ret < 0)
            J4A_ALOGD("init_output_stream error");
    }



    for (i = 0; i < mFFContext->nb_output_files; i++) {
        oc = mFFContext->output_files[i]->ctx;
        if (oc->oformat->flags & AVFMT_NOSTREAMS && oc->nb_streams == 0) {
            ret = check_init_output_file(mFFContext,mFFContext->output_files[i], i);
            if (ret < 0)
                goto dump_format;
        }
    }


    dump_format:
    /* dump the stream mapping */
    J4A_ALOGD( "Stream mapping:\n");

    for (i = 0; i < mFFContext->nb_output_streams; i++) {
        ost = mFFContext->output_streams[i];

        if (ost->attachment_filename) {
            /* an attached file */
            J4A_ALOGD("  File %s -> Stream #%d:%d\n",
                      ost->attachment_filename, ost->file_index, ost->index);
            continue;
        }

        J4A_ALOGD("  Stream #%d:%d -> #%d:%d",
                  mFFContext->input_streams[ost->source_index]->file_index,
                  mFFContext->input_streams[ost->source_index]->st->index,
                  ost->file_index,
                  ost->index);
        if (ost->sync_ist != mFFContext->input_streams[ost->source_index])
            av_log(NULL, AV_LOG_INFO, " [sync #%d:%d]",
                   ost->sync_ist->file_index,
                   ost->sync_ist->st->index);
        if (ost->stream_copy)
            J4A_ALOGD( " (copy)");

    }

    if (ret) {
        J4A_ALOGE( "%s\n", error);
        return ret;
    }

    return 0;
}

void close_output_stream(FFContext *mFFContext,OutputStream *ost)
{
    OutputFile *of = mFFContext->output_files[ost->file_index];

    ost->finished |= ENCODER_FINISHED;
    if (of->shortest) {
        int64_t end = av_rescale_q(ost->sync_opts - ost->first_pts, ost->enc_ctx->time_base, AV_TIME_BASE_Q);
        of->recording_time = FFMIN(of->recording_time, end);
    }
}

int need_output(FFContext *mFFContext)
{
    int i;

    for (i = 0; i < mFFContext->nb_output_streams; i++) {
        OutputStream *ost    = mFFContext->output_streams[i];
        OutputFile *of       = mFFContext->output_files[ost->file_index];
        AVFormatContext *os  = mFFContext->output_files[ost->file_index]->ctx;

        if (ost->finished ||
            (os->pb && avio_tell(os->pb) >= of->limit_filesize))
            continue;
        if (ost->frame_number >= ost->max_frames) {
            int j;
            for (j = 0; j < of->ctx->nb_streams; j++)
                close_output_stream(mFFContext,mFFContext->output_streams[of->ost_index + j]);
            continue;
        }

        return 1;
    }

    return 0;
}

OutputStream *choose_output(FFContext *mFFContext)
{
    int i;
    int64_t opts_min = INT64_MAX;
    OutputStream *ost_min = NULL;

    for (i = 0; i < mFFContext->nb_output_streams; i++) {
        OutputStream *ost = mFFContext->output_streams[i];
        int64_t opts = ost->st->cur_dts == AV_NOPTS_VALUE ? INT64_MIN :
                       av_rescale_q(ost->st->cur_dts, ost->st->time_base,
                                    AV_TIME_BASE_Q);
        if (ost->st->cur_dts == AV_NOPTS_VALUE)
            J4A_ALOGD("cur_dts is invalid (this is harmless if it occurs once at the start per stream)\n");

        if (!ost->initialized && !ost->inputs_done)
            return ost;

        if (!ost->finished && opts < opts_min) {
            opts_min = opts;
            ost_min  = ost->unavailable ? NULL : ost;
        }
    }
    return ost_min;
}

int got_eagain(FFContext *mFFContext)
{
    int i;
    for (i = 0; i < mFFContext->nb_output_streams; i++)
        if (mFFContext->output_streams[i]->unavailable)
            return 1;
    return 0;
}

void reset_eagain(FFContext *mFFContext)
{
    int i;
    for (i = 0; i < mFFContext->nb_input_files; i++)
        mFFContext->input_files[i]->eagain = 0;
    for (i = 0; i < mFFContext->nb_output_streams; i++)
        mFFContext->output_streams[i]->unavailable = 0;
}

int get_input_packet(FFContext *mFFContext,InputFile *f, AVPacket *pkt)
{
    int ret = 0;
    if (f->rate_emu) {
        int i;
        for (i = 0; i < f->nb_streams; i++) {
            InputStream *ist = mFFContext->input_streams[f->ist_index + i];
            int64_t pts = av_rescale(ist->dts, 1000000, AV_TIME_BASE);
            int64_t now = av_gettime_relative() - ist->start;
            if (pts > now)
                return AVERROR(EAGAIN);
        }
    }

    ret = av_read_frame(f->ctx, pkt);
    return ret;
}

void finish_output_stream(FFContext *mFFContext,OutputStream *ost)
{
    OutputFile *of = mFFContext->output_files[ost->file_index];
    int i;

    ost->finished = ENCODER_FINISHED | MUXER_FINISHED;

    if (of->shortest) {
        for (i = 0; i < of->ctx->nb_streams; i++)
            mFFContext->output_streams[of->ost_index + i]->finished = ENCODER_FINISHED | MUXER_FINISHED;
    }
}


int check_output_constraints(FFContext *mFFContext,InputStream *ist, OutputStream *ost)
{
    OutputFile *of = mFFContext->output_files[ost->file_index];
    int ist_index  = mFFContext->input_files[ist->file_index]->ist_index + ist->st->index;

    if (ost->source_index != ist_index)
        return 0;

    if (ost->finished)
        return 0;

    if (of->start_time != AV_NOPTS_VALUE && ist->pts < of->start_time)
        return 0;

    return 1;
}


void output_packet(FFContext *mFFContext,OutputFile *of, AVPacket *pkt, OutputStream *ost)
{
    int ret = 0;

    /* apply the output bitstream filters, if any */
    if (ost->nb_bitstream_filters) {
        int idx;

        ret = av_bsf_send_packet(ost->bsf_ctx[0], pkt);
        if (ret < 0)
            goto finish;

        idx = 1;
        while (idx) {
            /* get a packet from the previous filter up the chain */
            ret = av_bsf_receive_packet(ost->bsf_ctx[idx - 1], pkt);
            if (ret == AVERROR(EAGAIN)) {
                ret = 0;
                idx--;
                continue;
            } else if (ret < 0)
                goto finish;
            /* HACK! - aac_adtstoasc updates extradata after filtering the first frame when
             * the api states this shouldn't happen after init(). Propagate it here to the
             * muxer and to the next filters in the chain to workaround this.
             * TODO/FIXME - Make aac_adtstoasc use new packet side data instead of changing
             * par_out->extradata and adapt muxers accordingly to get rid of this. */
            if (!(ost->bsf_extradata_updated[idx - 1] & 1)) {
                ret = avcodec_parameters_copy(ost->st->codecpar, ost->bsf_ctx[idx - 1]->par_out);
                if (ret < 0)
                    goto finish;
                ost->bsf_extradata_updated[idx - 1] |= 1;
            }

            /* send it to the next filter down the chain or to the muxer */
            if (idx < ost->nb_bitstream_filters) {
                /* HACK/FIXME! - See above */
                if (!(ost->bsf_extradata_updated[idx] & 2)) {
                    ret = avcodec_parameters_copy(ost->bsf_ctx[idx]->par_out, ost->bsf_ctx[idx - 1]->par_out);
                    if (ret < 0)
                        goto finish;
                    ost->bsf_extradata_updated[idx] |= 2;
                }
                ret = av_bsf_send_packet(ost->bsf_ctx[idx], pkt);
                if (ret < 0)
                    goto finish;
                idx++;
            } else
                write_packet(mFFContext,of, pkt, ost, 0);
        }
    } else
        write_packet(mFFContext,of, pkt, ost, 0);

    finish:
    if (ret < 0 && ret != AVERROR_EOF) {
        J4A_ALOGE( "Error applying bitstream filters to an output "
                           "packet for stream #%d:%d.\n", ost->file_index, ost->index);

    }
}

int do_streamcopy(FFContext *mFFContext,InputStream *ist, OutputStream *ost, const AVPacket *pkt)
{
    OutputFile *of = mFFContext->output_files[ost->file_index];
    InputFile   *f = mFFContext->input_files [ist->file_index];
    int64_t start_time = (of->start_time == AV_NOPTS_VALUE) ? 0 : of->start_time;
    int64_t ost_tb_start_time = av_rescale_q(start_time, AV_TIME_BASE_Q, ost->mux_timebase);
    AVPicture pict;
    AVPacket opkt;

    av_init_packet(&opkt);

    if ((!ost->frame_number && !(pkt->flags & AV_PKT_FLAG_KEY)) &&
        !ost->copy_initial_nonkeyframes)
        return 0;

    if (!ost->frame_number && !ost->copy_prior_start) {
        int64_t comp_start = start_time;
        if (pkt->pts == AV_NOPTS_VALUE ?
            ist->pts < comp_start :
            pkt->pts < av_rescale_q(comp_start, AV_TIME_BASE_Q, ist->st->time_base))
            return 0;
    }

    if (of->recording_time != INT64_MAX &&
        ist->pts >= of->recording_time + start_time) {
        close_output_stream(mFFContext,ost);
        return 0;
    }

    if (f->recording_time != INT64_MAX) {
        start_time = f->ctx->start_time;
        if (ist->pts >= f->recording_time + start_time) {
            close_output_stream(mFFContext,ost);
            return 0;
        }
    }

    /* force the input stream PTS */
    if (ost->enc_ctx->codec_type == AVMEDIA_TYPE_VIDEO)
        ost->sync_opts++;

    if (pkt->pts != AV_NOPTS_VALUE)
        opkt.pts = av_rescale_q(pkt->pts, ist->st->time_base, ost->mux_timebase) - ost_tb_start_time;
    else
        opkt.pts = AV_NOPTS_VALUE;

    if (pkt->dts == AV_NOPTS_VALUE)
        opkt.dts = av_rescale_q(ist->dts, AV_TIME_BASE_Q, ost->mux_timebase);
    else
        opkt.dts = av_rescale_q(pkt->dts, ist->st->time_base, ost->mux_timebase);
    opkt.dts -= ost_tb_start_time;

    if (ost->st->codecpar->codec_type == AVMEDIA_TYPE_AUDIO && pkt->dts != AV_NOPTS_VALUE) {
        int duration = av_get_audio_frame_duration(ist->dec_ctx, pkt->size);
        if(!duration)
            duration = ist->dec_ctx->frame_size;
        opkt.dts = opkt.pts = av_rescale_delta(ist->st->time_base, pkt->dts,
                                               (AVRational){1, ist->dec_ctx->sample_rate}, duration, &ist->filter_in_rescale_delta_last,
                                               ost->mux_timebase) - ost_tb_start_time;
    }

    opkt.duration = av_rescale_q(pkt->duration, ist->st->time_base, ost->mux_timebase);

    opkt.flags    = pkt->flags;
    // FIXME remove the following 2 lines they shall be replaced by the bitstream filters
    if (  ost->st->codecpar->codec_id != AV_CODEC_ID_H264
          && ost->st->codecpar->codec_id != AV_CODEC_ID_MPEG1VIDEO
          && ost->st->codecpar->codec_id != AV_CODEC_ID_MPEG2VIDEO
          && ost->st->codecpar->codec_id != AV_CODEC_ID_VC1
            ) {
        int ret = av_parser_change(ost->parser, ost->parser_avctx,
                                   &opkt.data, &opkt.size,
                                   pkt->data, pkt->size,
                                   pkt->flags & AV_PKT_FLAG_KEY);
        if (ret < 0) {
            J4A_ALOGE("av_parser_change failed: %s\n",
                      av_err2str(ret));
            return -1;
        }
        if (ret) {
            opkt.buf = av_buffer_create(opkt.data, opkt.size, av_buffer_default_free, NULL, 0);
            if (!opkt.buf)
                return -1;
        }
    } else {
        opkt.data = pkt->data;
        opkt.size = pkt->size;
    }
    av_copy_packet_side_data(&opkt, pkt);

#if FF_API_LAVF_FMT_RAWPICTURE
    if (ost->st->codecpar->codec_type == AVMEDIA_TYPE_VIDEO &&
        ost->st->codecpar->codec_id == AV_CODEC_ID_RAWVIDEO &&
        (of->ctx->oformat->flags & AVFMT_RAWPICTURE)) {
        /* store AVPicture in AVPacket, as expected by the output format */
        int ret = avpicture_fill(&pict, opkt.data, (AVPixelFormat) ost->st->codecpar->format, ost->st->codecpar->width, ost->st->codecpar->height);
        if (ret < 0) {
            J4A_ALOGE( "avpicture_fill failed: %s\n",
                       av_err2str(ret));
            return -1;
        }
        opkt.data = (uint8_t *)&pict;
        opkt.size = sizeof(AVPicture);
        opkt.flags |= AV_PKT_FLAG_KEY;
    }
#endif

    output_packet(mFFContext,of, &opkt, ost);
    return 0;
}

int process_input_packet(FFContext *mFFContext,InputStream *ist, const AVPacket *pkt, int no_eof)
{
    int ret = 0, i;
    int repeating = 0;
    int eof_reached = 0;

    AVPacket avpkt;
    if (!ist->saw_first_ts) {
        ist->dts = ist->st->avg_frame_rate.num ? - ist->dec_ctx->has_b_frames * AV_TIME_BASE / av_q2d(ist->st->avg_frame_rate) : 0;
        ist->pts = 0;
        if (pkt && pkt->pts != AV_NOPTS_VALUE && !ist->decoding_needed) {
            ist->dts += av_rescale_q(pkt->pts, ist->st->time_base, AV_TIME_BASE_Q);
            ist->pts = ist->dts; //unused but better to set it to a value thats not totally wrong
        }
        ist->saw_first_ts = 1;
    }

    if (ist->next_dts == AV_NOPTS_VALUE)
        ist->next_dts = ist->dts;
    if (ist->next_pts == AV_NOPTS_VALUE)
        ist->next_pts = ist->pts;

    if (!pkt) {
        /* EOF handling */
        av_init_packet(&avpkt);
        avpkt.data = NULL;
        avpkt.size = 0;
    } else {
        avpkt = *pkt;
    }

    if (pkt && pkt->dts != AV_NOPTS_VALUE) {
        ist->next_dts = ist->dts = av_rescale_q(pkt->dts, ist->st->time_base, AV_TIME_BASE_Q);
        if (ist->dec_ctx->codec_type != AVMEDIA_TYPE_VIDEO || !ist->decoding_needed)
            ist->next_pts = ist->pts = ist->dts;
    }


    /* handle stream copy */
    if (!ist->decoding_needed) {
        ist->dts = ist->next_dts;
        switch (ist->dec_ctx->codec_type) {
            case AVMEDIA_TYPE_AUDIO:
                ist->next_dts += ((int64_t)AV_TIME_BASE * ist->dec_ctx->frame_size) /
                                 ist->dec_ctx->sample_rate;
                break;
            case AVMEDIA_TYPE_VIDEO:
                if (ist->framerate.num) {
                    // TODO: Remove work-around for c99-to-c89 issue 7
                    AVRational time_base_q = AV_TIME_BASE_Q;
                    int64_t next_dts = av_rescale_q(ist->next_dts, time_base_q, av_inv_q(ist->framerate));
                    ist->next_dts = av_rescale_q(next_dts + 1, av_inv_q(ist->framerate), time_base_q);
                } else if (pkt->duration) {
                    ist->next_dts += av_rescale_q(pkt->duration, ist->st->time_base, AV_TIME_BASE_Q);
                } else if(ist->dec_ctx->framerate.num != 0) {
                    int ticks= av_stream_get_parser(ist->st) ? av_stream_get_parser(ist->st)->repeat_pict + 1 : ist->dec_ctx->ticks_per_frame;
                    ist->next_dts += ((int64_t)AV_TIME_BASE *
                                      ist->dec_ctx->framerate.den * ticks) /
                                     ist->dec_ctx->framerate.num / ist->dec_ctx->ticks_per_frame;
                }
                break;
        }
        ist->pts = ist->dts;
        ist->next_pts = ist->next_dts;
    }
    for (i = 0; pkt && i < mFFContext->nb_output_streams; i++) {
        OutputStream *ost = mFFContext->output_streams[i];

        if (!check_output_constraints(mFFContext,ist, ost) || ost->encoding_needed)
            continue;

        do_streamcopy(mFFContext,ist, ost, pkt);
    }

    return !eof_reached;
}

int process_input(FFContext *mFFContext,int file_index)
{
    InputFile *ifile = mFFContext->input_files[file_index];
    AVFormatContext *is;
    InputStream *ist;
    AVPacket pkt;
    int ret, i, j;
    int64_t duration;
    int64_t pkt_dts;

    is  = ifile->ctx;
    ret = get_input_packet(mFFContext,ifile, &pkt);

    if (ret == AVERROR(EAGAIN)) {
        ifile->eagain = 1;
        return ret;
    }

    if (ret < 0) {
        if (ret != AVERROR_EOF) {
            print_error(is->filename, ret);
            return -1;
        }

        for (i = 0; i < ifile->nb_streams; i++) {
            ist = mFFContext->input_streams[ifile->ist_index + i];

            /* mark all outputs that don't go through lavfi as finished */
            for (j = 0; j < mFFContext->nb_output_streams; j++) {
                OutputStream *ost = mFFContext->output_streams[j];

                if (ost->source_index == ifile->ist_index + i &&
                    (ost->stream_copy || ost->enc->type == AVMEDIA_TYPE_SUBTITLE))
                    finish_output_stream(mFFContext,ost);
            }
        }

        ifile->eof_reached = 1;
        return AVERROR(EAGAIN);
    }

    reset_eagain(mFFContext);

    ist = mFFContext->input_streams[ifile->ist_index + pkt.stream_index];

    ist->data_size += pkt.size;
    ist->nb_packets++;

    if (ist->discard)
        goto discard_packet;

    if(!ist->wrap_correction_done && is->start_time != AV_NOPTS_VALUE && ist->st->pts_wrap_bits < 64){
        int64_t stime, stime2;
        // Correcting starttime based on the enabled streams
        // FIXME this ideally should be done before the first use of starttime but we do not know which are the enabled streams at that point.
        //       so we instead do it here as part of discontinuity handling
        if (   ist->next_dts == AV_NOPTS_VALUE
               && ifile->ts_offset == -is->start_time
               && (is->iformat->flags & AVFMT_TS_DISCONT)) {
            int64_t new_start_time = INT64_MAX;
            for (i=0; i<is->nb_streams; i++) {
                AVStream *st = is->streams[i];
                if(st->discard == AVDISCARD_ALL || st->start_time == AV_NOPTS_VALUE)
                    continue;
                new_start_time = FFMIN(new_start_time, av_rescale_q(st->start_time, st->time_base, AV_TIME_BASE_Q));
            }
            if (new_start_time > is->start_time) {
                av_log(is, AV_LOG_VERBOSE, "Correcting start time by %"PRId64"\n", new_start_time - is->start_time);
                ifile->ts_offset = -new_start_time;
            }
        }

        stime = av_rescale_q(is->start_time, AV_TIME_BASE_Q, ist->st->time_base);
        stime2= stime + (1ULL<<ist->st->pts_wrap_bits);
        ist->wrap_correction_done = 1;

        if(stime2 > stime && pkt.dts != AV_NOPTS_VALUE && pkt.dts > stime + (1LL<<(ist->st->pts_wrap_bits-1))) {
            pkt.dts -= 1ULL<<ist->st->pts_wrap_bits;
            ist->wrap_correction_done = 0;
        }
        if(stime2 > stime && pkt.pts != AV_NOPTS_VALUE && pkt.pts > stime + (1LL<<(ist->st->pts_wrap_bits-1))) {
            pkt.pts -= 1ULL<<ist->st->pts_wrap_bits;
            ist->wrap_correction_done = 0;
        }
    }

    /* add the stream-global side data to the first packet */
    if (ist->nb_packets == 1) {
        for (i = 0; i < ist->st->nb_side_data; i++) {
            AVPacketSideData *src_sd = &ist->st->side_data[i];
            uint8_t *dst_data;

            if (src_sd->type == AV_PKT_DATA_DISPLAYMATRIX)
                continue;

            if (av_packet_get_side_data(&pkt, src_sd->type, NULL))
                continue;

            dst_data = av_packet_new_side_data(&pkt, src_sd->type, src_sd->size);
            if (!dst_data)
                return -1;

            memcpy(dst_data, src_sd->data, src_sd->size);
        }
    }

    if (pkt.dts != AV_NOPTS_VALUE)
        pkt.dts += av_rescale_q(ifile->ts_offset, AV_TIME_BASE_Q, ist->st->time_base);
    if (pkt.pts != AV_NOPTS_VALUE)
        pkt.pts += av_rescale_q(ifile->ts_offset, AV_TIME_BASE_Q, ist->st->time_base);

//    if (pkt.pts != AV_NOPTS_VALUE)
//        pkt.pts *= ist->ts_scale;
//    if (pkt.dts != AV_NOPTS_VALUE)
//        pkt.dts *= ist->ts_scale;

    pkt_dts = av_rescale_q_rnd(pkt.dts, ist->st->time_base, AV_TIME_BASE_Q, (enum AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
    if ((ist->dec_ctx->codec_type == AVMEDIA_TYPE_VIDEO ||
         ist->dec_ctx->codec_type == AVMEDIA_TYPE_AUDIO) &&
        pkt_dts != AV_NOPTS_VALUE && ist->next_dts == AV_NOPTS_VALUE
        && (is->iformat->flags & AVFMT_TS_DISCONT) && ifile->last_ts != AV_NOPTS_VALUE) {
        int64_t delta   = pkt_dts - ifile->last_ts;
        if (delta < -1LL*mFFContext->dts_delta_threshold*AV_TIME_BASE ||
            delta >  1LL*mFFContext->dts_delta_threshold*AV_TIME_BASE){
            ifile->ts_offset -= delta;
            av_log(NULL, AV_LOG_DEBUG,
                   "Inter stream timestamp discontinuity %"PRId64", new offset= %"PRId64"\n",
                   delta, ifile->ts_offset);
            pkt.dts -= av_rescale_q(delta, AV_TIME_BASE_Q, ist->st->time_base);
            if (pkt.pts != AV_NOPTS_VALUE)
                pkt.pts -= av_rescale_q(delta, AV_TIME_BASE_Q, ist->st->time_base);
        }
    }

    duration = av_rescale_q(ifile->duration, ifile->time_base, ist->st->time_base);
    if (pkt.pts != AV_NOPTS_VALUE) {
        pkt.pts += duration;
        ist->max_pts = FFMAX(pkt.pts, ist->max_pts);
        ist->min_pts = FFMIN(pkt.pts, ist->min_pts);
    }

    if (pkt.dts != AV_NOPTS_VALUE)
        pkt.dts += duration;

    pkt_dts = av_rescale_q_rnd(pkt.dts, ist->st->time_base, AV_TIME_BASE_Q, (enum AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
    if ((ist->dec_ctx->codec_type == AVMEDIA_TYPE_VIDEO ||
         ist->dec_ctx->codec_type == AVMEDIA_TYPE_AUDIO) &&
        pkt_dts != AV_NOPTS_VALUE && ist->next_dts != AV_NOPTS_VALUE) {
        int64_t delta   = pkt_dts - ist->next_dts;
        if (is->iformat->flags & AVFMT_TS_DISCONT) {
            if (delta < -1LL*mFFContext->dts_delta_threshold*AV_TIME_BASE ||
                delta >  1LL*mFFContext->dts_delta_threshold*AV_TIME_BASE ||
                pkt_dts + AV_TIME_BASE/10 < FFMAX(ist->pts, ist->dts)) {
                ifile->ts_offset -= delta;
                av_log(NULL, AV_LOG_DEBUG,
                       "timestamp discontinuity %"PRId64", new offset= %"PRId64"\n",
                       delta, ifile->ts_offset);
                pkt.dts -= av_rescale_q(delta, AV_TIME_BASE_Q, ist->st->time_base);
                if (pkt.pts != AV_NOPTS_VALUE)
                    pkt.pts -= av_rescale_q(delta, AV_TIME_BASE_Q, ist->st->time_base);
            }
        } else {
            if ( delta < -1LL*mFFContext->dts_error_threshold*AV_TIME_BASE ||
                 delta >  1LL*mFFContext->dts_error_threshold*AV_TIME_BASE) {
                av_log(NULL, AV_LOG_WARNING, "DTS %"PRId64", next:%"PRId64" st:%d invalid dropping\n", pkt.dts, ist->next_dts, pkt.stream_index);
                pkt.dts = AV_NOPTS_VALUE;
            }
            if (pkt.pts != AV_NOPTS_VALUE){
                int64_t pkt_pts = av_rescale_q(pkt.pts, ist->st->time_base, AV_TIME_BASE_Q);
                delta   = pkt_pts - ist->next_dts;
                if ( delta < -1LL*mFFContext->dts_error_threshold*AV_TIME_BASE ||
                     delta >  1LL*mFFContext->dts_error_threshold*AV_TIME_BASE) {
                    av_log(NULL, AV_LOG_WARNING, "PTS %"PRId64", next:%"PRId64" invalid dropping st:%d\n", pkt.pts, ist->next_dts, pkt.stream_index);
                    pkt.pts = AV_NOPTS_VALUE;
                }
            }
        }
    }

    if (pkt.dts != AV_NOPTS_VALUE)
        ifile->last_ts = av_rescale_q(pkt.dts, ist->st->time_base, AV_TIME_BASE_Q);


    process_input_packet(mFFContext,ist, &pkt, 0);

    discard_packet:
    av_packet_unref(&pkt);

    return 0;
}

int transcode_step(FFContext *mFFContext)
{
    OutputStream *ost;
    InputStream  *ist = NULL;
    int ret;

    ost = choose_output(mFFContext);
    if (!ost) {
        if (got_eagain(mFFContext)) {
            reset_eagain(mFFContext);
            av_usleep(10000);
            return 0;
        }
        J4A_ALOGD("No more inputs to read from, finishing.\n");
        return AVERROR_EOF;
    }


    ist = mFFContext->input_streams[ost->source_index];


    ret = process_input(mFFContext,ist->file_index);
    if (ret == AVERROR(EAGAIN)) {
        if (mFFContext->input_files[ist->file_index]->eagain)
            ost->unavailable = 1;
        return 0;
    }

    if (ret < 0)
        return ret == AVERROR_EOF ? 0 : ret;

    return 0;
}

int transcode(FFContext *mFFContext){

    int ret, i;
    AVFormatContext *os;
    OutputStream *ost;
    InputStream *ist;
    int64_t timer_start;
    int64_t total_packets_written = 0;

    ret = transcode_init(mFFContext);


    timer_start = av_gettime_relative();

    while (!mFFContext->received_sigterm) {
        int64_t cur_time= av_gettime_relative();

        /* check if there's any stream where output is still needed */
        if (!need_output(mFFContext)) {
            J4A_ALOGE("No more output streams to write to, finishing.\n");
            break;
        }

        ret = transcode_step(mFFContext);
        if (ret < 0 && ret != AVERROR_EOF) {
            char errbuf[128];
            av_strerror(ret, errbuf, sizeof(errbuf));

            J4A_ALOGE( "Error while filtering: %s\n", errbuf);
            break;
        }

    }


    /* write the trailer if needed and close file */
    for (i = 0; i < mFFContext->nb_output_files; i++) {
        os = mFFContext->output_files[i]->ctx;
        if (!mFFContext->output_files[i]->header_written) {
            J4A_ALOGE(
                    "Nothing was written into output file %d (%s), because "
                            "at least one of its streams received no packets.\n",
                    i, os->filename);
            continue;
        }
        if ((ret = av_write_trailer(os)) < 0) {
            J4A_ALOGE("Error writing trailer of %s: %s\n", os->filename, av_err2str(ret));
            return -1;
        }
    }

    /* close each encoder */
    for (i = 0; i < mFFContext->nb_output_streams; i++) {
        ost = mFFContext->output_streams[i];
        if (ost->encoding_needed) {
            av_freep(&ost->enc_ctx->stats_in);
        }
        total_packets_written += ost->packets_written;
    }

    /* close each decoder */
    for (i = 0; i < mFFContext->nb_input_streams; i++) {
        ist = mFFContext->input_streams[i];
        if (ist->decoding_needed) {
            avcodec_close(ist->dec_ctx);
        }
    }

    /* finished ! */
    ret = 0;

    fail:

    if (mFFContext->output_streams) {
        for (i = 0; i < mFFContext->nb_output_streams; i++) {
            ost = mFFContext->output_streams[i];
            if (ost) {
                if (ost->logfile) {
                    if (fclose(ost->logfile))
                        J4A_ALOGE(
                                "Error closing logfile, loss of information possible: %s\n",
                                av_err2str(AVERROR(errno)));
                    ost->logfile = NULL;
                }
                av_freep(&ost->forced_kf_pts);
                av_freep(&ost->apad);
                av_freep(&ost->disposition);
                av_dict_free(&ost->encoder_opts);
                av_dict_free(&ost->sws_dict);
                av_dict_free(&ost->swr_opts);
                av_dict_free(&ost->resample_opts);
            }
        }
    }
    return ret;
}

void* ffmpeg_parse(void* arg)
{
    FFContext *mFFContext = (FFContext *) malloc(sizeof(FFContext));
    memset(mFFContext,0, sizeof(FFContext));
    mFFContext->audio_volume      = 256;
    mFFContext->received_sigterm = 0;
    mFFContext->dts_delta_threshold = 10;
    mFFContext->dts_error_threshold = 3600*30;
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


    transcode(mFFContext);

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