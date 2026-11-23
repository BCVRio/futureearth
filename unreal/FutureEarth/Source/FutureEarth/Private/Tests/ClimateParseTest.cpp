#include "ClimateSubsystem.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FClimateParseTest, "FutureEarth.Climate.ParseFutureState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FClimateParseTest::RunTest(const FString& Parameters)
{
	// A real response from the climate service (Miami Beach, 2526, worst case), trimmed.
	const FString Json = TEXT(R"({
		"location":{"lat":25.8,"lon":-80.15,"elevation_m":0},
		"year":2526,
		"scenario":{"id":"ssp585","label":"Worst case"},
		"confidence":{"level":"extrapolated","note":"x"},
		"global":{"warming_c_above_preindustrial":9.5,"sea_level_rise_m":{"low":2.78,"median":7.29,"high":14.21}},
		"local":{"baseline":{"hot_days_per_year":0},"warming_c_above_baseline":7.57,"precip_change_pct":3,"hot_days_per_year":177},
		"visual":{"water_level_m":7.29,"flood_depth_m":7.29,"inundated":true,"heat_stress":1,"aridity":0.54,"vegetation_dieback":0.73,"fire_risk":0.77}
	})");

	FClimateFutureState State;
	FString Error;
	TestTrue(TEXT("parses"), UClimateSubsystem::ParseFutureState(Json, State, Error));
	TestEqual(TEXT("year"), State.Year, 2526);
	TestEqual(TEXT("scenario"), State.Scenario, EClimateScenario::WorstCase);
	TestEqual(TEXT("confidence"), State.Confidence, EProjectionConfidence::Extrapolated);
	TestEqual(TEXT("water level"), State.WaterLevelM, 7.29);
	TestEqual(TEXT("sea level high"), State.SeaLevelRiseHighM, 14.21);
	TestTrue(TEXT("inundated"), State.bInundated);
	TestEqual(TEXT("dieback"), State.VegetationDieback, 0.73f);
	TestEqual(TEXT("hot days"), State.HotDaysPerYear, 177.0);

	// A null flood depth (no elevation data) must not fail the parse.
	FClimateFutureState NoElevation;
	TestTrue(TEXT("null flood depth"), UClimateSubsystem::ParseFutureState(
		TEXT(R"({"year":2100,"global":{},"local":{},"visual":{"flood_depth_m":null,"water_level_m":0.5}})"), NoElevation, Error));
	TestEqual(TEXT("flood depth default"), NoElevation.FloodDepthM, 0.0);

	FClimateFutureState Bad;
	TestFalse(TEXT("rejects error body"), UClimateSubsystem::ParseFutureState(TEXT(R"({"error":"nope"})"), Bad, Error));
	return true;
}

#endif
