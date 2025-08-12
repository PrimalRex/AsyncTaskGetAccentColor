// Copyright (c) Harris Barra. (MIT License)

#include "AsyncTaskGetAccentColor.h"
#include "Engine/Texture2DDynamic.h"
#include "Engine/TextureRenderTarget2D.h"

UAsyncTaskGetAccentColor* UAsyncTaskGetAccentColor::GetAccentColorAsync(UTexture2DDynamic* Texture, int32 DownsampleFactor)
{
	UAsyncTaskGetAccentColor* Task = NewObject<UAsyncTaskGetAccentColor>();
	Task->Texture = Texture;
	Task->DownsampleFactor = DownsampleFactor;
	return Task;
}

void UAsyncTaskGetAccentColor::Activate()
{
    Super::Activate();

    if (!Texture)
    {
        OnFail.Broadcast(FColor::Black);
        SetReadyToDestroy();
        return;
    }

    // Create temp render target
    UTextureRenderTarget2D* TempRT = NewObject<UTextureRenderTarget2D>();
    TempRT->InitCustomFormat(Texture->SizeX, Texture->SizeY, PF_B8G8R8A8, false);
    TempRT->UpdateResource();

    // Once our pixels have been copied to memory, we can implement both downsampling and color quantization
    auto OnPixelsReady = [this](const TArray<FColor>& Pixels)
    {
        // Reduce color space to 3 bits
        auto QuantizeColor = [](const FColor& Color) -> int32
        {
            return ((Color.R >> 5) << 6) | ((Color.G >> 5) << 3) | (Color.B >> 5);
        };

        // Split Tasks across 75% of available threads to prevent locking up system
        int Tasks = FMath::Max(1, (int)(GThreadPool->GetNumThreads() * 0.75f));
        int ChunkSize = FMath::Max(1, Pixels.Num() / Tasks);
        
        int32 NumBins = 512;
        TArray<int32> Histogram;
        Histogram.Init(0, NumBins);

        ParallelFor(Tasks, [&](int32 Index)
        {
            TArray<int32> LocalHist;
            LocalHist.Init(0, NumBins);

            // Downsample by skipping pixels
            for (int p = ChunkSize * Index; p < FMath::Min(Pixels.Num(), ChunkSize * (Index + 1)); p += DownsampleFactor)
            {
                int32 BinIndex = QuantizeColor(Pixels[p]);
                LocalHist[BinIndex]++;
            }

            // Synchronize to global histogram
            for (int32 i = 0; i < NumBins; ++i)
            {
                if (LocalHist[i] > 0)
                {
                    FPlatformAtomics::InterlockedAdd(&Histogram[i], LocalHist[i]);
                }
            }
        });

        // Create a list of indices to map bins to their frequencies
        TArray<TPair<int32, int32>> BinCounts; 
        for (int32 i = 0; i < NumBins; ++i)
        {
            if (Histogram[i] > 0)
            {
                BinCounts.Emplace(i, Histogram[i]);
            }
        }

        // Sort bins by frequency to get the most dominant colors
        BinCounts.Sort([](const TPair<int32, int32>& A, const TPair<int32, int32>& B)
        {
            return A.Value > B.Value;
        });

        // Select 20% of the most frequent bins
        // This is a heuristic to reduce the number of bins we consider for the accent color
        int32 NumToConsider = FMath::Clamp(static_cast<int32>(BinCounts.Num() * 0.2), 1, BinCounts.Num());
        TArray<TPair<int32,int32>> TopBins;
        for (int32 i = 0; i < NumToConsider; ++i)
        {
            TopBins.Add(BinCounts[i]);
        }

        // Sort these top bins by brightness descending
        TopBins.Sort([](const TPair<int32,int32>& A, const TPair<int32,int32>& B)
        {
            auto GetBrightness = [](int32 Bin) -> int32
            {
                int32 R = ((Bin >> 6) & 0x7) << 5;
                int32 G = ((Bin >> 3) & 0x7) << 5;
                int32 B = (Bin & 0x7) << 5;
                return R + G + B; 
            };

            return GetBrightness(A.Key) > GetBrightness(B.Key);
        });

        // Choose the 25% most frequent bin from the top bins
        // This is a heuristic to avoid extreme whites and pale tones
        int32 PickIndex = FMath::Clamp(static_cast<int32>(TopBins.Num() * 0.25), 0, TopBins.Num() - 1);
        int32 FinalBin = TopBins[PickIndex].Key;

        // Decode the final chosen bin
        FColor Accent;
        Accent.R = ((FinalBin >> 6) & 0x7) << 5;
        Accent.G = ((FinalBin >> 3) & 0x7) << 5;
        Accent.B = (FinalBin & 0x7) << 5;
        Accent.A = 255;

        OnSuccess.Broadcast(Accent);
        SetReadyToDestroy();
    };

    // Cache resource pointers
    auto TextureResource = Texture->GetResource();
    auto TempRTResource = TempRT->GetResource();

    // Run copy on render thread
    ENQUEUE_RENDER_COMMAND(CopyDynamicTexToRT)(
    [TextureResource, TempRTResource, OnPixelsReady](FRHICommandListImmediate& RHICmdList)
    {
        auto SrcTexture = TextureResource->TextureRHI.GetReference();
        auto DestTexture = TempRTResource->TextureRHI.GetReference();

        FRHICopyTextureInfo CopyInfo;
        RHICmdList.CopyTexture(SrcTexture, DestTexture, CopyInfo);

        // Once the copy is done, we can read pixels back on the game thread
        AsyncTask(ENamedThreads::GameThread, [TempRTResource, OnPixelsReady]()
        {
            TArray<FColor> OutPixels;
            static_cast<FTextureRenderTarget2DResource*>(TempRTResource)->ReadPixels(OutPixels);
            OnPixelsReady(OutPixels);
        });
    });
}

