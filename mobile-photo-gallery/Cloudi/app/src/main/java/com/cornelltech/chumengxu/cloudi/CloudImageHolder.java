package com.cornelltech.chumengxu.cloudi;

import android.support.v7.widget.RecyclerView;
import android.view.View;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

public class CloudImageHolder extends RecyclerView.ViewHolder {
    private final LinearLayout mImageContainer;
    private final ImageView mImageField;
    private final TextView mTextField;

    public CloudImageHolder(View itemView) {
        super(itemView);
        mImageContainer = itemView.findViewById(R.id.image_container);
        mImageField=itemView.findViewById(R.id.cloud_img);
        mTextField = itemView.findViewById(R.id.description_text);
    }

    public void bind(String img) {
        setDescription(img);
        setImage(R.drawable.sample_0);
    }

    private void setImage(int drawable) {
        mImageField.setImageResource(drawable);
    }

    private void setDescription(String description) {
        mTextField.setText(description);
    }

}