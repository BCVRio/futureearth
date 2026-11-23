#include "ClimateSubsystem.h"

#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

DEFINE_LOG_CATEGORY_STATIC(LogFutureEarth, Log, All);

namespace
{
	const TCHAR* ScenarioId(EClimateScenario Scenario)
	{
		switch (Scenario)
		{
		case EClimateScenario::BestCase: return TEXT("ssp126");
		case EClimateScenario::WorstCase: return TEXT("ssp585");
		default: return TEXT("ssp245");
		}
	}

	TSharedPtr<FJsonObject> Child(const TSharedPtr<FJsonObject>& Parent, const TCHAR* Name)
	{
		const TSharedPtr<FJsonObject>* Found = nullptr;
		return Parent.IsValid() && Parent->TryGetObjectField(Name, Found) && Found ? *Found : nullptr;
	}

	// Leaves Out untouched when the field is missing or null.
	void Number(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Name, double& Out)
	{
		double Value = 0.0;
		if (Obj.IsValid() && Obj->TryGetNumberField(Name, Value))
		{
			Out = Value;
		}
	}

	void Number(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Name, float& Out)
	{
		double Value = Out;
		Number(Obj, Name, Value);
		Out = static_cast<float>(Value);
	}
}

void UClimateSubsystem::RequestFutureState(double Latitude, double Longitude, int32 Year, EClimateScenario Scenario)
{
	const int32 Serial = ++LatestSerial;
	const FString Url = FString::Printf(TEXT("%s/future?lat=%.4f&lon=%.4f&year=%d&scenario=%s"),
		*ServiceBaseUrl, Latitude, Longitude, Year, ScenarioId(Scenario));

	const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(Url);
	Request->SetVerb(TEXT("GET"));
	// The first request for a new place downloads decades of model data and can take over a minute.
	Request->SetTimeout(240.f);
	Request->OnProcessRequestComplete().BindUObject(this, &UClimateSubsystem::HandleResponse, Serial);
	const bool bStarted = Request->ProcessRequest();
	UE_LOG(LogFutureEarth, Display, TEXT("Requesting %s (started=%d)"), *Url, bStarted);
}

void UClimateSubsystem::HandleResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnected, int32 Serial)
{
	if (Serial != LatestSerial)
	{
		return; // A newer request has been made; drop this answer.
	}

	FString Error;
	if (!bConnected || !Response.IsValid())
	{
		Error = FString::Printf(TEXT("Could not reach the climate service at %s"), *ServiceBaseUrl);
	}
	else
	{
		FClimateFutureState State;
		if (EHttpResponseCodes::IsOk(Response->GetResponseCode()) && ParseFutureState(Response->GetContentAsString(), State, Error))
		{
			Current = State;
			OnFutureStateUpdated.Broadcast(Current);
			return;
		}
		if (Error.IsEmpty())
		{
			Error = FString::Printf(TEXT("Climate service returned %d: %s"), Response->GetResponseCode(), *Response->GetContentAsString().Left(200));
		}
	}

	UE_LOG(LogFutureEarth, Warning, TEXT("%s"), *Error);
	OnFutureStateFailed.Broadcast(Error);
}

bool UClimateSubsystem::ParseFutureState(const FString& Json, FClimateFutureState& Out, FString& OutError)
{
	TSharedPtr<FJsonObject> Root;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) || !Root.IsValid())
	{
		OutError = TEXT("Climate service response was not valid JSON");
		return false;
	}

	const TSharedPtr<FJsonObject> Visual = Child(Root, TEXT("visual"));
	const TSharedPtr<FJsonObject> Local = Child(Root, TEXT("local"));
	const TSharedPtr<FJsonObject> Global = Child(Root, TEXT("global"));
	if (!Visual.IsValid() || !Local.IsValid() || !Global.IsValid())
	{
		OutError = TEXT("Climate service response is missing the visual, local or global block");
		return false;
	}

	const TSharedPtr<FJsonObject> Location = Child(Root, TEXT("location"));
	Number(Location, TEXT("lat"), Out.Latitude);
	Number(Location, TEXT("lon"), Out.Longitude);
	Number(Location, TEXT("elevation_m"), Out.ElevationM);

	double Year = 0.0;
	Number(Root, TEXT("year"), Year);
	Out.Year = FMath::RoundToInt32(Year);

	FString Id;
	if (const TSharedPtr<FJsonObject> Scenario = Child(Root, TEXT("scenario")))
	{
		Scenario->TryGetStringField(TEXT("id"), Id);
	}
	Out.Scenario = Id == TEXT("ssp126") ? EClimateScenario::BestCase
		: Id == TEXT("ssp585") ? EClimateScenario::WorstCase
		: EClimateScenario::CurrentPath;

	FString Level;
	if (const TSharedPtr<FJsonObject> Confidence = Child(Root, TEXT("confidence")))
	{
		Confidence->TryGetStringField(TEXT("level"), Level);
	}
	Out.Confidence = Level == TEXT("extrapolated") ? EProjectionConfidence::Extrapolated
		: Level == TEXT("low-confidence") ? EProjectionConfidence::LowConfidence
		: EProjectionConfidence::Projected;

	Number(Global, TEXT("warming_c_above_preindustrial"), Out.GlobalWarmingC);
	const TSharedPtr<FJsonObject> SeaLevel = Child(Global, TEXT("sea_level_rise_m"));
	Number(SeaLevel, TEXT("median"), Out.SeaLevelRiseM);
	Number(SeaLevel, TEXT("low"), Out.SeaLevelRiseLowM);
	Number(SeaLevel, TEXT("high"), Out.SeaLevelRiseHighM);

	Number(Local, TEXT("warming_c_above_baseline"), Out.LocalWarmingC);
	Number(Local, TEXT("precip_change_pct"), Out.PrecipChangePct);
	Number(Local, TEXT("hot_days_per_year"), Out.HotDaysPerYear);
	Number(Child(Local, TEXT("baseline")), TEXT("hot_days_per_year"), Out.BaselineHotDaysPerYear);

	Number(Visual, TEXT("water_level_m"), Out.WaterLevelM);
	Number(Visual, TEXT("flood_depth_m"), Out.FloodDepthM);
	Visual->TryGetBoolField(TEXT("inundated"), Out.bInundated);
	Number(Visual, TEXT("heat_stress"), Out.HeatStress);
	Number(Visual, TEXT("aridity"), Out.Aridity);
	Number(Visual, TEXT("vegetation_dieback"), Out.VegetationDieback);
	Number(Visual, TEXT("fire_risk"), Out.FireRisk);
	return true;
}
