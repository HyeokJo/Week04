#include "PCH.h"

#include "Console.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <locale>
#include <mutex>
#include <cstdarg>
#include "../../ErrorHandler.h"

namespace
{
    class FConsoleMessageStorage
    {
    public:
        FConsoleMessageStorage(size_t InCapacity)
            : Capacity(InCapacity)
        {
            Messages.resize(Capacity);
        }

        void PushHistory(FConsoleMessage Message)
        {
			ErrorHandler::Report(
				Message.Level == ELogLevel::Error,
				"Console Log",
				std::format("[{}] [{}] {}", Message.Time, static_cast<int>(Message.Level), Message.Text),
				ErrorHandler::EErrorLevel::Warning);
            if (Count < Capacity)
            {
                size_t Index = (Front + Count) % Capacity;
                Messages[Index] = std::move(Message);
                ++Count;
            }
            else
            {
                Messages[Front] = std::move(Message);
                Front = (Front + 1) % Capacity;
            }

        }

        void PushPending(FConsoleMessage Message)
        {
            PendingBuffers[WriteBufferIndex].push_back(std::move(Message));
        }

        size_t GetMessageCount() const
        {
            // return Messages.size();
            return Count;
        }

        const FConsoleMessage& GetMessage(size_t Index) const
        {
            size_t CircleIndex = (Front + Index) % Capacity;
            return Messages[CircleIndex];
        }

        void Clear()
        {
            Front = 0;
            Count = 0;
        }

        void SwapPendingBuffers()
        {
            std::swap(WriteBufferIndex, ReadBufferIndex);
        }

        void FlushReadBuffer()
        {
            // 원본 직접 참조
            auto& ReadBuffer = PendingBuffers[ReadBufferIndex];

            for (FConsoleMessage& Message : ReadBuffer)
            {
                PushHistory(std::move(Message));
            }

            ReadBuffer.clear();
        }

    private:
        size_t Capacity;
        size_t Front = 0;
        size_t Count = 0;
        TArray<FConsoleMessage> Messages;

        TArray<FConsoleMessage> PendingBuffers[2];

        size_t WriteBufferIndex = 0;
        size_t ReadBufferIndex = 1;
    };

    struct FConsoleState
    {
        std::vector<FConsoleMessageStorage> OutputStorages;
        std::mutex PendingMutex;

        FConsoleState()
        {
            OutputStorages.emplace_back(1000); // STDOut
            OutputStorages.emplace_back(1000); // STDError
        }
    };

    FConsoleState& GetConsoleState()
    {
        static FConsoleState State;
        return State;
    }

    FConsoleMessageStorage* ResolveStorage(FConsoleOutputHandle Handle)
    {
        FConsoleState& State = GetConsoleState();

        if (!Handle.IsValid())
        {
            return nullptr;
        }

        if (Handle.Index >= State.OutputStorages.size())
        {
            return nullptr;
        }

        return &State.OutputStorages[Handle.Index];
    }

    FString GetCurrentTimeString()
    {
        auto Now = std::chrono::system_clock::now();
        std::time_t NowTime = std::chrono::system_clock::to_time_t(Now);

        std::tm LocalTime{};
        localtime_s(&LocalTime, &NowTime);

        std::ostringstream Stream;
        Stream.imbue(std::locale(""));
        Stream << std::put_time(&LocalTime, "%H:%M:%S");

        return FString{ Stream.str() };
    }
}

namespace Console
{
    void Print(FConsoleOutputHandle Handle, FConsoleMessage Message)
    {
        Message.Time = GetCurrentTimeString();

        FConsoleMessageStorage* Storage = ResolveStorage(Handle);

        if (Storage == nullptr)
        {
            return;
        }

        Storage->PushHistory(std::move(Message));
    }

    void Clear(FConsoleOutputHandle Handle)
    {
        FConsoleMessageStorage* Storage = ResolveStorage(Handle);

        if (Storage == nullptr)
            return;

        Storage->Clear();
    }

    void AddLog(
        FConsoleOutputHandle Handle,
        ELogLevel Level,
        ELogCategory Category,
        const char* Format,
        ...)
    {
        va_list Args;
        va_start(Args, Format);

        va_list ArgsCopy;
        va_copy(ArgsCopy, Args);

        int Length = vsnprintf(nullptr, 0, Format, ArgsCopy);

        va_end(ArgsCopy);

        if (Length < 0)
        {
            va_end(Args);
            return;
        }

        FString Text(Length + 1, '\0');

        vsnprintf(
            Text.data(),
            Text.size(),
            Format,
            Args);

        va_end(Args);

        Text.resize(Length);

        FConsoleMessage Message;
        Message.Category = Category;
        Message.Level = Level;
        Message.Text = std::move(Text);

        Message.Time = GetCurrentTimeString();

        // Print(Handle, std::move(Message));

        FConsoleState& State = GetConsoleState();

        {
            std::lock_guard<std::mutex> Lock(State.PendingMutex);

            FConsoleMessageStorage* Storage = ResolveStorage(Handle);

            if (Storage == nullptr)
                return;

            Storage->PushPending(std::move(Message));
        }
    }

    void Flush(FConsoleOutputHandle Handle)
    {
        FConsoleState& State = GetConsoleState();
        FConsoleMessageStorage* Storage = ResolveStorage(Handle);

        if (Storage == nullptr)
            return;
        // lock 수명 짧게
        {
            std::lock_guard<std::mutex> Lock(State.PendingMutex);
            Storage->SwapPendingBuffers();
        }

        Storage->FlushReadBuffer();
    }

    size_t GetMessageCount(FConsoleOutputHandle Handle)
    {
        const FConsoleMessageStorage* Storage = ResolveStorage(Handle);

        return Storage ? Storage->GetMessageCount() : 0;
    }

    const FConsoleMessage& GetMessageAt(
        FConsoleOutputHandle Handle,
        size_t Index)
    {
        return ResolveStorage(Handle)->GetMessage(Index);
    }
}
