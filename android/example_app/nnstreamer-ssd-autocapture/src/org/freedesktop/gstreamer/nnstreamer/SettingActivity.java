package org.freedesktop.gstreamer.nnstreamer;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.view.View;
import android.widget.AdapterView;
import android.widget.Button;
import android.widget.EditText;
import android.widget.Spinner;
import android.widget.TextView;

public class SettingActivity extends Activity implements View.OnClickListener, AdapterView.OnItemSelectedListener {

    private boolean initialized = false;

    private Button buttonAdd;
    private Button buttonReset;
    private Button buttonComplete;
    private Button buttonDeleteList;

    private Spinner objectSpinner;

    private String selectedObject;

    private TextView textViewConditionList;

    private EditText editTextNumber;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_setting);

        initActivity();
    }

    private void initActivity() {
        if (initialized) {
            return;
        }

        buttonAdd = (Button) this.findViewById(R.id.setting_button_add);
        buttonAdd.setOnClickListener(this);

        buttonReset = (Button) this.findViewById(R.id.setting_button_reset);
        buttonReset.setOnClickListener(this);

        buttonComplete = (Button) this.findViewById(R.id.setting_button_settingComplete);
        buttonComplete.setOnClickListener(this);

        buttonDeleteList = (Button) this.findViewById(R.id.setting_button_deleteList);
        buttonDeleteList.setOnClickListener(this);

        objectSpinner = (Spinner) findViewById(R.id.setting_spinner);
        objectSpinner.setOnItemSelectedListener(this);

        textViewConditionList = (TextView) findViewById(R.id.setting_textView_ConditionList);

        editTextNumber = (EditText) findViewById(R.id.setting_editText_Number);

        initialized = true;
    }

    @Override
    public void onClick(View v) {
        final int viewId = v.getId();

        switch (viewId) {
            case R.id.setting_button_add:
                // When the data in selectedObject is exist
                // Add a condition to textView
                if(selectedObject.length() != 0){
                    String tmp = textViewConditionList.getText().toString();
                    String num = editTextNumber.getText().toString();
                    textViewConditionList.setText(tmp + selectedObject + " : " + num + "\n");
                }
                break;
            case R.id.setting_button_reset:
                editTextNumber.setText("");
                break;
            case R.id.setting_button_settingComplete:
                // To send ConditionList to NNStreamerActivity make a Intent and put data
                String data = textViewConditionList.getText().toString();
                Intent intent = new Intent();
                intent.putExtra("conditionList",data);
                setResult(RESULT_OK,intent);
                finish();
                break;
            case R.id.setting_button_deleteList:
                textViewConditionList.setText("");
                break;
            default:
                break;
        }
    }

    @Override
    public void onItemSelected(AdapterView<?> adapterView, View view, int i, long l) {
        //adapterView.getItemAtPosition(i)
        selectedObject = adapterView.getItemAtPosition(i).toString();
    }

    @Override
    public void onNothingSelected(AdapterView<?> adapterView) {

    }
}

