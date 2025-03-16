# Key information for contributors: 🎓

Local Fetch's API is located at [JsonAsAsset/LocalFetch](https://github.com/JsonAsAsset/LocalFetch)
It uses CUE4Parse to read game files, and it interacts with the config files inside of the project.

##### Adding Asset Types
> *Asset types without manual code will use **basic** importing, meaning it will only take the properties of the base object and import them.*
- Normal Asset types are found in [`JsonAsAsset/Private/Importers/Constructor/Importer.cpp`](https://github.com/JsonAsAsset/JsonAsAsset/blob/main/Source/JsonAsAsset/Private/Importers/Constructor/Importer.cpp#L82) You don't need to add anything here if you made a custom IImporter with the REGISTER_IMPORTER macro.

##### Custom Logic for Asset Types

Adding **manual** asset type imports is done in the [`JsonAsAsset/Public/Importers/Types`](https://github.com/JsonAsAsset/JsonAsAsset/tree/main/Source/JsonAsAsset/Public/Importers/Types) folder. Use other importers for reference on how to create one.

##### Settings

JsonAsAsset's settings are in [`Public/Settings/JsonAsAssetSettings.h`](https://github.com/JsonAsAsset/JsonAsAsset/blob/main/Source/JsonAsAsset/Public/Settings/JsonAsAssetSettings.h)

##### General Information
This plugin's importing feature uses data based off [CUE4Parse](https://github.com/FabianFG/CUE4Parse)'s JSON export format.
