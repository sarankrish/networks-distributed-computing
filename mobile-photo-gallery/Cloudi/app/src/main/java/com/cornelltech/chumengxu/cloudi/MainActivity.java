package com.cornelltech.chumengxu.cloudi;

import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.support.annotation.NonNull;
import android.support.v7.app.AppCompatActivity;
import android.support.v7.widget.LinearLayoutManager;
import android.support.v7.widget.RecyclerView;
import android.support.v7.widget.Toolbar;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.view.inputmethod.InputMethodManager;
import android.widget.CheckBox;
import android.widget.EditText;

import com.firebase.ui.auth.AuthUI;
import com.firebase.ui.auth.IdpResponse;
import com.firebase.ui.auth.ResultCodes;
import com.firebase.ui.database.FirebaseRecyclerAdapter;
import com.firebase.ui.database.FirebaseRecyclerOptions;
import com.google.android.gms.tasks.OnCompleteListener;
import com.google.android.gms.tasks.Task;
import com.google.firebase.auth.FirebaseAuth;
import com.google.firebase.auth.FirebaseUser;
import com.google.firebase.database.FirebaseDatabase;
import com.google.firebase.database.Query;
import com.google.firebase.storage.FirebaseStorage;

import java.util.Arrays;
import java.util.List;


public class MainActivity extends AppCompatActivity {
    public static final String EXTRA_MESSAGE = "com.cornelltech.chumengxu.MESSAGE";
    private static final int RC_SIGN_IN = 123;
    private Menu menu;
    //public Firebase storage bucket
    private static FirebaseDatabase mDatabase;
    private static FirebaseStorage mStorage;
    private RecyclerView mRecyclerViewPrivate;
    private RecyclerView mRecyclerViewPublic;
    private Query query_public;
    private Query query_private;
    private boolean isPreProcessed = false;

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        // Inflate the menu; this adds items to the action bar if it is present.
        getMenuInflater().inflate(R.menu.action_menu, menu);
        this.menu=menu;
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        switch (item.getItemId()) {
            case R.id.sign_in:
                if (FirebaseAuth.getInstance().getCurrentUser()==null) {
                    // Choose authentication providers
                    List<AuthUI.IdpConfig> providers = Arrays.asList(
                            new AuthUI.IdpConfig.Builder(AuthUI.EMAIL_PROVIDER).build(),
                            new AuthUI.IdpConfig.Builder(AuthUI.GOOGLE_PROVIDER).build());

                    // Create and launch sign-in intent
                    startActivityForResult(
                            AuthUI.getInstance()
                                    .createSignInIntentBuilder()
                                    .setAvailableProviders(providers)
                                    .build(),
                            RC_SIGN_IN);
                    return true;
                }
                else{
                    AuthUI.getInstance()
                            .signOut(this)
                            .addOnCompleteListener(new OnCompleteListener<Void>() {
                                public void onComplete(@NonNull Task<Void> task) {
                                    MenuItem signinMenuItem=menu.findItem(R.id.sign_in);
                                    signinMenuItem.setTitle("Sign in");
                                    initRecyclerView();
                                }
                            });
                    return true;
                }

            case R.id.upload:
                Intent intent2=new Intent(this,UploadActivity.class);
                intent2.putExtra(EXTRA_MESSAGE, FirebaseAuth.getInstance().getUid());
                startActivityForResult(intent2,0);
                return true;

            default:
                // If we got here, the user's action was not recognized.
                // Invoke the superclass to handle it.
                return super.onOptionsItemSelected(item);

        }
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);

        if (requestCode == RC_SIGN_IN) {
            IdpResponse response = IdpResponse.fromResultIntent(data);

            if (resultCode == ResultCodes.OK) {
                // Successfully signed in
                FirebaseUser user = FirebaseAuth.getInstance().getCurrentUser();
                MenuItem signinMenuItem=menu.findItem(R.id.sign_in);
                signinMenuItem.setTitle("Sign out");
                initRecyclerView();
                // ...
            } else {
                // Sign in failed, check response for error code
                // ...
            }
        }
    }


    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        Toolbar myToolbar = (Toolbar) findViewById(R.id.my_toolbar);
        setSupportActionBar(myToolbar);

        mDatabase = FirebaseDatabase.getInstance();
        mStorage = FirebaseStorage.getInstance();
        mRecyclerViewPrivate=findViewById(R.id.recycler_view_private);
        mRecyclerViewPrivate.setLayoutManager(new LinearLayoutManager(this));
        mRecyclerViewPublic=findViewById(R.id.recycler_view_public);
        mRecyclerViewPublic.setLayoutManager(new LinearLayoutManager(this));

    }

    private void initRecyclerView(){
        CheckBox checkbox=findViewById(R.id.processed_check);
        isPreProcessed=checkbox.isChecked();
        query_public = mDatabase
                .getReference()
                .child("public");
        updateRecyclerView(query_public,mRecyclerViewPublic);
        //query_public.getRef()
        if (FirebaseAuth.getInstance().getCurrentUser()!=null){
            mRecyclerViewPrivate.setVisibility(View.VISIBLE);
            findViewById(R.id.privtae_text).setVisibility(View.INVISIBLE);
            query_private = mDatabase
                    .getReference()
                    .child("users/"+FirebaseAuth.getInstance().getCurrentUser().getUid().toString());
            updateRecyclerView(query_private,mRecyclerViewPrivate);
        }
        else{
            findViewById(R.id.privtae_text).setVisibility(View.VISIBLE);
            mRecyclerViewPrivate.setVisibility(View.INVISIBLE);
        }
    }

    private void updateRecyclerView(Query query,RecyclerView recview){
            FirebaseRecyclerOptions<CloudImage> options =
                new FirebaseRecyclerOptions.Builder<CloudImage>()
                        .setQuery(query,CloudImage.class)
                        .build();

        FirebaseRecyclerAdapter adapter= new FirebaseRecyclerAdapter<CloudImage,CloudImageHolder>(options){
            @Override
            public CloudImageHolder onCreateViewHolder(ViewGroup parent, int viewType) {
                View view = LayoutInflater.from(parent.getContext())
                        .inflate(R.layout.cloud_image, parent, false);
                return new CloudImageHolder(view);
            }

            @Override
            protected void onBindViewHolder(CloudImageHolder holder, int position, CloudImage model) {
                holder.bind(getCacheDir(),model,isPreProcessed);
            }
        };
        recview.setAdapter(adapter);
        adapter.startListening();
    }

    @Override
    public void onStart() {
        super.onStart();
        EditText searchText=(EditText)findViewById(R.id.search_text);
        searchText.setText("");
        initRecyclerView();

    }

    public void searchByDescription(View view){
        EditText searchText=(EditText)findViewById(R.id.search_text);
        InputMethodManager inputManager = (InputMethodManager)getSystemService(Context.INPUT_METHOD_SERVICE);
        inputManager.hideSoftInputFromWindow(getCurrentFocus().getWindowToken(),InputMethodManager.HIDE_NOT_ALWAYS);

        String keyword=searchText.getText().toString(); //search keywords
        if (!TextUtils.isEmpty(keyword)) {
            query_public = mDatabase
                    .getReference()
                    .child("public")
                    .orderByChild("description")
                    .equalTo(keyword);
            updateRecyclerView(query_public, mRecyclerViewPublic);

            if (FirebaseAuth.getInstance().getCurrentUser() != null) {
                query_private = mDatabase
                        .getReference()
                        .child("users/" + FirebaseAuth.getInstance().getCurrentUser().getUid().toString())
                        .orderByChild("description")
                        .equalTo(keyword);
                updateRecyclerView(query_private, mRecyclerViewPrivate);
            }
        }
        else{
            initRecyclerView();
        }
    }

    public void showProcessed(View view){
        initRecyclerView();
    }
}


