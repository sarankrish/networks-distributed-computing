package com.cornelltech.chumengxu.cloudi;

import java.io.File;

/**
 * Created by Sabri on 12/6/2017.
 */

public class CloudImage {
    public String getName() {
        return mName;
    }

    public void setName(String mName) {
        this.mName = mName;
    }

    public String getDescription() {
        return mDescription;
    }

    public void setDescription(String mDescription) {
        this.mDescription = mDescription;
    }

    private String mName;
    private String mDescription;

    public String getStatus() {
        return mStatus;
    }

    public void setStatus(String mStatus) {
        this.mStatus = mStatus;
    }

    private String mStatus;

    public CloudImage(){}

    public CloudImage(String mName  ,String mDescription){
        mName = mName;
        mDescription=mDescription;
    }


}
