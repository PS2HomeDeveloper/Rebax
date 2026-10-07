#include "android_storage.h"

#if defined(__ANDROID__) && defined(__has_include)
#if __has_include(<jni.h>)
#define REBAX_ANDROID_JNI 1
#endif
#endif

#if defined(REBAX_ANDROID_JNI)

#include <jni.h>
#include <SDL.h>

#define PERM_READ "android.permission.READ_EXTERNAL_STORAGE"
#define PERM_WRITE "android.permission.WRITE_EXTERNAL_STORAGE"

static void clear_exception(JNIEnv *env) {
    if ((*env)->ExceptionCheck(env)) (*env)->ExceptionClear(env);
}

static int sdk_int(JNIEnv *env) {
    jclass cls = (*env)->FindClass(env, "android/os/Build$VERSION");
    if (cls == NULL) { clear_exception(env); return 0; }
    jfieldID field = (*env)->GetStaticFieldID(env, cls, "SDK_INT", "I");
    int value = field != NULL ? (int)(*env)->GetStaticIntField(env, cls, field) : 0;
    clear_exception(env);
    (*env)->DeleteLocalRef(env, cls);
    return value;
}

static int permission_granted(JNIEnv *env, jobject activity, const char *name) {
    jclass cls = (*env)->GetObjectClass(env, activity);
    jmethodID method = (*env)->GetMethodID(env, cls, "checkSelfPermission", "(Ljava/lang/String;)I");
    int granted = 1;
    if (method != NULL) {
        jstring text = (*env)->NewStringUTF(env, name);
        granted = (*env)->CallIntMethod(env, activity, method, text) == 0;
        (*env)->DeleteLocalRef(env, text);
    }
    clear_exception(env);
    (*env)->DeleteLocalRef(env, cls);
    return granted;
}

static int all_files_granted(JNIEnv *env) {
    jclass cls = (*env)->FindClass(env, "android/os/Environment");
    if (cls == NULL) { clear_exception(env); return 1; }
    jmethodID method = (*env)->GetStaticMethodID(env, cls, "isExternalStorageManager", "()Z");
    int granted = 1;
    if (method != NULL) granted = (*env)->CallStaticBooleanMethod(env, cls, method) == JNI_TRUE;
    clear_exception(env);
    (*env)->DeleteLocalRef(env, cls);
    return granted;
}

int android_storage_granted(void) {
    JNIEnv *env = (JNIEnv *)SDL_AndroidGetJNIEnv();
    jobject activity = (jobject)SDL_AndroidGetActivity();
    if (env == NULL || activity == NULL) return 1;
    int sdk = sdk_int(env);
    int granted = 1;
    if (sdk >= 30) granted = all_files_granted(env);
    else if (sdk >= 23) granted = permission_granted(env, activity, PERM_READ) && permission_granted(env, activity, PERM_WRITE);
    (*env)->DeleteLocalRef(env, activity);
    return granted;
}

static void open_all_files_settings(JNIEnv *env, jobject activity) {
    jclass intent_cls = (*env)->FindClass(env, "android/content/Intent");
    jclass uri_cls = (*env)->FindClass(env, "android/net/Uri");
    jclass activity_cls = (*env)->GetObjectClass(env, activity);
    if (intent_cls == NULL || uri_cls == NULL) { clear_exception(env); return; }

    jmethodID get_package = (*env)->GetMethodID(env, activity_cls, "getPackageName", "()Ljava/lang/String;");
    jmethodID parse = (*env)->GetStaticMethodID(env, uri_cls, "parse", "(Ljava/lang/String;)Landroid/net/Uri;");
    jmethodID init_uri = (*env)->GetMethodID(env, intent_cls, "<init>", "(Ljava/lang/String;Landroid/net/Uri;)V");
    jmethodID init_plain = (*env)->GetMethodID(env, intent_cls, "<init>", "(Ljava/lang/String;)V");
    jmethodID start = (*env)->GetMethodID(env, activity_cls, "startActivity", "(Landroid/content/Intent;)V");
    clear_exception(env);
    if (start == NULL) return;

    jobject intent = NULL;
    if (get_package != NULL && parse != NULL && init_uri != NULL) {
        jstring package = (jstring)(*env)->CallObjectMethod(env, activity, get_package);
        const char *chars = package != NULL ? (*env)->GetStringUTFChars(env, package, NULL) : NULL;
        if (chars != NULL) {
            char text[512];
            SDL_snprintf(text, sizeof(text), "package:%s", chars);
            (*env)->ReleaseStringUTFChars(env, package, chars);
            jstring uri_text = (*env)->NewStringUTF(env, text);
            jobject uri = (*env)->CallStaticObjectMethod(env, uri_cls, parse, uri_text);
            jstring action = (*env)->NewStringUTF(env, "android.settings.MANAGE_APP_ALL_FILES_ACCESS_PERMISSION");
            if (uri != NULL) intent = (*env)->NewObject(env, intent_cls, init_uri, action, uri);
        }
    }
    clear_exception(env);
    if (intent != NULL) {
        (*env)->CallVoidMethod(env, activity, start, intent);
        if (!(*env)->ExceptionCheck(env)) return;
        (*env)->ExceptionClear(env);
    }
    if (init_plain != NULL) {
        jstring action = (*env)->NewStringUTF(env, "android.settings.MANAGE_ALL_FILES_ACCESS_PERMISSION");
        jobject plain = (*env)->NewObject(env, intent_cls, init_plain, action);
        if (plain != NULL) (*env)->CallVoidMethod(env, activity, start, plain);
        clear_exception(env);
    }
}

static void request_runtime_permissions(JNIEnv *env, jobject activity) {
    jclass activity_cls = (*env)->GetObjectClass(env, activity);
    jclass string_cls = (*env)->FindClass(env, "java/lang/String");
    jmethodID request = (*env)->GetMethodID(env, activity_cls, "requestPermissions", "([Ljava/lang/String;I)V");
    if (request == NULL || string_cls == NULL) { clear_exception(env); return; }
    jobjectArray list = (*env)->NewObjectArray(env, 2, string_cls, NULL);
    (*env)->SetObjectArrayElement(env, list, 0, (*env)->NewStringUTF(env, PERM_READ));
    (*env)->SetObjectArrayElement(env, list, 1, (*env)->NewStringUTF(env, PERM_WRITE));
    (*env)->CallVoidMethod(env, activity, request, list, 7101);
    clear_exception(env);
}

void android_storage_request_if_needed(void) {
    JNIEnv *env = (JNIEnv *)SDL_AndroidGetJNIEnv();
    jobject activity = (jobject)SDL_AndroidGetActivity();
    if (env == NULL || activity == NULL) return;
    if (!android_storage_granted()) {
        int sdk = sdk_int(env);
        if (sdk >= 30) open_all_files_settings(env, activity);
        else if (sdk >= 23) request_runtime_permissions(env, activity);
    }
    (*env)->DeleteLocalRef(env, activity);
}

#else

int android_storage_granted(void) { return 1; }
void android_storage_request_if_needed(void) {}

#endif
