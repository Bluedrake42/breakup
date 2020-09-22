// Copyright 2020 Phyronnaz

#include "VoxelTexture.h"
#include "VoxelMessages.h"
#include "VoxelContainers/VoxelStaticArray.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"

DEFINE_VOXEL_MEMORY_STAT(STAT_VoxelTextureMemory);

static FAutoConsoleCommand CmdClearCache(
    TEXT("voxel.texture.ClearCache"),
    TEXT("Clears the voxel textures memory cache"),
    FConsoleCommandDelegate::CreateStatic(&FVoxelTextureUtilities::ClearCache));

struct FVoxelTextureCacheKey
{
	TWeakObjectPtr<UTexture> Texture;
	// Channel, in case it's a color texture converted to float
	EVoxelRGBA Channel = EVoxelRGBA(-1);

	FVoxelTextureCacheKey() = default;
	explicit FVoxelTextureCacheKey(TWeakObjectPtr<UTexture> Texture)
		: Texture(Texture)
	{
	}
	explicit FVoxelTextureCacheKey(TWeakObjectPtr<UTexture> Texture, EVoxelRGBA Channel)
		: Texture(Texture)
		, Channel(Channel)
	{
	}

	bool operator==(const FVoxelTextureCacheKey& Other) const
	{
		return Texture == Other.Texture && Channel == Other.Channel;
	}
	friend uint32 GetTypeHash(const FVoxelTextureCacheKey& Key)
	{
		return HashCombine(GetTypeHash(Key.Texture), GetTypeHash(Key.Channel));
	}
};

template<typename T>
inline auto& GetVoxelTextureCacheMap()
{
	check(IsInGameThread());
	static TMap<FVoxelTextureCacheKey, TVoxelSharedPtr<typename TVoxelTexture<T>::FTextureData>> Map;
	return Map;
}

inline void ExtractTextureData(UTexture* Texture, int32& OutSizeX, int32& OutSizeY, TArray<FColor>& OutData)
{
	VOXEL_FUNCTION_COUNTER();
	
	check(IsInGameThread());

	if (auto* Texture2D = Cast<UTexture2D>(Texture))
	{
		FTexture2DMipMap& Mip = Texture2D->PlatformData->Mips[0];
		OutSizeX = Mip.SizeX;
		OutSizeY = Mip.SizeY;

		const int32 Size = OutSizeX * OutSizeY;
		OutData.SetNumUninitialized(Size);

		auto& BulkData = Mip.BulkData;
		if (!ensureAlways(BulkData.GetBulkDataSize() > 0))
		{
			OutSizeX = 1;
			OutSizeY = 1;
			OutData.SetNum(1);
			return;
		}

		void* Data = BulkData.Lock(LOCK_READ_ONLY);
		if (!ensureAlways(Data))
		{
			Mip.BulkData.Unlock();
			OutSizeX = 1;
			OutSizeY = 1;
			OutData.SetNum(1);
			return;
		}

		FMemory::Memcpy(OutData.GetData(), Data, Size * sizeof(FColor));
		Mip.BulkData.Unlock();
		return;
	}

	if (auto* TextureRenderTarget = Cast<UTextureRenderTarget2D>(Texture))
	{
		FRenderTarget* RenderTarget = TextureRenderTarget->GameThread_GetRenderTargetResource();
		if (ensure(RenderTarget))
		{
			const auto Format = TextureRenderTarget->GetFormat();

			OutSizeX = TextureRenderTarget->GetSurfaceWidth();
			OutSizeY = TextureRenderTarget->GetSurfaceHeight();

			const int32 Size = OutSizeX * OutSizeY;
			OutData.SetNumUninitialized(Size);

			switch (Format)
			{
			case PF_B8G8R8A8:
			{
				if (ensure(RenderTarget->ReadPixels(OutData))) return;
				break;
			}
			case PF_R8G8B8A8:
			{
				if (ensure(RenderTarget->ReadPixels(OutData))) return;
				break;
			}
			case PF_FloatRGBA:
			{
				TArray<FLinearColor> LinearColors;
				LinearColors.SetNumUninitialized(Size);
				if (ensure(RenderTarget->ReadLinearColorPixels(LinearColors)))
				{
					for (int32 Index = 0; Index < Size; Index++)
					{
						OutData[Index] = LinearColors[Index].ToFColor(false);
					}
					return;
				}
				break;
			}
			default:
				ensure(false);
			}
		}
	}

	ensure(false);
	OutSizeX = 1;
	OutSizeY = 1;
	OutData.SetNum(1);
}

TVoxelTexture<FColor> FVoxelTextureUtilities::CreateFromTexture_Color(UTexture* Texture)
{
	VOXEL_FUNCTION_COUNTER();

	auto& Data = GetVoxelTextureCacheMap<FColor>().FindOrAdd(FVoxelTextureCacheKey(Texture));
	if (!Data.IsValid())
	{
		FString Error;
		if (!CanCreateFromTexture(Texture, Error))
		{
			FVoxelMessages::Error("Can't create Voxel Texture: " + Error, Texture);
			return {};
		}

		int32 SizeX = -1;
		int32 SizeY = -1;
		TArray<FColor> TextureData;
		ExtractTextureData(Texture, SizeX, SizeY, TextureData);

		Data = MakeVoxelShared<TVoxelTexture<FColor>::FTextureData>();
		Data->SetSize(SizeX, SizeY);
		for (int32 Index = 0; Index < SizeX * SizeY; Index++)
		{
			Data->SetValue(Index, TextureData[Index]);
		}
	}

	return TVoxelTexture<FColor>(Data.ToSharedRef());
}

TVoxelTexture<float> FVoxelTextureUtilities::CreateFromTexture_Float(UTexture* Texture, EVoxelRGBA Channel)
{
	VOXEL_FUNCTION_COUNTER();

	auto& Data = GetVoxelTextureCacheMap<float>().FindOrAdd(FVoxelTextureCacheKey(Texture, Channel));
	if (!Data.IsValid())
	{
		FString Error;
		if (!CanCreateFromTexture(Texture, Error))
		{
			FVoxelMessages::Error("Can't create Voxel Texture: " + Error, Texture);
			return {};
		}

		const auto ColorTexture = CreateFromTexture_Color(Texture);

		Data = MakeVoxelShared<TVoxelTexture<float>::FTextureData>();
		Data->SetSize(ColorTexture.GetSizeX(), ColorTexture.GetSizeY());

		const int32 Num = ColorTexture.GetSizeX() * ColorTexture.GetSizeY();
		for (int32 Index = 0; Index < Num; Index++)
		{
			const FColor Color = ColorTexture.GetTextureData()[Index];
			uint8 Result = 0;
			switch (Channel)
			{
			case EVoxelRGBA::R:
				Result = Color.R;
				break;
			case EVoxelRGBA::G:
				Result = Color.G;
				break;
			case EVoxelRGBA::B:
				Result = Color.B;
				break;
			case EVoxelRGBA::A:
				Result = Color.A;
				break;
			}
			const float Value = FVoxelUtilities::UINT8ToFloat(Result);
			Data->SetValue(Index, Value);
		}
	}
	return TVoxelTexture<float>(Data.ToSharedRef());
}

bool FVoxelTextureUtilities::CanCreateFromTexture(UTexture* Texture, FString& OutError)
{
	if (!Texture)
	{
		OutError = "Invalid texture";
		return false;
	}
	if (auto* Texture2D = Cast<UTexture2D>(Texture))
	{
#if WITH_EDITORONLY_DATA
		if (Texture2D->MipGenSettings != TextureMipGenSettings::TMGS_NoMipmaps)
		{
			OutError = "Texture MipGenSettings must be NoMipmaps";
			return false;
		}
#endif
		if (Texture2D->CompressionSettings != TextureCompressionSettings::TC_VectorDisplacementmap &&
			Texture2D->CompressionSettings != TextureCompressionSettings::TC_EditorIcon)
		{
			OutError = "Texture CompressionSettings must be VectorDisplacementmap or UserInterface2D";
			return false;
		}
		if (Texture2D->GetPixelFormat() != PF_B8G8R8A8)
		{
			OutError = "Texture pixel format must be B8G8R8A8, try switching CompressionSettings to VectorDisplacementmap";
			return false;
		}
		return true;
	}
	if (auto* TextureRenderTarget = Cast<UTextureRenderTarget2D>(Texture))
	{
		const auto Format = TextureRenderTarget->GetFormat();
		if (Format != EPixelFormat::PF_R8G8B8A8 && 
			Format != EPixelFormat::PF_FloatRGBA &&
			Format != EPixelFormat::PF_B8G8R8A8)
		{
			OutError = "Render Target PixelFormat must be R8G8B8A8, B8G8R8A8 or R8G8B8A8 (is " + FString(GPixelFormats[Format].Name) + ")";
			return false;
		}
		if (!TextureRenderTarget->GameThread_GetRenderTargetResource())
		{
			OutError = "Render Target resource must be created";
			return false;
		}
		return true;
	}
	OutError = "Texture must be a Texture2D or a TextureRenderTarget2D";
	return false;
}

void FVoxelTextureUtilities::FixTexture(UTexture* Texture)
{
	VOXEL_FUNCTION_COUNTER();
	
	if (!ensure(Texture))
	{
		return;
	}
#if WITH_EDITORONLY_DATA
	Texture->MipGenSettings = TextureMipGenSettings::TMGS_NoMipmaps;
#endif
	Texture->CompressionSettings = TextureCompressionSettings::TC_VectorDisplacementmap;
	Texture->UpdateResource();
	Texture->MarkPackageDirty();
}

void FVoxelTextureUtilities::ClearCache()
{
	VOXEL_FUNCTION_COUNTER();
	
	GetVoxelTextureCacheMap<FColor>().Empty();
	GetVoxelTextureCacheMap<float>().Empty();
}

void FVoxelTextureUtilities::ClearCache(UTexture* Texture)
{
	VOXEL_FUNCTION_COUNTER();
	
	GetVoxelTextureCacheMap<FColor>().Remove(FVoxelTextureCacheKey(Texture));
	GetVoxelTextureCacheMap<float>().Remove(FVoxelTextureCacheKey(Texture));
	GetVoxelTextureCacheMap<float>().Remove(FVoxelTextureCacheKey(Texture, EVoxelRGBA::R));
	GetVoxelTextureCacheMap<float>().Remove(FVoxelTextureCacheKey(Texture, EVoxelRGBA::G));
	GetVoxelTextureCacheMap<float>().Remove(FVoxelTextureCacheKey(Texture, EVoxelRGBA::B));
	GetVoxelTextureCacheMap<float>().Remove(FVoxelTextureCacheKey(Texture, EVoxelRGBA::A));
}

void FVoxelTextureUtilities::CreateOrUpdateUTexture2D(const TVoxelTexture<float>& Texture, UTexture2D*& InOutTexture)
{
	VOXEL_FUNCTION_COUNTER();
	
	if (!InOutTexture || 
		!InOutTexture->PlatformData || 
		InOutTexture->PlatformData->Mips.Num() == 0 || 
		InOutTexture->PlatformData->PixelFormat != EPixelFormat::PF_R32_FLOAT ||
		InOutTexture->GetSizeX() != Texture.GetSizeX() || 
		InOutTexture->GetSizeY() != Texture.GetSizeY())
	{
		InOutTexture = UTexture2D::CreateTransient(Texture.GetSizeX(), Texture.GetSizeY(), EPixelFormat::PF_R32_FLOAT);
		InOutTexture->CompressionSettings = TC_HDR;
		InOutTexture->SRGB = false;
		InOutTexture->Filter = TF_Bilinear;
	}
	
	FTexture2DMipMap& Mip = InOutTexture->PlatformData->Mips[0];
	float* Data = reinterpret_cast<float*>(Mip.BulkData.Lock(LOCK_READ_WRITE));
	if (!ensureAlways(Data)) return;

	FMemory::Memcpy(Data, Texture.GetTextureData().GetData(), Texture.GetSizeX() * Texture.GetSizeY() * sizeof(float));

	Mip.BulkData.Unlock();
	
	InOutTexture->UpdateResource();
}

void FVoxelTextureUtilities::CreateOrUpdateUTexture2D(const TVoxelTexture<FColor>& Texture, UTexture2D*& InOutTexture)
{
	VOXEL_FUNCTION_COUNTER();
	
	if (!InOutTexture || 
		!InOutTexture->PlatformData || 
		InOutTexture->PlatformData->Mips.Num() == 0 || 
		InOutTexture->PlatformData->PixelFormat != EPixelFormat::PF_B8G8R8A8 ||
		InOutTexture->GetSizeX() != Texture.GetSizeX() || 
		InOutTexture->GetSizeY() != Texture.GetSizeY())
	{
		InOutTexture = UTexture2D::CreateTransient(Texture.GetSizeX(), Texture.GetSizeY(), EPixelFormat::PF_B8G8R8A8);
		InOutTexture->CompressionSettings = TC_VectorDisplacementmap;
		InOutTexture->SRGB = false;
		InOutTexture->Filter = TF_Bilinear;
	}
	
	FTexture2DMipMap& Mip = InOutTexture->PlatformData->Mips[0];
	FColor* Data = reinterpret_cast<FColor*>(Mip.BulkData.Lock(LOCK_READ_WRITE));
	if (!ensureAlways(Data)) return;

	FMemory::Memcpy(Data, Texture.GetTextureData().GetData(), Texture.GetSizeX() * Texture.GetSizeY() * sizeof(FColor));

	Mip.BulkData.Unlock();
	
	InOutTexture->UpdateResource();
}

TVoxelTexture<FColor> FVoxelTextureUtilities::CreateColorTextureFromFloatTexture(const TVoxelTexture<float>& Texture, EVoxelRGBA Channel, bool bNormalize)
{
	VOXEL_FUNCTION_COUNTER();

	const auto GetColor = [&](float Value)
	{
		if (bNormalize)
		{
			Value = (Value - Texture.GetMin()) / (Texture.GetMax() - Texture.GetMin());
		}
		const float ByteValue = FVoxelUtilities::FloatToUINT8(Value);
		
		FColor Color(ForceInit);
		switch (Channel)
		{
		case EVoxelRGBA::R:
			Color.R = ByteValue;
			break;
		case EVoxelRGBA::G:
			Color.G = ByteValue;
			break;
		case EVoxelRGBA::B:
			Color.B = ByteValue;
			break;
		case EVoxelRGBA::A:
			Color.A = ByteValue;
			break;
		}
		return Color;
	};

	auto Data = MakeVoxelShared<TVoxelTexture<FColor>::FTextureData>();
	Data->SetSize(Texture.GetSizeX(), Texture.GetSizeY());
	Data->SetBounds(GetColor(Texture.GetMin()), GetColor(Texture.GetMax()));
	
	const int32 Num = Texture.GetSizeX() * Texture.GetSizeY();
	for (int32 Index = 0; Index < Num; Index++)
	{
		const float Value = Texture.GetTextureData()[Index];
		Data->SetValue_NoBounds(Index, GetColor(Value));
	}

	return TVoxelTexture<FColor>(Data);
}

TVoxelTexture<float> FVoxelTextureUtilities::Normalize(const TVoxelTexture<float>& Texture)
{
	VOXEL_ASYNC_FUNCTION_COUNTER();

	auto Data = MakeVoxelShared<TVoxelTexture<float>::FTextureData>();
	Data->SetSize(Texture.GetSizeX(), Texture.GetSizeY());
	Data->SetBounds(0, 1);

	const float Min = Texture.GetMin();
	const float Max = Texture.GetMax();
	const int32 Num = Texture.GetSizeX() * Texture.GetSizeY();
	for (int32 Index = 0; Index < Num; Index++)
	{
		const float Value = Texture.GetTextureData()[Index];
		Data->SetValue_NoBounds(Index,(Value - Min) / (Max - Min));
	}

	return TVoxelTexture<float>(Data);
}