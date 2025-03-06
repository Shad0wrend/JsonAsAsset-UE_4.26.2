# Key information for contributors: 🎓

Local Fetch's API is located at [JsonAsAsset/LocalFetch](https://github.com/JsonAsAsset/LocalFetch)
It uses CUE4Parse to read game files, and it interacts with the config files inside of the project.

##### Adding Asset Types
> *Asset types without manual code will use **basic** importing, meaning it will only take the properties of the base object and import them.*
- Normal Asset types are found in [`JsonAsAsset/Private/Importers/Constructor/Importer.cpp`](https://github.com/JsonAsAsset/JsonAsAsset/blob/main/Source/JsonAsAsset/Private/Importers/Constructor/Importer.cpp#L81)
- Adding support for Local Fetch of asset types [`JsonAsAsset/Private/Modules/LocalFetchModule.cpp`](https://github.com/JsonAsAsset/JsonAsAsset/blob/main/Source/JsonAsAsset/Private/Modules/LocalFetchModule.cpp#L17)

##### Custom Logic for Asset Types

Adding **manual** asset type imports is done in the [`JsonAsAsset/Public/Importers/Types`](https://github.com/JsonAsAsset/JsonAsAsset/tree/main/Source/JsonAsAsset/Public/Importers/Types) folder.

##### Settings

JsonAsAsset's settings are in [`Public/Settings/JsonAsAssetSettings.h`](https://github.com/JsonAsAsset/JsonAsAsset/blob/main/Source/JsonAsAsset/Public/Settings/JsonAsAssetSettings.h)

##### General Information
This plugin's importing feature uses data based off [CUE4Parse](https://github.com/FabianFG/CUE4Parse)'s JSON export format.
