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


JNIEXPORT jint JNICALL Java_ican_ytx_com_andoridmediademuxerandmuxer_MediaUtils_muxer
        (JNIEnv *env, jobject obj, jstring inputVideoFile, jstring inputAudioFile, jstring outputMediaFile)
{
    const char *in_filename_v = env->GetStringUTFChars(inputVideoFile, NULL);
    const char *in_filename_a = env->GetStringUTFChars(inputAudioFile, NULL);
    const char *out_filename = env->GetStringUTFChars(outputMediaFile, NULL);//Output file URL
    J4A_ALOGD("avcodec_configuration=%s",avcodec_configuration());
    av_log_set_callback(log_callback);
    avcodec_register_all();
    avfilter_register_all();
    av_register_all();
    avformat_network_init();


    ffmpeg_parse(in_filename_a,in_filename_v,out_filename);


    J4A_ALOGD( "muxer finish.\n");
    env->ReleaseStringUTFChars(inputVideoFile, in_filename_v);
    env->ReleaseStringUTFChars(inputAudioFile, in_filename_a);
    env->ReleaseStringUTFChars(outputMediaFile, out_filename);
    return 0;
}
