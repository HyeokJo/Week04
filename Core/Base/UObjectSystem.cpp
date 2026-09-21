#include "PCH.h"
#include "UObjectSystem.h"

#include "FGuid.h"
#include "FObjectItem.h"
#include "UObject.h"

namespace
{
    struct FObjectRegistryState
    {
        std::vector<FObjectItem> ObjectItems;
        TMap<FGuid, std::uint32_t> GuidToIndexMap;
        std::vector<std::uint32_t> FreeIndices;
        std::uint32_t ObjectCount = 0;
    };

    FObjectRegistryState& GetRegistryState()
    {
        static FObjectRegistryState State;
        return State;
    }
}

FObjectHandle UObjectSystem::Register(UObject* Object)
{
    if (Object == nullptr)
    {
        return {};
    }

    FObjectRegistryState& State = GetRegistryState();

    std::uint32_t Index;

    if (!State.FreeIndices.empty())
    {
        Index = State.FreeIndices.back();
        State.FreeIndices.pop_back();
    }
    else
    {
        Index = static_cast<std::uint32_t>(
            State.ObjectItems.size());

        State.ObjectItems.emplace_back();
    }

    FObjectItem& Item = State.ObjectItems[Index];
    Item.Object = Object;

    const FObjectHandle Handle
    {
        Index,
        Item.Generation
    };

    Object->SetHandle(Handle);

    ++State.ObjectCount;

    State.GuidToIndexMap[Object->GetGuid()] = Index;

    return Handle;
}

FObjectHandle UObjectSystem::RegisterWithGuid(UObject* Object, const FGuid& InGuid)
{
    Object->RestoreGuid(InGuid);

    return Register(Object);
}


void UObjectSystem::Unregister(
    UObject* Object,
    FObjectHandle Handle)
{
    if (Object == nullptr)
    {
        return;
    }

    FObjectRegistryState& State = GetRegistryState();

    if (Handle.Index >= State.ObjectItems.size())
    {
        return;
    }

    FObjectItem& Item = State.ObjectItems[Handle.Index];

    if (Item.Object != Object)
    {
        return;
    }

    if (Item.Generation != Handle.Generation)
    {
        return;
    }

    State.GuidToIndexMap.erase(Object->GetGuid());

    Item.Object = nullptr;

    ++Item.Generation;

    // Generation 0은 Invalid Handle 용도로 비워 둔다.
    if (Item.Generation == 0)
    {
        ++Item.Generation;
    }

    State.FreeIndices.push_back(Handle.Index);

    --State.ObjectCount;
}

UObject* UObjectSystem::Resolve(FObjectHandle Handle)
{
    if (!Handle.IsValid())
    {
        return nullptr;
    }

    FObjectRegistryState& State = GetRegistryState();

    if (Handle.Index >= State.ObjectItems.size())
    {
        return nullptr;
    }

    FObjectItem& Item = State.ObjectItems[Handle.Index];

    if (Item.Object == nullptr)
    {
        return nullptr;
    }

    if (Item.Generation != Handle.Generation)
    {
        return nullptr;
    }

    return Item.Object;
}

FObjectHandle UObjectSystem::FindHandleByGuid(const FGuid& Guid)
{
    FObjectRegistryState& State = GetRegistryState();

    auto It = State.GuidToIndexMap.find(Guid);
    if (It != State.GuidToIndexMap.end())
    {
        std::uint32_t Index = It->second;

        return FObjectHandle
        {
            Index,
            State.ObjectItems[Index].Generation
        };
    }

    return {};
}

FObjectHandle UObjectSystem::GetHandle(
    const UObject* Object)
{
    if (Object == nullptr)
    {
        return {};
    }

    return Object->GetHandle();
}

std::uint32_t UObjectSystem::GetObjectCount()
{
    return GetRegistryState().ObjectCount;
}
