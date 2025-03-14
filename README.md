# IMFTool
A tool for editing IMF CPLs and creating new versions of an existing IMF package

### Pre-compiled versions
The latest binary installers for macOS and Windows are available at
[Releases](../../releases/)

## What is IMF Tool
IMF Tool supports browsing and limited editing of IMF [[1]](#imf-intro) packages (IMPs).
In particular, it supports the addition and deletion of audio tracks and subtitle tracks (versioning).
Introductory videos are available on YouTube [[2]](#imf-video), [[3]](#imf-video2).

## Workflows and Features:
-	Open an IMP, visualize the timeline of the CPL(s) included in the IMP
-	Create a new IMP, based on importing existing image track files
-	Supports App#2E JPEG2000 and App#5 ACES
-	Experimental supoprt of HTJ2K in App 2E and App4 DCDM 
-	Track file support includes IAB (SMPTE ST 2067-201), ISXD (SMPTE ST 2067-202), MGA S-ADM (SMPTE ST 2067-203) and ADM Audio (Draft SMPTE ST 2067-204)
-	Add Sidecar Assets to an IMP ("Add Asset" --> Add Sidecar Assets)
-	Create, view and edit Sidecar Composition Maps (SCMs)
-	Add a Photon QC report as sidecar file (hint: export the sidecar QC report as a Partial IMP, this leaves the Original IMP unmodified!)
-	Load ancestor Original Versions of Supplemental IMPs for preview and versioning
-	Create Photon[[4]](#photon) QC report
-	Edit CPL metadata
-	Add new virtual tracks (limited to audio, subtitles and markers)
-	Import and wrap audio and timed text resources
-	Delete virtual tracks
-	Add new segments
-	Set edit points and markers
-	Export as Full or Partial IMP
-	App#5 ACES support includes ACES preview and Target Frame preview and export
-	Decoding and rendering of IMSC1 text profile subtitles in a separate window. (TTML tab, limited IMSC feature set)
-	Overlay of IMSC1 image profile subtitles
-	Ingest of IMF 1.0 (PKL ST 429-8 and CPL ST 2067-3:2013) and IMF 1.1 (PKL ST 2067-2:2016 and CPL ST 2067-3:2016)
-	Outgest will be IMF 2020 only
-	Editing of the ContentVersionList element
-	Edit Marker Annotations

## CREDITS
The development of this tool has kindly been sponsored by Netflix Inc.
The App#5 extensions were supported by the Academy of Motion Picture Arts and Sciences, Sony Pictures, Warner Bros., Universal Studios and 20th Century Fox

## Binary installers
Please visit
[Releases](../../releases/)
to download the latest binary installers.

**For being notified about new releases, please "Watch" IMF Tool (on the top of this page).**

## Building
IMF Tool is a cross-platform application and can be compiled for macOS (x86_64 and armv8), Windows 10/11 (x86_64) and Linux (x86_64).
The build system is based on CMake in conjunction with [conan package manager](https://conan.io/).
> [!IMPORTANT]
> It is not possible to build IMF Tool without conan package manager at the moment.
 
#### Dependencies
- Qt 6.8 ([conan recipe](https://github.com/Privatehive/conan-Qt))
- QtAppBase 1.X ([conan recipe](https://github.com/Privatehive/QtAppBase))
- For IAB, ProRes, S-ADM and ADM support, a patched version of asdcplib is required ([conan recipe](https://github.com/IMFTool/asdcplib))
- Xerces-C 3.2 ([conan recipe](https://conan.io/center/recipes/xerces-c?version=3.2.5))
- OpenJPEG 2.5 ([conan recipe](https://conan.io/center/recipes/openjpeg?version=2.5.2))
- regxmllibc ([conan recipe](https://github.com/Privatehive/conan-regxmllib))
- The [build option](#build-options) `app5Support` requires OpenEXR ([conan recipe](https://conan.io/center/recipes/openexr?version=3.3.1)), IMath ([conan recipe](https://conan.io/center/recipes/imath?version=3.1.9))

### Build process

#### Install a modern c++ compiler with c++17 support:

* macOS: Install latest XCode
* Windows: 
  * MinGW: will be automatically downloaded during the build process if conan host profile [windowsMinGW.profile](./hostProfiles/windowsMinGW.profile) is used. No need to install MinGW manually.
  * MSVC (not recommended - use MinGW): Install Microsoft Visual Studio 2022.
* Linux: Install gcc or clang (untested) and binutils via the package manager of your favorite Linux distribution.

#### Install Conan package manager:

The preferred way to install [Conan package manager](https://conan.io/) is to install [python3](https://www.python.org/) first. Then install Conan via pip/pip3:

```bash
pip install conan
```

> [!NOTE]
> python is also required during the Qt build process

#### Create a conan host profile

Conan can automatically detect the installed compiler and create a so called [host profile](https://docs.conan.io/2/reference/config_files/profiles.html). This host profile is called `default` and contains os and compiler specific settings:

> [!NOTE]
> A warning message will show up if no compiler could be detected. For Windows this is fine if we use the provided MinGW host profile later during the build process.

```bash
conan profile detect
```

> [!NOTE] If you use MSVC make sure that the host profile contains `compiler.cppstd=17`. You can find the file path of the host profile by running `conan profile path default`.

*Recommendation:* You can add CMake as a tool dependency to your host profile. This will automatically download CMake during the build process and you will not have to install CMake yourself:

```bash
printf "[tool_requires]\ncmake/3.23.5" >> $(conan profile path default)
```

#### Add additional Conan remote

The above-mentioned dependencies on the IMF tool must also be built before IMF Tool can be built. Conan needs for each dependency a so called recipe (which is just a file called `conanfile.py`) describing the build steps that have to be performed.
In general those recipes are downloaded from a remote ([Conan Center](https://conan.io/center) by default). Not all recipes we need are provided by Conan Center - the recipes for Qt, QtBaseApp, asdcplib and regxmllib are provided by this [remote](https://conan.privatehive.de/ui/repos/tree/General/public-conan).

Add the additional remote:

```bash
conan remote add privatehive https://conan.privatehive.de/artifactory/api/conan/public-conan
```

> [!TIP]
> You can also do without remotes completely. All you have to do is change to the directory in which a conanfile.py is located and run `conan export ./`. This is particularly helpful in the recipe development process because you are indipendent of a remote server. All required Conan recipes are linked above in the [dependencies section](#dependencies).

#### Build IMF Tool

Clone this repository:

```bash
git clone https://github.com/IMFTool/IMFTool.git
cd IMFTool
```

Now start the Conan build process:

For macOS, Linux, Windows MSVC run:

```bash
conan create ./ -pr:h=default --build missing
```

For Windows MinGW build run:

```bash
conan create ./ -pr:h=hostProfiles/windowsMinGW.profile --build missing
```

The build process takes some time to finish... After a successful build process Conan prints the package folder where you find the IMF Tool binaries:

```bash
[...]
imf-tool/1.9.8@com.imftool/snapshot: Package '485b2690249c03b50cb65f306a1d71791523ae73' created
imf-tool/1.9.8@com.imftool/snapshot: Full package reference: imf-tool/1.9.8@com.imftool/snapshot#6c1e180614bc2d492d21c7828b30a700:485b2690249c03b50cb65f306a1d71791523ae73#68de6cde00be3e4a3ac30e2f254156b4
imf-tool/1.9.8@com.imftool/snapshot: Package folder /mnt/conan/p/b/imf-t41b35db28849d/p
                                                    ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
                                                    Here you find the IMF Tool binaries
```

In the package folder you will find (depending on your build host):

* macOS: dmg and app binary
* Linux: AppImage binary
* Windows: installer binary

> [!TIP]
> There is also a Conan command to copy the binaries from the package folder to the current working directory:
> `conan install --requires="imf-tool/1.9.8@com.imftool/snapshot" --deployer-package="*"`

### Build Options

When building IMF Tool, the following build options can be provided to the `conan create` or `conan install` commands (prefixed with an `-o` ):
* `app5Support=True/False`: Include App#5 ACES support (`True` by default)
* `bundleJVM=True/False`: Bundle IMF Tool with a Java VM (`True` by default). The advantage is that the user does not have to install a JVM himself. The disadvantage is that the IMF Tool is about 50 MiB larger.

### Code signing

If desired, the IMF Tool binaries can be signed. To do so create a new conan profile named `sign_env` in your profiles directory with the following content:

**Windows**

```ini
[buildenv]
WIN_CODESIGN_OPTIONS=...
```

> `WIN_CODESIGN_OPTIONS`: Options passed to [signtool](https://learn.microsoft.com/de-de/dotnet/framework/tools/signtool-exe#sign-command-options). Each option must be separated by a `;` e.g.: `/n;MyCodeSigningCert;/fd;SHA256;/t;http://timestamp.digicert.com`

Then provide the `sign_env` profile as an additional host profile to the conan command besides the MinGW host profile (replace MinGW host profile with the `default` profile if MinGW is not used):

```bash
conan create ./ -pr:h=hostProfiles/windowsMinGW.profile -pr:h=sign_env --build missing
```

**macOS**

```ini
[buildenv]
APPLE_CODESIGN_IDENTITY=...
```

> `APPLE_CODESIGN_IDENTITY`: The identity used to sign the app: It's the 10 character long alpha numeric string of the Apple Developer cert found in the Keychain Access

Then provide the `sign_env` profile as an additional host profile to the conan command:

```bash
conan create ./ -pr:h=default -pr:h=sign_env --build missing
```

## DEV Setup

Make sure you have gone through the steps in the [build process](#build-process) and have successfully built IMF Tool at least once.

To be able to debug IMF Tool an IDE is required, notably: VSCode (with installed C/C++ Extension Pack), CLion, Xcode, Visual Studio.

First build all dependencies (without optimizations) and create a build folder. You can name the build folder as you wish (name given after the `-of` argument):

```bash
conan install ./ -pr:h=default -of build-folder -s build_type=Debug --build missing
```

### VSCode

> [!NOTE]
> There seem to be some macOS-related bugs that may be fixed in a VSCode C/C++ extension update in January. The preferred way for macOS is to use Xcode.

Install the [C/C++ Extension Pack](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools-extension-pack) first, then open the IMF Tool root folder in VSCode (File → Open Folder).
Switch to the CMake tab on the left.

Click on the ✎ icon in the Configure section and select the 'conan-debug' config.
Then start debugger by clicking the ▶ icon to the right of the 'Debug' section.

### Xcode

Xcode lacks native support for CMake preset files. We need to generate an Xcode project file:

```bash
conan install ./ -pr:h=default -of build-folder -s build_type=Debug -o xcode=True --build missing
source build-folder/conanbuild.sh
cmake --preset conan-default
```

In the build folder you should find an IMF-Tool.xcodeproj that can be opened in Xcode.

### CLion

In the CMake settings dialog (Build, Execution, Deployment → CMake) disable the default "Debug" preset (uncheck the checkbox "Enable profile"). Then select the "conan-debug" preset and enable it (check the checkbox "Enable profile"). Then click OK.

### Visual Studio

Not yet tested.

## DISCLAIMER
  THERE IS NO WARRANTY FOR THE PROGRAM, TO THE EXTENT PERMITTED BY
APPLICABLE LAW.  EXCEPT WHEN OTHERWISE STATED IN WRITING THE COPYRIGHT
HOLDERS AND/OR OTHER PARTIES PROVIDE THE PROGRAM "AS IS" WITHOUT WARRANTY
OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING, BUT NOT LIMITED TO,
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
PURPOSE.  THE ENTIRE RISK AS TO THE QUALITY AND PERFORMANCE OF THE PROGRAM
IS WITH YOU.  SHOULD THE PROGRAM PROVE DEFECTIVE, YOU ASSUME THE COST OF
ALL NECESSARY SERVICING, REPAIR OR CORRECTION.

<a name="imf-intro"></a>[1] IMF: Interoperable Master Format. For an introduction see here:
http://techblog.netflix.com/2016/03/imf-prescription-for-versionitis.html

<a name="imf-video"></a>[2] IMF-Tool introductory video: https://www.youtube.com/watch?v=Zi3p8oElPM8

<a name="imf-video2"></a>[3] IMF-Tool introductory video Part 2: https://www.youtube.com/watch?v=k6OIx7WfF8s

<a name="photon"></a>[4] Photon IMF QC tool https://github.com/Netflix/photon
