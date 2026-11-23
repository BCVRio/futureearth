#pragma once

#include "CoreMinimal.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Styling/SlateTypes.h"
#include "Widgets/SCompoundWidget.h"
#include "FutureStateTypes.h"

class AFutureWorldDirector;

/**
 * The on-screen interface: a large year, how far ahead it is, a confidence badge,
 * three data chips, the scenario switch and the time dial. Reads everything from
 * the director and calls back into it when the dial or scenario changes.
 */
class SFutureEarthHud : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SFutureEarthHud) {}
		SLATE_ARGUMENT(TWeakObjectPtr<AFutureWorldDirector>, Director)
	SLATE_END_ARGS()

	SFutureEarthHud();
	void Construct(const FArguments& InArgs);

	/** The dial is not linear: Now, +50, +100, +200 and +500 years sit at equal spacing. */
	static int32 YearsAheadFromDial(float Dial);
	static float DialFromYearsAhead(int32 YearsAhead);

private:
	TSharedRef<SWidget> MakeChip(const FLinearColor& DotColor, TAttribute<FText> Text);
	TSharedRef<SWidget> MakeScenarioButton(EClimateScenario Scenario, const FText& Label);
	const FClimateFutureState* State() const;

	TWeakObjectPtr<AFutureWorldDirector> Director;
	int32 BaseYear = 2026;

	FSlateRoundedBoxBrush Glass;
	FSlateRoundedBoxBrush Pill;
	FSlateRoundedBoxBrush Outline;
	FSlateRoundedBoxBrush Dot;
	FSlateRoundedBoxBrush Track;
	FSlateRoundedBoxBrush Thumb;
	FSliderStyle DialStyle;
};
