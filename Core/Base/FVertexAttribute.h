#pragma once

#include <concepts>
#include <ranges>
#include <span>
#include <type_traits>

enum class EVertexAttribute : uint8
{
	Position,
	Normal,
	UV,
	Color,

	MAX
};

template<EVertexAttribute Attribute>
struct TVertexAttributeTraits;

template<>
struct TVertexAttributeTraits<EVertexAttribute::Position> {
	using Type = FVector3;
};

template<>
struct TVertexAttributeTraits<EVertexAttribute::Normal> {
	using Type = FVector3;
};

template<>
struct TVertexAttributeTraits<EVertexAttribute::UV> {
	using Type = FVector2D;
};

template<>
struct TVertexAttributeTraits<EVertexAttribute::Color>
{
	using Type = FColor4;
};

template<EVertexAttribute Attribute>
using TVertexAttributeElementType = typename TVertexAttributeTraits<Attribute>::Type;

template<EVertexAttribute Attribute>
struct TVertexAttributeView {
	using ElementType = TVertexAttributeElementType<Attribute>;

	static constexpr EVertexAttribute AttributeType = Attribute;

	std::span<const ElementType> Data;
};

template<typename T>
struct TIsVertexAttributeView : std::false_type
{
};

template<EVertexAttribute Attribute>
struct TIsVertexAttributeView<TVertexAttributeView<Attribute>> : std::true_type
{
};

template<typename T>
concept CVertexAttributeView = TIsVertexAttributeView<std::remove_cvref_t<T>>::value;

template<EVertexAttribute Attribute, std::ranges::contiguous_range Range>
	requires std::ranges::sized_range<Range>&& std::same_as<std::ranges::range_value_t<Range>, TVertexAttributeElementType<Attribute>>
TVertexAttributeView<Attribute> MakeVertexAttribute(const Range& Data) {
	using ElementType = TVertexAttributeElementType<Attribute>;

	return TVertexAttributeView<Attribute>{ std::span<const ElementType>{ std::ranges::data(Data), std::ranges::size(Data) } };
}