#pragma once
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "CoreMinimal.h"
#include <utility>

struct FStressThreadLauncher {
    template<class Callable> struct FRunner final : FRunnable {
        Callable Work;
        explicit FRunner(Callable InWork) : Work(std::move(InWork)) {}
        uint32 Run() override { Work(); return 0; }
    };
    template<class Callable> struct FHandle {
        TUniquePtr<FRunner<Callable>> Runner;
        TUniquePtr<FRunnableThread> Thread;
        void Join() { if (Thread) Thread->WaitForCompletion(); }
    };
    template<class Callable> FHandle<Callable> Launch(const char* Name, Callable Work) {
        auto Runner = MakeUnique<FRunner<Callable>>(std::move(Work));
        TUniquePtr<FRunnableThread> Thread(FRunnableThread::Create(Runner.Get(), UTF8_TO_TCHAR(Name)));
        if (!Thread) UE_LOG(LogTemp, Fatal, TEXT("Stress thread creation failed: %s"), UTF8_TO_TCHAR(Name));
        return {MoveTemp(Runner), MoveTemp(Thread)};
    }
};
