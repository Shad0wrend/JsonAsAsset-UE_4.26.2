# JsonAsAsset

[![Discord](https://img.shields.io/badge/Join%20Discord-Collector?color=0363ff&logo=discord&logoColor=white&style=for-the-badge)](https://discord.gg/h9s6qpBnUT)
[![GitHub Repo stars](https://img.shields.io/github/stars/JsonAsAsset/JsonAsAsset?style=for-the-badge&logo=&color=gold)](https://github.com/JsonAsAsset/JsonAsAsset/stargazers)
[![GitHub Downloads (all assets, all releases)](https://img.shields.io/github/downloads/JsonAsAsset/JsonAsAsset/total?style=for-the-badge&label=DOWNLOADS&color=blue)](https://github.com/JsonAsAsset/JsonAsAsset/releases)

[![Unreal Engine 5 Supported)](https://img.shields.io/badge/UE5.0+-black?logo=unrealengine&style=for-the-badge&labelColor=grey)](#install)
[![Unreal Engine 4.27.2 Supported)](https://img.shields.io/badge/4.27.2-black?logo=unrealengine&style=for-the-badge&labelColor=grey)](#install)
[![Unreal Engine 4.26.2 Supported)](https://img.shields.io/badge/4.26.2-black?logo=unrealengine&style=for-the-badge&labelColor=grey)](#install)

### Description:

A user friendly plugin for [Unreal Engine](https://www.unrealengine.com/en-US) that reads [JSON](https://www.json.org/json-en.html) files extracted from [FModel](https://fmodel.app), and regenerates assets as they were from the game's files.

✨ [Contributors](#contribute)

### Example Use:

- Porting **materials**, data assets, data tables *[(see more here)](#intro)*
- Porting **physics assets** for skeletal meshes
- Porting **sound effects** to Unreal Engine
- Automating the porting process for users

The goal of this project is to simplify the process for the porting and modding communities, enabling them to more easily port over assets into Unreal Engine.

-----------------

**Table of Contents**:
<br> 

> 1. [Introduction to JsonAsAsset](#intro)
> 1. [Installation](#install)  
>    2.1 [Setup FModel](#setup-fmodel)  
>    2.2 [Setup Settings](#setup-settings)  
> 3. [Local Fetch](#setup-local-fetch)

**Extras**:
<br> 

> - [Common Errors 🐛](#common-errors)
> - [Licensing 📜](#licensing)

-----------------

<a name="intro"></a>
## 1. Introduction to JsonAsAsset
> [!NOTE]
> Please note that this plugin is intended solely for **personal and educational use**. Do not use it to create or distribute **commercial products** without obtaining the necessary **licenses and permissions**. It is important to respect **intellectual property rights** and only use assets that you are **authorized to use**. We do not assume any responsibility for the way the created content is used.

JsonAsAsset is a user-friendly Unreal Engine plugin for importing assets from packaged games using JSON files. With a sleek interface, it simplifies mapping JSON data to Unreal Engine structures, supporting both manual and automated workflows.

<details>
  <summary>Supported Asset Types</summary>

###### Sound Classes
Most/all sound classes are supported, however, SoundWave is handled manually in SoundCue.

###### Materials
 - Material
 - MaterialFunction
 - MaterialParameterCollection
 - PhysicalMaterial
 - SubsurfaceProfile

**Material Data**:
<br> Unreal Engine games made below 4.12 (a guess) will have material data, any games made above that version will most definitely not have any material data, and therefore the actual data will be stripped and cannot be imported. **Unless** you are using a User Generated Content editor, then it's possible the material data will be there.
  
###### Curve Asset Types
 - CurveFloat
 - CurveTable
 - CurveVector
 - CurveLinearColor
 - CurveLinearColorAtlas

###### Skeleton/Animation Asset Types
 - SkeletalMeshLODSettings
 - Animation Curves (Animation Sequences / Animation Montages)
> Save a JSON file and have a pre-existing animation selected in the editor, then import the json
 - Blendspace
 - PhysicsAsset

###### Data Asset Types
- DataAsset
- DataTable

###### Landscape
- LandscapeGrassType
- PhysicalMaterial

</details>

<a name="install"></a>
## 2. Installation
> [!WARNING]
> JsonAsAsset may not work for every Unreal Engine 5 version; please check Releases for compatibility. Unreal Engine 4 is not maintained at the moment and is not planned to be supported.
> *`(See branches for the current available Unreal Engine 4 versions)`*

1. Go to the [Releases page](/../../releases) for the plugin.
2. **Download the release** that matches your version of Unreal Engine. If there **isn't a release that matches your version**, you will need to [**compile the plugin yourself**](https://dev.epicgames.com/community/learning/tutorials/qz93/unreal-engine-building-plugins).
3. Extract the downloaded files to your project's Plugins folder. If there isn't a Plugins folder, create one in the root directory of your project.
4. Open your Unreal Engine project.
5. Click on Edit -> Plugins.
6. In the Plugins window, search for `JsonAsAsset` and enable it.
7. Restart the editor for the changes to take effect.

<a name="setup-fmodel"></a>
#### 2.1 Setup FModel
If you haven't already, install FModel and set it up correctly.
<img align="left" width="150" height="150" src="https://github.com/user-attachments/assets/d8e4f9c9-1268-4aee-ab1a-dabee31b3069?raw=true">
The JSON format/file has to be from a program that fits the format of FModel's JSON export files:
<br><br>

- [FModel](https://fmodel.app) *(Software for exploring Unreal Engine games)*

-------------------

Now that you've installed FModel and set it up up correctly, we can continue to setting up JsonAsAsset for our own Unreal Engine project.

<a name="setup-settings"></a>
##### 2.2 Setup Settings
<img align="right" width="300" height="180" src=https://github.com/JsonAsAsset/JsonAsAsset/assets/73559984/aad4e86a-6f0b-4e66-aef1-13d30d8215de)>

> [!NOTE]
> If you have FModel installed and setup correctly, you can now press the FModel Settings button in your Settings to grab it from there!

We need to change the export directory to allow the plugin to differentiate between what's your directory and what's the game path it should put it in.

First, open up the JsonAsAsset plugin settings (basically, do what's on the picture on the right) and make sure you are looking at the property "Export Directory."

Now open up FModel and go to your settings. `(Settings -> General)` There will be a setting called `Output Directory,` copy that and go back to Unreal Engine. Now you need to click on the three dots, jump to the folder you copied, go to the folder named `Exports,` then press `Select Folder.`

-------------------

That’s the basic setup! To bulk import assets and **what they reference**, you must set up [`Local Fetch`](#setup-local-fetch)!

<a name="setup-local-fetch"></a>
## 3. Setting Up *Local Fetch*
Running the API requires ASP.NET 8.0 to be installed; please install this [here](https://dotnet.microsoft.com/en-us/download/dotnet/thank-you/runtime-aspnetcore-8.0.1-windows-x64-installer).

<img align="right" width="460" height="156" src=https://github.com/user-attachments/assets/2e3a3680-ccba-4847-9e81-50242085ae63>

> [!TIP]
> Please ensure you have the plugin in your project's directory and not the Engine.

Before we can launch Local Fetch and begin working on automated references, you need to input all the information about your game first.

Many of these settings are similar to FModel, but manually select a file or directory using UE's file selector.

<img align="right" width="220" height="156" src=https://github.com/user-attachments/assets/ede73451-9e69-40d9-b1e2-4ee3a00838c9>

###### Launching Local Fetch

> [!IMPORTANT]
> You must launch Local Fetch through UE and not by the executable file. The reason is that the local host port is different when you launch it through UE, so you must do so.

Go ahead and click on the JsonAsAsset logo (<img width="25" height="25" src=https://github.com/JsonAsAsset/JsonAsAsset/assets/73559984/b90ab71f-d9ac-4349-96eb-620aadf7812f>) and hover over the list `"Command-line Application"` and press `"Execute Local Fetch (.EXE)"`.

A window should pop-up, and once the console says `[CORE] Running API`, Local Fetch has been successfully started!

</details>

-----------------------

<a name="common-errors"></a>
#### Common Errors 🐛

<details>
  <summary>Attempted to create a package with name containing double slashes. PackageName: ...</summary>
  
------------
  
Please set your Export Directory to your actual "Output/Exports" directory

***INCORRECT***: T:/FModel/Output

***CORRECT***: T:/FModel/Output/Exports
</details>

<details>
  <summary>Assertion failed: TextureReferenceIndex != INDEX_NONE</summary>

------------
  
This is a known issue in our code that we haven't fully resolved yet. While previous attempts to fix it have been unsuccessful, here’s a partial solution to reduce its occurrence:

- Re-launch your Unreal Engine project, go to JsonAsAsset's plugin settings and enable ***"Expose Pins"*** or ***"Skip Result Node Connection"***. Also enable ***"Allow Package Saving"***.
</details>

<details>
  <summary>Local Fetch closes instantly when executing</summary>

------------

There may be a few reasons why the application automatically closes, but mostly the issue stems from something missing:

### 1. Your settings are wrong, and need modifications
> If you don't have anything in the settings, how is it gonna execute correctly? Make sure all the information about your game is set in the settings.

### 2. ASP.NET 8.0 not installed
> Running the API requires ASP.NET 8.0 to be installed, please install this [here](https://dotnet.microsoft.com/en-us/download/dotnet/thank-you/runtime-aspnetcore-8.0.2-windows-x64-installer).

### 3. oo2core missing
> Run the API manually in the file explorer once, this should download the file to use in the api. (Plugins/JsonAsAsset/Binaries/Win64/LocalFetch)
</details>

--------------------

<a name="contribute"></a>
## ✨ Contributors

Thanks go to these wonderful people:

<!-- prettier-ignore-start -->
<!-- markdownlint-disable -->
<table>
  <tbody>
    <tr>
      <td align="center" valign="top" width="25.28%"><a href="https://github.com/Tectors"><img src="https://github.com/Tectors.png" width="100px;" alt="Tector"/><br /><sub><b>Tector</b></sub></a><br/>Developer</a></td>
      <td align="center" valign="top" width="14.28%"><a href="https://github.com/GMatrixGames"><img src="https://github.com/GMatrixGames.png" width="100px;" alt="GMatrixGames"/><br /><sub><b>GMatrixGames</b></sub></a><br/>Developer</a></td>
      <td align="center" valign="top" width="14.28%"><a href="https://github.com/zyloxmods"><img src="https://github.com/zyloxmods.png" width="80px;" alt="Zylox"/><br /><sub><b>Zylox</b></sub></a><br/>Maintainer</a></td>
  </tbody>
</table>

<table>
  <tbody>
    <tr>
      <td align="center" valign="top" width="6.28%" height="50px">
        <a href="https://github.com/NathanFelipeRH">
          <br/><b>Nathan Felipe</b>
        </a>
        <br/><span>Help with Sound Code</span>
      </td>
  </tbody>
</table>

- Thanks to the people who contributed to [UEAssetToolkit](https://github.com/Buckminsterfullerene02/UEAssetToolkit-Fixes)!
- Logo uses a font by [Brylark](https://ko-fi.com/brylark), support him at his ko-fi!

#### [Would you like to contribute?](https://github.com/JsonAsAsset/JsonAsAsset/blob/main/Source/README.md#key-information-for-contributors-)

<a name="licensing"></a>
#### Licensing 📜

JsonAsAsset is licensed under the MIT License, read more in the [LICENSE](https://github.com/JsonAsAsset/JsonAsAsset/blob/main/LICENSE) file. The plugin also uses [Detex](https://github.com/hglm/detex) and [NVIDIA Texture Tools](https://docs.nvidia.com/texture-tools/index.html).
