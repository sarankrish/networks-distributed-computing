package com.cornelltech.chumengxu.cloudi;

import android.app.ProgressDialog;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.support.v7.app.AppCompatActivity;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.Toast;

import com.google.android.gms.tasks.OnSuccessListener;
import com.google.firebase.storage.FirebaseStorage;
import com.google.firebase.storage.StorageReference;
import com.google.firebase.storage.UploadTask;

public class UploadActivity extends AppCompatActivity {

    private StorageReference mStorageRef;

    private Button mSelectImage;

    private ProgressDialog mProgressDialog;

    private static final int GALLERY_INTENT = 2;

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
        mSelectImage = (Button) findViewById(R.id.upload);
        mProgressDialog = new ProgressDialog(this);
        mSelectImage.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                Intent intent = new Intent(Intent.ACTION_PICK);
                intent.setType("image/*");
                startActivityForResult(intent,GALLERY_INTENT);
            }
        });
    }
    
    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent imageReturnedIntent) {
        super.onActivityResult(requestCode, resultCode, imageReturnedIntent);
        Uri selectedImage = imageReturnedIntent.getData();
        EditText imagePathText = (EditText) findViewById(R.id.imagePathView);
        imagePathText.setText(selectedImage.toString());

        if(requestCode == GALLERY_INTENT && resultCode == RESULT_OK){
            Log.d("Success","Upload attempt");
            mProgressDialog.setMessage("Uploading ....");
            mProgressDialog.show();
            StorageReference filepath = mStorageRef.child("Public").child(selectedImage.getLastPathSegment());

            filepath.putFile(selectedImage).addOnSuccessListener(new OnSuccessListener<UploadTask.TaskSnapshot>() {
                @Override
                public void onSuccess(UploadTask.TaskSnapshot taskSnapshot) {
                    Log.d("Success","Upload done");
                    Toast.makeText(UploadActivity.this,"Upload done.",Toast.LENGTH_LONG).show();
                    mProgressDialog.dismiss();
                }
            });
        }
    }

}
