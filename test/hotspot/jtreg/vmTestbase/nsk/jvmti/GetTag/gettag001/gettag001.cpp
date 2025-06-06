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
#define DEBUGEE_CLASS_NAME    "nsk/jvmti/GetTag/gettag001"
#define OBJECT_CLASS_NAME     "nsk/jvmti/GetTag/gettag001TestedClass"
#define OBJECT_CLASS_SIG      "L" OBJECT_CLASS_NAME ";"
#define OBJECT_FIELD_NAME     "testedObject"

/* ============================================================================= */

/** Agent algorithm. */
static void JNICALL
agentProc(jvmtiEnv* jvmti, JNIEnv* jni, void* arg) {
    NSK_DISPLAY0("Wait for object created\n");
    if (!NSK_VERIFY(nsk_jvmti_waitForSync(timeout)))
        return;

    /* perform testing */
    {
        jobject testedObject = nullptr;
        jlong objectTag = 0;

        NSK_DISPLAY0(">>> Obtain tested object from a static field of debugee class\n");
        {
            jclass debugeeClass = nullptr;
            jfieldID objectField = nullptr;

            NSK_DISPLAY1("Find debugee class: %s\n", DEBUGEE_CLASS_NAME);
            if (!NSK_JNI_VERIFY(jni, (debugeeClass =
                    jni->FindClass(DEBUGEE_CLASS_NAME)) != nullptr)) {
                nsk_jvmti_setFailStatus();
                return;
            }
            NSK_DISPLAY1("  ... found class: 0x%p\n", (void*)debugeeClass);

            NSK_DISPLAY1("Find static field: %s\n", OBJECT_FIELD_NAME);
            if (!NSK_JNI_VERIFY(jni, (objectField =
                    jni->GetStaticFieldID(debugeeClass, OBJECT_FIELD_NAME, OBJECT_CLASS_SIG)) != nullptr)) {
                nsk_jvmti_setFailStatus();
                return;
            }
            NSK_DISPLAY1("  ... got fieldID: 0x%p\n", (void*)objectField);

            NSK_DISPLAY1("Get object from static field: %s\n", OBJECT_FIELD_NAME);
            if (!NSK_JNI_VERIFY(jni, (testedObject =
                    jni->GetStaticObjectField(debugeeClass, objectField)) != nullptr)) {
                nsk_jvmti_setFailStatus();
                return;
            }
            NSK_DISPLAY1("  ... got object: 0x%p\n", (void*)testedObject);

            NSK_DISPLAY1("Create global reference for object: 0x%p\n", (void*)testedObject);
            if (!NSK_JNI_VERIFY(jni, (testedObject =
                    jni->NewGlobalRef(testedObject)) != nullptr)) {
                nsk_jvmti_setFailStatus();
                return;
            }
            NSK_DISPLAY1("  ... got reference: 0x%p\n", (void*)testedObject);
        }

        NSK_DISPLAY0(">>> Testcase #1: Get tag of the object and check if it is zero\n");
        {
            jlong objectTag = 100;

            NSK_DISPLAY1("Get tag for object: 0x%p\n", (void*)testedObject);
            if (!NSK_JVMTI_VERIFY(
                    jvmti->GetTag(testedObject, &objectTag))) {
                nsk_jvmti_setFailStatus();
                return;
            }
            NSK_DISPLAY1("  ... got tag: %ld\n", (long)objectTag);

            if (objectTag != 0) {
                NSK_COMPLAIN2("GetTag returns not zero tag for untagged object\n"
                              "#   got tag:  %ld\n"
                              "#   expected: %ld\n",
                              (long)objectTag, (long)0);
                nsk_jvmti_setFailStatus();
            } else {
                NSK_DISPLAY0("SUCCESS: Got tag is zero for untagged object\n");
            }
        }

        NSK_DISPLAY0(">>> Clean used data\n");
        {
            NSK_DISPLAY1("Delete object reference: 0x%p\n", (void*)testedObject);
            NSK_TRACE(jni->DeleteGlobalRef(testedObject));
        }
    }

    NSK_DISPLAY0("Let debugee to finish\n");
    if (!NSK_VERIFY(nsk_jvmti_resumeSync()))
        return;
}

/* ============================================================================= */

/** Agent library initialization. */
#ifdef STATIC_BUILD
JNIEXPORT jint JNICALL Agent_OnLoad_gettag001(JavaVM *jvm, char *options, void *reserved) {
    return Agent_Initialize(jvm, options, reserved);
}
JNIEXPORT jint JNICALL Agent_OnAttach_gettag001(JavaVM *jvm, char *options, void *reserved) {
    return Agent_Initialize(jvm, options, reserved);
}
JNIEXPORT jint JNI_OnLoad_gettag001(JavaVM *jvm, char *options, void *reserved) {
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

    /* add required capabilities */
    {
        jvmtiCapabilities caps;

        memset(&caps, 0, sizeof(caps));
        caps.can_tag_objects = 1;
        if (!NSK_JVMTI_VERIFY(
                jvmti->AddCapabilities(&caps))) {
            return JNI_ERR;
        }
    }

    /* register agent proc and arg */
    if (!NSK_VERIFY(nsk_jvmti_setAgentProc(agentProc, nullptr)))
        return JNI_ERR;

    return JNI_OK;
}

/* ============================================================================= */

}
