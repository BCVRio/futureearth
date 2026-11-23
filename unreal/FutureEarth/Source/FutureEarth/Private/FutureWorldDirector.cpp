#include "FutureWorldDirector.h"

#include "ClimateSubsystem.h"
#include "SFutureEarthHud.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/PlayerController.h"
#include "Components/ActorComponent.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
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
	ApplyMapTilesKey();
	ListenForPhone();
	if (bShowHud && !FParse::Param(FCommandLine::Get(), TEXT("FutureEarthNoHud")))
	{
		ShowHud();
	}
	if (UClimateSubsystem* Climate = GetGameInstance() ? GetGameInstance()->GetSubsystem<UClimateSubsystem>() : nullptr)
	{
		Climate->OnFutureStateUpdated.AddDynamic(this, &AFutureWorldDirector::HandleState);
	}
	Refresh();
}

void AFutureWorldDirector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HudWidget.IsValid() && GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(HudWidget.ToSharedRef());
	}
	HudWidget.Reset();
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
	LastState = State;
	bHasState = true;

	if (SeaSurfaceActor)
	{
		FVector Location = SeaSurfaceActor->GetActorLocation();
		const double SeaLevelZ = bSeaLevelFromGeoid ? (State.GeoidHeightM - GeoreferenceOriginHeightM) * 100.0 : PresentSeaLevelZ;
		Location.Z = SeaLevelZ + State.WaterLevelM * 100.0; // metres -> cm
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
		// World Z (cm) of the water surface, for effects such as the tide line. Far below when there is no sea.
		Set(TEXT("WaterSurfaceZ"), SeaSurfaceActor ? SeaSurfaceActor->GetActorLocation().Z : -1.0e7);
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
		FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("Screenshots/FutureEarthCapture.png"), true, false);
	}
	else if (CaptureShotFrame >= 0 && CaptureFrames >= CaptureShotFrame + 60)
	{
		FPlatformMisc::RequestExit(false);
	}
}

void AFutureWorldDirector::ApplyMapTilesKey()
{
	// Google Photorealistic 3D Tiles need an API key in the tileset URL. The key is read
	// from the environment at runtime and never saved into a level, so it cannot end up
	// in source control. Tilesets opt in with the actor tag "GoogleTiles". Cesium is
	// reached by reflection so this module builds without the plugin.
	const FString Key = FPlatformMisc::GetEnvironmentVariable(TEXT("GOOGLE_MAPS_API_KEY"));
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* Tileset = *It;
		if (Tileset->GetClass()->GetName() != TEXT("Cesium3DTileset") || !Tileset->ActorHasTag(TEXT("GoogleTiles")))
		{
			continue;
		}
		UFunction* SetUrl = Tileset->FindFunction(TEXT("SetUrl"));
		if (Key.IsEmpty() || !SetUrl)
		{
			UE_LOG(LogTemp, Warning, TEXT("FutureEarth: %s has no map tiles (%s)"), *Tileset->GetName(),
				Key.IsEmpty() ? TEXT("GOOGLE_MAPS_API_KEY is not set") : TEXT("Cesium3DTileset::SetUrl not found"));
			continue;
		}
		struct { FString InUrl; } Params{ TEXT("https://tile.googleapis.com/v1/3dtiles/root.json?key=") + Key };
		Tileset->ProcessEvent(SetUrl, &Params);
		UE_LOG(LogTemp, Display, TEXT("FutureEarth: map tiles key applied to %s"), *Tileset->GetName());
	}
}

void AFutureWorldDirector::ShowHud()
{
	if (!GEngine || !GEngine->GameViewport)
	{
		return; // No viewport, e.g. a headless run.
	}
	HudWidget = SNew(SFutureEarthHud).Director(this);
	GEngine->GameViewport->AddViewportWidgetContent(HudWidget.ToSharedRef(), 10);

	// The dial and scenario switch need a visible cursor that can click the interface.
	if (APlayerController* Player = GetWorld()->GetFirstPlayerController())
	{
		Player->bShowMouseCursor = true;
		FInputModeGameAndUI Mode;
		Mode.SetHideCursorDuringCapture(false);
		Player->SetInputMode(Mode);
	}
}

void AFutureWorldDirector::ListenForPhone()
{
	// Pixel Streaming delivers messages from the web page through its input component.
	// It is reached by reflection so this module builds without linking the plugin.
	UClass* InputClass = FindObject<UClass>(nullptr, TEXT("/Script/PixelStreaming2.PixelStreaming2Input"));
	FMulticastDelegateProperty* OnInput = InputClass ? FindFProperty<FMulticastDelegateProperty>(InputClass, TEXT("OnInputEvent")) : nullptr;
	if (!OnInput)
	{
		return; // Pixel Streaming is not enabled.
	}
	UActorComponent* Input = NewObject<UActorComponent>(this, InputClass, TEXT("PhoneInput"));
	Input->RegisterComponent();
	FScriptDelegate Handler;
	Handler.BindUFunction(this, GET_FUNCTION_NAME_CHECKED(AFutureWorldDirector, HandlePhoneMessage));
	OnInput->AddDelegate(MoveTemp(Handler), Input);
}

void AFutureWorldDirector::HandlePhoneMessage(const FString& Descriptor)
{
	// {"f":[east,north,up],"u":[east,north,up]} : where the back of the phone points, and its top edge.
	TSharedPtr<FJsonObject> Message;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Descriptor), Message) || !Message.IsValid())
	{
		return;
	}
	const TArray<TSharedPtr<FJsonValue>>* F = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* U = nullptr;
	if (Message->TryGetArrayField(TEXT("f"), F) && Message->TryGetArrayField(TEXT("u"), U) && F->Num() == 3 && U->Num() == 3)
	{
		SetViewDirectionENU(
			FVector((*F)[0]->AsNumber(), (*F)[1]->AsNumber(), (*F)[2]->AsNumber()),
			FVector((*U)[0]->AsNumber(), (*U)[1]->AsNumber(), (*U)[2]->AsNumber()));
	}
}

void AFutureWorldDirector::SetViewDirectionENU(FVector Forward, FVector Up)
{
	if (Forward.IsNearlyZero() || Up.IsNearlyZero())
	{
		return;
	}
	// East-North-Up to Unreal with Cesium's axes (X east, Y south, Z up).
	const FVector UnrealForward(Forward.X, -Forward.Y, Forward.Z);
	const FVector UnrealUp(Up.X, -Up.Y, Up.Z);
	const FRotator Rotation = FRotationMatrix::MakeFromXZ(UnrealForward, UnrealUp).Rotator();

	if (APlayerController* Player = GetWorld()->GetFirstPlayerController())
	{
		Player->SetControlRotation(Rotation);
		if (AActor* Target = Player->GetViewTarget())
		{
			Target->SetActorRotation(Rotation);
		}
	}
}
