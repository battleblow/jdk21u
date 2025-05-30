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
#include "jvmti.h"
#include "agent_common.hpp"
#include "JVMTITools.h"

extern "C" {


#define PASSED 0
#define STATUS_FAILED 2

#define RETURN_FAILED errCode = STATUS_FAILED; fflush(0); return

static jvmtiEnv *jvmti = nullptr;
static jvmtiCapabilities caps;
static jvmtiEventCallbacks callbacks;
static jint errCode = PASSED;
static jboolean printdump = JNI_TRUE;
static jmethodID midCheckPoint = nullptr;
static jmethodID midRun = nullptr;

static jint framesExpected = 0;
static jint framesCount = 0;

static const char *cls_exp = "Lnsk/jvmti/unit/ForceEarlyReturn/earlyretvoid$earlyretThread;";
static const char *name_exp = "countDown";
static const char *sig_exp = "(I)V";
static const char *argName = "nestingCount";

void check(jvmtiEnv *jvmti_env, jthread thr, jmethodID mid,
           jlocation loc, jint frame_no) {
    jvmtiError err;
    jclass cls;
    jlocation loc_exp = (frame_no == 0) ? 25 : 11;
    char *sigClass, *name, *sig, *generic;
    jvmtiLocalVariableEntry *table = nullptr;
    jint entryCount = 0;
    jint argValue;
    jint j;

    err = jvmti_env->GetMethodDeclaringClass(mid, &cls);
    if (err != JVMTI_ERROR_NONE) {
        printf("(GetMethodDeclaringClass#%d) unexpected error: %s (%d)\n",
               frame_no, TranslateError(err), err);
        RETURN_FAILED;
    }

    err = jvmti_env->GetClassSignature(cls, &sigClass, &generic);
    if (err != JVMTI_ERROR_NONE) {
        printf("(GetClassSignature#%d) unexpected error: %s (%d)\n",
               frame_no, TranslateError(err), err);
        RETURN_FAILED;
    }

    err = jvmti_env->GetMethodName(mid, &name, &sig, &generic);
    if (err != JVMTI_ERROR_NONE) {
        printf("(GetMethodName#%d) unexpected error: %s (%d)\n",
               frame_no, TranslateError(err), err);
        RETURN_FAILED;
    }

    /* Get Local Variable Table to be able to get the argument value
     * from current method frame and compare it with the expected value
     */
    err = jvmti_env->GetLocalVariableTable(mid, &entryCount, &table);
    if (err != JVMTI_ERROR_NONE) {
        printf("(GetLocalVariableTable#%d) unexpected error: %s (%d)\n",
               frame_no, TranslateError(err), err);
        RETURN_FAILED;
    }
    if (table != nullptr) {
        for (j = 0; j < entryCount; j++) {
            if (strcmp(table[j].name, argName) == 0) {
                err = jvmti_env->GetLocalInt(thr, 0,
                    table[j].slot, &argValue);
                if (err != JVMTI_ERROR_NONE) {
                    printf("(GetLocalInt#%d) unexpected error: %s (%d)\n",
                           frame_no, TranslateError(err), err);
                    RETURN_FAILED;
                }
            }
        }
    }

    if (printdump == JNI_TRUE) {
        printf(">>> step %d: \"%s.%s%s\"\n", frame_no, sigClass, name, sig);
        printf(">>>   location: %#x%08x", (jint)(loc >> 32), (jint)loc);
        printf(", arg value: %d\n", argValue);
    }

    if (sigClass == nullptr || strcmp(sigClass, cls_exp) != 0) {
        printf("(step %d) wrong class sig: \"%s\",\n", frame_no, sigClass);
        printf(" expected: \"%s\"\n", cls_exp);
        RETURN_FAILED;
    }
    if (name == nullptr || strcmp(name, name_exp) != 0) {
        printf("(step %d) wrong method name: \"%s\",", frame_no, name);
        printf(" expected: \"%s\"\n", name_exp);
        RETURN_FAILED;
    }
    if (sig == nullptr || strcmp(sig, sig_exp) != 0) {
        printf("(step %d) wrong method sig: \"%s\",", frame_no, sig);
        printf(" expected: \"%s\"\n", sig_exp);
        RETURN_FAILED;
    }
    if (loc != loc_exp) {
        printf("(step %d) wrong location: %#x%08x,",
               frame_no, (jint)(loc >> 32), (jint)loc);
        printf(" expected: %#x\n", (jint)loc_exp);
        RETURN_FAILED;
    }
    if (argValue != frame_no) {
        printf("(step %d) wrong argument value: %d,", frame_no, argValue);
        printf(" expected: %d\n", frame_no);
        RETURN_FAILED;
    }

    if (sigClass != nullptr) {
        jvmti_env->Deallocate((unsigned char*)sigClass);
    }
    if (name != nullptr) {
        jvmti_env->Deallocate((unsigned char*)name);
    }
    if (sig != nullptr) {
        jvmti_env->Deallocate((unsigned char*)sig);
    }
    if (table != nullptr) {
        for (j = 0; j < entryCount; j++) {
            jvmti_env->Deallocate((unsigned char*)(table[j].name));
            jvmti_env->Deallocate((unsigned char*)(table[j].signature));
        }
        jvmti_env->Deallocate((unsigned char*)table);
    }
    fflush(0);
}

void JNICALL Breakpoint(jvmtiEnv *jvmti_env, JNIEnv *env,
        jthread thread, jmethodID method, jlocation location) {
    jvmtiError err;

    if (midCheckPoint != method) {
        printf("bp: don't know where we get called from");
        RETURN_FAILED;
    }

    if (printdump == JNI_TRUE) {
        printf(">>> breakpoint in checkPoint\n");
    }

    err = jvmti_env->ClearBreakpoint(midCheckPoint, 0);
    if (err != JVMTI_ERROR_NONE) {
        printf("(ClearBreakpoint) unexpected error: %s (%d)\n",
               TranslateError(err), err);
        RETURN_FAILED;
    }

    err = jvmti_env->SetEventNotificationMode(JVMTI_ENABLE,
        JVMTI_EVENT_SINGLE_STEP, thread);
    if (err != JVMTI_ERROR_NONE) {
        printf("Cannot enable single step: %s (%d)\n",
               TranslateError(err), err);
        RETURN_FAILED;
    }

    err = jvmti_env->ForceEarlyReturnVoid(thread);
    if (err != JVMTI_ERROR_NONE) {
        printf("(ForceEarlyReturn) unexpected error: %s (%d)\n",
               TranslateError(err), err);
        RETURN_FAILED;
    }
    fflush(0);
}

void JNICALL SingleStep(jvmtiEnv *jvmti_env, JNIEnv *env,
        jthread thread, jmethodID method, jlocation location) {
    jvmtiError err;

    if (method == midRun) {
        if (printdump == JNI_TRUE) {
            printf(">>> returned early %d frames till method \"run()\"\n",
                   framesCount);
        }

        err = jvmti_env->SetEventNotificationMode(JVMTI_DISABLE,
            JVMTI_EVENT_SINGLE_STEP, thread);
        if (err != JVMTI_ERROR_NONE) {
            printf("Cannot disable single step: %s (%d)\n",
                   TranslateError(err), err);
            RETURN_FAILED;
        }
    } else {
        check(jvmti_env, thread, method, location, framesCount);
        framesCount++;

        err = jvmti_env->ForceEarlyReturnVoid(thread);
        if (err != JVMTI_ERROR_NONE) {
            printf("(ForceEarlyReturn) unexpected error: %s (%d)\n",
                   TranslateError(err), err);
            RETURN_FAILED;
        }
    }
    fflush(0);
}

#ifdef STATIC_BUILD
JNIEXPORT jint JNICALL Agent_OnLoad_earlyretvoid(JavaVM *jvm, char *options, void *reserved) {
    return Agent_Initialize(jvm, options, reserved);
}
JNIEXPORT jint JNICALL Agent_OnAttach_earlyretvoid(JavaVM *jvm, char *options, void *reserved) {
    return Agent_Initialize(jvm, options, reserved);
}
JNIEXPORT jint JNI_OnLoad_earlyretvoid(JavaVM *jvm, char *options, void *reserved) {
    return JNI_VERSION_1_8;
}
#endif
jint Agent_Initialize(JavaVM *jvm, char *options, void *reserved) {
    jvmtiError err;
    jint res;

    if (options != nullptr && strcmp(options, "printdump") == 0) {
        printf("Printdump is turned on!\n");

        printdump = JNI_TRUE;
    }

    res = jvm->GetEnv((void **) &jvmti, JVMTI_VERSION_1_1);
    if (res != JNI_OK || jvmti == nullptr) {
        printf("Wrong error code from a valid call to GetEnv!\n");
        return JNI_ERR;
    }

    err = jvmti->GetPotentialCapabilities(&caps);
    if (err != JVMTI_ERROR_NONE) {
        printf("(GetPotentialCapabilities) unexpected error: %s (%d)\n",
               TranslateError(err), err);
        return JNI_ERR;
    }

    err = jvmti->AddCapabilities(&caps);
    if (err != JVMTI_ERROR_NONE) {
        printf("(AddCapabilities) unexpected error: %s (%d)\n",
               TranslateError(err), err);
        return JNI_ERR;
    }

    err = jvmti->GetCapabilities(&caps);
    if (err != JVMTI_ERROR_NONE) {
        printf("(GetCapabilities) unexpected error: %s (%d)\n",
               TranslateError(err), err);
        return JNI_ERR;
    }

    if (!caps.can_force_early_return) {
        printf("Warning: ForceEarlyReturn is not implemented\n");
    }

    if (caps.can_generate_breakpoint_events &&
            caps.can_generate_single_step_events) {
        callbacks.Breakpoint = &Breakpoint;
        callbacks.SingleStep = &SingleStep;
        err = jvmti->SetEventCallbacks(&callbacks, sizeof(callbacks));
        if (err != JVMTI_ERROR_NONE) {
            printf("(SetEventCallbacks) unexpected error: %s (%d)\n",
                   TranslateError(err), err);
            return JNI_ERR;
        }
    } else {
        printf("Warning: Breakpoint or SingleStep event are not implemented\n");
    }

    return JNI_OK;
}

JNIEXPORT void JNICALL
Java_nsk_jvmti_unit_ForceEarlyReturn_earlyretvoid_getReady(
    JNIEnv *env, jclass c, jclass cls, jint depth) {
    jvmtiError err;

    if (jvmti == nullptr) {
        printf("JVMTI client was not properly loaded!\n");
        RETURN_FAILED;
    }

    if (!caps.can_force_early_return ||
            !caps.can_generate_breakpoint_events ||
            !caps.can_generate_single_step_events) {
        return;
    }

    midRun = env->GetMethodID(cls, "run", "()V");
    if (midRun == nullptr) {
        printf("Cannot find Method ID for method run\n");
        RETURN_FAILED;
    }

    midCheckPoint = env->GetMethodID(cls, "checkPoint", "()V");
    if (midCheckPoint == nullptr) {
        printf("Cannot find Method ID for method checkPoint\n");
        RETURN_FAILED;
    }

    err = jvmti->SetBreakpoint(midCheckPoint, 0);
    if (err != JVMTI_ERROR_NONE) {
        printf("(SetBreakpoint) unexpected error: %s (%d)\n",
               TranslateError(err), err);
        RETURN_FAILED;
    }

    err = jvmti->SetEventNotificationMode(JVMTI_ENABLE,
        JVMTI_EVENT_BREAKPOINT, nullptr);
    if (err != JVMTI_ERROR_NONE) {
        printf("Failed to enable BREAKPOINT event: %s (%d)\n",
               TranslateError(err), err);
        RETURN_FAILED;
    } else {
        framesExpected = depth;
    }
    fflush(0);
}

JNIEXPORT jint JNICALL
Java_nsk_jvmti_unit_ForceEarlyReturn_earlyretvoid_check(JNIEnv *env, jclass cls) {
    if (framesCount != framesExpected) {
        printf("Wrong number of returned early frames: %d, expected: %d\n",
            framesCount, framesExpected);
        errCode = STATUS_FAILED;
    }
    fflush(0);
    return errCode;
}

}
