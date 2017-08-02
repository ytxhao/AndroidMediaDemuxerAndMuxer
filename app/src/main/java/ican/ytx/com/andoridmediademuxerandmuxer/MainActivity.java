package ican.ytx.com.andoridmediademuxerandmuxer;

import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.view.View;
import android.widget.Button;

public class MainActivity extends AppCompatActivity implements View.OnClickListener {

    Button btDemuxer;
    Button btMuxer;
    MediaUtils mMediaUtils;
    String INPUT_DEMUXER_FILE = "/storage/emulated/0/cuc_ieschool.ts";
    String OUTPUT_DEMUXER_VEDIO_FILE = "/storage/emulated/0/cuc_ieschool.h264";
    String OUTPUT_DEMUXER_AUDIO_FILE = "/storage/emulated/0/cuc_ieschool.aac";



    //String INPUT_MUXER_VEDIO_FILE = "/storage/emulated/0/cuc_ieschool_640x360.h265";
    String INPUT_MUXER_VEDIO_FILE = OUTPUT_DEMUXER_VEDIO_FILE;
    //String INPUT_MUXER_AUDIO_FILE = "/storage/emulated/0/huoyuanjia.mp3";
    String INPUT_MUXER_AUDIO_FILE = OUTPUT_DEMUXER_AUDIO_FILE;
    String OUTPUT_MUXER_MEDIA_FILE = "/storage/emulated/0/cuc_ieschool.mp4";



    static {
        System.loadLibrary("native-lib");
    }
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        btDemuxer = (Button) findViewById(R.id.btDemuxer);
        btMuxer = (Button) findViewById(R.id.btMuxer);


        btDemuxer.setOnClickListener(this);
        btMuxer.setOnClickListener(this);

        mMediaUtils = new MediaUtils();
    }

    @Override
    public void onClick(View v) {

        int id = v.getId();

        switch (id){
            case R.id.btDemuxer:

                new Thread(new Runnable() {
                    @Override
                    public void run() {
                        mMediaUtils.demuxer(INPUT_DEMUXER_FILE,OUTPUT_DEMUXER_VEDIO_FILE,OUTPUT_DEMUXER_AUDIO_FILE);
                    }
                }).start();
                break;
            case R.id.btMuxer:

                new Thread(new Runnable() {
                    @Override
                    public void run() {
                        mMediaUtils.muxer(INPUT_MUXER_VEDIO_FILE,INPUT_MUXER_AUDIO_FILE,OUTPUT_MUXER_MEDIA_FILE);
                    }
                }).start();
                break;
        }

    }
}


