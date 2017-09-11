package ican.ytx.com.andoridmediademuxerandmuxer;

import android.Manifest;
import android.app.Activity;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.provider.Settings;
import android.support.annotation.NonNull;
import android.support.v4.app.ActivityCompat;
import android.support.v4.content.ContextCompat;
import android.support.v7.app.AlertDialog;
import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.webkit.PermissionRequest;
import android.widget.Button;
import android.widget.Toast;

public class MainActivity extends AppCompatActivity implements View.OnClickListener {

    private static final String TAG = "MainActivity";

//    public static final int PERMISSIONS_GRANTED = 0; // 权限授权
//    public static final int PERMISSIONS_DENIED = 1; // 权限拒绝
//
//    private static final int PERMISSION_REQUEST_CODE = 0; // 系统权限管理页面的参数
////    private static final String EXTRA_PERMISSIONS =
////            "me.chunyu.clwang.permission.extra_permission"; // 权限参数
//    private static final String PACKAGE_URL_SCHEME = "package:"; // 方案
//
//    private PermissionsChecker mChecker; // 权限检测器
//    private boolean isRequireCheck; // 是否需要系统权限检测, 防止和系统提示框重叠

    private static final int REQUEST_EXTERNAL_STORAGE = 1;
    private static String[] PERMISSIONS_STORAGE = {
            "android.permission.READ_EXTERNAL_STORAGE",
            "android.permission.WRITE_EXTERNAL_STORAGE" };

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
        verifyStoragePermissions(this);

//        mChecker = new PermissionsChecker(this);
//        isRequireCheck = true;

        btDemuxer = (Button) findViewById(R.id.btDemuxer);
        btMuxer = (Button) findViewById(R.id.btMuxer);


        btDemuxer.setOnClickListener(this);
        btMuxer.setOnClickListener(this);

        mMediaUtils = new MediaUtils();
    }

    // 返回传递的权限参数
    private String[] getPermissions() {
        return PERMISSIONS_STORAGE;
        //return getIntent().getStringArrayExtra(EXTRA_PERMISSIONS);
    }

    // 请求权限兼容低版本
//    private void requestPermissions(String... permissions) {
//        ActivityCompat.requestPermissions(this, permissions, PERMISSION_REQUEST_CODE);
//    }

    // 全部权限均已获取
//    private void allPermissionsGranted() {
//        setResult(PERMISSIONS_GRANTED);
//        finish();
//    }

    @Override
    protected void onResume() {
        super.onResume();
//        if (isRequireCheck) {
//            String[] permissions = getPermissions();
//            if (mChecker.lacksPermissions(permissions)) {
//                ActivityCompat.requestPermissions(this, PERMISSIONS_STORAGE,REQUEST_EXTERNAL_STORAGE); // 请求权限
//            } else {
//                allPermissionsGranted(); // 全部权限都已获取
//            }
//        } else {
//            isRequireCheck = true;
//        }
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



    public static void verifyStoragePermissions(Activity activity) {

        try {
            //检测是否有写的权限
//            boolean hasPermission = (ContextCompat.checkSelfPermission(activity,
//                    Manifest.permission.WRITE_EXTERNAL_STORAGE) == PackageManager.PERMISSION_GRANTED);
            int permission = ActivityCompat.checkSelfPermission(activity,
                    "android.permission.WRITE_EXTERNAL_STORAGE");
            if (permission != PackageManager.PERMISSION_GRANTED) {
                // 没有写的权限，去申请写的权限，会弹出对话框
                ActivityCompat.requestPermissions(activity, PERMISSIONS_STORAGE,REQUEST_EXTERNAL_STORAGE);
            }
        } catch (Exception e) {
            e.printStackTrace();
        }
    }


    @Override
    public void onRequestPermissionsResult(int requestCode, @NonNull String[] permissions, @NonNull int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);

        switch (requestCode){
            case REQUEST_EXTERNAL_STORAGE:
                if (grantResults.length > 0 && grantResults[0] == PackageManager.PERMISSION_GRANTED)
                {
                    //已授权
                    Log.i(TAG, "Permission has been granted by user");
//                    isRequireCheck = true;
//                    allPermissionsGranted();
                }else
                {
                    Log.i(TAG, "Permission has been denied by user");
                    //拒绝授权
                    //finish();
                    //verifyStoragePermissions(this);
                    //showMissingPermissionDialog();
                    //isRequireCheck = false;
                    //showMissingPermissionDialog();
                }
                break;
        }

    }

    /*
    // 含有全部的权限
    private boolean hasAllPermissionsGranted(@NonNull int[] grantResults) {
        for (int grantResult : grantResults) {
            if (grantResult == PackageManager.PERMISSION_DENIED) {
                return false;
            }
        }
        return true;
    }

    // 显示缺失权限提示
    private void showMissingPermissionDialog() {
        AlertDialog.Builder builder = new AlertDialog.Builder(this);
        builder.setTitle("help");
        builder.setMessage("help_text");

        // 拒绝, 退出应用
        builder.setNegativeButton("继续", new DialogInterface.OnClickListener() {
            @Override
            public void onClick(DialogInterface dialog, int which) {
                setResult(PERMISSIONS_DENIED);
                finish();
            }
        });

        builder.setPositiveButton("设置", new DialogInterface.OnClickListener() {
            @Override
            public void onClick(DialogInterface dialog, int which) {
                startAppSettings();
            }
        });

        builder.setCancelable(false);

        builder.show();
    }

    // 启动应用的设置
    private void startAppSettings() {
        Intent intent = new Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS);
        intent.setData(Uri.parse(PACKAGE_URL_SCHEME + getPackageName()));
        startActivity(intent);
    }
    */
}


