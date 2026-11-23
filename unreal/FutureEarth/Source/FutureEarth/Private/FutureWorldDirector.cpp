#include "FutureWorldDirector.h"

#include "ClimateSubsystem.h"
#include "Engine/GameInstance.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Materials/MaterialParameterCollection.h"

AFutureWorldDirector::AFutureWorldDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false; // Only ticks in capture self-test mode.
}

void AFutureWorldDirector::BeginPlay()
{
	Super::BeginPlay();
	// Test overrides: -FutureYear=2526 on the command line.
	FParse::Value(FCommandLine::Get(), TEXT("FutureYear="), Year);
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

	UE_LOG(LogTemp, Display, TEXT("FutureEarth applied: year=%d water_level_m=%.2f sea_z=%.1f aridity=%.2f dieback=%.2f heat=%.2f"),
		State.Year, State.WaterLevelM, SeaSurfaceActor ? SeaSurfaceActor->GetActorLocation().Z : 0.0,
		State.Aridity, State.VegetationDieback, State.HeatStress);

	OnFutureStateApplied(State);

	// Self-test: -FutureEarthCapture=<seconds> saves a screenshot that long after the
	// first state arrives, then quits. Used to check the data-to-picture loop unattended.
	float Delay = 0.f;
	if (CaptureAtSeconds == 0.0 && FParse::Value(FCommandLine::Get(), TEXT("FutureEarthCapture="), Delay) && Delay > 0.f)
	{
		CaptureAtSeconds = FPlatformTime::Seconds() + Delay;
		SetActorTickEnabled(true);
	}
}

void AFutureWorldDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	++CaptureFrames;
	// Wait for both the delay and enough rendered frames for lighting to settle,
	// then give the screenshot a further 60 frames to be written before quitting.
	if (CaptureShotFrame < 0 && FPlatformTime::Seconds() >= CaptureAtSeconds && CaptureFrames >= 90)
	{
		CaptureShotFrame = CaptureFrames;
		FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("Screenshots/FutureEarthCapture.png"), false, false);
	}
	else if (CaptureShotFrame >= 0 && CaptureFrames >= CaptureShotFrame + 60)
	{
		FPlatformMisc::RequestExit(false);
	}
}
