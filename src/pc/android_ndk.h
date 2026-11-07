#ifndef ANDROID_NDK_H
#define ANDROID_NDK_H

/* The part of the Android NDK API the port uses (NativeActivity, windows,
 * input, the looper, the log), declared as in the NDK headers
 * (android/native_activity.h, native_window.h, input.h, looper.h, log.h,
 * jni.h). */

#include <stddef.h>
#include <stdint.h>

/* ---- jni.h (only what the port calls, by the index in the function table) */
typedef void *jobject;
typedef const struct JNINativeInterface *JNIEnv;
typedef const struct JNIInvokeInterface *JavaVM;

struct JNIInvokeInterface {
    void *reserved[3];
    int (*DestroyJavaVM)(JavaVM *);
    int (*AttachCurrentThread)(JavaVM *, JNIEnv **, void *);
    int (*DetachCurrentThread)(JavaVM *);
    int (*GetEnv)(JavaVM *, void **, int);
};

/* ---- native_activity.h */
typedef struct ANativeWindow ANativeWindow;
typedef struct AInputQueue AInputQueue;
typedef struct AInputEvent AInputEvent;
typedef struct ALooper ALooper;
typedef struct AAssetManager AAssetManager;
typedef struct ARect {
    int32_t left, top, right, bottom;
} ARect;

struct ANativeActivityCallbacks;

typedef struct ANativeActivity {
    struct ANativeActivityCallbacks *callbacks;
    JavaVM *vm;
    JNIEnv *env;
    jobject clazz;
    const char *internalDataPath;
    const char *externalDataPath;
    int32_t sdkVersion;
    void *instance;
    AAssetManager *assetManager;
    const char *obbPath;
} ANativeActivity;

typedef struct ANativeActivityCallbacks {
    void (*onStart)(ANativeActivity *activity);
    void (*onResume)(ANativeActivity *activity);
    void *(*onSaveInstanceState)(ANativeActivity *activity, size_t *outSize);
    void (*onPause)(ANativeActivity *activity);
    void (*onStop)(ANativeActivity *activity);
    void (*onDestroy)(ANativeActivity *activity);
    void (*onWindowFocusChanged)(ANativeActivity *activity, int hasFocus);
    void (*onNativeWindowCreated)(ANativeActivity *activity, ANativeWindow *window);
    void (*onNativeWindowResized)(ANativeActivity *activity, ANativeWindow *window);
    void (*onNativeWindowRedrawNeeded)(ANativeActivity *activity, ANativeWindow *window);
    void (*onNativeWindowDestroyed)(ANativeActivity *activity, ANativeWindow *window);
    void (*onInputQueueCreated)(ANativeActivity *activity, AInputQueue *queue);
    void (*onInputQueueDestroyed)(ANativeActivity *activity, AInputQueue *queue);
    void (*onContentRectChanged)(ANativeActivity *activity, const ARect *rect);
    void (*onConfigurationChanged)(ANativeActivity *activity);
    void (*onLowMemory)(ANativeActivity *activity);
} ANativeActivityCallbacks;

void ANativeActivity_finish(ANativeActivity *activity);
void ANativeActivity_setWindowFlags(ANativeActivity *activity, uint32_t addFlags, uint32_t removeFlags);
#define AWINDOW_FLAG_KEEP_SCREEN_ON 0x00000080
#define AWINDOW_FLAG_FULLSCREEN 0x00000400

/* ---- native_window.h */
typedef struct ANativeWindow_Buffer {
    int32_t width;
    int32_t height;
    int32_t stride;
    int32_t format;
    void *bits;
    uint32_t reserved[6];
} ANativeWindow_Buffer;
#define WINDOW_FORMAT_RGBX_8888 2
int32_t ANativeWindow_getWidth(ANativeWindow *window);
int32_t ANativeWindow_getHeight(ANativeWindow *window);
int32_t ANativeWindow_setBuffersGeometry(ANativeWindow *window, int32_t width, int32_t height, int32_t format);
int32_t ANativeWindow_lock(ANativeWindow *window, ANativeWindow_Buffer *outBuffer, ARect *inOutDirtyBounds);
int32_t ANativeWindow_unlockAndPost(ANativeWindow *window);
void ANativeWindow_acquire(ANativeWindow *window);
void ANativeWindow_release(ANativeWindow *window);

/* ---- looper.h */
#define ALOOPER_PREPARE_ALLOW_NON_CALLBACKS 1
typedef int (*ALooper_callbackFunc)(int fd, int events, void *data);
ALooper *ALooper_prepare(int opts);
int ALooper_pollOnce(int timeoutMillis, int *outFd, int *outEvents, void **outData);

/* ---- input.h */
#define AINPUT_EVENT_TYPE_KEY 1
#define AINPUT_EVENT_TYPE_MOTION 2
#define AKEY_EVENT_ACTION_DOWN 0
#define AKEY_EVENT_ACTION_UP 1
#define AMOTION_EVENT_ACTION_MASK 0xff
#define AMOTION_EVENT_ACTION_POINTER_INDEX_MASK 0xff00
#define AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT 8
#define AMOTION_EVENT_ACTION_DOWN 0
#define AMOTION_EVENT_ACTION_UP 1
#define AMOTION_EVENT_ACTION_MOVE 2
#define AMOTION_EVENT_ACTION_CANCEL 3
#define AMOTION_EVENT_ACTION_POINTER_DOWN 5
#define AMOTION_EVENT_ACTION_POINTER_UP 6
#define AMOTION_EVENT_AXIS_X 0
#define AMOTION_EVENT_AXIS_Y 1
#define AMOTION_EVENT_AXIS_HAT_X 15
#define AMOTION_EVENT_AXIS_HAT_Y 16
#define AINPUT_SOURCE_CLASS_JOYSTICK 0x10
#define AKEYCODE_BACK 4
#define AKEYCODE_DPAD_UP 19
#define AKEYCODE_DPAD_DOWN 20
#define AKEYCODE_DPAD_LEFT 21
#define AKEYCODE_DPAD_RIGHT 22
#define AKEYCODE_DPAD_CENTER 23
#define AKEYCODE_ENTER 66
#define AKEYCODE_BUTTON_A 96
#define AKEYCODE_BUTTON_B 97
#define AKEYCODE_BUTTON_X 99
#define AKEYCODE_BUTTON_Y 100
#define AKEYCODE_BUTTON_L1 102
#define AKEYCODE_BUTTON_R1 103
#define AKEYCODE_BUTTON_L2 104
#define AKEYCODE_BUTTON_R2 105
#define AKEYCODE_BUTTON_START 108
#define AKEYCODE_BUTTON_SELECT 109
int32_t AInputEvent_getType(const AInputEvent *event);
int32_t AInputEvent_getSource(const AInputEvent *event);
int32_t AKeyEvent_getAction(const AInputEvent *key_event);
int32_t AKeyEvent_getKeyCode(const AInputEvent *key_event);
int32_t AMotionEvent_getAction(const AInputEvent *motion_event);
size_t AMotionEvent_getPointerCount(const AInputEvent *motion_event);
int32_t AMotionEvent_getPointerId(const AInputEvent *motion_event, size_t pointer_index);
float AMotionEvent_getX(const AInputEvent *motion_event, size_t pointer_index);
float AMotionEvent_getY(const AInputEvent *motion_event, size_t pointer_index);
float AMotionEvent_getAxisValue(const AInputEvent *motion_event, int32_t axis, size_t pointer_index);
int32_t AInputQueue_getEvent(AInputQueue *queue, AInputEvent **outEvent);
int32_t AInputQueue_preDispatchEvent(AInputQueue *queue, AInputEvent *event);
void AInputQueue_finishEvent(AInputQueue *queue, AInputEvent *event, int handled);
void AInputQueue_attachLooper(AInputQueue *queue, ALooper *looper, int ident, ALooper_callbackFunc callback,
                              void *data);
void AInputQueue_detachLooper(AInputQueue *queue);

/* ---- log.h */
#define ANDROID_LOG_INFO 4
#define ANDROID_LOG_ERROR 6
int __android_log_write(int prio, const char *tag, const char *text);

#endif /* ANDROID_NDK_H */
