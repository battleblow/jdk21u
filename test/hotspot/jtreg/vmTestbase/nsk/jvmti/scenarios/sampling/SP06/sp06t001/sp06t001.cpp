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
static JNIEnv* jni = nullptr;
static jvmtiEnv *jvmti = nullptr;
static jlong timeout = 0;

/* constants */
#define THREADS_COUNT   6
#define EVENTS_COUNT    2
#define MAX_NAME_LENGTH 100
#define MAX_STACK_SIZE  100

/* tested events */
static jvmtiEvent eventsList[EVENTS_COUNT] = {
    JVMTI_EVENT_COMPILED_METHOD_LOAD,
    JVMTI_EVENT_COMPILED_METHOD_UNLOAD
};

/* thread description structure */
typedef struct {
    char threadName[MAX_NAME_LENGTH];
    char methodName[MAX_NAME_LENGTH];
    char methodSig[MAX_NAME_LENGTH];
    int minDepth;
    jthread thread;
    jclass cls;
    jmethodID method;
    int methodCompiled;
} ThreadDesc;

/* descriptions of tested threads */
static ThreadDesc threadsDesc[THREADS_COUNT] = {
    { "threadRunning", "testedMethod", "(ZI)V", 2, nullptr, nullptr, nullptr, NSK_FALSE },
    { "threadEntering", "testedMethod", "(ZI)V", 2, nullptr, nullptr, nullptr, NSK_FALSE },
    { "threadWaiting", "testedMethod", "(ZI)V", 2, nullptr, nullptr, nullptr, NSK_FALSE },
    { "threadSleeping", "testedMethod", "(ZI)V", 2, nullptr, nullptr, nullptr, NSK_FALSE },
    { "threadRunningInterrupted", "testedMethod", "(ZI)V", 2, nullptr, nullptr, nullptr, NSK_FALSE },
    { "threadRunningNative", "testedMethod", "(ZI)V", 2, nullptr, nullptr, nullptr, NSK_FALSE }
};

/* indexes of known threads */
static const int interruptedThreadIndex = THREADS_COUNT - 2;
static const int nativeThreadIndex = THREADS_COUNT - 1;

/* ============================================================================= */

/* testcase(s) */
static int prepare();
static int generateEvents();
static int checkSuspendedThreads();
static int suspendThreadsIndividually(int suspend);
static int clean();

/* ============================================================================= */

/** Agent algorithm. */
static void JNICALL
agentProc(jvmtiEnv* jvmti, JNIEnv* agentJNI, void* arg) {
    jni = agentJNI;

    NSK_DISPLAY0("Wait for debuggee to become ready\n");
    if (!nsk_jvmti_waitForSync(timeout))
        return;

    {
        NSK_DISPLAY0("Prepare data\n");
        if (!prepare()) {
            nsk_jvmti_setFailStatus();
            return;
        }

        NSK_DISPLAY0("Generate missed events\n");
        if (!generateEvents())
            return;

        NSK_DISPLAY0("Suspend each thread\n");
        if (!suspendThreadsIndividually(NSK_TRUE))
            return;

        NSK_DISPLAY0("Check stack frames of suspended threads\n");
        if (!checkSuspendedThreads())
            return;

        NSK_DISPLAY0("Resume each thread\n");
        if (!suspendThreadsIndividually(NSK_FALSE))
            return;

        NSK_DISPLAY0("Clean data\n");
        if (!clean()) {
            nsk_jvmti_setFailStatus();
            return;
        }
    }

    NSK_DISPLAY0("Let debuggee to finish\n");
    if (!nsk_jvmti_resumeSync())
        return;
}

/* ============================================================================= */

/**
 * Generate missed events (COMPILED_METHOD_LOAD only).
 */
static int generateEvents() {
    if (!NSK_JVMTI_VERIFY(jvmti->GenerateEvents(JVMTI_EVENT_COMPILED_METHOD_LOAD))) {
        nsk_jvmti_setFailStatus();
        return NSK_FALSE;
    }
    return NSK_TRUE;
}

/**
 * Prepare data.
 *    - clean threads list
 *    - get all live threads
 *    - get threads name
 *    - find tested threads
 *    - make global refs
 *    - enable events
 */
static int prepare() {
    jthread *allThreadsList = nullptr;
    jint allThreadsCount = 0;
    int found = 0;
    int i;

    NSK_DISPLAY1("Find tested threads: %d\n", THREADS_COUNT);

    /* clean threads list */
    for (i = 0; i < THREADS_COUNT; i++) {
        threadsDesc[i].thread = (jthread)nullptr;
        threadsDesc[i].method = (jmethodID)nullptr;
        threadsDesc[i].methodCompiled = NSK_FALSE;
    }

    /* get all live threads */
    if (!NSK_JVMTI_VERIFY(jvmti->GetAllThreads(&allThreadsCount, &allThreadsList)))
        return NSK_FALSE;

    if (!NSK_VERIFY(allThreadsCount > 0 && allThreadsList != nullptr))
        return NSK_FALSE;

    /* find tested threads */
    for (i = 0; i < allThreadsCount; i++) {
        jvmtiThreadInfo threadInfo;

        if (!NSK_VERIFY(allThreadsList[i] != nullptr))
            return NSK_FALSE;

        if (!NSK_JVMTI_VERIFY(jvmti->GetThreadInfo(allThreadsList[i], &threadInfo)))
            return NSK_FALSE;

        if (threadInfo.name != nullptr) {
            int j;

            for (j = 0; j < THREADS_COUNT; j++) {
                if (strcmp(threadInfo.name, threadsDesc[j].threadName) == 0) {
                    threadsDesc[j].thread = allThreadsList[i];
                    NSK_DISPLAY3("    thread #%d (%s): 0x%p\n",
                                            j, threadInfo.name, (void*)threadsDesc[j].thread);
                }
            }
        }
    }

    /* deallocate all threads list */
    if (!NSK_JVMTI_VERIFY(jvmti->Deallocate((unsigned char*)allThreadsList)))
        return NSK_FALSE;

    /* check if all tested threads found */
    found = 0;
    for (i = 0; i < THREADS_COUNT; i++) {
        if (threadsDesc[i].thread == nullptr) {
            NSK_COMPLAIN2("Not found tested thread #%d (%s)\n", i, threadsDesc[i].threadName);
        } else {
            found++;
        }
    }

    if (found < THREADS_COUNT)
        return NSK_FALSE;

    /* get threads class and frame method */
    NSK_DISPLAY0("Find tested methods:\n");
    for (i = 0; i < THREADS_COUNT; i++) {

        if (!NSK_JNI_VERIFY(jni, (threadsDesc[i].cls =
                jni->GetObjectClass(threadsDesc[i].thread)) != nullptr))
            return NSK_FALSE;

        if (!NSK_JNI_VERIFY(jni, (threadsDesc[i].method =
                jni->GetMethodID(threadsDesc[i].cls, threadsDesc[i].methodName, threadsDesc[i].methodSig)) != nullptr))
            return NSK_FALSE;

        NSK_DISPLAY4("    thread #%d (%s): 0x%p (%s)\n",
                                i, threadsDesc[i].threadName,
                                (void*)threadsDesc[i].method,
                                threadsDesc[i].methodName);
    }

    /* make global refs */
    for (i = 0; i < THREADS_COUNT; i++) {
        if (!NSK_JNI_VERIFY(jni, (threadsDesc[i].thread = (jthread)
                jni->NewGlobalRef(threadsDesc[i].thread)) != nullptr))
            return NSK_FALSE;
        if (!NSK_JNI_VERIFY(jni, (threadsDesc[i].cls = (jclass)
                jni->NewGlobalRef(threadsDesc[i].cls)) != nullptr))
            return NSK_FALSE;
    }

    NSK_DISPLAY0("Enable tested events\n");
    if (!nsk_jvmti_enableEvents(JVMTI_ENABLE, EVENTS_COUNT, eventsList, nullptr))
        return NSK_FALSE;

    return NSK_TRUE;
}

/**
 * Suspend or resume tested threads.
 */
static int suspendThreadsIndividually(int suspend) {
    int i;

    for (i = 0; i < THREADS_COUNT; i++) {
        if (suspend) {
            NSK_DISPLAY2("    suspend thread #%d (%s)\n", i, threadsDesc[i].threadName);
            if (!NSK_JVMTI_VERIFY(jvmti->SuspendThread(threadsDesc[i].thread)))
                nsk_jvmti_setFailStatus();
        } else {
            NSK_DISPLAY2("    resume thread #%d (%s)\n", i, threadsDesc[i].threadName);
            if (!NSK_JVMTI_VERIFY(jvmti->ResumeThread(threadsDesc[i].thread)))
                nsk_jvmti_setFailStatus();
        }
    }
    return NSK_TRUE;
}

/**
 * Testcase: check tested threads.
 *    - invoke getFrameCount() for each thread
 *    - check if frameCount is not less than minimal stack depth
 *    - invoke getStackTrace() for each thread
 *    - check if stack depth is equal to frameCount
 *
 * Returns NSK_TRUE if test may continue; or NSK_FALSE for test break.
 */
static int checkSuspendedThreads() {
    char kind[256] = "";
    int i;

    /* check each thread */
    for (i = 0; i < THREADS_COUNT; i++) {
        jint frameCount = 0;
        jint frameStackSize = 0;
        jvmtiFrameInfo frameStack[MAX_STACK_SIZE];
        int found = 0;

        /* make proper kind */
        strcpy(kind, threadsDesc[i].methodCompiled ? "compiled " : "not compiled ");
        NSK_DISPLAY2("  thread #%d (%s):\n", i, threadsDesc[i].threadName);

        /* get frame count */
        if (!NSK_JVMTI_VERIFY(jvmti->GetFrameCount(threadsDesc[i].thread, &frameCount))) {
            nsk_jvmti_setFailStatus();
            return NSK_TRUE;
        }

        NSK_DISPLAY1("    frameCount:  %d\n", (int)frameCount);

        /* get stack trace */
        if (!NSK_JVMTI_VERIFY(
                jvmti->GetStackTrace(threadsDesc[i].thread, 0, MAX_STACK_SIZE, frameStack, &frameStackSize))) {
            nsk_jvmti_setFailStatus();
            return NSK_TRUE;
        }

        NSK_DISPLAY1("    stack depth: %d\n", (int)frameStackSize);

        /* check frame count */
        if (frameCount < threadsDesc[i].minDepth) {
            NSK_COMPLAIN5("Too few frameCount of %s thread #%d (%s):\n"
                            "#   got frameCount:   %d\n"
                            "#   expected minimum: %d\n",
                            kind, i, threadsDesc[i].threadName,
                            (int)frameCount, threadsDesc[i].minDepth);
            nsk_jvmti_setFailStatus();
        }

        if (frameStackSize != frameCount) {
            NSK_COMPLAIN5("Different frames count for %s thread #%d (%s):\n"
                            "#   getStackTrace(): %d\n"
                            "#   getFrameCount(): %d\n",
                            kind, i, threadsDesc[i].threadName,
                            (int)frameStackSize, (int)frameCount);
            nsk_jvmti_setFailStatus();
        }

    }

    /* test may continue */
    return NSK_TRUE;
}

/**
 * Clean data.
 *   - disable events
 *   - dispose global references to tested threads
 */
static int clean() {
    int i;

    NSK_DISPLAY0("Disable events\n");
    if (!nsk_jvmti_enableEvents(JVMTI_DISABLE, EVENTS_COUNT, eventsList, nullptr))
        return NSK_FALSE;

    NSK_DISPLAY0("Dispose global references to threads\n");
    for (i = 0; i < THREADS_COUNT; i++) {
        NSK_TRACE(jni->DeleteGlobalRef(threadsDesc[i].thread));
        NSK_TRACE(jni->DeleteGlobalRef(threadsDesc[i].cls));
    }

    return NSK_TRUE;
}

/* ============================================================================= */

/**
 * COMPILED_METHOD_LOAD callback.
 *   - turn on flag that method is compiled
 */
JNIEXPORT void JNICALL
callbackCompiledMethodLoad(jvmtiEnv* jvmti, jmethodID method,
                            jint code_size, const void* code_addr,
                            jint map_length, const jvmtiAddrLocationMap* map,
                            const void* compile_info) {
    int i;

    /* check if event is for tested method and turn flag on */
    for (i = 0; i < THREADS_COUNT; i++) {
        if (threadsDesc[i].method == method) {
            threadsDesc[i].methodCompiled = NSK_TRUE;

            NSK_DISPLAY2("  COMPILED_METHOD_LOAD for method #%d (%s):\n",
                                i, threadsDesc[i].methodName);
            NSK_DISPLAY1("    methodID:   0x%p\n",
                                (void*)threadsDesc[i].method);
            NSK_DISPLAY1("    code_size:  %d\n",
                                (int)code_size);
            NSK_DISPLAY1("    map_length: %d\n",
                                (int)map_length);
            break;
        }
    }
}

/**
 * COMPILED_METHOD_UNLOAD callback.
 *   - turn off flag that method is compiled
 */
JNIEXPORT void JNICALL
callbackCompiledMethodUnload(jvmtiEnv* jvmti, jmethodID method,
                             const void* code_addr) {
    int i;

    /* check if event is for tested method and turn flag off */
    for (i = 0; i < THREADS_COUNT; i++) {
        if (threadsDesc[i].method == method) {
            threadsDesc[i].methodCompiled = NSK_FALSE;

            NSK_DISPLAY2("  COMPILED_METHOD_UNLOAD for method #%d (%s):\n",
                                i, threadsDesc[i].methodName);
            NSK_DISPLAY1("    methodID:   0x%p\n",
                                (void*)threadsDesc[i].method);
            break;
        }
    }
}

/* ============================================================================= */

volatile int testedThreadReady = NSK_FALSE;
volatile int testedThreadShouldFinish = NSK_FALSE;

/** Native running method in tested thread. */
JNIEXPORT void JNICALL
Java_nsk_jvmti_scenarios_sampling_SP06_sp06t001ThreadRunningNative_testedMethod(JNIEnv* jni,
                                                                            jobject obj,
                                                                            jboolean simulate,
                                                                            jint i) {
    if (!simulate) {
        volatile int k = 0, n = 1000;

        /* run in a continous loop */
        testedThreadReady = NSK_TRUE;
        while (!testedThreadShouldFinish) {
            if (n <= 0)
                n = 1000;
            if (k >= n)
                k = 0;
            k++;
        }
    }
}

/** Wait for native method is running. */
JNIEXPORT jboolean JNICALL
Java_nsk_jvmti_scenarios_sampling_SP06_sp06t001ThreadRunningNative_checkReady(JNIEnv* jni,
                                                                            jobject obj) {
    while (!testedThreadReady) {
        nsk_jvmti_sleep(1000);
    }
    return testedThreadReady ? JNI_TRUE : JNI_FALSE;
}

/** Let native method to finish. */
JNIEXPORT void JNICALL
Java_nsk_jvmti_scenarios_sampling_SP06_sp06t001ThreadRunningNative_letFinish(JNIEnv* jni,
                                                                            jobject obj) {
    testedThreadShouldFinish = NSK_TRUE;
}

/* ============================================================================= */

/** Agent library initialization. */
#ifdef STATIC_BUILD
JNIEXPORT jint JNICALL Agent_OnLoad_sp06t001(JavaVM *jvm, char *options, void *reserved) {
    return Agent_Initialize(jvm, options, reserved);
}
JNIEXPORT jint JNICALL Agent_OnAttach_sp06t001(JavaVM *jvm, char *options, void *reserved) {
    return Agent_Initialize(jvm, options, reserved);
}
JNIEXPORT jint JNI_OnLoad_sp06t001(JavaVM *jvm, char *options, void *reserved) {
    return JNI_VERSION_1_8;
}
#endif
jint Agent_Initialize(JavaVM *jvm, char *options, void *reserved) {

    if (!NSK_VERIFY(nsk_jvmti_parseOptions(options)))
        return JNI_ERR;

    timeout = nsk_jvmti_getWaitTime() * 60 * 1000;

    if (!NSK_VERIFY((jvmti =
            nsk_jvmti_createJVMTIEnv(jvm, reserved)) != nullptr))
        return JNI_ERR;

    {
        jvmtiCapabilities caps;
        memset(&caps, 0, sizeof(caps));
        caps.can_suspend = 1;
        caps.can_generate_compiled_method_load_events = 1;
        if (!NSK_JVMTI_VERIFY(jvmti->AddCapabilities(&caps)))
            return JNI_ERR;
    }

    {
        jvmtiEventCallbacks eventCallbacks;
        memset(&eventCallbacks, 0, sizeof(eventCallbacks));
        eventCallbacks.CompiledMethodLoad = callbackCompiledMethodLoad;
        eventCallbacks.CompiledMethodUnload = callbackCompiledMethodUnload;
        if (!NSK_JVMTI_VERIFY(jvmti->SetEventCallbacks(&eventCallbacks, sizeof(eventCallbacks))))
            return JNI_ERR;
    }

    if (!NSK_VERIFY(nsk_jvmti_setAgentProc(agentProc, nullptr)))
        return JNI_ERR;

    return JNI_OK;
}

/* ============================================================================= */

}
