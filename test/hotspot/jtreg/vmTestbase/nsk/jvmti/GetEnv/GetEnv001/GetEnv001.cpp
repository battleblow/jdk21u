/*
 * Copyright (c) 2007, 2024, Oracle and/or its affiliates. All rights reserved.
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <jvmti.h>
#include "agent_common.hpp"
#include <jvmti_tools.h>
#include "JVMTITools.h"

extern "C" {

/* ============================================================================= */

static int loaded_classes_num = 0;

/* ============================================================================= */
static void JNICALL
ClassFileLoadHook (
        jvmtiEnv *jvmti
        , JNIEnv *jni
        , jclass class_being_redefined
        , jobject loader
        , const char* name
        , jobject protection_domain
        , jint class_data_len
        , const unsigned char* class_data
        , jint *new_class_data_len
        , unsigned char** new_class_data
        )
{
    loaded_classes_num++;

    NSK_DISPLAY2("ClassFileLoadHook: class \"%s\", loader %X\n"
            , name
            , loader
            );
}

/* ============================================================================= */
JNIEXPORT jint JNICALL
Java_nsk_jvmti_GetEnv_GetEnv001_GetEnv001_getLoadedClassesCount(JNIEnv *env, jobject owner) {
    return loaded_classes_num;
}

/* ============================================================================= */
/* Agent initialization procedure */
#ifdef STATIC_BUILD
JNIEXPORT jint JNICALL Agent_OnLoad_GetEnv001(JavaVM *jvm, char *options, void *reserved) {
    return Agent_Initialize(jvm, options, reserved);
}
JNIEXPORT jint JNICALL Agent_OnAttach_GetEnv001(JavaVM *jvm, char *options, void *reserved) {
    return Agent_Initialize(jvm, options, reserved);
}
JNIEXPORT jint JNI_OnLoad_GetEnv001(JavaVM *jvm, char *options, void *reserved) {
    return JNI_VERSION_1_8;
}
#endif
jint Agent_Initialize(JavaVM *vm, char *options, void *reserved)
{
    jvmtiEventCallbacks callbacks;
    jvmtiCapabilities caps;
    jvmtiEnv* jvmti;

    if (!NSK_VERIFY(nsk_jvmti_parseOptions(options))) {
        return JNI_ERR;
    }

    if (vm->GetEnv((void **) &jvmti, JVMTI_VERSION_1_1) != JNI_OK || jvmti == nullptr) {
        NSK_COMPLAIN0("JVMTI_VERSION_1_1 isn't supported.");
        return JNI_OK;
    }

    if (!NSK_JVMTI_VERIFY(jvmti->GetCapabilities(&caps)))
        return JNI_ERR;

    caps.can_retransform_classes = 1;

    // Register all necessary JVM capabilities
    if (!NSK_JVMTI_VERIFY(jvmti->AddCapabilities(&caps)))
        return JNI_ERR;

    // Register all necessary event callbacks
    memset(&callbacks, 0, sizeof(callbacks));
    callbacks.ClassFileLoadHook = &ClassFileLoadHook;

    if (!NSK_JVMTI_VERIFY(jvmti->SetEventCallbacks(&callbacks, sizeof(callbacks))))
        return JNI_ERR;

    // Enable class retransformation
    if (!NSK_JVMTI_VERIFY(jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_CLASS_FILE_LOAD_HOOK, nullptr)))
        return JNI_ERR;

    return JNI_OK;
}

}
