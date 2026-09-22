#pragma once

#include "Core/Base/UObject.h"
#include "Core/Base/FObjectHandle.h"
#include "Core/Base/UObjectSystem.h"
#include "./ErrorHandler.h"

#include <concepts>
#include <cstddef>


/// <summary>
/// GetItemCount: Object 담긴 리스트 전체 길이(빈 슬롯 포함)
/// 
/// </summary>
namespace UObjectSystem
{
    enum class ETypeMatch
    {
        Exact,
        IncludeDerived
    };

    // UObject에서 파생된 타입만 사용가능
    template<typename TObject> requires std::derived_from<TObject, UObject>
    class TObjectIterator
    {
    public:
        // 순회 범위 (start, end - 1까지)
        TObjectIterator(uint32 Start, uint32 End)
            : _CurrentIndex(Start), _EndIndex(End)
        {
            if (Start > End)
                ErrorHandler::Report("Invalid start!", "Start > End", ErrorHandler::EErrorLevel::Critical);

            AdvanceToNextValidObject();
        }


        // 레퍼런스 리턴
        TObject& operator*() const
        {
            UObject* Object = UObjectSystem::Resolve(_Handle);

            if (_CurrentIndex == _EndIndex || Object == nullptr ||
                !Object->GetTypeInfo()->IsA(TObject::StaticTypeInfo()))
            {
                ErrorHandler::Report("Refer Error!", "Does not refer to a live object", ErrorHandler::EErrorLevel::Critical);
            }

            return *static_cast<TObject*>(Object);
        }

        TObject* operator->() const
        {
            return &operator*();
        }

        TObjectIterator& operator++()
        {
            if (_CurrentIndex < _EndIndex)
            {
                ++_CurrentIndex;
                AdvanceToNextValidObject();
            }
            return *this;

        }

        TObjectIterator& operator--()
        {
            for (uint32 index = _CurrentIndex; index > 0;)
            {
                --index;

                FObjectHandle Handle;

                if (UObjectSystem::TryGet(index, Handle))
                {
                    UObject* Object = UObjectSystem::Resolve(Handle);

                    // 같은 타입인지
                    if (Object != nullptr && Object->GetTypeInfo()->IsA(TObject::StaticTypeInfo()))
                    {
                        _CurrentIndex = index;
                        _Handle = Handle;
                        return *this;
                    }
                }
            }
            ErrorHandler::Report("Invalid decrement!", "No previous object", ErrorHandler::EErrorLevel::Critical);
            return *this;
        }
   

        operator bool() const
        {
            // return UObjectSystem::TryGet(_CurrentIndex, _handle);
            return _CurrentIndex < _EndIndex;
        }

        // 후순위
        TObject& operator [](size_t index)
        {
            TObjectIterator It = *this;

            for (std::size_t i = 0; i < index; ++i)
            {
                ++It;
            }

            return *It;

        }

        TObject& operator[](std::size_t index) const
        {
            TObjectIterator It = *this;

            for (std::size_t i = 0; i < index; ++i)
                ++It;

            return *It;
        }

        bool operator==(const TObjectIterator& Other) const
        {
            return _CurrentIndex == Other._CurrentIndex &&
                _EndIndex == Other._EndIndex;
        }

        bool operator!=(const TObjectIterator& Other) const
        {
            return !(*this == Other);
        }


    private:
        bool AdvanceToNextValidObject()
        {
            while (Advance())
            {
                UObject* Object = UObjectSystem::Resolve(_Handle);

                // 타입검사(완전히 같은 타입)
                if (Object != nullptr && Object->GetTypeInfo()->IsA(TObject::StaticTypeInfo()))
                    return true;

                ++_CurrentIndex; // 핵심: 타입이 다르거나 빈 슬롯이면 다음 인덱스로

            }
            _CurrentIndex = _EndIndex;
            return false;
        }

        bool Advance()
        {
            while (_CurrentIndex < _EndIndex)
            {
                if (UObjectSystem::TryGet(_CurrentIndex, _Handle))
                    return true; // 이 슬롯은 타입 검사를 해볼 수 있음

                ++_CurrentIndex; // 빈 슬롯이면 계속 탐색
            }

            _Handle = {}; // 초기화
            return false; // 범위의 끝
        }

        /*bool Advance()
        {
            ++_CurrentIndex;
            return UObjectSystem::TryGet(_CurrentIndex, _handle);
        }*/

        FObjectHandle _Handle;

        uint32 _CurrentIndex;
        uint32 _EndIndex;
    };

    template<typename TObject> requires std::derived_from<TObject, UObject>
    class TObjectRange
    {
    public:
        TObjectRange(uint32 End) : _End(End) {}

        TObjectIterator<TObject> begin() const
        {
            return { 0, _End };
        }

        TObjectIterator<TObject> end() const
        {
            return { _End, _End };
        }

    private:
        uint32 _End;
    };

    template<typename TObject> requires std::derived_from<TObject, UObject>
    TObjectRange<TObject> Objects()
    {
        return TObjectRange<TObject>(UObjectSystem::GetItemCount());
    }

}
