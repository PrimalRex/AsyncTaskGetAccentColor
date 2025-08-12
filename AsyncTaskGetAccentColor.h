// Copyright (c) Harris Barra. (MIT License)

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "AsyncTaskGetAccentColor.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnReady, FColor, AccentColor);

/**
 * This is a Blueprint Async Task that computes an accent color from a dynamic texture.
 * It downsamples the texture to reduce computation time and returns the most dominant color.
 * It also runs multithreaded to reduce game thread blocking.
 */
UCLASS()
class RIFTFLOW_API UAsyncTaskGetAccentColor : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable)
	FOnReady OnSuccess;

	UPROPERTY(BlueprintAssignable)
	FOnReady OnFail;
	
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true"))
	static UAsyncTaskGetAccentColor* GetAccentColorAsync(UTexture2DDynamic* Texture, int DownsampleFactor = 1);

	virtual void Activate() override;

protected:
	UPROPERTY()
	UTexture2DDynamic* Texture;

	int DownsampleFactor;
};
