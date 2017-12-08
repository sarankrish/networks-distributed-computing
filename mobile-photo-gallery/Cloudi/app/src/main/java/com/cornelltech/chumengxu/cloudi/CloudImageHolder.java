package com.cornelltech.chumengxu.cloudi;

import android.content.Context;
import android.net.Uri;
import android.support.annotation.NonNull;
import android.support.v7.widget.RecyclerView;
import android.view.View;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import com.bumptech.glide.Glide;
import com.firebase.ui.storage.images.FirebaseImageLoader;
import com.google.android.gms.tasks.OnFailureListener;
import com.google.android.gms.tasks.OnSuccessListener;
import com.google.firebase.auth.FirebaseAuth;
import com.google.firebase.storage.FileDownloadTask;
import com.google.firebase.storage.FirebaseStorage;
import com.google.firebase.storage.StorageReference;

import java.io.File;
import java.io.IOException;

public class CloudImageHolder extends RecyclerView.ViewHolder {
    private final LinearLayout mImageContainer;
    private final ImageView mImageField;
    private final TextView mTextField;
    private final FirebaseStorage mStorage;

    public CloudImageHolder(View itemView) {
        super(itemView);
        mImageContainer = itemView.findViewById(R.id.image_container);
        mImageField=itemView.findViewById(R.id.cloud_img);
        mTextField = itemView.findViewById(R.id.description_text);
        mStorage = FirebaseStorage.getInstance();
    }

    public void bind(File cacheDir,CloudImage img,boolean flag){
        setDescription(img.getDescription());
        String imgName = img.getName();
        if(flag){
            imgName = "ascii-"+img.getName();
        }
        if(img.getStatus().equals("public")){
            StorageReference imagePathRef = mStorage.getReference().child("Public/"+imgName);
            final File localFile = new File(cacheDir,img.getName()+".jpg");
            imagePathRef.getFile(localFile).addOnSuccessListener(new OnSuccessListener<FileDownloadTask.TaskSnapshot>() {
                @Override
                public void onSuccess(FileDownloadTask.TaskSnapshot taskSnapshot) {
                    setImage(localFile);
                }
            });
        }else{
            StorageReference imagePathRef = mStorage.getReference().child("Users/"+ FirebaseAuth.getInstance().getCurrentUser().getUid().toString()+"/"+imgName);

            final File localFile = new File(cacheDir,img.getName()+".jpg");
            imagePathRef.getFile(localFile).addOnSuccessListener(new OnSuccessListener<FileDownloadTask.TaskSnapshot>() {
                @Override
                public void onSuccess(FileDownloadTask.TaskSnapshot taskSnapshot) {

                    setImage(localFile);
                }
            });

        }


    }

    private void setImage(File imageFile) {
        mImageField.setImageURI(Uri.fromFile(imageFile));
    }

    private void setDescription(String description) {
        mTextField.setText(description);
    }

}