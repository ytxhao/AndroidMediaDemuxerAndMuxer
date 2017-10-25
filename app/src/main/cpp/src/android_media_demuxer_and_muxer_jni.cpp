#include <jni.h>
#include <string>
#include <ican_ytx_com_MediaUtils.h>
#include "muxer_common_new.h"




JNIEXPORT jint JNICALL Java_ican_ytx_com_andoridmediademuxerandmuxer_MediaUtils_demuxer
        (JNIEnv *env, jobject obj, jstring inputFile,jstring outputVideoFile,jstring outputAudioFile)
{
    return 0;
}



void log_callback(void* ptr, int level, const char* fmt,va_list vl)
{
    J4A_VLOGD(fmt,vl);
}

//void *test(void *a){
//
//}

JNIEXPORT jint JNICALL Java_ican_ytx_com_andoridmediademuxerandmuxer_MediaUtils_muxer
        (JNIEnv *env, jobject obj, jstring inputVideoFile, jstring inputAudioFile, jstring outputMediaFile)
{
    const char *in_filename_v = env->GetStringUTFChars(inputVideoFile, NULL);
    const char *in_filename_a = env->GetStringUTFChars(inputAudioFile, NULL);
    const char *out_filename = env->GetStringUTFChars(outputMediaFile, NULL);//Output file URL
    hls2mp4_context_t *context;
    context = (hls2mp4_context_t *) malloc(sizeof(hls2mp4_context_t));
    if(!context){
        J4A_ALOGE( "Out of Memory\n");
        return -1;
    }
    memset(context, 0, sizeof(hls2mp4_context_t));
    context->in_filename_a = (char *) malloc(1024);
    context->in_filename_v = (char *) malloc(1024);
    context->out_filename = (char *) malloc(1024);
    memset(context->in_filename_a, 0, 1024);
    memset(context->in_filename_v, 0, 1024);
    memset(context->out_filename, 0, 1024);


    sprintf(context->in_filename_a, "%s", in_filename_a);
    sprintf(context->in_filename_v, "%s", in_filename_v);
    sprintf(context->out_filename, "%s", out_filename);

    int ret;

    av_log_set_callback(log_callback);
    pthread_t thread_id;
    ret = pthread_create(&thread_id, NULL, ffmpeg_parse, (void*)context);
    if(ret != 0){
        J4A_ALOGE( "create thead error\n");
        return -1;
    }



    //ffmpeg_parse(in_filename_a,in_filename_v,out_filename);


    J4A_ALOGD( "muxer finish.\n");
    env->ReleaseStringUTFChars(inputVideoFile, in_filename_v);
    env->ReleaseStringUTFChars(inputAudioFile, in_filename_a);
    env->ReleaseStringUTFChars(outputMediaFile, out_filename);
    return 0;
}


