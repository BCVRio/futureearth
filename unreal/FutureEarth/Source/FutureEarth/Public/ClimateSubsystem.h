#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Interfaces/IHttpRequest.h"
#include "FutureStateTypes.h"
#include "ClimateSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFutureStateUpdated, const FClimateFutureState&, State);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFutureStateFailed, const FString&, Error);

/** Fetches FutureState from the climate service (services/climate). */
UCLASS(Config = Game)
class FUTUREEARTH_API UClimateSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** Base URL of the climate service. Set in Config/DefaultGame.ini. */
	UPROPERTY(Config, BlueprintReadWrite, Category = "FutureEarth")
	FString ServiceBaseUrl = TEXT("http://localhost:8787");

	/** Ask for the state of a place and year. Only the newest request's answer is delivered. */
	UFUNCTION(BlueprintCallable, Category = "FutureEarth")
	void RequestFutureState(double Latitude, double Longitude, int32 Year, EClimateScenario Scenario);

	UPROPERTY(BlueprintAssignable, Category = "FutureEarth")
	FOnFutureStateUpdated OnFutureStateUpdated;

	UPROPERTY(BlueprintAssignable, Category = "FutureEarth")
	FOnFutureStateFailed OnFutureStateFailed;

	/** The most recent state received. */
	UPROPERTY(BlueprintReadOnly, Category = "FutureEarth")
	FClimateFutureState Current;

	static bool ParseFutureState(const FString& Json, FClimateFutureState& Out, FString& OutError);

private:
	void HandleResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnected, int32 Serial);

	int32 LatestSerial = 0;
};
