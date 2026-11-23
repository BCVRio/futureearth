#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FutureStateTypes.h"
#include "FutureWorldDirector.generated.h"

class UMaterialParameterCollection;

/**
 * Place one in a level. It asks the climate service for the chosen place, year
 * and scenario, then dresses the world: raises the sea surface and writes the
 * 0..1 climate dials into a Material Parameter Collection that materials,
 * foliage and sky read.
 */
UCLASS()
class FUTUREEARTH_API AFutureWorldDirector : public AActor
{
	GENERATED_BODY()

public:
	AFutureWorldDirector();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FutureEarth", meta = (ClampMin = "-90", ClampMax = "90"))
	double Latitude = 25.79;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FutureEarth", meta = (ClampMin = "-180", ClampMax = "180"))
	double Longitude = -80.13;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FutureEarth", meta = (ClampMin = "2020", ClampMax = "2526"))
	int32 Year = 2076;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FutureEarth")
	EClimateScenario Scenario = EClimateScenario::CurrentPath;

	/** The ocean / water plane to raise. Its Z is set to PresentSeaLevelZ plus the projected rise. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FutureEarth|Scene")
	TObjectPtr<AActor> SeaSurfaceActor;

	/**
	 * For georeferenced (real-world) levels. When on, PresentSeaLevelZ is ignored and sea level
	 * is placed from the geoid: Z = (geoid height - GeoreferenceOriginHeightM) * 100.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FutureEarth|Scene")
	bool bSeaLevelFromGeoid = false;

	/** Ellipsoid height (m) of the Cesium georeference origin, i.e. of world Z = 0. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FutureEarth|Scene", meta = (EditCondition = "bSeaLevelFromGeoid"))
	double GeoreferenceOriginHeightM = 0.0;

	/** World Z (cm) of today's mean sea level in this level. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FutureEarth|Scene")
	double PresentSeaLevelZ = 0.0;

	/** Receives scalars HeatStress, Aridity, VegetationDieback, FireRisk, WaterLevelM, LocalWarmingC, WaterSurfaceZ. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FutureEarth|Scene")
	TObjectPtr<UMaterialParameterCollection> ClimateParameters;

	/** Show the built-in on-screen interface (year, data chips, scenario switch, time dial). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FutureEarth")
	bool bShowHud = true;

	/** True once the first climate state has arrived. */
	UFUNCTION(BlueprintPure, Category = "FutureEarth")
	bool HasState() const { return bHasState; }

	UFUNCTION(BlueprintPure, Category = "FutureEarth")
	const FClimateFutureState& GetLastState() const { return LastState; }

	UFUNCTION(BlueprintCallable, Category = "FutureEarth")
	void SetYear(int32 NewYear);

	UFUNCTION(BlueprintCallable, Category = "FutureEarth")
	void SetScenario(EClimateScenario NewScenario);

	UFUNCTION(BlueprintCallable, Category = "FutureEarth")
	void SetLocation(double NewLatitude, double NewLongitude);

	/** Re-request the state for the current place, year and scenario. */
	UFUNCTION(BlueprintCallable, Category = "FutureEarth")
	void Refresh();

	/** Called after a new state has been applied; hook HUD and extra effects here. */
	UFUNCTION(BlueprintImplementableEvent, Category = "FutureEarth")
	void OnFutureStateApplied(const FClimateFutureState& State);

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION()
	void HandleState(const FClimateFutureState& State);

	void ApplyMapTilesKey();
	void ShowHud();

	FClimateFutureState LastState;
	bool bHasState = false;
	TSharedPtr<class SWidget> HudWidget;

	double CaptureAtSeconds = 0.0;
	int32 CaptureFrames = 0;
	int32 CaptureShotFrame = -1;
};
