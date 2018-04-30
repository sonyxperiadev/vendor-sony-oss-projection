/*
 * Copyright (C) 2018 AngeloGioacchino Del Regno <kholk11@gmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package sonyxperiadev.projectorsettings;

import android.annotation.TargetApi;
import android.app.Activity;
import android.content.pm.ActivityInfo;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Point;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.TransitionDrawable;
import android.os.Build;
import android.os.Bundle;
import android.view.Display;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.SeekBar;
import android.widget.TextView;
import android.view.View;

import android.util.Log;

import org.w3c.dom.Text;

public class MainActivity extends Activity {

    // Used to load the 'ucomm_wrapper_jni' library on application startup.
    static {
        System.loadLibrary("projectorsettings_jni");
    }

    static final int TAG_FOCUS = 1;
    static final int TAG_KEYSTONE = 2;
/*
    static final int FOCUS_VALUE_MIN = -266;
    static final int FOCUS_VALUE_MAX = 266;
    static final int FOCUS_SHIFT_VAL = 117; // subtract 117 to send to UCOMM
*/
    static final int FOCUS_VALUE_MIN = 0;
    static final int FOCUS_VALUE_MAX = 532;
    static final int FOCUS_SHIFT_TXT = -266;
    static final int FOCUS_SHIFT_VAL = FOCUS_SHIFT_TXT - 117; /* subtract 117 to send to UCOMM */

    static final int KEYSTONE_VALUE_MAX = 344;
    static final int KEYSTONE_VALUE_MIN = 0;
    static final int KEYSTONE_SHIFT_VAL = -172;

    static int ADJ_VALUE_MIN = 0;
    static int ADJ_VALUE_MAX = 0;
    static int ADJ_SHIFT_VAL = 0;
    static int ADJ_SHIFT_TXT = 0;

    ImageView imgv;
    Bitmap bmap, bmapGrid;
    TransitionDrawable imgTransitions;
    LinearLayout mainView;
    LinearLayout adjLayout;
    Boolean showing_grid = false;
    Boolean send_enabled = false;
    Boolean set_focus = false;
    Boolean set_keystone = false;
    SeekBar seekBarAdj;

    @Override
    protected void onStart() {
        super.onStart();
        setVisible(true);
    }

    @TargetApi(Build.VERSION_CODES.O)
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        getWindow().getDecorView().setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION |
                        View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN |
                        View.SYSTEM_UI_FLAG_LAYOUT_STABLE |
                        View.SYSTEM_UI_FLAG_IMMERSIVE |
                        View.SYSTEM_UI_FLAG_HIDE_NAVIGATION |
                        View.SYSTEM_UI_FLAG_FULLSCREEN);

        /* Set keep screen on to provide a better manual focus experience */
        getWindow().getDecorView().setKeepScreenOn(true);

        /* "Implicitly" suggest to the user to focus in landscape mode */
        setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);

        /* Load the main "Adjust" layout */
        adjLayout = findViewById(R.id.adjLayout);

        Point dispSz = new Point();
        Display disp = getWindowManager().getDefaultDisplay();
        disp.getSize(dispSz);

        /* stroke: 1% of display width */
        int strokeWidth = (dispSz.x / 100) * 1;

        imgv = findViewById(R.id.bgimg);
        bmap = Bitmap.createBitmap(dispSz.x, dispSz.y, Bitmap.Config.ARGB_8888);
        bmapGrid = Bitmap.createBitmap(dispSz.x, dispSz.y, Bitmap.Config.ARGB_8888);
        Canvas can = new Canvas(bmap);
        Canvas gridCan = new Canvas(bmapGrid);
        gridCan.drawColor(Color.BLACK);
        can.drawColor(Color.BLACK);

        Paint backgroundGrid = new Paint();
        backgroundGrid.setStyle(Paint.Style.STROKE);
        backgroundGrid.setColor(Color.WHITE);
        backgroundGrid.setStrokeWidth(strokeWidth);


        int dpi = getResources().getDisplayMetrics().densityDpi;
        int squaresize = dpi/2; /* half-inch */
        int num_x = dispSz.x / squaresize;
        int num_y = dispSz.y / squaresize;
        int square_dist_x = dispSz.x / num_x;
        int square_dist_y = dispSz.y / num_y;

        int xstart, ystart;

        /* Then draw squares inside */
        for (int i = 0; i <= num_x; i++) {
            xstart = square_dist_x * i;
            gridCan.drawLine(xstart, 0, xstart, dispSz.y, backgroundGrid);
        }

        for (int i = 0; i <= num_y; i++) {
            ystart = square_dist_y * i;
            gridCan.drawLine(0, ystart, dispSz.x, ystart, backgroundGrid);
        }

        /* Draw a square around the screen */
        backgroundGrid.setColor(Color.RED);
        can.drawLine(0, 0, 0, dispSz.y, backgroundGrid);
        can.drawLine(0, 0, dispSz.x, 0, backgroundGrid);
        can.drawLine(dispSz.x, 0, dispSz.x, dispSz.y, backgroundGrid);
        can.drawLine(0, dispSz.y, dispSz.x, dispSz.y, backgroundGrid);

        /* Bitmaps done. Assign transition */
        Drawable[] bitmapLayers = new Drawable[2];
        bitmapLayers[0] = new BitmapDrawable(getResources(), bmap);
        bitmapLayers[1] = new BitmapDrawable(getResources(), bmapGrid);

        imgTransitions = new TransitionDrawable(bitmapLayers);
        imgv.setImageDrawable(imgTransitions);
        imgv.setScaleType(ImageView.ScaleType.FIT_XY);
        //imgv.setImageBitmap(bmap);
        //imgv.setImageBitmap(bmapGrid);

        mainView = findViewById(R.id.mainView);

        ViewGroup.MarginLayoutParams layoutParams =
                (ViewGroup.MarginLayoutParams) adjLayout.getLayoutParams();

        layoutParams.leftMargin = square_dist_x;
        layoutParams.rightMargin = square_dist_x;
        layoutParams.topMargin = square_dist_y;
        layoutParams.bottomMargin = square_dist_y;

     //   mainView.setBackgroundColor(Color.TRANSPARENT);

      //  mainView.setLayoutParams(layoutParams);
        adjLayout.setLayoutParams(layoutParams);


        seekBarAdj = (SeekBar)findViewById(R.id.seekBar);
        seekBarAdj.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            int current_value = 0;

            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                current_value = progress;
                int addval = current_value + ADJ_SHIFT_VAL;
                if (!send_enabled)
                    return;

                Log.e("ProjectorSettings", "Sending ADJ value " + addval);
                if (set_focus)
                    ucommsvrSetFocus(current_value + ADJ_SHIFT_VAL);
                else if (set_keystone)
                    ucommsvrSetKeystone(current_value + ADJ_SHIFT_VAL);
            }

            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {

            }

            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {
                TextView adjText = (TextView)findViewById(R.id.stepText);
                adjText.setText(Integer.toString(current_value + ADJ_SHIFT_TXT));
            }
        });

        // Example of a call to a native method
        //TextView tv = (TextView) findViewById(R.id.sample_text);
        //tv.setText(stringFromJNI());
    }



    /**
     * A native method that is implemented by the 'ucomm_wrapper_jni' native library,
     * which is packaged with this application.
     */
    public native int ucommsvrSetFocus(int focus);
    public native int ucommsvrSetKeystone(int ksval);
    public native int ucommsvrGetFocus();
    public native int ucommsvrGetKeystone();

    public void __refreshBtnColor(Button btn) {
        if (showing_grid) {
            btn.setBackground(getDrawable(R.drawable.button_style_blue));
            btn.setTextColor(Color.parseColor("#5e97f6"));
        } else {
            btn.setBackground(getDrawable(R.drawable.button_style));
            btn.setTextColor(Color.parseColor("#FFFFFF"));
        }
    }

    public void __refreshImgBtnColor(ImageButton btn) {
        if (showing_grid) {
            btn.setBackground(getDrawable(R.drawable.button_style_blue));
            btn.setColorFilter(Color.parseColor("#5e97f6"));

        } else {
            btn.setBackground(getDrawable(R.drawable.button_style));
            btn.setColorFilter(Color.parseColor("#FFFFFF"));
        }
    }

    public void __refreshTextColor(TextView tView) {
        if (showing_grid)
            tView.setTextColor(Color.parseColor("#5e97f6"));
        else
            tView.setTextColor(Color.parseColor("#FFFFFF"));
    }

    public void refreshColors() {
        __refreshBtnColor((Button)findViewById(R.id.m25btn));
        __refreshBtnColor((Button)findViewById(R.id.m10btn));
        __refreshBtnColor((Button)findViewById(R.id.m1btn));
        __refreshBtnColor((Button)findViewById(R.id.p1btn));
        __refreshBtnColor((Button)findViewById(R.id.p10btn));
        __refreshBtnColor((Button)findViewById(R.id.p25btn));
        __refreshImgBtnColor((ImageButton)findViewById(R.id.btnBack));
        __refreshImgBtnColor((ImageButton)findViewById(R.id.btnGrid));
        __refreshTextColor((TextView)findViewById(R.id.stepText));
        __refreshTextColor((TextView)findViewById(R.id.settingTypeTxt));
    }

    public void onClick_btnGrid(View view) {
        ImageButton btn = (ImageButton) view;
        Drawable tempDraw;

        if (!showing_grid) {
            tempDraw = getResources().getDrawable(R.drawable.square_button);
            imgTransitions.startTransition(150);
        } else {
            tempDraw = getResources().getDrawable(R.drawable.grid_button);
            imgTransitions.reverseTransition(150);
        }

        btn.setImageDrawable(tempDraw);

        showing_grid = !showing_grid;

        refreshColors();
    }

    public void onClick_btnFocus(View view) {
        int focusval = ucommsvrGetFocus();
        send_enabled = false;
        adjLayout.setTag(TAG_FOCUS);
        mainView.setVisibility(View.INVISIBLE);

        ADJ_VALUE_MIN = FOCUS_VALUE_MIN;
        ADJ_VALUE_MAX = FOCUS_VALUE_MAX;
        ADJ_SHIFT_VAL = FOCUS_SHIFT_VAL;
        ADJ_SHIFT_TXT = FOCUS_SHIFT_TXT;

        Log.e("ProjectorSettings", "Got focus val from JNI: " + focusval);

        //seekBarAdj.setProgress(ADJ_VALUE_MAX/2);
        seekBarAdj.setProgress(focusval + (-ADJ_SHIFT_TXT));
        seekBarAdj.setMin(ADJ_VALUE_MIN);
        seekBarAdj.setMax(ADJ_VALUE_MAX);

        TextView adjText = (TextView)findViewById(R.id.stepText);
        adjText.setText(Integer.toString(focusval)); // + ADJ_SHIFT_TXT));

        TextView settingTypeText = (TextView)findViewById(R.id.settingTypeTxt);
        settingTypeText.setText(R.string.focusSettings);

        set_focus = true;
        set_keystone = false;

        adjLayout.setVisibility(View.VISIBLE);
        send_enabled = true;
    }

    public void onClick_btnKeystone(View view) {
        int ksval = 0;
        send_enabled = false;

        adjLayout.setTag(TAG_KEYSTONE);
        mainView.setVisibility(View.INVISIBLE);

        ADJ_VALUE_MIN = KEYSTONE_VALUE_MIN;
        ADJ_VALUE_MAX = KEYSTONE_VALUE_MAX;
        ADJ_SHIFT_VAL = KEYSTONE_SHIFT_VAL;
        ADJ_SHIFT_TXT = KEYSTONE_SHIFT_VAL;

        seekBarAdj.setProgress(ADJ_VALUE_MAX/2);
        seekBarAdj.setMin(ADJ_VALUE_MIN);
        seekBarAdj.setMax(ADJ_VALUE_MAX);

        TextView adjText = (TextView)findViewById(R.id.stepText);
        adjText.setText(Integer.toString(ksval + ADJ_SHIFT_TXT));

        TextView settingTypeText = (TextView)findViewById(R.id.settingTypeTxt);
        settingTypeText.setText(R.string.keystoneSettings);

        set_focus = false;
        set_keystone = true;

        adjLayout.setVisibility(View.VISIBLE);

        send_enabled = true;
    }

    public void onClick_btnBack(View view) {
        if (showing_grid) {
            ImageButton btnGrid = findViewById(R.id.btnGrid);
            onClick_btnGrid(btnGrid);
        }
        send_enabled = false;
        adjLayout.setVisibility(View.INVISIBLE);
        mainView.setVisibility(View.VISIBLE);
    }

    public void onClick_btnAdjust(View view) {
        Button btn = (Button) view;
        int addVal = Integer.valueOf((String)btn.getTag());
        int curSeek = seekBarAdj.getProgress();

        seekBarAdj.setProgress(curSeek + addVal);
        TextView adjText = (TextView)findViewById(R.id.stepText);
        adjText.setText(Integer.toString(curSeek + addVal + ADJ_SHIFT_TXT));
    }
}
