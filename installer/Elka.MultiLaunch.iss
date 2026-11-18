#ifndef Configuration
#define Configuration "Release"
#endif
[Setup]
AppId={{6D57716D-BF2A-4F7A-91FD-EE2A31C1BD9C}
AppName=Elka Multi-Launch
AppVersion=0.2.0-dev.1
AppPublisher=ElkaSoft
DefaultDirName={localappdata}\Programs\ElkaSoft\MultiLaunch
DefaultGroupName=ElkaSoft
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir=..\artifacts
OutputBaseFilename=Elka.MultiLaunch-0.2.0-dev.1-setup-win-x64
SetupIconFile=..\assets\app.ico
UninstallDisplayIcon={app}\Elka.MultiLaunch.exe
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern dark
CloseApplications=yes
RestartApplications=no
[Files]
Source: "..\build\{#Configuration}\Elka.MultiLaunch.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\README.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\VALIDATION.md"; DestDir: "{app}"; Flags: ignoreversion
[Icons]
Name: "{autoprograms}\ElkaSoft\Elka Multi-Launch"; Filename: "{app}\Elka.MultiLaunch.exe"
[Tasks]
Name: "admin"; Description: "Run Multi-Launch as administrator"; GroupDescription: "How should Multi-Launch run?"; Flags: unchecked
Name: "startup"; Description: "Start with Windows (uses Task Scheduler when administrator mode is selected)"; GroupDescription: "Windows startup:"; Flags: unchecked
[Run]
Filename: "{app}\Elka.MultiLaunch.exe"; Description: "Open Elka Multi-Launch"; Flags: nowait postinstall skipifsilent
[Code]
procedure CurStepChanged(CurStep: TSetupStep);
var
  Params: String;
  Code: Integer;
  Started: Boolean;
begin
  if CurStep = ssPostInstall then begin
    Params := '--configure-install';
    if WizardIsTaskSelected('admin') then Params := Params + ' --admin';
    if WizardIsTaskSelected('startup') then Params := Params + ' --startup';
    if WizardIsTaskSelected('admin') and WizardIsTaskSelected('startup') then
      Started := ShellExec('runas', ExpandConstant('{app}\Elka.MultiLaunch.exe'), Params, '', SW_HIDE, ewWaitUntilTerminated, Code)
    else
      Started := Exec(ExpandConstant('{app}\Elka.MultiLaunch.exe'), Params, '', SW_HIDE, ewWaitUntilTerminated, Code);
    if (not Started) or (Code <> 0) then
      MsgBox('The app was installed, but its startup options were not fully applied. Open Settings in Multi-Launch to finish configuring them.', mbInformation, MB_OK);
  end;
end;

function RemoveOwnStartup(): Boolean;
var
  Service, Folder, Tasks, Task: Variant;
  I, Code, ActionCount: Integer;
  Command, AppExe, TaskPath, TaskName, ActionPath: String;
begin
  Result := True;
  AppExe := ExpandConstant('{app}\Elka.MultiLaunch.exe');
  if RegQueryStringValue(HKCU, 'Software\Microsoft\Windows\CurrentVersion\Run', 'Elka Multi-Launch', Command) then
    if CompareText(Command, '"' + AppExe + '" --tray') = 0 then
      RegDeleteValue(HKCU, 'Software\Microsoft\Windows\CurrentVersion\Run', 'Elka Multi-Launch');
  try
    Service := CreateOleObject('Schedule.Service');
    Service.Connect();
    try
      Folder := Service.GetFolder('\ElkaSoft');
    except
      exit;
    end;
    Tasks := Folder.GetTasks(0);
    for I := Tasks.Count downto 1 do begin
      Task := Tasks.Item(I);
      TaskName := Task.Name;
      ActionCount := Task.Definition.Actions.Count;
      if (Pos('MultiLaunch-', TaskName) = 1) and (ActionCount = 1) then begin
        ActionPath := Task.Definition.Actions.Item(1).Path;
        if CompareText(ActionPath, AppExe) = 0 then begin
          TaskPath := Task.Path;
          try
            Folder.DeleteTask(Task.Name, 0);
          except
            if not ShellExec('runas', ExpandConstant('{sys}\schtasks.exe'), '/Delete /TN "' + TaskPath + '" /F', '', SW_HIDE, ewWaitUntilTerminated, Code) then
              Result := False
            else if Code <> 0 then Result := False;
          end;
        end;
      end;
    end;
  except
    Result := False;
  end;
  if not Result then MsgBox('Windows startup could not be removed. Turn off Start with Windows in Multi-Launch Settings as administrator, save, and try uninstalling again. Your app and profiles have been kept.', mbInformation, MB_OK);
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if CurUninstallStep = usUninstall then
    if not RemoveOwnStartup() then Abort;
end;
