#include "SFutureEarthHud.h"

#include "FutureWorldDirector.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	// Design tokens from design/ux.
	const FLinearColor Ember = FLinearColor(FColor(0xFF, 0x7A, 0x3D));
	const FLinearColor Water = FLinearColor(FColor(0x3D, 0xB4, 0xFF));
	const FLinearColor Amber = FLinearColor(FColor(0xFF, 0xD1, 0x66));
	const FLinearColor Ink = FLinearColor(1.f, 1.f, 1.f, 1.f);
	const FLinearColor Dim = FLinearColor(1.f, 1.f, 1.f, 0.62f);
	const FLinearColor GlassTint = FLinearColor(0.012f, 0.018f, 0.022f, 0.72f);

	// Dial stops, at equal spacing along the slider.
	const int32 Stops[] = { 0, 50, 100, 200, 500 };
	const int32 NumStops = UE_ARRAY_COUNT(Stops);

	FSlateFontInfo Font(const TCHAR* Weight, int32 Size)
	{
		return FCoreStyle::GetDefaultFontStyle(Weight, Size);
	}
}

SFutureEarthHud::SFutureEarthHud()
	: Glass(GlassTint, 22.f)
	, Pill(FLinearColor(1.f, 1.f, 1.f, 0.10f), 16.f)
	, Outline(FLinearColor::Transparent, 13.f, FLinearColor::White, 1.5f)
	, Dot(FLinearColor::White, 5.f, FVector2f(10.f, 10.f))
	, Track(FLinearColor(1.f, 1.f, 1.f, 0.38f), 2.f)
	, Thumb(Ember, 12.f, FLinearColor(1.f, 1.f, 1.f, 0.9f), 2.f, FVector2f(24.f, 24.f))
{
	DialStyle = FCoreStyle::Get().GetWidgetStyle<FSliderStyle>("Slider");
	DialStyle.SetBarThickness(4.f)
		.SetNormalBarImage(Track).SetHoveredBarImage(Track).SetDisabledBarImage(Track)
		.SetNormalThumbImage(Thumb).SetHoveredThumbImage(Thumb).SetDisabledThumbImage(Thumb);
}

int32 SFutureEarthHud::YearsAheadFromDial(float Dial)
{
	const float Scaled = FMath::Clamp(Dial, 0.f, 1.f) * (NumStops - 1);
	const int32 Index = FMath::Min(FMath::FloorToInt32(Scaled), NumStops - 2);
	return FMath::RoundToInt32(FMath::Lerp(static_cast<float>(Stops[Index]), static_cast<float>(Stops[Index + 1]), Scaled - Index));
}

float SFutureEarthHud::DialFromYearsAhead(int32 YearsAhead)
{
	YearsAhead = FMath::Clamp(YearsAhead, Stops[0], Stops[NumStops - 1]);
	for (int32 Index = 0; Index < NumStops - 1; ++Index)
	{
		if (YearsAhead <= Stops[Index + 1])
		{
			const float Within = static_cast<float>(YearsAhead - Stops[Index]) / static_cast<float>(Stops[Index + 1] - Stops[Index]);
			return (Index + Within) / (NumStops - 1);
		}
	}
	return 1.f;
}

const FClimateFutureState* SFutureEarthHud::State() const
{
	return Director.IsValid() && Director->HasState() ? &Director->GetLastState() : nullptr;
}

TSharedRef<SWidget> SFutureEarthHud::MakeChip(const FLinearColor& DotColor, TAttribute<FText> Text)
{
	return SNew(SBorder)
		.BorderImage(&Pill)
		.Padding(FMargin(14.f, 8.f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 9.f, 0.f)
			[
				SNew(SImage).Image(&Dot).ColorAndOpacity(DotColor)
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(STextBlock).Font(Font(TEXT("Regular"), 15)).ColorAndOpacity(Ink).Text(Text)
			]
		];
}

TSharedRef<SWidget> SFutureEarthHud::MakeScenarioButton(EClimateScenario Scenario, const FText& Label)
{
	const auto IsSelected = [this, Scenario]() { return Director.IsValid() && Director->Scenario == Scenario; };
	return SNew(SButton)
		.ButtonStyle(FCoreStyle::Get(), "NoBorder")
		.ContentPadding(0.f)
		.OnClicked_Lambda([this, Scenario]()
		{
			if (Director.IsValid())
			{
				Director->SetScenario(Scenario);
			}
			return FReply::Handled();
		})
		[
			SNew(SBorder)
			.BorderImage(&Pill)
			.BorderBackgroundColor_Lambda([IsSelected]() { return IsSelected() ? FSlateColor(FLinearColor(1.f, 1.f, 1.f, 2.2f)) : FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.f)); })
			.HAlign(HAlign_Center)
			.Padding(FMargin(18.f, 9.f))
			[
				SNew(STextBlock)
				.Font(Font(TEXT("Bold"), 15))
				.Text(Label)
				.ColorAndOpacity_Lambda([IsSelected]() { return FSlateColor(IsSelected() ? Ink : Dim); })
			]
		];
}

void SFutureEarthHud::Construct(const FArguments& InArgs)
{
	Director = InArgs._Director;
	BaseYear = FDateTime::Now().GetYear();

	TSharedRef<SHorizontalBox> StopLabels = SNew(SHorizontalBox);
	for (int32 Index = 0; Index < NumStops; ++Index)
	{
		if (Index > 0)
		{
			StopLabels->AddSlot().FillWidth(1.f)[ SNew(SSpacer) ];
		}
		StopLabels->AddSlot().AutoWidth()
		[
			SNew(STextBlock)
			.Font(Font(TEXT("Regular"), 13))
			.ColorAndOpacity(Dim)
			.Text(Index == 0 ? FText::FromString(TEXT("Now")) : FText::FromString(FString::Printf(TEXT("+%d"), Stops[Index])))
		];
	}

	ChildSlot
	[
		SNew(SOverlay)

		// Place label, top left.
		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(36.f)
		[
			SNew(SBorder).BorderImage(&Glass).Padding(FMargin(18.f, 10.f))
			[
				SNew(STextBlock).Font(Font(TEXT("Regular"), 15)).ColorAndOpacity(Ink)
				.Text_Lambda([this]()
				{
					if (!Director.IsValid())
					{
						return FText::GetEmpty();
					}
					return FText::FromString(FString::Printf(TEXT("%.3f°%s  %.3f°%s"),
						FMath::Abs(Director->Latitude), Director->Latitude >= 0 ? TEXT("N") : TEXT("S"),
						FMath::Abs(Director->Longitude), Director->Longitude >= 0 ? TEXT("E") : TEXT("W")));
				})
			]
		]

		// Main panel, bottom left.
		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(36.f)
		[
			SNew(SBox).WidthOverride(640.f)
			[
				SNew(SBorder).BorderImage(&Glass).Padding(FMargin(30.f, 22.f, 30.f, 24.f))
				[
					SNew(SVerticalBox)

					// Year and confidence badge.
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom)
						[
							SNew(STextBlock).Font(Font(TEXT("Bold"), 84)).ColorAndOpacity(Ink)
							.Text_Lambda([this]() { return FText::AsNumber(Director.IsValid() ? Director->Year : BaseYear, &FNumberFormattingOptions::DefaultNoGrouping()); })
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(22.f, 18.f, 0.f, 0.f)
						[
							SNew(SBorder).BorderImage(&Outline).Padding(FMargin(14.f, 6.f))
							.Visibility_Lambda([this]() { return State() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
							.BorderBackgroundColor_Lambda([this]()
							{
								const FClimateFutureState* S = State();
								const EProjectionConfidence C = S ? S->Confidence : EProjectionConfidence::Projected;
								return FSlateColor(C == EProjectionConfidence::Extrapolated ? Ember : C == EProjectionConfidence::LowConfidence ? Amber : Dim);
							})
							[
								SNew(STextBlock).Font(Font(TEXT("Bold"), 13))
								.ColorAndOpacity_Lambda([this]()
								{
									const FClimateFutureState* S = State();
									const EProjectionConfidence C = S ? S->Confidence : EProjectionConfidence::Projected;
									return FSlateColor(C == EProjectionConfidence::Extrapolated ? Ember : C == EProjectionConfidence::LowConfidence ? Amber : Ink);
								})
								.Text_Lambda([this]()
								{
									const FClimateFutureState* S = State();
									const EProjectionConfidence C = S ? S->Confidence : EProjectionConfidence::Projected;
									return FText::FromString(C == EProjectionConfidence::Extrapolated ? TEXT("Extrapolated") : C == EProjectionConfidence::LowConfidence ? TEXT("Low confidence") : TEXT("Projected"));
								})
							]
						]
					]

					// How far ahead.
					+ SVerticalBox::Slot().AutoHeight().Padding(4.f, -6.f, 0.f, 16.f)
					[
						SNew(STextBlock).Font(Font(TEXT("Regular"), 20)).ColorAndOpacity(Ink)
						.Text_Lambda([this]()
						{
							const int32 Ahead = (Director.IsValid() ? Director->Year : BaseYear) - BaseYear;
							return FText::FromString(Ahead <= 0 ? FString(TEXT("Today")) : FString::Printf(TEXT("%d years from now"), Ahead));
						})
					]

					// Data chips.
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 18.f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 10.f, 0.f)
						[
							MakeChip(Water, TAttribute<FText>::CreateLambda([this]()
							{
								const FClimateFutureState* S = State();
								return FText::FromString(S ? FString::Printf(TEXT("+%.2f m sea level"), S->SeaLevelRiseM) : FString(TEXT("Loading climate data…")));
							}))
						]
						+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 10.f, 0.f)
						[
							MakeChip(Ember, TAttribute<FText>::CreateLambda([this]()
							{
								const FClimateFutureState* S = State();
								return FText::FromString(S ? FString::Printf(TEXT("+%.1f °C"), S->LocalWarmingC) : FString(TEXT("–")));
							}))
						]
						+ SHorizontalBox::Slot().AutoWidth()
						[
							MakeChip(Ember, TAttribute<FText>::CreateLambda([this]()
							{
								const FClimateFutureState* S = State();
								return FText::FromString(S ? FString::Printf(TEXT("%d days over 35 °C"), FMath::RoundToInt32(S->HotDaysPerYear)) : FString(TEXT("–")));
							}))
						]
					]

					// Scenario switch.
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 18.f)
					[
						SNew(SBorder).BorderImage(&Pill).Padding(4.f)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().FillWidth(1.f)[ MakeScenarioButton(EClimateScenario::BestCase, FText::FromString(TEXT("Best case"))) ]
							+ SHorizontalBox::Slot().FillWidth(1.f)[ MakeScenarioButton(EClimateScenario::CurrentPath, FText::FromString(TEXT("Current path"))) ]
							+ SHorizontalBox::Slot().FillWidth(1.f)[ MakeScenarioButton(EClimateScenario::WorstCase, FText::FromString(TEXT("Worst case"))) ]
						]
					]

					// Time dial.
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SSlider)
						.Style(&DialStyle)
						.SliderBarColor(FLinearColor::White)
						.SliderHandleColor(FLinearColor::White)
						.Value_Lambda([this]() { return DialFromYearsAhead((Director.IsValid() ? Director->Year : BaseYear) - BaseYear); })
						.OnValueChanged_Lambda([this](float Dial)
						{
							if (Director.IsValid())
							{
								Director->SetYear(BaseYear + YearsAheadFromDial(Dial));
							}
						})
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f, 0.f, 0.f)
					[
						StopLabels
					]
				]
			]
		]
	];
}
