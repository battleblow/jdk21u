/*
 * Copyright (c) 2004, 2024, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */

#include <stdlib.h>
#include <string.h>
#include "jni_tools.h"
#include "agent_common.hpp"
#include "jvmti_tools.h"

#define PASSED 0
#define STATUS_FAILED 2

extern "C" {

/* ========================================================================== */

/* scaffold objects */
static jlong timeout = 0;

/* test objects */
static jint klass_byte_count = 0;
static unsigned char *klass_bytes = nullptr;
static int ClassFileLoadHookEventFlag = NSK_FALSE;

const char* CLASS_NAME = "nsk/jvmti/scenarios/multienv/MA07/ma07t001a";
static const jint magicNumber_1 = 0x12345678;
static const jint magicNumber_2 = (jint)0x87654321;
static const unsigned char newMagicNumber = 0x1;

/* ========================================================================== */

/** callback functions **/

static void JNICALL
ClassFileLoadHook(jvmtiEnv *jvmti_env, JNIEnv *jni_env,
        jclass class_being_redefined, jobject loader,
        const char* name, jobject protection_domain,
        jint class_data_len, const unsigned char* class_data,
        jint *new_class_data_len, unsigned char** new_class_data) {
    int found_1 = NSK_FALSE;
    int found_2 = NSK_FALSE;
    jint magicIndex = 0;
    jint i;

    if (name != nullptr && (strcmp(name, CLASS_NAME) == 0)) {
        ClassFileLoadHookEventFlag = NSK_TRUE;
        NSK_DISPLAY0("ClassFileLoadHook event\n");

        if (!NSK_VERIFY(class_being_redefined == nullptr)) {
            nsk_jvmti_setFailStatus();
            return;
        }

        if (!NSK_JVMTI_VERIFY(jvmti_env->Allocate(class_data_len, &klass_bytes)))
            nsk_jvmti_setFailStatus();

        else {
            memcpy(klass_bytes, class_data, class_data_len);
            klass_byte_count = class_data_len;

            for (i = 0; i < klass_byte_count - 3; i++) {
                if (((jint)klass_bytes[i+3] |
                    ((jint)klass_bytes[i+2] << 8) |
                    ((jint)klass_bytes[i+1] << 16) |
                    ((jint)klass_bytes[i] << 24)) == magicNumber_1) {
                    NSK_DISPLAY2("index of 0x%x: %d\n", magicNumber_1, i);
                    found_1 = NSK_TRUE;
                    magicIndex = i;
                } else if (((jint)klass_bytes[i+3] |
                    ((jint)klass_bytes[i+2] << 8) |
                    ((jint)klass_bytes[i+1] << 16) |
                    ((jint)klass_bytes[i] << 24)) == magicNumber_2) {
                    NSK_DISPLAY2("index of 0x%x: %d\n", magicNumber_2, i);
                    found_2 = NSK_TRUE;
                }
            }

            if (!NSK_VERIFY(found_1)) {
                NSK_COMPLAIN1("magic number 0x%x not found\n", magicNumber_1);
                nsk_jvmti_setFailStatus();
            }

            if (!NSK_VERIFY(found_2)) {
                NSK_COMPLAIN1("magic number 0x%x not found\n", magicNumber_2);
                nsk_jvmti_setFailStatus();
            }

            if (found_1) {
                NSK_DISPLAY1("Instrumenting with %d\n", newMagicNumber);
                klass_bytes[magicIndex] = 0;
                klass_bytes[magicIndex+1] = 0;
                klass_bytes[magicIndex+2] = 0;
                klass_bytes[magicIndex+3] = newMagicNumber;
                *new_class_data = klass_bytes;
                *new_class_data_len = klass_byte_count;
            }
        }
    }
}

/* ========================================================================== */

/** Agent algorithm. */
static void JNICALL
agentProc(jvmtiEnv* jvmti, JNIEnv* jni, void* arg) {

    if (!nsk_jvmti_waitForSync(timeout))
        return;

    if (!NSK_VERIFY(ClassFileLoadHookEventFlag)) {
        NSK_COMPLAIN0("Missing ClassFileLoadHook event\n");
        nsk_jvmti_setFailStatus();
    }

    if (!nsk_jvmti_resumeSync())
        return;
}

/* ========================================================================== */

/** Agent library initialization. */
#ifdef STATIC_BUILD
JNIEXPORT jint JNICALL Agent_OnLoad_ma07t001(JavaVM *jvm, char *options, void *reserved) {
    return Agent_Initialize(jvm, options, reserved);
}
JNIEXPORT jint JNICALL Agent_OnAttach_ma07t001(JavaVM *jvm, char *options, void *reserved) {
    return Agent_Initialize(jvm, options, reserved);
}
JNIEXPORT jint JNI_OnLoad_ma07t001(JavaVM *jvm, char *options, void *reserved) {
    return JNI_VERSION_1_8;
}
#endif
jint Agent_Initialize(JavaVM *jvm, char *options, void *reserved) {
    jvmtiEnv* jvmti = nullptr;
    jvmtiEventCallbacks callbacks;

    NSK_DISPLAY0("Agent_OnLoad\n");

    if (!NSK_VERIFY(nsk_jvmti_parseOptions(options)))
        return JNI_ERR;

    timeout = nsk_jvmti_getWaitTime() * 60 * 1000;

    if (!NSK_VERIFY((jvmti =
            nsk_jvmti_createJVMTIEnv(jvm, reserved)) != nullptr))
        return JNI_ERR;

    if (!NSK_VERIFY(nsk_jvmti_setAgentProc(agentProc, nullptr)))
        return JNI_ERR;

    memset(&callbacks, 0, sizeof(callbacks));
    callbacks.ClassFileLoadHook = &ClassFileLoadHook;
    if (!NSK_VERIFY(nsk_jvmti_init_MA(&callbacks)))
        return JNI_ERR;

    if (!NSK_JVMTI_VERIFY(jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_CLASS_FILE_LOAD_HOOK, nullptr)))
        return JNI_ERR;

    return JNI_OK;
}

/* ========================================================================== */

}
