package ican.ytx.com.andoridmediademuxerandmuxer;

/**
 * Created by Administrator on 2017/7/31.
 */

public class MediaUtils {
    public native  int demuxer(String inputFile,String outputFileVideo,String outputFileAudio);
    public native  int muxer(String inputFileVideo,String inputFileAudio,String outputFileMedia);
}
