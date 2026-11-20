; Inno Setup script for the Windows installer.
; Build:  ISCC /DAppVersion=0.1.0 installer\windows\TSS-1.iss
; (paths are relative to this file; expects a Release build in ..\..\build)

#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif

#define Artefacts "..\..\build\TSS1_artefacts\Release"

[Setup]
AppId={{5E1D2C8A-7B3F-4A69-9C1E-7A55C0115001}
AppName=TSS-1
AppVersion={#AppVersion}
AppVerName=TSS-1 {#AppVersion}
AppPublisher=Transistor Labs
DefaultDirName={autopf}\Transistor Labs\TSS-1
DefaultGroupName=Transistor Labs
DisableDirPage=yes
DisableProgramGroupPage=yes
OutputDir=..\..\dist
OutputBaseFilename=TSS-1-{#AppVersion}-Windows-Setup
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayName=TSS-1

[Types]
Name: "full";   Description: "VST3 plug-in and standalone app"
Name: "custom"; Description: "Choose components"; Flags: iscustom

[Components]
Name: "vst3"; Description: "VST3 plug-in (Ableton Live, Bitwig, Reaper, Cubase...)"; Types: full custom
Name: "app";  Description: "Standalone app";                                         Types: full custom

[Files]
Source: "{#Artefacts}\VST3\TSS-1.vst3\*"; DestDir: "{commoncf64}\VST3\TSS-1.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#Artefacts}\Standalone\TSS-1.exe"; DestDir: "{app}"; Components: app; Flags: ignoreversion

[Dirs]
; the speech engine writes its voice data here on first use (an ASCII path every user can write to)
Name: "{commonappdata}\Transistor Labs\TSS-1"; Permissions: users-modify

[UninstallDelete]
Type: filesandordirs; Name: "{commonappdata}\Transistor Labs\TSS-1"

[Icons]
Name: "{autoprograms}\TSS-1"; Filename: "{app}\TSS-1.exe"; Components: app

[Messages]
FinishedLabel=TSS-1 is installed.%n%nRescan plug-ins in your DAW (Ableton Live: Settings > Plug-Ins > Rescan).
