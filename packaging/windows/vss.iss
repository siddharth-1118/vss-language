; VSS Programming Language Inno Setup Script
; Generates a production-quality Windows Installer (VSS-{VERSION}-Setup.exe)
;
; Compile locally:
;   iscc packaging\windows\vss.iss
;
; Compile via CI (inject source dir and version):
;   iscc /DSOURCEDIR="D:\a\vss-language\vss-language" /DVERSION="1.2.0" packaging\windows\vss.iss

; ── Preprocessor defaults (overridable via /D on the command line) ────────────
#ifndef SOURCEDIR
  #define SOURCEDIR "E:\vss-language"
#endif

#ifndef VERSION
  #define VERSION "1.0.0"
#endif

[Setup]
AppId={{D37BF9E2-441F-433E-A528-76D4C3A3F1F1}
AppName=VSS Programming Language
AppVersion={#VERSION}
AppPublisher=Siddharth
DefaultDirName={commonpf}\VSS
DefaultGroupName=VSS Programming Language
DisableProgramGroupPage=yes
LicenseFile={#SOURCEDIR}\LICENSE
InfoBeforeFile={#SOURCEDIR}\README.md
OutputDir={#SOURCEDIR}
OutputBaseFilename=VSS-{#VERSION}-Setup
Compression=lzma
SolidCompression=yes
WizardStyle=modern
ChangesEnvironment=yes

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "envpath"; Description: "Add VSS to the system PATH (recommended)"; Flags: unchecked

[Files]
Source: "{#SOURCEDIR}\vss\vss.exe";        DestDir: "{app}";           Flags: ignoreversion
Source: "{#SOURCEDIR}\vss\stdlib\*";       DestDir: "{app}\stdlib";    Flags: recursesubdirs createallsubdirs skipifsourcedoesntexist
Source: "{#SOURCEDIR}\vss\packages\*";     DestDir: "{app}\packages";  Flags: recursesubdirs createallsubdirs skipifsourcedoesntexist
Source: "{#SOURCEDIR}\vss\examples\*";     DestDir: "{app}\examples";  Flags: recursesubdirs createallsubdirs skipifsourcedoesntexist
Source: "{#SOURCEDIR}\LICENSE";            DestDir: "{app}";           Flags: ignoreversion skipifsourcedoesntexist
Source: "{#SOURCEDIR}\README.md";          DestDir: "{app}";           Flags: ignoreversion skipifsourcedoesntexist
Source: "{#SOURCEDIR}\CHANGELOG.md";       DestDir: "{app}";           Flags: ignoreversion skipifsourcedoesntexist

[Icons]
Name: "{group}\VSS Command Line"; Filename: "cmd.exe"; Parameters: "/K cd /d ""{app}"""
Name: "{group}\VSS Readme";       Filename: "{app}\README.md"
Name: "{group}\Uninstall VSS";    Filename: "{uninstallexe}"

[Code]
const
  EnvironmentKey = 'SYSTEM\CurrentControlSet\Control\Session Manager\Environment';

function GetSystemPath(): string;
var
  CurrentPath: string;
begin
  if RegQueryStringValue(HKLM, EnvironmentKey, 'Path', CurrentPath) then
    Result := CurrentPath
  else
    Result := '';
end;

function IsInPath(Path, PathList: string): Boolean;
begin
  Result := Pos(';' + UpperCase(Path) + ';', ';' + UpperCase(PathList) + ';') > 0;
end;

procedure AddToPath();
var
  CurrentPath, NewPath, AppDir: string;
begin
  AppDir := ExpandConstant('{app}');
  CurrentPath := GetSystemPath();
  if not IsInPath(AppDir, CurrentPath) then
  begin
    if (CurrentPath <> '') and (CurrentPath[Length(CurrentPath)] <> ';') then
      NewPath := CurrentPath + ';' + AppDir
    else
      NewPath := CurrentPath + AppDir;
    if RegWriteStringValue(HKLM, EnvironmentKey, 'Path', NewPath) then
      Log('Added ' + AppDir + ' to system PATH.')
    else
      Log('Failed to add ' + AppDir + ' to system PATH.');
  end;
end;

procedure RemoveFromPath();
var
  CurrentPath, NewPath, AppDir: string;
begin
  AppDir := ExpandConstant('{app}');
  CurrentPath := GetSystemPath();
  if IsInPath(AppDir, CurrentPath) then
  begin
    NewPath := CurrentPath;
    StringChange(NewPath, AppDir + ';', '');
    StringChange(NewPath, ';' + AppDir, '');
    StringChange(NewPath, AppDir, '');
    StringChange(NewPath, ';;', ';');
    if RegWriteStringValue(HKLM, EnvironmentKey, 'Path', NewPath) then
      Log('Removed ' + AppDir + ' from system PATH.')
    else
      Log('Failed to remove ' + AppDir + ' from system PATH.');
  end;
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if (CurStep = ssPostInstall) and WizardIsTaskSelected('envpath') then
    AddToPath();
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if CurUninstallStep = usPostUninstall then
    RemoveFromPath();
end;
