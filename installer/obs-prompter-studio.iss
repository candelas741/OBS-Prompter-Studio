#define MyAppName "Prompter Studio"
#define MyAppVersion "1.1.0"
#define MyAppPublisher "Psicocartoon Studio"
#define MyAppURL "https://github.com/candelas741/OBS-Prompter-Studio"

[Setup]
AppId={{9A77C4C1-64D9-49A1-8C5A-8F16C0B3B8A1}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={autopf}\obs-studio
DisableProgramGroupPage=yes
UsePreviousAppDir=no
AppendDefaultDirName=no
OutputDir=..\dist
OutputBaseFilename=Prompter-Studio-Setup
Compression=lzma
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
WizardStyle=modern
LicenseFile=..\LICENSE
InfoBeforeFile=..\dist\README_INSTALACION.txt
; Si agrega un icono al proyecto, descomente y ajuste esta linea:
; SetupIconFile=..\resources\icons\obs-prompter-studio.ico

[Languages]
Name: "spanish"; MessagesFile: "compiler:Languages\Spanish.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
Source: "..\dist\Prompter-Studio\obs-plugins\64bit\obs-prompter-studio.dll"; DestDir: "{app}\obs-plugins\64bit"; Flags: ignoreversion
Source: "..\dist\Prompter-Studio\data\obs-plugins\obs-prompter-studio\*"; DestDir: "{app}\data\obs-plugins\obs-prompter-studio"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\dist\Prompter-Studio\LICENSE.txt"; DestDir: "{app}\data\obs-plugins\obs-prompter-studio"; Flags: ignoreversion
Source: "..\dist\Prompter-Studio\NOTICE.txt"; DestDir: "{app}\data\obs-plugins\obs-prompter-studio"; Flags: ignoreversion
Source: "..\dist\Prompter-Studio\CHANGELOG.txt"; DestDir: "{app}\data\obs-plugins\obs-prompter-studio"; Flags: ignoreversion
Source: "..\dist\README_INSTALACION.txt"; DestDir: "{app}\data\obs-plugins\obs-prompter-studio"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\Prompter Studio - README"; Filename: "{app}\data\obs-plugins\obs-prompter-studio\README_INSTALACION.txt"

[Code]
function InitializeSetup(): Boolean;
begin
  Result := True;
end;

function NextButtonClick(CurPageID: Integer): Boolean;
var
  ObsExe: string;
begin
  Result := True;
  if CurPageID = wpSelectDir then begin
    ObsExe := AddBackslash(WizardDirValue) + 'bin\64bit\obs64.exe';
    if not FileExists(ObsExe) then begin
      MsgBox('La carpeta seleccionada no parece ser una instalacion valida de OBS Studio.' #13#10#13#10 +
             'Seleccione la carpeta donde esta instalado OBS, por ejemplo:' #13#10 +
             'C:\Program Files\obs-studio\', mbError, MB_OK);
      Result := False;
    end;
  end;
end;
