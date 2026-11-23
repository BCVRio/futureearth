#include "FutureWorldDirector.h"

#include "ClimateSubsystem.h"
#include "Engine/GameInstance.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Materials/MaterialParameterCollection.h"

AFutureWorldDirector::AFutureWorldDirector()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AFutureWorldDirector::BeginPlay()
{
	Super::BeginPlay();
	if (UClimateSubsystem* Climate = GetGameInstance() ? GetGameInstance()->GetSubsystem<UClimateSubsystem>() : nullptr)
	{
		Climate->OnFutureStateUpdated.AddDynamic(this, &AFutureWorldDirector::HandleState);
	}
	Refresh();
}

void AFutureWorldDirector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UClimateSubsystem* Climate = GetGameInstance() ? GetGameInstance()->GetSubsystem<UClimateSubsystem>() : nullptr)
	{
		Climate->OnFutureStateUpdated.RemoveDynamic(this, &AFutureWorldDirector::HandleState);
	}
	Super::EndPlay(EndPlayReason);
}

void AFutureWorldDirector::SetYear(int32 NewYear)
{
	NewYear = FMath::Clamp(NewYear, 2020, 2526);
	if (NewYear != Year)
	{
		Year = NewYear;
		Refresh();
	}
}

void AFutureWorldDirector::SetScenario(EClimateScenario NewScenario)
{
	if (NewScenario != Scenario)
	{
		Scenario = NewScenario;
		Refresh();
	}
}

void AFutureWorldDirector::SetLocation(double NewLatitude, double NewLongitude)
{
	Latitude = FMath::Clamp(NewLatitude, -90.0, 90.0);
	Longitude = FMath::Clamp(NewLongitude, -180.0, 180.0);
	Refresh();
}

void AFutureWorldDirector::Refresh()
{
	if (UClimateSubsystem* Climate = GetGameInstance() ? GetGameInstance()->GetSubsystem<UClimateSubsystem>() : nullptr)
	{
		Climate->RequestFutureState(Latitude, Longitude, Year, Scenario);
	}
}

void AFutureWorldDirector::HandleState(const FClimateFutureState& State)
{
	if (SeaSurfaceActor)
	{
		FVector Location = SeaSurfaceActor->GetActorLocation();
		Location.Z = PresentSeaLevelZ + State.WaterLevelM * 100.0; // metres -> cm
		SeaSurfaceActor->SetActorLocation(Location);
	}

	if (ClimateParameters)
	{
		const auto Set = [this](const TCHAR* Name, double Value)
		{
			UKismetMaterialLibrary::SetScalarParameterValue(this, ClimateParameters, FName(Name), static_cast<float>(Value));
		};
		Set(TEXT("HeatStress"), State.HeatStress);
		Set(TEXT("Aridity"), State.Aridity);
		Set(TEXT("VegetationDieback"), State.VegetationDieback);
		Set(TEXT("FireRisk"), State.FireRisk);
		Set(TEXT("WaterLevelM"), State.WaterLevelM);
		Set(TEXT("LocalWarmingC"), State.LocalWarmingC);
	}

	OnFutureStateApplied(State);
}
