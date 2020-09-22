// Copyright 2020 Phyronnaz

#pragma once

#include <limits>

#include "CoreMinimal.h"
#include "VoxelValue.h"
#include "VoxelMinimal.h"
#include "VoxelUtilities/VoxelBaseUtilities.h"
#include "VoxelRange.generated.h"

class FVoxelRangeFailStatus : public TThreadSingleton<FVoxelRangeFailStatus>
{
public:
	bool HasFailed() const { return bHasFailed; }
	bool HasWarning() const { return bHasWarning; }
	const TCHAR* GetMessage() const { return Message; }
	
public:
	void Fail(const TCHAR* InError)
	{
		// Note: bHasFailed might be true already if the generated graph has scoped ifs that failed
		if (!HasFailed())
		{
			bHasFailed = true;
			Message = InError;
		}
	}
	void Warning(const TCHAR* InError)
	{
		if (!HasFailed() && !HasWarning())
		{
			bHasWarning = true;
			Message = InError;
		}
	}
	void Reset()
	{
		bHasFailed = false;
		bHasWarning = false;
		Message = nullptr;
	}

private:
	bool bHasFailed = false;
	bool bHasWarning = false;

	const TCHAR* Message = nullptr;
	
	// Not inline, else it's messed up across modules
	static VOXEL_API uint32& GetTlsSlot();

	friend TThreadSingleton<FVoxelRangeFailStatus>;
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

struct FVoxelBoolRange
{
	bool bCanBeTrue = true;
	bool bCanBeFalse = true;

	FVoxelBoolRange() = default;
	FVoxelBoolRange(bool bValue)
	{
		if (bValue)
		{
			bCanBeTrue = true;
			bCanBeFalse = false;
		}
		else
		{
			bCanBeTrue = false;
			bCanBeFalse = true;
		}
	}
	FVoxelBoolRange(bool bCanBeTrue, bool bCanBeFalse)
		: bCanBeTrue(bCanBeTrue)
		, bCanBeFalse(bCanBeFalse)
	{
		check(bCanBeTrue || bCanBeFalse);
	}
	
	FString ToString() const
	{
		return bCanBeTrue && bCanBeFalse ? "true, false" : bCanBeTrue ? "true" : "false";
	}

	FVoxelBoolRange operator!() const
	{
		return { bCanBeFalse, bCanBeTrue };
	}
	FVoxelBoolRange operator&&(const FVoxelBoolRange& Other) const
	{
		if (!bCanBeFalse && !Other.bCanBeFalse)
		{
			return FVoxelBoolRange::True();
		}
		else if (!bCanBeTrue || !Other.bCanBeTrue)
		{
			return FVoxelBoolRange::False();
		}
		else
		{
			return FVoxelBoolRange::TrueOrFalse();
		}
	}
	FVoxelBoolRange operator||(const FVoxelBoolRange& Other) const
	{
		if (!bCanBeFalse || !Other.bCanBeFalse)
		{
			return FVoxelBoolRange::True();
		}
		else if (!bCanBeTrue && !Other.bCanBeTrue)
		{
			return FVoxelBoolRange::False();
		}
		else
		{
			return FVoxelBoolRange::TrueOrFalse();
		}
	}

	operator bool() const
	{
		if (bCanBeTrue && !bCanBeFalse)
		{
			return true;
		}
		else if (!bCanBeTrue && bCanBeFalse)
		{
			return false;
		}
		else
		{
			checkVoxelSlow(bCanBeTrue && bCanBeFalse);
			FVoxelRangeFailStatus::Get().Fail(TEXT("condition can be true or false"));
			return false;
		}
	}

	static FVoxelBoolRange True() { return { true, false }; }
	static FVoxelBoolRange False() { return { false, true }; }
	static FVoxelBoolRange TrueOrFalse() { return { true, true }; }

	static bool If(const FVoxelBoolRange& Condition, bool bDefaultValue)
	{
		auto& RangeFailStatus = FVoxelRangeFailStatus::Get();
		if (RangeFailStatus.HasFailed())
		{
			return true; // If already failed do nothing
		}
		const bool bCondition = Condition;
		if (RangeFailStatus.HasFailed())
		{
			RangeFailStatus.Reset();
			return bDefaultValue;
		}
		else
		{
			return bCondition;
		}
	}
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename T>
T NegativeInfinity();
template<typename T>
T PositiveInfinity();

template<>
inline constexpr float NegativeInfinity<float>()
{
	return -std::numeric_limits<float>::infinity();
}
template<>
inline constexpr float PositiveInfinity<float>()
{
	return std::numeric_limits<float>::infinity();
}

template<>
inline constexpr double NegativeInfinity<double>()
{
	return -std::numeric_limits<double>::infinity();
}
template<>
inline constexpr double PositiveInfinity<double>()
{
	return std::numeric_limits<double>::infinity();
}

template<>
inline constexpr int32 NegativeInfinity<int32>()
{
	return MIN_int32;
}
template<>
inline constexpr int32 PositiveInfinity<int32>()
{
	return MAX_int32;
}

template<>
inline constexpr uint16 NegativeInfinity<uint16>()
{
	return MIN_uint16;
}
template<>
inline constexpr uint16 PositiveInfinity<uint16>()
{
	return MAX_uint16;
}

template<>
inline constexpr FVoxelValue NegativeInfinity<FVoxelValue>()
{
	return FVoxelValue::Full();
}
template<>
inline constexpr FVoxelValue PositiveInfinity<FVoxelValue>()
{
	return FVoxelValue::Empty();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename T>
FString PrettyPrint(const T& Value)
{
	return LexToString(Value);
}

template<>
inline FString PrettyPrint<float>(const float& Value)
{
	return FString::SanitizeFloat(Value);
}

template<>
inline FString PrettyPrint<double>(const double& Value)
{
	return FString::SanitizeFloat(Value);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename T>
struct TVoxelRange
{
	T Min;
	T Max;

	TVoxelRange() = default;
	TVoxelRange(T Value)
		: Min(Value)
		, Max(Value)
	{
	}
	TVoxelRange(T Min, T Max)
		: Min(Min)
		, Max(Max)
	{
		ensure(Min <= Max);
	}
	template<typename TOther>
	explicit TVoxelRange(const TVoxelRange<TOther>& Range)
		: Min(Range.Min)
		, Max(Range.Max)
	{
	}

public:
	static constexpr bool bIsInteger = TIsIntegral<T>::Value;
	
public:
	template<typename ...TArgs>
	static TVoxelRange<T> FromList(TArgs... Values)
	{
		return { FVoxelUtilities::VariadicMin(Values...), FVoxelUtilities::VariadicMax(Values...) };
	}
	static TVoxelRange<T> Union(const TVoxelRange<T>& A, const TVoxelRange<T>& B)
	{
		return { FMath::Min(A.Min, B.Min), FMath::Max(A.Max, B.Max) };
	}
	static TVoxelRange<T> Intersection(const TVoxelRange<T>& A, const TVoxelRange<T>& B)
	{
		const T NewMin = FMath::Max(A.Min, B.Min);
		const T NewMax = FMath::Min(A.Max, B.Max);
		if (ensure(NewMin <= NewMax))
		{
			return { NewMin, NewMax };
		}
		else
		{
			return Union(A, B);
		}
	}
	template<typename ...TArgs>
	static TVoxelRange<T> Union(const TVoxelRange<T>& A, const TVoxelRange<T>& B, TArgs... Args)
	{
		return Union(Union(A, B), Args...);
	}
	template<typename ...TArgs>
	static TVoxelRange<T> Intersection(const TVoxelRange<T>& A, const TVoxelRange<T>& B, TArgs... Args)
	{
		return Intersection(Intersection(A, B), Args...);
	}
	static TVoxelRange<T> Infinite()
	{
		return { NegativeInfinity<T>(), PositiveInfinity<T>() };
	}
	static TVoxelRange<T> PositiveInfinite()
	{
		return { 0, PositiveInfinity<T>() };
	}
	static TVoxelRange<T> NegativeInfinite()
	{
		return { NegativeInfinity<T>(), 0 };
	}

	FString ToString() const
	{
		return IsSingleValue() ? PrettyPrint(Min) : FString::Printf(TEXT("%s, %s"), *PrettyPrint(Min), *PrettyPrint(Max));
	}
	
	template<typename TOther>
	bool Contains(const TOther& Other) const
	{
		return Min <= Other && Other <= Max;
	}
	template<typename TOther>
	bool Contains(const TVoxelRange<TOther>& Other) const
	{
		return Min <= Other.Min && Other.Max <= Max;
	}
	template<typename TOther>
	bool Intersects(const TVoxelRange<TOther>& Other) const
	{
		return Contains(Other.Min) || Contains(Other.Max) || Other.Contains(Min) || Other.Contains(Max);
	}

	bool IsSingleValue() const
	{
		return Min == Max;
	}
	T GetSingleValue() const
	{
		ensure(IsSingleValue());
		return Min;
	}

	bool IsSingleSign() const
	{
		return Min == 0 || Max == 0 || (Min < 0) == (Max < 0);
	}
	T GetSign() const
	{
		ensure(IsSingleSign());
		return Min == 0 ? FMath::Sign(Max) : FMath::Sign(Min);
	}
	TVoxelRange<T> ExtendToInfinity() const
	{
		TVoxelRange<T> New = *this;
		if (Min < 0)
		{
			New.Min = NegativeInfinity<T>();
			New.Max = Max > 0 ? PositiveInfinity<T>() : 0;
		}
		else
		{
			New.Min = 0;
			New.Max = PositiveInfinity<T>();
		}
		return New;
	}

	bool IsNegativeInfinity() const
	{
		return Min == NegativeInfinity<T>();
	}
	bool IsPositiveInfinity() const
	{
		return Max == PositiveInfinity<T>();
	}
	bool IsInfinity() const
	{
		return IsNegativeInfinity() || IsPositiveInfinity();
	}

	template<typename TOther>
	TVoxelRange<T>& operator=(const TVoxelRange<TOther>& Other)
	{
		Min = Other.Min;
		Max = Other.Max;
		return *this;
	}
	
	template<typename F>
	auto Apply(F Op) const
	{
		return TVoxelRange<decltype(Op(Min))>{ Op(Min), Op(Max) };
	}

public:
	template<typename TOther>
	FVoxelBoolRange operator==(const TVoxelRange<TOther>& Other) const
	{
		if (IsSingleValue() && Other.IsSingleValue() && Min == Other.Min)
		{
			checkVoxelSlow(Max == Other.Max);
			return FVoxelBoolRange::True();
		}
		else if (!Intersects(Other))
		{
			return FVoxelBoolRange::False();
		}
		else
		{
			return FVoxelBoolRange::TrueOrFalse();
		}
	}
	template<typename TOther>
	FVoxelBoolRange operator!=(const TVoxelRange<TOther>& Other) const
	{
		return !(*this == Other);
	}
	template<typename TOther>
	FVoxelBoolRange operator<(const TVoxelRange<TOther>& Other) const
	{
		if (Max < Other.Min)
		{
			return FVoxelBoolRange::True();
		}
		else if (Other.Max <= Min)
		{
			return FVoxelBoolRange::False();
		}
		else
		{
			return FVoxelBoolRange::TrueOrFalse();
		}
	}
	template<typename TOther>
	FVoxelBoolRange operator>(const TVoxelRange<TOther>& Other) const
	{
		if (Min > Other.Max)
		{
			return FVoxelBoolRange::True();
		}
		else if (Other.Min >= Max)
		{
			return FVoxelBoolRange::False();
		}
		else
		{
			return FVoxelBoolRange::TrueOrFalse();
		}
	}
	template<typename TOther>
	FVoxelBoolRange operator<=(const TVoxelRange<TOther>& Other) const
	{
		return !(*this > Other);
	}
	template<typename TOther>
	FVoxelBoolRange operator>=(const TVoxelRange<TOther>& Other) const
	{
		return !(*this < Other);
	}
	
public:
	template<typename TOther>
	TVoxelRange<T> operator+(const TVoxelRange<TOther>& Other) const
	{
		return { Min + Other.Min, Max + Other.Max };
	}
	template<typename TOther>
	TVoxelRange<T> operator-(const TVoxelRange<TOther>& Other) const
	{
		return { Min - Other.Max, Max - Other.Min };
	}
	TVoxelRange<T> operator-() const
	{
		return { -Max, -Min };
	}
	template<typename TOther>
	TVoxelRange<T> operator*(const TVoxelRange<TOther>& Other) const
	{
		return TVoxelRange::FromList(Min * Other.Min, Min * Other.Max, Max * Other.Min, Max * Other.Max);
	}
	template<typename TOther>
	TVoxelRange<T> operator/(const TVoxelRange<TOther>& Other) const
	{
		if (Other.IsSingleValue() && Other.GetSingleValue() == 0)
		{
			if (bIsInteger)
			{
				// That's how integer / 0 is handled in voxel graphs
				return 0;
			}
			if (IsSingleValue() && GetSingleValue() == 0)
			{
				FVoxelRangeFailStatus::Get().Warning(TEXT("0 / 0 encountered, will result in a nan"));
				return Infinite();
			}
			if (0 < Min)
			{
				// Single value: +inf
				return PositiveInfinity<T>();
			}
			if (Max < 0)
			{
				// Single value: -inf
				return NegativeInfinity<T>();
			}
			return Infinite();
		}

		if (!Other.Contains(0)) // Will also handle single value cases
		{
			if (Other.IsInfinity())
			{
				ensureVoxelSlowNoSideEffects(Other.IsSingleSign()); // Does not contain 0
				ensureVoxelSlowNoSideEffects(Other.GetSign() != 0); // Else wouldn't be infinity, and does not contain 0
				const auto Inf = ExtendToInfinity();
				return TVoxelRange::FromList(Inf.Min / Other.GetSign(), Inf.Max / Other.GetSign());
			}
			return TVoxelRange::FromList(Min / Other.Min, Min / Other.Max, Max / Other.Min, Max / Other.Max);
		}
		else
		{
			if (Other.IsSingleSign())
			{
				ensureVoxelSlowNoSideEffects(Other.GetSign() != 0); // Else would be a single value
				const auto Inf = ExtendToInfinity();
				return TVoxelRange::FromList(Inf.Min / Other.GetSign(), Inf.Max / Other.GetSign());
			}
			return Infinite();
		}
	}

public:
	TVoxelRange<T> operator+(T Other) const
	{
		return { Min + Other, Max + Other };
	}
	TVoxelRange<T> operator-(T Other) const
	{
		return { Min - Other, Max - Other};
	}
	TVoxelRange<T> operator*(T Other) const
	{
		return { FMath::Min(Min * Other, Max * Other), FMath::Max(Min * Other, Max * Other) };
	}
	TVoxelRange<T> operator/(T Other) const
	{
		return { FMath::Min(Min / Other, Max / Other), FMath::Max(Min / Other, Max / Other) };
	}

	friend FVoxelBoolRange operator==(const TVoxelRange<T>& Range, T Other)
	{
		return Range == TVoxelRange<T>(Other);
	}
	friend FVoxelBoolRange operator<(const TVoxelRange<T>& Range, T Other)
	{
		return Range < TVoxelRange<T>(Other);
	}
	friend FVoxelBoolRange operator>(const TVoxelRange<T>& Range, T Other)
	{
		return Range > TVoxelRange<T>(Other);
	}
	friend FVoxelBoolRange operator<=(const TVoxelRange<T>& Range, T Other)
	{
		return Range <= TVoxelRange<T>(Other);
	}
	friend FVoxelBoolRange operator>=(const TVoxelRange<T>& Range, T Other)
	{
		return Range >= TVoxelRange<T>(Other);
	}
	friend TVoxelRange<T> operator-(T Other, const TVoxelRange<T>& Range)
	{
		return TVoxelRange<T>(Other) - Range;
	}
	friend TVoxelRange<T> operator+(T Other, const TVoxelRange<T>& Range)
	{
		return TVoxelRange<T>(Other) + Range;
	}
	friend TVoxelRange<T> operator*(T Other, const TVoxelRange<T>& Range)
	{
		return TVoxelRange<T>(Other) * Range;
	}
	friend TVoxelRange<T> operator/(T Other, const TVoxelRange<T>& Range)
	{
		return TVoxelRange<T>(Other) / Range;
	}

	template<typename U>
	TVoxelRange<T>& operator-=(const U& Other)
	{
		*this = *this - Other;
		return *this;
	}
	template<typename U>
	TVoxelRange<T>& operator+=(const U& Other)
	{
		*this = *this + Other;
		return *this;
	}
	template<typename U>
	TVoxelRange<T>& operator*=(const U& Other)
	{
		*this = *this * Other;
		return *this;
	}
	template<typename U>
	TVoxelRange<T>& operator/=(const U& Other)
	{
		*this = *this / Other;
		return *this;
	}
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename T>
FArchive& operator<<(FArchive& Ar, TVoxelRange<T>& Range)
{
	Ar << Range.Min;
	Ar << Range.Max;
	return Ar;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

struct FVoxelMaterialRange
{
	FVoxelMaterialRange() = default;
	FVoxelMaterialRange(const struct FVoxelMaterial&) {}
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

struct FVoxelColorRange
{
	FVoxelColorRange() = default;
	FVoxelColorRange(const struct FColor&) {}
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

// For display and serialization
USTRUCT()
struct FVoxelRange
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Voxel")
	double Min = 0;

	UPROPERTY(EditAnywhere, Category = "Voxel")
	double Max = 0;

	FVoxelRange() = default;
	FVoxelRange(float Min, float Max)
		: Min(Min)
		, Max(Max)
	{
	}
	FVoxelRange(const TVoxelRange<v_flt>& Range)
		: Min(Range.Min)
		, Max(Range.Max)
	{
	}

	operator TVoxelRange<v_flt>() const
	{
		return { v_flt(Min), v_flt(Max) };
	}
};