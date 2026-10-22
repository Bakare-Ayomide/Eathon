/*
 * ============================================================================
 * EATHON ANDROID NDK JNI BRIDGE (eathon_jni.c)
 * Package: dev.eathon.studio
 *
 * Bridges the native Kotlin Android UI (EathonStudio, inspired by Pydroid)
 * directly to the C99 Eathon Compiler (compiler/src/eathonc.c) and EathonDB
 * Storage Engine (compiler/src/eatdb.c) inside libeathon_jni.so.
 *
 * Why In-Process Native Execution on Android:
 *   Android 10+ (API 29+) enforces W^X and mounts app-writable directories
 *   (getFilesDir(), getCacheDir(), /sdcard) with MS_NOEXEC, preventing apps
 *   from calling execve() on dynamically emitted ELF binaries in app storage.
 *   By compiling eathonc.c and eatdb.c directly into libeathon_jni.so (which
 *   resides in the APK's executable nativeLibraryDir), Eathon programs are
 *   lexed, parsed, typechecked, compiled to C99/LLVM IR, and executed at
 *   native speed offline on Android.
 * ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "eathon_platform.h"

#if defined(__has_include)
#  if __has_include(<jni.h>)
#    include <jni.h>
#    define EATHON_HAS_JNI_H 1
#  endif
#endif

#ifndef EATHON_HAS_JNI_H
/* Minimal JNI ABI fallback declarations so eathon_jni.c can also be compiled
 * and tested as a shared library on desktop hosts without a full JDK installed */
typedef const struct JNINativeInterface *JNIEnv;
typedef void *jobject;
typedef void *jstring;
typedef int jint;
struct JNINativeInterface {
    const char *(*GetStringUTFChars)(JNIEnv *, jstring, unsigned char *);
    void (*ReleaseStringUTFChars)(JNIEnv *, jstring, const char *);
    jstring (*NewStringUTF)(JNIEnv *, const char *);
};
#define JNIEXPORT
#define JNICALL
#endif

/* External entry points from compiler/src/eathonc.c and compiler/src/eatdb.c */
extern int eathon_compile_and_run_buffer(const char *source_code,
                                         const char *source_path,
                                         char *out_stdout_buf,
                                         size_t out_stdout_cap,
                                         char *out_c99_buf,
                                         size_t out_c99_cap,
                                         int *out_exit_code);

extern int eatdb_execute_script(const char *db_file,
                                const char *script,
                                char *out_json_buf,
                                size_t out_json_cap);

static void json_escape_into(char *dst, size_t cap, size_t *used, const char *src) {
    if (!src) src = "";
    for (; *src; src++) {
        const char *rep = NULL;
        char ch[2] = { *src, '\0' };
        if (*src == '"') rep = "\\\"";
        else if (*src == '\\') rep = "\\\\";
        else if (*src == '\n') rep = "\\n";
        else if (*src == '\r') rep = "\\r";
        else if (*src == '\t') rep = "\\t";
        else rep = ch;
        int n = snprintf(dst + *used, cap - *used, "%s", rep);
        if (n > 0 && *used + (size_t)n < cap) *used += (size_t)n;
    }
}

/*
 * Pure C helper (callable from JNI and from automated shared-library tests)
 * Returns a JSON payload with:
 *   { "success": true|false, "exitCode": N, "platform": "...", "arch": "...",
 *     "stdout": "...", "c99Code": "..." }
 */
int eathon_jni_compile_and_run_json(const char *source_code,
                                    const char *source_path,
                                    char *out_json,
                                    size_t out_json_cap) {
    char *stdout_buf = (char *)calloc(1, 32768);
    char *c99_buf = (char *)calloc(1, 32768);
    if (!stdout_buf || !c99_buf) {
        free(stdout_buf);
        free(c99_buf);
        return 1;
    }

    int exit_code = 0;
    int rc = eathon_compile_and_run_buffer(
        source_code,
        source_path,
        stdout_buf,
        32768,
        c99_buf,
        32768,
        &exit_code
    );

    size_t used = 0;
    int n = snprintf(out_json + used, out_json_cap - used,
                     "{\"success\":%s,\"exitCode\":%d,\"platform\":\"%s\",\"arch\":\"%s\",\"stdout\":\"",
                     (rc == 0) ? "true" : "false",
                     exit_code,
                     EATHON_PLATFORM_NAME,
                     EATHON_ARCH_NAME);
    if (n > 0 && used + (size_t)n < out_json_cap) used += (size_t)n;

    json_escape_into(out_json, out_json_cap, &used, stdout_buf);

    n = snprintf(out_json + used, out_json_cap - used, "\",\"c99Code\":\"");
    if (n > 0 && used + (size_t)n < out_json_cap) used += (size_t)n;

    json_escape_into(out_json, out_json_cap, &used, c99_buf);

    snprintf(out_json + used, out_json_cap - used, "\"}");

    free(stdout_buf);
    free(c99_buf);
    return rc;
}

JNIEXPORT jstring JNICALL
Java_dev_eathon_studio_EathonBridge_compileAndRunNative(JNIEnv *env,
                                                        jobject thiz,
                                                        jstring j_source,
                                                        jstring j_path) {
    (void)thiz;
    const char *c_source = (*env)->GetStringUTFChars(env, j_source, NULL);
    const char *c_path = (*env)->GetStringUTFChars(env, j_path, NULL);

    char *json_buf = (char *)calloc(1, 98304);
    if (json_buf) {
        eathon_jni_compile_and_run_json(c_source, c_path, json_buf, 98304);
    }

    (*env)->ReleaseStringUTFChars(env, j_source, c_source);
    (*env)->ReleaseStringUTFChars(env, j_path, c_path);

    jstring result = (*env)->NewStringUTF(env, json_buf ? json_buf : "{\"success\":false}");
    free(json_buf);
    return result;
}

JNIEXPORT jstring JNICALL
Java_dev_eathon_studio_EathonBridge_executeEatDbNative(JNIEnv *env,
                                                       jobject thiz,
                                                       jstring j_db_path,
                                                       jstring j_script) {
    (void)thiz;
    const char *c_db_path = (*env)->GetStringUTFChars(env, j_db_path, NULL);
    const char *c_script = (*env)->GetStringUTFChars(env, j_script, NULL);

    char *json_buf = (char *)calloc(1, 32768);
    if (json_buf) {
        eatdb_execute_script(c_db_path, c_script, json_buf, 32768);
    }

    (*env)->ReleaseStringUTFChars(env, j_db_path, c_db_path);
    (*env)->ReleaseStringUTFChars(env, j_script, c_script);

    jstring result = (*env)->NewStringUTF(env, json_buf ? json_buf : "{\"success\":false}");
    free(json_buf);
    return result;
}

JNIEXPORT jstring JNICALL
Java_dev_eathon_studio_EathonBridge_getCompilerInfoNative(JNIEnv *env, jobject thiz) {
    (void)thiz;
    char info[256];
    snprintf(info, sizeof(info),
             "eathonc v1.0.2-native (%s / %s) + eatdb v1.0.2 slotted-page WAL engine",
             EATHON_PLATFORM_NAME, EATHON_ARCH_NAME);
    return (*env)->NewStringUTF(env, info);
}
