package ican.ytx.com.andoridmediademuxerandmuxer;


import android.content.Intent;
import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.view.View;
import android.widget.Button;
import android.widget.Toast;

public class MainActivity extends AppCompatActivity implements View.OnClickListener {

    private static final String TAG = "MainActivity";


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


    private static final int REQUEST_CODE = 0; // 请求码
    // 所需的全部权限
    static final String[] PERMISSIONS = new String[]{
            android.Manifest.permission.READ_EXTERNAL_STORAGE,
            android.Manifest.permission.WRITE_EXTERNAL_STORAGE,
//            android.Manifest.permission.ACCESS_FINE_LOCATION,
//            android.Manifest.permission.CALL_PHONE,
//            android.Manifest.permission.CAMERA,
//            android.Manifest.permission.SEND_SMS,
//            android.Manifest.permission.GET_ACCOUNTS,
//            android.Manifest.permission.RECORD_AUDIO,
    };
    private PermissionsChecker mPermissionsChecker; // 权限检测器

    static {
        System.loadLibrary("native-lib");
    }
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        mPermissionsChecker = new PermissionsChecker(this);
        // 缺少权限时, 进入权限配置页面
        if (mPermissionsChecker.lacksPermissions(PERMISSIONS)) {
            PermissionsActivity.startActivityForResult(this, REQUEST_CODE, PERMISSIONS);
        }

        btDemuxer = (Button) findViewById(R.id.btDemuxer);
        btMuxer = (Button) findViewById(R.id.btMuxer);


        btDemuxer.setOnClickListener(this);
        btMuxer.setOnClickListener(this);

        mMediaUtils = new MediaUtils();
    }


    @Override
    protected void onResume() {
        super.onResume();

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

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode == REQUEST_CODE && resultCode == PermissionsActivity.PERMISSIONS_DENIED) {
            finish();
            Toast.makeText(getApplicationContext(),"拒绝权限将导致部分功能无法使用",Toast.LENGTH_SHORT).show();
        }
    }
}


