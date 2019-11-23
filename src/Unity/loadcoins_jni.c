#include <stdlib.h>
#include <jni.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>

// load 'coins' file from apk asset directory

void nspv_log_message(char *format, ...);
extern char *coinsCached;

JNIEXPORT jint JNICALL
Java_com_DefaultCompany_TestAndroidSO_MyUnityPlayerActivity_loadCoinsFile(JNIEnv* env, jclass clazz, jobject assetManager)
{
    //SLresult result;
    char fname[] = "data/coins";
    int rc = 0;

    // convert Java string to UTF-8
    //const char *utf8 = (*env)->GetStringUTFChars(env, fname, NULL);
    //assert(NULL != utf8);
    if (coinsCached == NULL)
    {
        // use asset manager to open asset by filename
        AAssetManager* mgr = AAssetManager_fromJava(env, assetManager);
        if (NULL == mgr) {
            nspv_log_message("cannot access AAssetManager");
            return -1;
        }

        AAsset* asset = AAssetManager_open(mgr, fname, AASSET_MODE_STREAMING);
        // release the Java string and UTF-8
        //(*env)->ReleaseStringUTFChars(env, filename, utf8);
        if (asset == NULL) {
            nspv_log_message("cannot access asset %s", fname);
            return -1;
        }

        size_t len = AAsset_getLength(asset);
        if (len == 0) {
            nspv_log_message("coins asset len is 0");
            return -1;
        }
        coinsCached = malloc(len + 1); // will be freed in NSPV_coinlist_scan
        size_t nread = AAsset_read(asset, coinsCached, len);
        if (nread != len) {
            nspv_log_message("cannot read asset buffer, read bytes = %d", nread);
            free(coinsCached);
            coinsCached = NULL;
            rc = -1;
        }
        coinsCached[nread] = '\0';
        nspv_log_message("coins file content = %s", coinsCached);
        AAsset_close(asset);
    }
    else
    {
        nspv_log_message("coins file already loaded");
    }

    return rc;
}
