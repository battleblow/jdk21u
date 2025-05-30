/*
 * Copyright (c) 2003, 2024, Oracle and/or its affiliates. All rights reserved.
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

#include <string.h>
#include "jvmti.h"
#include "agent_common.hpp"
#include "jni_tools.h"
#include "jvmti_tools.h"

extern "C" {

/* ============================================================================= */

/* scaffold objects */
static jlong timeout = 0;

/* constant names */
#define THREAD_NAME     "TestedThread"

/* constants */
#define STORAGE_DATA_SIZE       1024
#define STORAGE_DATA_CHAR       'X'

/* storage structure */
typedef struct _StorageStructure {
    char data[STORAGE_DATA_SIZE];
} StorageStructure;

/* ============================================================================= */

/** Agent algorithm. */
static void JNICALL
agentProc(jvmtiEnv* jvmti, JNIEnv* jni, void* arg) {

    NSK_DISPLAY0("Wait for thread to start\n");
    if (!nsk_jvmti_waitForSync(timeout))
        return;

    /* perform testing */
    {
        StorageStructure storageData;
        StorageStructure* initialStorage = &storageData;
        StorageStructure* obtainedStorage = nullptr;

        memset(storageData.data, STORAGE_DATA_CHAR, STORAGE_DATA_SIZE);

        NSK_DISPLAY1("SetThreadLocalStorage() for current agent thread with pointer: %p\n",
                                                    (void*)initialStorage);
        if (!NSK_JVMTI_VERIFY(jvmti->SetThreadLocalStorage(nullptr, (void*)initialStorage))) {
            nsk_jvmti_setFailStatus();
            return;
        }

        NSK_DISPLAY0("Let debuggee to run\n");
        if (!nsk_jvmti_resumeSync())
            return;

        NSK_DISPLAY0("Wait for debuggee to run\n");
        if (!nsk_jvmti_waitForSync(timeout))
            return;

        NSK_DISPLAY0("GetThreadLocalStorage() for current agent thread\n");
        if (!NSK_JVMTI_VERIFY(jvmti->GetThreadLocalStorage(nullptr, (void**)&obtainedStorage))) {
            nsk_jvmti_setFailStatus();
            return;
        }
        NSK_DISPLAY1("  ... got storage: %p\n", (void*)obtainedStorage);

        NSK_DISPLAY0("Check storage data obtained for current agent thread\n");
        if (obtainedStorage != initialStorage) {
            NSK_COMPLAIN2("Wrong storage pointer returned for tested thread:\n"
                          "#   got pointer: %p\n"
                          "#   expected:    %p\n",
                            (void*)obtainedStorage, (void*)initialStorage);
            nsk_jvmti_setFailStatus();
        } else {
            int changed = 0;
            int i;

            for (i = 0; i < STORAGE_DATA_SIZE; i++) {
                if (obtainedStorage->data[i] != STORAGE_DATA_CHAR) {
                    changed++;
                }
            }

            if (changed > 0) {
                NSK_COMPLAIN2("Data changed in returned storage for current agent thread:\n"
                          "#   changed bytes: %d\n"
                          "#   total bytes:   %d\n",
                            changed, STORAGE_DATA_SIZE);
                nsk_jvmti_setFailStatus();
            }
        }
    }

    NSK_DISPLAY0("Let debugee to finish\n");
    if (!nsk_jvmti_resumeSync())
        return;
}

/* ============================================================================= */

/** Agent library initialization. */
#ifdef STATIC_BUILD
JNIEXPORT jint JNICALL Agent_OnLoad_setthrdstor002(JavaVM *jvm, char *options, void *reserved) {
    return Agent_Initialize(jvm, options, reserved);
}
JNIEXPORT jint JNICALL Agent_OnAttach_setthrdstor002(JavaVM *jvm, char *options, void *reserved) {
    return Agent_Initialize(jvm, options, reserved);
}
JNIEXPORT jint JNI_OnLoad_setthrdstor002(JavaVM *jvm, char *options, void *reserved) {
    return JNI_VERSION_1_8;
}
#endif
jint Agent_Initialize(JavaVM *jvm, char *options, void *reserved) {
    jvmtiEnv* jvmti = nullptr;

    /* init framework and parse options */
    if (!NSK_VERIFY(nsk_jvmti_parseOptions(options)))
        return JNI_ERR;

    timeout = nsk_jvmti_getWaitTime() * 60 * 1000;

    /* create JVMTI environment */
    if (!NSK_VERIFY((jvmti =
            nsk_jvmti_createJVMTIEnv(jvm, reserved)) != nullptr))
        return JNI_ERR;

    /* register agent proc and arg */
    if (!NSK_VERIFY(nsk_jvmti_setAgentProc(agentProc, nullptr)))
        return JNI_ERR;

    return JNI_OK;
}

/* ============================================================================= */

}
