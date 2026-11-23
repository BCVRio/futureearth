#pragma once

#include "CoreMinimal.h"
#include "FutureStateTypes.generated.h"

/** Emissions pathway. Matches the `scenario` values of the climate service. */
UENUM(BlueprintType)
enum class EClimateScenario : uint8
{
	BestCase UMETA(DisplayName = "Best case (SSP1-2.6)"),
	CurrentPath UMETA(DisplayName = "Current path (SSP2-4.5)"),
	WorstCase UMETA(DisplayName = "Worst case (SSP5-8.5)"),
};

/** How far the numbers can be trusted; shown as a badge next to the year. */
UENUM(BlueprintType)
enum class EProjectionConfidence : uint8
{
	Projected UMETA(DisplayName = "Projected"),
	LowConfidence UMETA(DisplayName = "Low confidence"),
	Extrapolated UMETA(DisplayName = "Extrapolated"),
};

/** The projected climate at one place and year, as returned by GET /future. */
USTRUCT(BlueprintType)
struct FUTUREEARTH_API FClimateFutureState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "FutureEarth") double Latitude = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "FutureEarth") double Longitude = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "FutureEarth") double ElevationM = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "FutureEarth") int32 Year = 0;
	UPROPERTY(BlueprintReadOnly, Category = "FutureEarth") EClimateScenario Scenario = EClimateScenario::CurrentPath;
	UPROPERTY(BlueprintReadOnly, Category = "FutureEarth") EProjectionConfidence Confidence = EProjectionConfidence::Projected;

	// Physical projections, for the HUD.
	UPROPERTY(BlueprintReadOnly, Category = "FutureEarth|Climate") double GlobalWarmingC = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "FutureEarth|Climate") double LocalWarmingC = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "FutureEarth|Climate") double SeaLevelRiseM = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "FutureEarth|Climate") double SeaLevelRiseLowM = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "FutureEarth|Climate") double SeaLevelRiseHighM = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "FutureEarth|Climate") double PrecipChangePct = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "FutureEarth|Climate") double HotDaysPerYear = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "FutureEarth|Climate") double BaselineHotDaysPerYear = 0.0;

	// Scene drivers. Water level is metres above today's mean sea level; the rest are 0..1.
	UPROPERTY(BlueprintReadOnly, Category = "FutureEarth|Visual") double WaterLevelM = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "FutureEarth|Visual") double FloodDepthM = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "FutureEarth|Visual") bool bInundated = false;
	UPROPERTY(BlueprintReadOnly, Category = "FutureEarth|Visual") float HeatStress = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "FutureEarth|Visual") float Aridity = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "FutureEarth|Visual") float VegetationDieback = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "FutureEarth|Visual") float FireRisk = 0.f;
};
