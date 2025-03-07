// Copyright JAA Contributors 2024-2025

#include "./Modules/UI/AboutJsonAsAsset.h"
#include "Modules/UI/StyleModule.h"

#include "Interfaces/IPluginManager.h"
#if ENGINE_MAJOR_VERSION >= 5
#include "Styling/StyleColors.h"
#endif
#include "Utilities/AppStyleCompatibility.h"

#define LOCTEXT_NAMESPACE "AboutJsonAsAsset"

void SAboutJsonAsAsset::Construct(const FArguments& InArgs) {
	// Plugin Details
	TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin("JsonAsAsset");

	TSharedPtr<SButton> FModelButton;
	TSharedPtr<SButton> GithubButton;

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4428)
#endif
	AboutLines.Add(
		MakeShareable(
			new FLineDefinition(
				LOCTEXT("JsonAsAssetDetails", 
					"JsonAsAsset is a plugin used to convert JSON to uassets inside the content browser. "
					"We are not liable or responsible for any activity you may perform with this plugin, "
					"nor for any loss or damage caused by its usage."
				),
				9, 
				FLinearColor::White, 
				FMargin(0.f, 2.f)
			)
		)
	);
#ifdef _MSC_VER
#pragma warning(pop)
#endif

	FText Version = FText::FromString("Version: " + Plugin->GetDescriptor().VersionName);
	FText Title = FText::FromString("JsonAsAsset");

	ChildSlot
		[
			SNew(SBorder)
				.Padding(16.f).BorderImage(FAppStyle::Get().GetBrush("Brushes.Panel"))
				[
					SNew(SVerticalBox)

						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0.f, 16.f, 0.f, 0.f)
						[
							SNew(SHorizontalBox)
								+ SHorizontalBox::Slot()
								.FillWidth(1.0)
								[
									SNew(SVerticalBox)
										+ SVerticalBox::Slot()
										.Padding(0.f, 4.f)
										[
											SNew(STextBlock)
#if ENGINE_MAJOR_VERSION >= 5
												.ColorAndOpacity(FStyleColors::ForegroundHover)
#endif
												.Font(FAppStyle::Get().GetFontStyle("AboutScreen.TitleFont"))
												.Text(Title)
										]

										+ SVerticalBox::Slot()
										.Padding(0.f, 4.f)
										[
											SNew(SEditableText)
												.IsReadOnly(true)
#if ENGINE_MAJOR_VERSION >= 5
												.ColorAndOpacity(FStyleColors::ForegroundHover)
#endif
												.Text(Version)
										]
								]

								+ SHorizontalBox::Slot()
								.AutoWidth()
								.VAlign(VAlign_Center)
								.HAlign(HAlign_Right)
								.Padding(0.0f, 0.0f, 8.f, 0.0f)
								[
									SAssignNew(FModelButton, SButton)
										.ButtonStyle(FAppStyle::Get(), "SimpleButton")
										.OnClicked(this, &SAboutJsonAsAsset::OnFModelButtonClicked)
										.ContentPadding(0.f).ToolTipText(LOCTEXT("FModelButton", "FModel"))
										[
											SNew(SImage)
												.Image(FJsonAsAssetStyle::Get().GetBrush(TEXT("JsonAsAsset.FModel.Icon")))
												.ColorAndOpacity(FSlateColor::UseForeground())
										]
								]

								+ SHorizontalBox::Slot()
								.AutoWidth()
								.VAlign(VAlign_Center)
								.HAlign(HAlign_Right)
								.Padding(0.0f, 0.0f, 8.f, 0.0f)
								[
									SAssignNew(GithubButton, SButton)
										.ButtonStyle(FAppStyle::Get(), "SimpleButton")
										.OnClicked(this, &SAboutJsonAsAsset::OnGithubButtonClicked)
										.ContentPadding(0.f).ToolTipText(LOCTEXT("GithubButton", "Github"))
										[
											SNew(SImage)
												.Image(FJsonAsAssetStyle::Get().GetBrush(TEXT("JsonAsAsset.Github.Icon")))
												.ColorAndOpacity(FSlateColor::UseForeground())
										]
								]
						]

						+ SVerticalBox::Slot()
						.Padding(FMargin(0.f, 16.f))
						.AutoHeight()
						[
							SNew(SListView<TSharedRef<FLineDefinition>>)
#if ENGINE_MAJOR_VERSION >= 5
								.ListViewStyle(&FAppStyle::Get().GetWidgetStyle<FTableViewStyle>("SimpleListView"))
#endif
								.ListItemsSource(&AboutLines)
								.OnGenerateRow(this, &SAboutJsonAsAsset::MakeAboutTextItemWidget)
								.SelectionMode(ESelectionMode::None)
						]

						+ SVerticalBox::Slot()
						.Padding(FMargin(0.f, 16.f, 0.0f, 0.0f))
						.AutoHeight()
						[
							SNew(SHorizontalBox)

								+ SHorizontalBox::Slot()
								.HAlign(HAlign_Right)
								.VAlign(VAlign_Bottom)
						]
				]
		];
}

// ReSharper disable once CppMemberFunctionMayBeStatic
// ReSharper disable once CppPassValueParameterByConstReference
TSharedRef<ITableRow> SAboutJsonAsAsset::MakeAboutTextItemWidget(TSharedRef<FLineDefinition> Item, const TSharedRef<STableViewBase>& OwnerTable) {
	if (Item->Text.IsEmpty())
		return SNew(STableRow<TSharedPtr<FString>>, OwnerTable)
		.Style(&FAppStyle::Get().GetWidgetStyle<FTableRowStyle>("SimpleTableView.Row"))
		.Padding(6.0f)[
			SNew(SSpacer)
		];
	else
		return SNew(STableRow<TSharedPtr<FString>>, OwnerTable)
		.Style(&FAppStyle::Get().GetWidgetStyle<FTableRowStyle>("SimpleTableView.Row"))
		.Padding(Item->Margin)[
			SNew(STextBlock)
				.LineHeightPercentage(1.3f)
				.AutoWrapText(true)
				.ColorAndOpacity(Item->TextColor)
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", Item->FontSize))
				.Text(Item->Text)
		];
}

// ReSharper disable once CppMemberFunctionMayBeStatic
FReply SAboutJsonAsAsset::OnFModelButtonClicked() {
	FString TheURL = "https://fmodel.app";
	FPlatformProcess::LaunchURL(*TheURL, nullptr, nullptr);

	return FReply::Handled();
}

// ReSharper disable once CppMemberFunctionMayBeStatic
FReply SAboutJsonAsAsset::OnGithubButtonClicked() {
	FString TheURL = "https://github.com/JsonAsAsset/JsonAsAsset";
	FPlatformProcess::LaunchURL(*TheURL, nullptr, nullptr);

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE