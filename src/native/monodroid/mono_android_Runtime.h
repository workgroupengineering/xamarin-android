/* DO NOT EDIT THIS FILE - it is machine generated */
#include <jni.h>
/* Header for class mono_android_Runtime */

#ifndef _Included_mono_android_Runtime
#define _Included_mono_android_Runtime
#ifdef __cplusplus
extern "C" {
#endif
/*
 * Class:     mono_android_Runtime
 * Method:    init
 * Signature: (Ljava/lang/String;[Ljava/lang/String;Ljava/lang/String;[Ljava/lang/String;Ljava/lang/ClassLoader;[Ljava/lang/String;[Ljava/lang/String;Ljava/lang/String;I[Ljava/lang/String;)V
 */
JNIEXPORT void JNICALL Java_mono_android_Runtime_init
  (JNIEnv *, jclass, jstring, jobjectArray, jstring, jobjectArray, jobject, jobjectArray, jobjectArray, jstring, jint, jobjectArray);

/*
 * Class:     mono_android_Runtime
 * Method:    initInternal
 * Signature: (Ljava/lang/String;[Ljava/lang/String;Ljava/lang/String;[Ljava/lang/String;ILjava/lang/ClassLoader;[Ljava/lang/String;IZZ)V
 */
JNIEXPORT void JNICALL Java_mono_android_Runtime_initInternal
  (JNIEnv *, jclass, jstring, jobjectArray, jstring, jobjectArray, jint, jobject, jobjectArray, jint, jboolean, jboolean);

/*
 * Class:     mono_android_Runtime
 * Method:    register
 * Signature: (Ljava/lang/String;Ljava/lang/Class;Ljava/lang/String;)V
 */
JNIEXPORT void JNICALL Java_mono_android_Runtime_register
  (JNIEnv *, jclass, jstring, jclass, jstring);

/*
 * Class:     mono_android_Runtime
 * Method:    notifyTimeZoneChanged
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_mono_android_Runtime_notifyTimeZoneChanged
  (JNIEnv *, jclass);

/*
 * Class:     mono_android_Runtime
 * Method:    createNewContext
 * Signature: ([Ljava/lang/String;[Ljava/lang/String;Ljava/lang/ClassLoader;)I
 */
JNIEXPORT jint JNICALL Java_mono_android_Runtime_createNewContext
  (JNIEnv *, jclass, jobjectArray, jobjectArray, jobject);

/*
 * Class:     mono_android_Runtime
 * Method:    createNewContextWithData
 * Signature: ([Ljava/lang/String;[Ljava/lang/String;[[B[Ljava/lang/String;Ljava/lang/ClassLoader;Z)I
 */
JNIEXPORT jint JNICALL Java_mono_android_Runtime_createNewContextWithData
  (JNIEnv *, jclass, jobjectArray, jobjectArray, jobjectArray, jobjectArray, jobject, jboolean);

/*
 * Class:     mono_android_Runtime
 * Method:    switchToContext
 * Signature: (I)V
 */
JNIEXPORT void JNICALL Java_mono_android_Runtime_switchToContext
  (JNIEnv *, jclass, jint);

/*
 * Class:     mono_android_Runtime
 * Method:    destroyContexts
 * Signature: ([I)V
 */
JNIEXPORT void JNICALL Java_mono_android_Runtime_destroyContexts
  (JNIEnv *, jclass, jintArray);

/*
 * Class:     mono_android_Runtime
 * Method:    propagateUncaughtException
 * Signature: (Ljava/lang/Thread;Ljava/lang/Throwable;)V
 */
JNIEXPORT void JNICALL Java_mono_android_Runtime_propagateUncaughtException
  (JNIEnv *, jclass, jobject, jthrowable);

/*
 * Class:     mono_android_Runtime
 * Method:    dumpTimingData
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_mono_android_Runtime_dumpTimingData
  (JNIEnv *, jclass);

#ifdef __cplusplus
}
#endif
#endif