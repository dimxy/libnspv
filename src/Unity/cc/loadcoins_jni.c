#include <stdlib.h>
#include <jni.h>
#include <android/asset_manager.h>

void nspv_log_message(char *format, ...);

JNIEXPORT jint JNICALL
Java_com_DefaultCompany_TestAndroidSO_MyUnityPlayerActivity_loadCoinsFile(JNIEnv* env, jclass clazz, jobject assetManager)
{
    SLresult result;
    char fname[] = "coins";
    int rc = 0;

    // convert Java string to UTF-8
    //const char *utf8 = (*env)->GetStringUTFChars(env, fname, NULL);
    //assert(NULL != utf8);

    // use asset manager to open asset by filename
    AAssetManager* mgr = AAssetManager_fromJava(env, assetManager);
    if (NULL == mgr) {
        nspv_log_message("cannot access assetmanager");
        return -1;
    }
    AAsset* asset = AAssetManager_open(mgr, fname, AASSET_MODE_STREAMING);
    // release the Java string and UTF-8
    //(*env)->ReleaseStringUTFChars(env, filename, utf8);
    if (asset == NULL) {
        nspv_log_message("cannot access asset %s", fname);
        return -1;
    }

    size_t len = AAsset_length(asset);
    char *bufp = malloc(len + 1);
    size_t nread = AAsset_read(asset, *bufp, len);
    if (nread != len) {
        nspv_log_message("cannot read asset buffer, read bytes = %d", nread);
        free(bufp);
        bufp = NULL;
        rc = -1;
    }
    if (nread > 0)
        bufp[nread] = '\0';

    nspv_log_message("read file = %s", bufp);

    AAsset_close(asset);
    free(bufp);
    return rc;
}
