/* Copyright JsonAsAsset Contributors 2024-2025 */

#include "JsonAsAsset.h"

#include "./Importers/Constructor/Importer.h"

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

#if ENGINE_MAJOR_VERSION >= 5
#include "Interfaces/IMainFrameModule.h"
#else
/* ReSharper disable once CppUnusedIncludeDirective */
#include "MainFrame/Public/Interfaces/IMainFrameModule.h"
#endif

#if ENGINE_MAJOR_VERSION == 4
#include "ToolMenus.h"
#include "Logging/MessageLog.h"
#include "LevelEditor.h"
#endif

#include "PhysicsEngine/BodySetup.h"

#include "Settings/JsonAsAssetSettings.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Framework/Notifications/NotificationManager.h"
#include "IMessageLogListing.h"
#include "ISettingsModule.h"
#include "MessageLogModule.h"
#include "Styling/SlateIconFinder.h"

/* Settings */
#include "./Settings/Details/JsonAsAssetSettingsDetails.h"

/* ReSharper disable once CppUnusedIncludeDirective */
#include "HttpModule.h"
#include "Logging/MessageLog.h"
#include "Modules/Tools/AnimationData.h"
#include "Modules/Tools/ConvexCollision.h"
#include "Modules/UI/AboutJsonAsAsset.h"
#include "Modules/UI/CommandsModule.h"
#include "Modules/UI/StyleModule.h"
#include "Utilities/AppStyleCompatibility.h"
#include "Utilities/RemoteUtilities.h"
/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

#ifdef _MSC_VER
#undef GetObject
#endif

#define LOCTEXT_NAMESPACE "FJsonAsAssetModule"

#if PLATFORM_WINDOWS
    static TWeakPtr<SNotificationItem> ImportantNotificationPtr;
    static TWeakPtr<SNotificationItem> LocalFetchNotificationPtr;
#endif

void FJsonAsAssetModule::PluginButtonClicked() {
	Settings = GetMutableDefault<UJsonAsAssetSettings>();

	const FString ExportDirectoryPath = Settings->ExportDirectory.Path;
	
	if (ExportDirectoryPath.IsEmpty()) {
		return;
	}

	/* Invalid Export Directory */
	if (ExportDirectoryPath.Contains("\\")) {
		/* Fix up export directory */
		Settings->ExportDirectory.Path = ExportDirectoryPath.Replace(TEXT("\\"), TEXT("/"));
		
		SavePluginConfig(Settings);
	}

	/* Launch Local Fetch if not opened already */
	if (Settings->bEnableLocalFetch) {
		TSharedPtr<SNotificationItem> NotificationItem = LocalFetchNotificationPtr.Pin();

		if (NotificationItem.IsValid()) {
			NotificationItem->SetFadeOutDuration(0.001);
			NotificationItem->Fadeout();
			LocalFetchNotificationPtr.Reset();
		}

		bool bIsLocalHost = Settings->LocalFetchUrl.StartsWith("http://localhost");

		if (bIsLocalHost && !IsProcessRunning("LocalFetch.exe")) {
			bool bLocalFetchLaunched = LocalFetchModule::LaunchLocalFetch();

			if (!bLocalFetchLaunched) {
				FNotificationInfo Info(LOCTEXT("JsonAsAssetNotificationTitle", "Local Fetch API Required"));
				
				SetNotificationSubText(Info, LOCTEXT("JsonAsAssetNotificationText",
					"Start the Local Fetch API to use JsonAsAsset seamlessly. "
					"For guidance on Local Fetch settings, check the documentation."
				));

				Info.HyperlinkText = LOCTEXT("JsonAsAssetDocumentationLink", "Documentation");
				Info.Hyperlink = FSimpleDelegate::CreateStatic([]() {
					const FString URL = "https://github.com/JsonAsAsset/JsonAsAsset";
					FPlatformProcess::LaunchURL(*URL, nullptr, nullptr);
				});

				Info.bFireAndForget = false;
				Info.FadeOutDuration = 3.0f;
				Info.ExpireDuration = 3.0f;
				Info.bUseLargeFont = false;
				Info.bUseThrobber = false;
				Info.Image = FJsonAsAssetStyle::Get().GetBrush("JsonAsAsset.Toolbar.Icon");

				Info.ButtonDetails.Add(
					FNotificationButtonInfo(LOCTEXT("StartLocalFetch", "Execute LocalFetch API (.EXE)"), FText::GetEmpty(),
						FSimpleDelegate::CreateStatic([]() {
							TSharedPtr<SNotificationItem> NotificationItem = LocalFetchNotificationPtr.Pin();

							if (NotificationItem.IsValid()) {
								NotificationItem->SetFadeOutDuration(0.001);
								NotificationItem->Fadeout();
								LocalFetchNotificationPtr.Reset();
							}

							LocalFetchModule::LaunchLocalFetch();
						})
					)
				);

				LocalFetchNotificationPtr = FSlateNotificationManager::Get().AddNotification(Info);
				LocalFetchNotificationPtr.Pin()->SetCompletionState(SNotificationItem::CS_Pending);

				return;
			}	
		}
	}

	if (Settings->bEnableLocalFetch) {
		/* If GameName isn't set, try getting it from the API */
		if (Settings->AssetSettings.GameName.IsEmpty()) {
			SendHttpRequest(TEXT("http://localhost:1500/api/name"), [this](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful) {
				if (bWasSuccessful && Response.IsValid()) {
					UJsonAsAssetSettings* Settings = GetMutableDefault<UJsonAsAssetSettings>();

					Settings->AssetSettings.GameName = Response->GetContentAsString();
					
					SavePluginConfig(Settings);
				}
				
				PluginButtonClicked();
			});

			return;
		}
	}

	/* Dialog for a JSON File */
	TArray<FString> OutFileNames = OpenFileDialog("Open JSON file", "JSON Files|*.json");
	if (OutFileNames.Num() == 0)
		return;

	for (FString& File : OutFileNames) {
		/* Clear Message Log */
		FMessageLogModule& MessageLogModule = FModuleManager::GetModuleChecked<FMessageLogModule>("MessageLog");
		TSharedRef<IMessageLogListing> LogListing = (MessageLogModule.GetLogListing("JsonAsAsset"));
		LogListing->ClearMessages();

		/* Import asset by IImporter */
		IImporter::ImportReference(File);
	}
}

void FJsonAsAssetModule::StartupModule() {
    /* Initialize plugin style, reload textures, and register commands */
    FJsonAsAssetStyle::Initialize();
    FJsonAsAssetStyle::ReloadTextures();
    FJsonAsAssetCommands::Register();

	this->PropertySerializer = NewObject<UPropertySerializer>();
	this->GObjectSerializer = NewObject<UObjectSerializer>();

	GObjectSerializer->SetPropertySerializer(PropertySerializer);

    /* Set up plugin command list and map actions */
    PluginCommands = MakeShareable(new FUICommandList);
    PluginCommands->MapAction(
        FJsonAsAssetCommands::Get().PluginAction,
        FExecuteAction::CreateRaw(this, &FJsonAsAssetModule::PluginButtonClicked),
        FCanExecuteAction()
    );

    /* Register menus on startup */
    UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FJsonAsAssetModule::RegisterMenus));

    /* Check for export directory in settings */
    Settings = GetMutableDefault<UJsonAsAssetSettings>();
	
	if (Settings->ExportDirectory.Path.IsEmpty()) {
	    const FText TitleText = LOCTEXT("JsonAsAssetNotificationTitle", "Missing Export Directory");
	    const FText MessageText = LOCTEXT("JsonAsAssetNotificationText",
	        "JsonAsAsset requires an export directory to handle references and locally check for files to import. "
	        "The plugin will not function properly without this set.\n\nFor more information, please see the documentation for JsonAsAsset."
	    );

	    FNotificationInfo Info(TitleText);

	#if ENGINE_MAJOR_VERSION >= 5
	    Info.SubText = MessageText;
	#else
	    Info.Text = MessageText;
	#endif

	    /* Set up hyperlink for documentation */
	    Info.HyperlinkText = LOCTEXT("JsonAsAssetDocumentation", "Documentation");
	    Info.Hyperlink = FSimpleDelegate::CreateStatic([]() { 
	        const FString URL = "https://github.com/JsonAsAsset/JsonAsAsset";
	        FPlatformProcess::LaunchURL(*URL, nullptr, nullptr); 
	    });

	    /* Notification settings */
	    Info.bFireAndForget = false;
	    Info.FadeOutDuration = 3.0f;
	    Info.ExpireDuration = 0.0f;
	    Info.bUseLargeFont = false;
	    Info.bUseThrobber = false;

	    /* Add button to open plugin settings */
	    Info.ButtonDetails.Add(
	        FNotificationButtonInfo(
	            LOCTEXT("OpenPluginSettings", "Open Settings"),
	            FText::GetEmpty(),
	            FSimpleDelegate::CreateStatic([]() {
	                const TSharedPtr<SNotificationItem> NotificationItem = ImportantNotificationPtr.Pin();
	                if (NotificationItem.IsValid()) {
	                    NotificationItem->Fadeout();
	                    ImportantNotificationPtr.Reset();
	                }

	                /* Navigate to plugin settings */
	                FModuleManager::LoadModuleChecked<ISettingsModule>("Settings")
	                    .ShowViewer("Editor", "Plugins", "JsonAsAsset");
	            })
	        )
	    );

	    ImportantNotificationPtr = FSlateNotificationManager::Get().AddNotification(Info);
	    ImportantNotificationPtr.Pin()->SetCompletionState(SNotificationItem::CS_Pending);
	}

    /* Set up message log for JsonAsAsset */
    {
        FMessageLogModule& MessageLogModule = FModuleManager::LoadModuleChecked<FMessageLogModule>("MessageLog");
        FMessageLogInitializationOptions InitOptions;
        InitOptions.bShowPages = true;
        InitOptions.bAllowClear = true;
        InitOptions.bShowFilters = true;
        MessageLogModule.RegisterLogListing("JsonAsAsset", NSLOCTEXT("JsonAsAsset", "JsonAsAssetLogLabel", "JsonAsAsset"), InitOptions);
    }

#if ENGINE_MAJOR_VERSION == 4
	FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor"); {
    	TSharedPtr<FExtender> ToolbarExtender = MakeShareable(new FExtender);
    	ToolbarExtender->AddToolBarExtension("Settings", EExtensionHook::After, PluginCommands, FToolBarExtensionDelegate::CreateRaw(this, &FJsonAsAssetModule::AddToolbarExtension));

    	LevelEditorModule.GetToolBarExtensibilityManager()->AddExtender(ToolbarExtender);
	}
#endif

    /* Register custom class layout for settings */
    FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
    PropertyModule.RegisterCustomClassLayout(UJsonAsAssetSettings::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FJsonAsAssetSettingsDetails::MakeInstance));

	Plugin = IPluginManager::Get().FindPlugin("JsonAsAsset");
	
	CheckForUpdates();
}

void FJsonAsAssetModule::ShutdownModule() {
	/* Unregister startup callback and tool menus */
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);

	/* Shutdown the plugin style and unregister commands */
	FJsonAsAssetStyle::Shutdown();
	FJsonAsAssetCommands::Unregister();

	/* Unregister message log listing if the module is loaded */
	if (FModuleManager::Get().IsModuleLoaded("MessageLog")) {
		FMessageLogModule& MessageLogModule = FModuleManager::GetModuleChecked<FMessageLogModule>("MessageLog");
		MessageLogModule.UnregisterLogListing("JsonAsAsset");
	}
}

void FJsonAsAssetModule::RegisterMenus() {
	FToolMenuOwnerScoped OwnerScoped(this);

	/* Extend the Level Editor toolbar */
	UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar.PlayToolBar");
	FToolMenuSection& Section = Menu->FindOrAddSection("JsonAsAsset");

	TSharedPtr<FUICommandList> Actions = MakeShared<FUICommandList>();
	Plugin = IPluginManager::Get().FindPlugin("JsonAsAsset");

	const FUIAction UIAction(
		FExecuteAction::CreateRaw(this, &FJsonAsAssetModule::PluginButtonClicked),
		FCanExecuteAction::CreateLambda([this]() { return !Settings->ExportDirectory.Path.IsEmpty(); })
	);

	/* JsonAsAsset Button */
	FToolMenuEntry PluginActionButtonEntry = FToolMenuEntry::InitToolBarButton(
		FName("JsonAsAsset"),
		FToolUIActionChoice(UIAction),
		FText::FromString(Plugin->GetDescriptor().VersionName),
		FText::GetEmpty(),
		TAttribute<FSlateIcon>::Create(
			TAttribute<FSlateIcon>::FGetter::CreateLambda([this]() -> FSlateIcon {
				return Settings->ExportDirectory.Path.IsEmpty() 
					? FSlateIcon(FJsonAsAssetStyle::Get().GetStyleSetName(), FName("JsonAsAsset.Toolbar.Icon.Warning"))
					: FSlateIcon(FJsonAsAssetStyle::Get().GetStyleSetName(), FName("JsonAsAsset.Toolbar.Icon"));
			})
		),
		EUserInterfaceActionType::Button
	);

	PluginActionButtonEntry.ToolTip = TAttribute<FText>::Create(
		TAttribute<FText>::FGetter::CreateLambda([this]() -> FText {
			return Settings->ExportDirectory.Path.IsEmpty()
				? FText::FromString("The button is disabled because no export directory has been specified. Please set an export directory in the plugin settings.")
				: LOCTEXT("JsonAsAsset_Tooltip", "Execute JsonAsAsset");
		})
	);
	
#if ENGINE_MAJOR_VERSION != 4
	PluginActionButtonEntry.StyleNameOverride = "CalloutToolbar";
	PluginActionButtonEntry.SetCommandList(PluginCommands);
#endif

	Section.AddEntry(PluginActionButtonEntry);

	/* Settings dropdown */
	const FToolMenuEntry PluginMenuEntry = FToolMenuEntry::InitComboButton(
		"JsonAsAssetMenu",
		FUIAction(),
		FOnGetContent::CreateRaw(this, &FJsonAsAssetModule::CreateToolbarDropdown),
		LOCTEXT("JsonAsAssetButtonLabel", "JsonAsAsset"),
		LOCTEXT("JsonAsAssetButtonTooltip", "Open JsonAsAsset Tool-bar"),
		FSlateIcon(),
		true
	);
	
	Section.AddEntry(PluginMenuEntry);
}

#if ENGINE_MAJOR_VERSION == 4
void FJsonAsAssetModule::AddToolbarExtension(FToolBarBuilder& Builder) {
	Builder.AddToolBarButton(
		FUIAction(
			FExecuteAction::CreateRaw(this, &FJsonAsAssetModule::PluginButtonClicked),
			FCanExecuteAction(),
			FGetActionCheckState(),
			FIsActionButtonVisible::CreateLambda([this]() {
				static const auto CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("Plugin.HideActions"));

				if (CVar) {
					return CVar && CVar->GetInt() == 0;
				}

				return true;
			})
		),
		NAME_None,
		FText::FromString(Plugin->GetDescriptor().VersionName),
		LOCTEXT("JsonAsAssetExecuteAction", "Execute JsonAsAsset"),
		FSlateIcon(FJsonAsAssetStyle::Get().GetStyleSetName(), FName("JsonAsAsset.Toolbar.Icon"))
	);

	Builder.AddComboButton(
		FUIAction(
			FExecuteAction(),
			FCanExecuteAction(),
			FGetActionCheckState(),
			FIsActionButtonVisible::CreateLambda([this]() {
				static const auto CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("Plugin.HideActions"));

				if (CVar) {
					return CVar && CVar->GetInt() == 0;
				}

				return true;
			})
		),
		FOnGetContent::CreateRaw(this, &FJsonAsAssetModule::CreateToolbarDropdown),
		FText::FromString(Plugin->GetDescriptor().VersionName),
		LOCTEXT("JsonAsAssetButtonTooltip", "Open JsonAsAsset Tool-bar"),
		FSlateIcon(FJsonAsAssetStyle::Get().GetStyleSetName(), FName("JsonAsAsset.Toolbar.Icon")),
		true
	);
}
#endif

TSharedRef<SWidget> FJsonAsAssetModule::CreateToolbarDropdown() {
	Settings = GetMutableDefault<UJsonAsAssetSettings>();

	FMenuBuilder MenuBuilder(false, nullptr);
	
	MenuBuilder.BeginSection(
		"JsonAsAssetTopSection", 
		FText::Format(LOCTEXT("JsonToolsVersion", "JsonAsAsset v{0}"), FText::FromString(Plugin->GetDescriptor().VersionName))
	); {
		MenuBuilder.AddSubMenu(
			LOCTEXT("JsonAsAssetAssetTypesMenu", "Asset Types"),
			LOCTEXT("JsonAsAssetAssetTypesMenuToolTip", "List of supported assets for JsonAsAsset"),
			FNewMenuDelegate::CreateLambda([this](FMenuBuilder& InnerMenuBuilder) {
				SupportedAssetsDropdown(InnerMenuBuilder);
			}),
			false,
			FSlateIcon(FAppStyle::Get().GetStyleSetName(), "LevelEditor.Tabs.Viewports")
		);

		MenuBuilder.AddMenuEntry(
		LOCTEXT("JsonAsAssetDocumentationButton", "Documentation"),
		LOCTEXT("JsonAsAssetDocumentationButtonTooltip", "View JsonAsAsset documentation"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Documentation"),
			FUIAction(
				FExecuteAction::CreateLambda([this]() {
					FString URL = "https://github.com/JsonAsAsset/JsonAsAsset";
					FPlatformProcess::LaunchURL(*URL, nullptr, nullptr);
				})
			),
			NAME_None
		);

		if (Settings->AssetSettings.bEnableAssetTools) {
			MenuBuilder.AddSubMenu(
				LOCTEXT("JsonAsAssetAssetToolsMenu", "Open Asset Tools"),
				LOCTEXT("JsonAsAssetAssetToolsMenuToolTip", "Extra functionality / tools to do very specific data importing with assets."),
				FNewMenuDelegate::CreateLambda([this](FMenuBuilder& InnerMenuBuilder) {
					InnerMenuBuilder.BeginSection("JsonAsAssetSection", LOCTEXT("JsonAsAssetSection", "Asset Tools"));
					{
						InnerMenuBuilder.AddMenuEntry(
							LOCTEXT("JsonAsAssetAssetToolsCollisionExButton", "Import Folder Collision Convex"),
							LOCTEXT("JsonAsAssetAssetToolsButtonTooltip", "Imports convex collision data using Local Fetch and applies it to the corresponding assets."),
							FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.BspMode"),

							FUIAction(
								FExecuteAction::CreateStatic(&FToolConvexCollision::Execute)
							),
							NAME_None
						);

						InnerMenuBuilder.AddMenuEntry(
							LOCTEXT("JsonAsAssetAssetToolsAnimationDataButton", "Import Animation Data from Local Fetch"),
							LOCTEXT("JsonAsAssetAssetToolsAnimationDataButtonTooltip", "Imports animation data using Local Fetch and applies it to the corresponding assets in the content browser folder."),
							FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.BspMode"),

							FUIAction(
								FExecuteAction::CreateStatic(&FToolAnimationData::Execute)
							),
							NAME_None
						);
					}
					InnerMenuBuilder.EndSection();
				}),
				false,
				FSlateIcon(FAppStyle::Get().GetStyleSetName(), "ProjectSettings.TabIcon")
			);
		}
	}

	MenuBuilder.EndSection();

	bActionRequired = Settings->ExportDirectory.Path.IsEmpty();

	if (bActionRequired) {
		MenuBuilder.BeginSection("JsonAsAssetActionRequired", FText::FromString("Action Required")); {
			/* Export Directory Missing */
			if (Settings->ExportDirectory.Path.IsEmpty())
				MenuBuilder.AddMenuEntry(
					LOCTEXT("JsonAsAssetActionRequiredButton", "Missing Export Directory"),
					LOCTEXT("JsonAsAssetActionRequiredButtonTooltip", "Update the export directory in JsonAsAsset's plugin settings."),
					FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.WarningWithColor"),
					FUIAction(
						FExecuteAction::CreateLambda([this]() {
							/* Send user to plugin */
							FModuleManager::LoadModuleChecked<ISettingsModule>("Settings")
								.ShowViewer("Editor", "Plugins", "JsonAsAsset");
						})
					),
					NAME_None
				);
		}
		MenuBuilder.EndSection();
	}

	CreateLocalFetchDropdown(MenuBuilder);

	MenuBuilder.AddSeparator();
	MenuBuilder.AddMenuEntry(
		LOCTEXT("JsonAsAssetSettingsButton", "Open Plugin Settings"),
		LOCTEXT("JsonAsAssetSettingsButtonTooltip", "Navigate to the JsonAsAsset plugin settings"),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Settings"),
		FUIAction(
			FExecuteAction::CreateLambda([this]() {
				/* Send user to plugin settings */
				FModuleManager::LoadModuleChecked<ISettingsModule>("Settings")
					.ShowViewer("Editor", "Plugins", "JsonAsAsset");
			})
		),
		NAME_None
	);
	MenuBuilder.AddMenuEntry(
		LOCTEXT("JsonAsAssetMessageLogButton", "Open Message Log"),
		LOCTEXT("JsonAsAssetMessageLogButtonTooltip", "View logs of JsonAsAsset"),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "MessageLog.TabIcon"),
		FUIAction(
			FExecuteAction::CreateLambda([this]() {
				FMessageLog MessageLogger = FMessageLog(FName("JsonAsAsset"));
				MessageLogger.Open(EMessageSeverity::Info, true);
			})
		),
		NAME_None
	);

	CreateVersioningDropdown(MenuBuilder);

	return MenuBuilder.MakeWidget();
}

void FJsonAsAssetModule::CreateLocalFetchDropdown(FMenuBuilder MenuBuilder) const {
	/* Local Fetch must be enabled, and if there is an action required, don't create Local Fetch's dropdown */
	if (!Settings->bEnableLocalFetch || bActionRequired) {
		return;
	}
	
	MenuBuilder.BeginSection("JsonAsAssetSection", FText::FromString("Local Fetch"));
	MenuBuilder.AddSubMenu(
		LOCTEXT("JsonAsAssetLocalFetchTypesMenu", "Asset Types"),
		LOCTEXT("JsonAsAssetLocalFetchTypesMenuToolTip", "List of supported classes that can be locally fetched using the API"),
		FNewMenuDelegate::CreateLambda([this](FMenuBuilder& InnerMenuBuilder) {
			SupportedAssetsDropdown(InnerMenuBuilder, true);
		}),
		false,
		FSlateIcon()
	);

	MenuBuilder.AddSubMenu(
		LOCTEXT("JsonAsAssetLocalFetchCMDMenu", "Command-line Application"),
		LOCTEXT("", ""),
		FNewMenuDelegate::CreateLambda([this](FMenuBuilder& InnerMenuBuilder) {
			InnerMenuBuilder.BeginSection("JsonAsAssetSection", LOCTEXT("JsonAsAssetSection", "Console"));
			{
				const bool bIsLocalFetchRunning = IsProcessRunning("LocalFetch.exe");

				if (bIsLocalFetchRunning) {
					InnerMenuBuilder.AddMenuEntry(
						FText::FromString("Restart Local Fetch (.EXE)"),
						FText::FromString(""),
						FSlateIcon(),
						FUIAction(
							FExecuteAction::CreateLambda([this]() {
								LocalFetchModule::CloseLocalFetch();
								LocalFetchModule::LaunchLocalFetch();
							}),
							FCanExecuteAction::CreateLambda([this]() {
								return IsProcessRunning("LocalFetch.exe");
							})
						)
					);

					InnerMenuBuilder.AddMenuEntry(
						FText::FromString("Shutdown Local Fetch (.EXE)"),
						FText::FromString(""),
						FSlateIcon(),
						FUIAction(
							FExecuteAction::CreateLambda([this]() {
								LocalFetchModule::CloseLocalFetch();
							}),
							FCanExecuteAction::CreateLambda([this]() {
								return IsProcessRunning("LocalFetch.exe");
							})
						)
					);
				} else {
					InnerMenuBuilder.AddMenuEntry(
						FText::FromString("Execute Local Fetch (.EXE)"),
						FText::FromString(""),
						FSlateIcon(),
						FUIAction(
							FExecuteAction::CreateLambda([this]() {
								TSharedPtr<SNotificationItem> NotificationItem = LocalFetchNotificationPtr.Pin();

								if (NotificationItem.IsValid()) {
									NotificationItem->SetFadeOutDuration(0.001);
									NotificationItem->Fadeout();
									LocalFetchNotificationPtr.Reset();
								}

								LocalFetchModule::LaunchLocalFetch();
							}),
							FCanExecuteAction::CreateLambda([this]() {
								return !IsProcessRunning("LocalFetch.exe");
							})
						)
					);
				}
			}
			InnerMenuBuilder.EndSection();
		}),
		false,
		FSlateIcon()
	);
	MenuBuilder.EndSection();
}

void FJsonAsAssetModule::CreateVersioningDropdown(FMenuBuilder MenuBuilder) const {
	if (Versioning.bLatestVersion || !Versioning.bIsValid) {
		MenuBuilder.AddSeparator();
		
		CreateLastDropdown(MenuBuilder);

		return;
	}
	
	MenuBuilder.BeginSection("JsonAsAssetVersioningSection", FText::FromString("Versioning"));
	
	FText Text, Tooltip;
	FSlateIcon Icon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "Blueprint.CompileStatus.Background", NAME_None);

	/* A new release is available */
	if (Versioning.bNewVersionAvailable) {
		Text = FText::FromString("New Version Available");
		
		Tooltip = FText::FromString("Update your installation to version " + Versioning.VersionName + " of JsonAsAsset");

		Icon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "Cascade.AddLODBeforeCurrent.Small");
	} else if (Versioning.bFutureVersion) {
		Text = FText::FromString("Developmental Build");
		
		Tooltip = FText::FromString("You are currently running a developmental build of JsonAsAsset");
		
	} else {
		Text = FText::FromString("Latest Version");
		
		Tooltip = FText::FromString("You are currently using the latest version of JsonAsAsset");
	}

	MenuBuilder.AddMenuEntry(
		Text,
		Tooltip,
		Icon,
		FUIAction(
			FExecuteAction::CreateLambda([this]() {
				if (Versioning.bNewVersionAvailable) {
					FPlatformProcess::LaunchURL(*Versioning.HTMLUrl, nullptr, nullptr);
				}
			})
		),
		NAME_None
	);
	
	MenuBuilder.EndSection();
}

void FJsonAsAssetModule::CreateLastDropdown(FMenuBuilder MenuBuilder) const {
	MenuBuilder.AddMenuEntry(
		LOCTEXT("JsonAsAssetAboutButton", "About JsonAsAsset"),
		LOCTEXT("JsonAsAssetAboutButtonTooltip", "More information about JsonAsAsset"),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "MessageLog.Action"),
		FUIAction(
			FExecuteAction::CreateLambda([this]() {
#if ENGINE_MAJOR_VERSION >= 5
				TSharedPtr<SWindow> AboutWindow =
					SNew(SWindow)
					.Title(LOCTEXT("AboutJsonAsAsset", "About JsonAsAsset"))
					.ClientSize(FVector2D(720.f, 170.f))
					.SupportsMaximize(false).SupportsMinimize(false)
					.SizingRule(ESizingRule::FixedSize)
					[
						SNew(SAboutJsonAsAsset)
					];

				IMainFrameModule& MainFrame = FModuleManager::LoadModuleChecked<IMainFrameModule>("MainFrame");
				TSharedPtr<SWindow> ParentWindow = MainFrame.GetParentWindow();

				if (ParentWindow.IsValid())
					FSlateApplication::Get().AddModalWindow(AboutWindow.ToSharedRef(), ParentWindow.ToSharedRef());
				else FSlateApplication::Get().AddWindow(AboutWindow.ToSharedRef());
#endif
			})
		),
		NAME_None
	);
}

void FJsonAsAssetModule::SupportedAssetsDropdown(FMenuBuilder& InnerMenuBuilder, bool bIsLocalFetch) {
	TMap<FString, TArray<FString>> CategoriesAndTypes = ImporterTemplatedTypes;

	/* Add asset types from the factory registry */
	for (auto Pair : IImporter::GetFactoryRegistry()) {
		if (CategoriesAndTypes.Find(Pair.Value.Category)) {
			CategoriesAndTypes[Pair.Value.Category].Append(Pair.Key);
		}
		else {
			CategoriesAndTypes.Add(Pair.Value.Category, Pair.Key);
		}
	}

	/* Sort categories by alphabetical order */
	TArray<TPair<FString, TArray<FString>>> SortedCategoryPairs;
	for (const TPair<FString, TArray<FString>>& Pair : CategoriesAndTypes) {
		SortedCategoryPairs.Add(Pair);
	}

	SortedCategoryPairs.Sort([](const TPair<FString, TArray<FString>>& A, const TPair<FString, TArray<FString>>& B) {
		return A.Key < B.Key;
	});

	/* Sort asset types by alphabetical order */
	for (TPair<FString, TArray<FString>>& CategoryPair : CategoriesAndTypes) {
		CategoryPair.Value.Sort([](const FString& A, const FString& B) {
			return A < B;
		});
	}

	/* Go through each pair and add the entries */
	for (const TPair<FString, TArray<FString>>& Pair : SortedCategoryPairs) {
		FString Category = Pair.Key;
		TArray<FString> AssetTypes = Pair.Value;
					
		InnerMenuBuilder.BeginSection(FName(*Category), FText::FromString(Category));

		for (const FString& AssetType : AssetTypes) {
			if (bIsLocalFetch) {
				if (!IImporter::CanImportWithLocalFetch(AssetType)) {
					continue;
				}
			}
			const UClass* Class = FindObject<UClass>(nullptr, *("/Script/Engine." + AssetType));
			FText Description = Class ? Class->GetToolTipText() : FText::FromString(AssetType);

			InnerMenuBuilder.AddMenuEntry(
				FText::FromString(AssetType),
				Description,
				FSlateIconFinder::FindCustomIconForClass(Class, TEXT("ClassThumbnail")),
				FUIAction(
					FExecuteAction::CreateLambda([]() {
					})
				)
			);
		}

		InnerMenuBuilder.EndSection();
	}
}

void FJsonAsAssetModule::CheckForUpdates() {
	Versioning.SetValid(false);

	FHttpModule* HttpModule = &FHttpModule::Get();

#if ENGINE_MAJOR_VERSION >= 5
	const TSharedRef<IHttpRequest> Request = HttpModule->CreateRequest();
#else
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = HttpModule->CreateRequest();
#endif
	
#if ENGINE_MAJOR_VERSION >= 5
	const TSharedPtr<IHttpResponse> Response = FRemoteUtilities::ExecuteRequestSync(Request);
#else
	const TSharedPtr<IHttpResponse, ESPMode::ThreadSafe> Response = FRemoteUtilities::ExecuteRequestSync(Request);
#endif
	
    Request->SetURL(TEXT("https://api.github.com/repos/JsonAsAsset/JsonAsAsset/releases/latest"));
    Request->SetVerb(TEXT("GET"));
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));

    Request->OnProcessRequestComplete().BindLambda([this](FHttpRequestPtr Req, FHttpResponsePtr Resp, bool bSuccess) {
        /* Check if the request was successful and the response is valid */
        if (!bSuccess || !Resp.IsValid()) {
            UE_LOG(LogTemp, Warning, TEXT("HTTP request failed or no internet connection."));
        	
            return;
        }

        const FString ResponseString = Resp->GetContentAsString();

        /* Deserialize the JSON response */
        TSharedPtr<FJsonObject> JsonObject;
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseString);
        if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid()) {
            return;
        }

        /* It must have the name property */
        if (!JsonObject->HasField(TEXT("name"))) {
            return;
        }

    	const FString VersionName = JsonObject->GetStringField(TEXT("name"));
    	const FString CurrentVersionName = Plugin->GetDescriptor().VersionName;

    	const int LatestVersion = ConvertVersionStringToInt(VersionName);
    	const int CurrentVersion = ConvertVersionStringToInt(Plugin->GetDescriptor().VersionName);

    	Versioning = FJsonAsAssetVersioning(CurrentVersion, LatestVersion, JsonObject->GetStringField(TEXT("html_url")), VersionName, CurrentVersionName);
    	Versioning.SetValid(true);
    });

    Request->ProcessRequest();
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FJsonAsAssetModule, JsonAsAsset)
