#define _WIN32_WINNT 0x0A00

#include "lab3.h"
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>

#define NUMBER_OF_THREADS 11
#define WRITE_COUNT 3

HANDLE tHandles[NUMBER_OF_THREADS];
HANDLE write_lock; // mutex
SRWLOCK phase_lock; // mutex based lock for usage with windows condition variables
CONDITION_VARIABLE phase_cond, thread_finished_cond; // cond variable

HANDLE semH, semI, semK; // semaphores
int phase, finished_threads_count;

unsigned int lab3_thread_graph_id() {
    return 6;
}

const char *lab3_unsynchronized_threads() {
    return "bcd";
}

const char *lab3_sequential_threads() {
    return "hik";
}

void spawn_thread(int thread_id, LPTHREAD_START_ROUTINE func) {
    tHandles[thread_id] = CreateThread(nullptr, 0, func, nullptr, 0, nullptr);
    if (tHandles[thread_id] == nullptr) {
        std::cerr << "Unable to create a thread" << std::endl;
        exit(GetLastError());
    }
}

void load_async(const char *s) {
    for (int i = 0; i < WRITE_COUNT; ++i) {
        WaitForSingleObject(write_lock, INFINITE);
        std::cout << s << std::flush;
        ReleaseMutex(write_lock);
        computation();
    }
    // notify main thread about finished work
    AcquireSRWLockExclusive(&phase_lock);
    finished_threads_count ++;
    WakeAllConditionVariable(&thread_finished_cond);
    ReleaseSRWLockExclusive(&phase_lock);
}

void load_sync(const char *s, HANDLE &cur, HANDLE &next) {
    for (int i = 0; i < WRITE_COUNT; ++i) {
        WaitForSingleObject(cur, INFINITE);
        WaitForSingleObject(write_lock, INFINITE);
        std::cout << s << std::flush;
        ReleaseMutex(write_lock);
        computation();
        ReleaseSemaphore(next, 1, nullptr);
    }
    // notify main thread about finished work
    AcquireSRWLockExclusive(&phase_lock);
    finished_threads_count++;
    WakeAllConditionVariable(&thread_finished_cond);
    ReleaseSRWLockExclusive(&phase_lock);
}

void wait_phase(int phase_num) {
    AcquireSRWLockExclusive(&phase_lock);
    while (phase != phase_num) {
        // wait for wakeup call
        SleepConditionVariableSRW(&phase_cond, &phase_lock, INFINITE, 0);
    }
    ReleaseSRWLockExclusive(&phase_lock);
}

void change_phase(int phase_num) {
    // before changing phases, wait for other threads to finish
    AcquireSRWLockExclusive(&phase_lock);
    int finished_threads;
    if (phase == 1) finished_threads = 1;
    else if (phase >= 2 && phase <= 5) finished_threads = 3;
    else if (phase == 6) finished_threads = 2;
    else exit(2); // invalid phase

    while (finished_threads_count != finished_threads) {
        // wait for other threads to finish
        // wakes up to check condition every time other thread finished printing
        SleepConditionVariableSRW(&thread_finished_cond, &phase_lock, INFINITE, 0);
    }

    phase = phase_num;
    finished_threads_count = 0;
    // wake up all threads that are waiting for next phase
    WakeAllConditionVariable(&phase_cond);
    ReleaseSRWLockExclusive(&phase_lock);
}

DWORD WINAPI thread_a(LPVOID pVoid); // 0
DWORD WINAPI thread_b(LPVOID pVoid); // 1
DWORD WINAPI thread_c(LPVOID pVoid); // 2
DWORD WINAPI thread_d(LPVOID pVoid); // 3
DWORD WINAPI thread_e(LPVOID pVoid); // 4
DWORD WINAPI thread_g(LPVOID pVoid); // 5
DWORD WINAPI thread_f(LPVOID pVoid); // 6
DWORD WINAPI thread_h(LPVOID pVoid); // 7
DWORD WINAPI thread_i(LPVOID pVoid); // 8
DWORD WINAPI thread_k(LPVOID pVoid); // 9
DWORD WINAPI thread_m(LPVOID pVoid); // 10

DWORD WINAPI thread_a(LPVOID pVoid) {
    load_async("a");

    change_phase(2);
    spawn_thread(1, thread_b);
    spawn_thread(2, thread_c);
    spawn_thread(3, thread_d);

    WaitForSingleObject(tHandles[1], INFINITE); // b
    return 0;
}

DWORD WINAPI thread_b(LPVOID pVoid) {
    load_async("b");

    change_phase(3);
    spawn_thread(5, thread_g);

    WaitForSingleObject(tHandles[5], INFINITE); // g
    return 0;
}


DWORD WINAPI thread_c(LPVOID pVoid) {
    load_async("c");
    spawn_thread(4, thread_e);
    return 0;
}

DWORD WINAPI thread_d(LPVOID pVoid) {
    load_async("d");
    wait_phase(3);
    load_async("d");
    return 0;
}

DWORD WINAPI thread_e(LPVOID pVoid) {
    wait_phase(3);
    load_async("e");

    WaitForSingleObject(tHandles[3], INFINITE);
    change_phase(4);
    spawn_thread(6, thread_f);
    spawn_thread(7, thread_h);
    return 0;
}

DWORD WINAPI thread_g(LPVOID pVoid) {
    load_async("g");
    wait_phase(4);
    load_async("g");

    WaitForSingleObject(tHandles[6], INFINITE);
    change_phase(5);
    spawn_thread(8, thread_i);
    spawn_thread(9, thread_k);

    WaitForSingleObject(tHandles[9], INFINITE); // k
    return 0;
}

DWORD WINAPI thread_f(LPVOID pVoid) {
    load_async("f");
    return 0;
}

DWORD WINAPI thread_h(LPVOID pVoid) { // sync hik
    load_async("h");
    wait_phase(5);
    load_sync("h", semH, semI);

    WaitForSingleObject(tHandles[8], INFINITE); // i
    change_phase(6);
    spawn_thread(10, thread_m);
    return 0;
}

DWORD WINAPI thread_i(LPVOID pVoid) {
    load_sync("i", semI, semK);
    return 0;
}

DWORD WINAPI thread_k(LPVOID pVoid) {
    load_sync("k", semK, semH);
    wait_phase(6);
    load_async("k");

    WaitForSingleObject(tHandles[10], INFINITE); // m
    return 0;
}

DWORD WINAPI thread_m(LPVOID pVoid) {
    load_async("m");
    return 0;
}


int lab3_init() {
    // initialize mutex
    write_lock = CreateMutex(NULL, FALSE, NULL);
    if (write_lock == nullptr) {
        std::cerr << "Mutex init failed" << std::endl;
        return GetLastError();
    }
    InitializeSRWLock(&phase_lock);

    // init cond var
    InitializeConditionVariable(&phase_cond);
    InitializeConditionVariable(&thread_finished_cond);

    // initialize semaphores
    semH = CreateSemaphore(NULL, 1, 1, NULL);
    semI = CreateSemaphore(NULL, 0, 1, NULL);
    semK = CreateSemaphore(NULL, 0, 1, NULL);

    if (semH == nullptr || semI == nullptr || semK == nullptr) {
        printf("Unable to create semaphore");
        return GetLastError();
    }

    phase = 1;
    finished_threads_count = 0;

    spawn_thread(0, thread_a);

    WaitForSingleObject(tHandles[0], INFINITE);

    CloseHandle(write_lock);
    CloseHandle(phase_lock);

    CloseHandle(semH);
    CloseHandle(semI);
    CloseHandle(semK);

    for (HANDLE &tHandle : tHandles) CloseHandle(tHandle);

    return 0;
}
