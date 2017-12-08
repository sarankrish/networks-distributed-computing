package com.cornelltech.chumengxu.cloudi;

import android.app.ProgressDialog;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.support.v7.app.AppCompatActivity;
import android.text.TextUtils;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.Toast;

import com.google.android.gms.tasks.OnSuccessListener;
import com.google.firebase.database.DatabaseReference;
import com.google.firebase.database.FirebaseDatabase;
import com.google.firebase.storage.FirebaseStorage;
import com.google.firebase.storage.StorageReference;
import com.google.firebase.storage.UploadTask;

public class UploadActivity extends AppCompatActivity {
    private StorageReference mStorageRef;
    private DatabaseReference mDataBaseRef;
    private Uri selectedImage;
    private ProgressDialog mProgressDialog;
    private String uid;

    public void pickImage(View view){
        Intent pickImage = new Intent(Intent.ACTION_PICK,
                android.provider.MediaStore.Images.Media.EXTERNAL_CONTENT_URI);
        startActivityForResult(pickImage , 1);//one can be replaced with any action code
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_upload);
        mStorageRef = FirebaseStorage.getInstance().getReference();
        mDataBaseRef = FirebaseDatabase.getInstance().getReference();
        mProgressDialog = new ProgressDialog(this);
        Intent intent = getIntent();
        uid = intent.getStringExtra(MainActivity.EXTRA_MESSAGE);
        if (TextUtils.isEmpty(uid)){
            CheckBox checkbox=findViewById(R.id.PrivacyCheckBox);
            checkbox.setEnabled(false);
            Button uploadButton=findViewById(R.id.upload_button);
            uploadButton.setEnabled(false);
        }
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent imageReturnedIntent) {
        super.onActivityResult(requestCode, resultCode, imageReturnedIntent);
        selectedImage = imageReturnedIntent.getData();
        EditText imagePathText = (EditText) findViewById(R.id.imagePathView);
        imagePathText.setText(selectedImage.toString());
    }

    public void uploadImage(View view){
        EditText imagePathText = (EditText) findViewById(R.id.imagePathView);
        EditText descriptionText=(EditText) findViewById(R.id.imageDescriptionView);
        String description=descriptionText.getText().toString();

        Log.d("Success","Upload attempt");
        mProgressDialog.setMessage("Uploading ....");
        mProgressDialog.show();
        StorageReference filepath=mStorageRef;
        CheckBox checkbox=(CheckBox)findViewById(R.id.PrivacyCheckBox);
        if (!TextUtils.isEmpty(uid)){
            if (!checkbox.isChecked()) {
                filepath = mStorageRef.child("Public").child(selectedImage.getLastPathSegment());
                mDataBaseRef.child("public").child(selectedImage.getLastPathSegment()).child("name").setValue(selectedImage.getLastPathSegment());
                mDataBaseRef.child("public").child(selectedImage.getLastPathSegment()).child("description").setValue(description);
                mDataBaseRef.child("public").child(selectedImage.getLastPathSegment()).child("status").setValue("public");

            }
            else{
                filepath = mStorageRef.child("Users/"+uid).child(selectedImage.getLastPathSegment());
                mDataBaseRef.child("users").child(uid).child(selectedImage.getLastPathSegment()).child("name").setValue(selectedImage.getLastPathSegment());
                mDataBaseRef.child("users").child(uid).child(selectedImage.getLastPathSegment()).child("description").setValue(description);
                mDataBaseRef.child("users").child(uid).child(selectedImage.getLastPathSegment()).child("status").setValue("private");
            }
        }
        else{
            filepath = mStorageRef.child("Public").child(selectedImage.getLastPathSegment());
            mDataBaseRef.child("public").child(selectedImage.getLastPathSegment()).child("name").setValue(selectedImage.getLastPathSegment());
            mDataBaseRef.child("public").child(selectedImage.getLastPathSegment()).child("description").setValue(description);
            mDataBaseRef.child("public").child(selectedImage.getLastPathSegment()).child("status").setValue("public");
        }

        filepath.putFile(selectedImage).addOnSuccessListener(new OnSuccessListener<UploadTask.TaskSnapshot>() {
            @Override
            public void onSuccess(UploadTask.TaskSnapshot taskSnapshot) {
                Log.d("Success","Upload done");
                Toast.makeText(UploadActivity.this,"Upload done.",Toast.LENGTH_LONG).show();
                mProgressDialog.dismiss();
                finish();
                return;
            }
        });
    }

}
