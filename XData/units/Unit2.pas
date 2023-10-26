unit Unit2;

interface

uses
  Winapi.Windows,
  Winapi.Messages,
  WinAPi.ShellAPI,
  psAPI,

  System.SysUtils,
  System.Variants,
  System.Types,
  System.Classes,
  System.IOUtils,
  System.DateUtils,
  System.JSON,
  System.StrUtils,
  System.Generics.Collections,

  Vcl.Graphics,
  Vcl.Controls,
  Vcl.Forms,
  Vcl.Dialogs,
  Vcl.StdCtrls,
  Vcl.ExtCtrls,

  Data.DB,

  IdStack,
  IdGlobal,
  IdGlobalProtocols,
  IdBaseComponent,
  IdComponent,
  IdTCPConnection,
  IdTCPClient,
  IdHTTP,
  IdMessageClient,
  IdMessage,
  IdMessageBuilder,
  IdAttachment,
  IdMessageParts,
  IdEMailAddress,
  IdAttachmentFile,
  IdSMTPBase,
  IdSMTP,
  IdAttachmentMemory,

  FireDAC.Stan.Intf,
  FireDAC.Stan.Option,
  FireDAC.Stan.Error,
  FireDAC.UI.Intf,
  FireDAC.Phys.Intf,
  FireDAC.Stan.Def,
  FireDAC.Stan.Pool,
  FireDAC.Stan.Async,
  FireDAC.Phys,
  FireDAC.VCLUI.Wait,
  FireDAC.Stan.ExprFuncs,
  FireDAC.Phys.SQLiteDef,
  FireDAC.Stan.Param,
  FireDAC.DatS,
  FireDAC.DApt.Intf,
  FireDAC.DApt,
  FireDAC.Comp.DataSet,
  FireDAC.Comp.Client,
  FireDAC.Phys.SQLite,

  Unit1, IdSSLOpenSSL, IdIOHandler, IdIOHandlerSocket, IdIOHandlerStack,
  IdSSL, System.Net.URLClient, System.Net.HttpClient,
  System.Net.HttpClientComponent;

type
  TMainForm = class(TForm)
    mmInfo: TMemo;
    btStart: TButton;
    btStop: TButton;
    btSwagger: TButton;
    DBConn: TFDConnection;
    FDPhysSQLiteDriverLink1: TFDPhysSQLiteDriverLink;
    Query1: TFDQuery;
    tmrStart: TTimer;
    tmrInit: TTimer;
    NetHTTPClient1: TNetHTTPClient;
    btRedoc: TButton;

    procedure btStartClick(ASender: TObject);
    procedure btStopClick(ASender: TObject);
    procedure FormCreate(ASender: TObject);
    function GetAppName: String;
    function GetAppRelease: TDateTime;
    function GetAppReleaseUTC: TDateTime;
    function GetAppVersion: String;
    procedure GetAppParameters(List: TStringList);
    function GetAppFileName: String;
    function GetAppFileSize: Int64;
    function GetAppTimeZone: String;
    function GetAppTimeZoneOffset: Integer;
    procedure GetIPAddresses(List: TStringList);
    function GetMemoryUsage: NativeUInt;
    procedure btSwaggerClick(Sender: TObject);
    procedure tmrStartTimer(Sender: TObject);
    procedure FormShow(Sender: TObject);
    procedure tmrInitTimer(Sender: TObject);
    procedure NetHTTPClient1ValidateServerCertificate(
      const Sender: TObject; const ARequest: TURLRequest;
      const Certificate: TCertificate; var Accepted: Boolean);
    procedure NetHTTPClient1RequestError(const Sender: TObject;
      const AError: string);
    procedure btRedocClick(Sender: TObject);
  public
    AppName: String;
    AppVersion: String;
    AppRelease: TDateTime;
    AppReleaseUTC: TDateTime;
    AppParameters: TStringList;
    AppFileSize: Int64;
    AppFileName: String;
    AppTimeZone: String;
    AppTimeZoneOffset: Integer;
    IPAddresses: TStringList;
    AppConfigFile: String;
    AppConfiguration: TJSONObject;
    ChatModels: TStringList;
    AppCacheFolder: String;

    AppIconsFolder: String;
    AppIcons: TJSONArray;
    AppIconSets: String;

    MailServerAvailable: Boolean;
    MailServerHost: String;
    MailServerPort: Integer;
    MailServerUser: String;
    MailServerPass: String;
    MailServerFrom: String;
    MailServerName: String;

    DatabaseName: String;
    DatabaseAlias: String;
    DatabaseEngine: String;
    DatabaseUsername: String;
    DatabasePassword: String;
    DatabaseConfig: String;

    SiteCheckSuccess: Boolean;
    SiteCheckName: String;
    SiteCheckURL: String;

  strict private
    procedure UpdateGUI;
  end;

var
  MainForm: TMainForm;

implementation

{$R *.dfm}

{ TMainForm }
uses
  Unit3;

procedure TMainForm.btRedocClick(Sender: TObject);
var
  url: String;
const
  cHttp = '://+';
  cHttpLocalhost = '://localhost';
begin
  url := StringReplace(
      ServerContainer.XDataServer.BaseUrl,
      cHttp, cHttpLocalhost, [rfIgnoreCase])+'/redoc';
  ShellExecute(0, 'open', PChar(url), nil, nil, SW_SHOWNORMAL);
end;


procedure TMainForm.btStartClick(ASender: TObject);
begin
  ServerContainer.SparkleHttpSysDispatcher.Start;
  UpdateGUI;
end;

procedure TMainForm.btStopClick(ASender: TObject);
begin
  ServerContainer.SparkleHttpSysDispatcher.Stop;
  UpdateGUI;
end;

procedure TMainForm.btSwaggerClick(Sender: TObject);
var
  url: String;
const
  cHttp = '://+';
  cHttpLocalhost = '://localhost';
begin
  url := StringReplace(
      ServerContainer.XDataServer.BaseUrl,
      cHttp, cHttpLocalhost, [rfIgnoreCase])+'/swaggerui';
  ShellExecute(0, 'open', PChar(url), nil, nil, SW_SHOWNORMAL);
end;

procedure TMainForm.FormCreate(ASender: TObject);
begin
  tmrInit.Enabled := True;
end;

procedure TMainForm.FormShow(Sender: TObject);
begin
  if MainForm.Tag = 0 then
  begin
    MainForm.Tag := 1;
    MainForm.WindowState := wsMaximized;
    MainForm.WindowState := wsMinimized;
  end;
end;

function TMainForm.GetAppFileName: String;
begin
  Result := ParamStr(0);
end;

function TMainForm.GetAppFileSize: Int64;
var
  SearchRec: TSearchRec;
begin
  Result := -1;
  if FindFirst(ParamStr(0), faAnyFile, SearchRec) = 0
  then Result := SearchRec.Size;
  FindClose(SearchRec);
end;

function TMainForm.GetAppName: String;
begin
  Result := MainForm.Caption;
end;

procedure TMainForm.GetAppParameters(List: TStringList);
var
  i: Integer;
begin
  i := 1;
  while i <= ParamCount do
  begin
    List.Add('"'+ParamStr(i)+'"');
    i := i + 1;
  end;
end;

function TMainForm.GetAppRelease: TDateTime;
begin
  Result := System.IOUtils.TFile.GetLastWriteTime(ParamStr(0));
end;

function TMainForm.GetAppReleaseUTC: TDateTime;
begin
  Result := System.IOUtils.TFile.GetLastWriteTimeUTC(ParamStr(0));
end;

function TMainForm.GetAppTimeZone: String;
var
  ZoneInfo: TTimeZoneInformation;
begin
  GetTimeZoneInformation(ZoneInfo);
  Result := ZoneInfo.StandardName;
end;

function TMainForm.GetAppTimeZoneOffset: Integer;
var
  ZoneInfo: TTimeZoneInformation;
begin
  GetTimeZoneInformation(ZoneInfo);
  Result := ZoneInfo.Bias;
end;

// https://stackoverflow.com/questions/1717844/how-to-determine-delphi-application-version
function TMainForm.GetAppVersion: String;
const
  c_StringInfo = 'StringFileInfo\040904E4\FileVersion';
var
  n, Len : cardinal;
  Buf, Value : PChar;
  exeName:String;
begin
  exeName := ParamStr(0);
  Result := '';
  n := GetFileVersionInfoSize(PChar(exeName),n);
  if n > 0 then begin
    Buf := AllocMem(n);
    try
      GetFileVersionInfo(PChar(exeName),0,n,Buf);
      if VerQueryValue(Buf,PChar(c_StringInfo),Pointer(Value),Len) then begin
        Result := Trim(Value);
      end;
    finally
      FreeMem(Buf,n);
    end;
  end;
end;

// https://stackoverflow.com/questions/576538/delphi-how-to-get-all-local-ips
procedure TMainForm.GetIPAddresses(List: TStringList);
var
  i: Integer;
  IPList: TIdStackLocalAddressList;
  IPAddr: TIdStackLocalAddress;
begin
  TIdStack.IncUsage;
  List.Clear;
  IPList := TIdStackLocalAddressList.Create;
  try
    GStack.GetLocalAddressList(IPList);
    for i := 0 to IPList.Count-1 do
    begin
      IPAddr := IPList[I];
      case IPAddr.IPVersion of
        Id_IPv4: begin
                   List.Add('IPV4: '+IPAddr.IPAddress);
                 end;
        Id_IPv6: begin
                   List.Add('IPV6: '+IPAddr.IPAddress);
                 end;
        end;
    end;
  finally
    IPList.Free;
    TIdStack.DecUsage;
  end;
  List.Sort;
  i := 0;
  while i < List.Count do
  begin
    List[i] := '"'+List[i]+'"';
    i := i +1;
  end;
end;

// https://stackoverflow.com/questions/437683/how-to-get-the-memory-used-by-a-delphi-program
function TMainForm.GetMemoryUsage: NativeUInt;
var
  MemCounters: TProcessMemoryCounters;
begin
  Result := 0;
  MemCounters.cb := SizeOf(MemCounters);
  if GetProcessMemoryInfo(GetCurrentProcess, @MemCounters, SizeOf(MemCounters))
  then Result := MemCounters.WorkingSetSize
  else mmInfo.Lines.add('ERROR: WorkingSetSize not available');
end;


procedure TMainForm.NetHTTPClient1RequestError(const Sender: TObject; const AError: string);
var
  SMTP1: TIdSMTP;
  Msg1: TIdMessage;
  Addr1: TIdEmailAddressItem;
  Html1: TIdMessageBuilderHtml;
  SMTPResult: WideString;
begin
  mmInfo.Lines.Add('   WARNING: Site Check failed: '+SiteCheckURL);
  mmInfo.Lines.Add('   WARNING: '+AError);
  SiteCheckSuccess := False;

  if not(MailServerAvailable) then
  begin
    mmInfo.Lines.Add('   WARNING: Site Check failure notification e-mail not sent (Mail services not configured)');
  end
  else
  begin

    // Send warning email
    Msg1  := nil;
    Addr1 := nil;
    SMTP1 := TIdSMTP.Create(nil);
    SMTP1.Host     := MainForm.MailServerHost;
    SMTP1.Port     := MainForm.MailServerPort;
    SMTP1.Username := MainForm.MailServerUser;
    SMTP1.Password := MainForm.MailServerPass;

    try
      Html1 := TIdMessageBuilderHtml.Create;
      try
        Html1.Html.Add('<html>');
        Html1.Html.Add('<head>');
        Html1.Html.Add('</head>');
        Html1.Html.Add('<body><pre>');
        Html1.Html.Add('Site Check: ');
        Html1.Html.Add('...Server: '+SiteCheckName);
        Html1.Html.Add('...URL: '+SiteCheckURL);
        Html1.Html.Add('...Error: '+AError);
        Html1.Html.Add('</pre></body>');
        Html1.Html.Add('</html>');
        Html1.HtmlCharSet := 'utf-8';

        Msg1 := Html1.NewMessage(nil);
        Msg1.Subject := 'Site Check failure notification: '+SiteCheckName;
        Msg1.From.Text := MainForm.MailServerFrom;
        Msg1.From.Name := MainForm.MailServerName;

        Addr1 := Msg1.Recipients.Add;
        Addr1.Address := MainForm.MailserverFrom;

        SMTP1.Connect;
        try
          try
            SMTP1.Send(Msg1);
          except on E: Exception do
            begin
              SMTPResult := SMTPResult+'[ '+E.ClassName+' ] '+E.Message+Chr(10);
            end;
          end;
        finally
          SMTP1.Disconnect();
        end;
      finally
        Addr1.Free;
        Msg1.Free;
        Html1.Free;
      end;
    except on E: Exception do
      begin
        SMTPResult := SMTPResult+'[ '+E.ClassName+' ] '+E.Message+Chr(10);
      end;
    end;
    SMTP1.Free;

    if SMTPResult = ''
    then mmInfo.Lines.Add('   WARNING: Site Check notification e-mail sent to '+MailServerName+' <'+MailServerFrom+'>')
    else
    begin
      mmInfo.Lines.Add('   WARNING: Site Check notification e-mail to '+MailServerName+' <'+MailServerFrom+'> FAILED.');
      mmInfo.Lines.Add('   WARNING: SMTP Error: '+SMTPResult);
    end;
  end;
end;

procedure TMainForm.NetHTTPClient1ValidateServerCertificate(
  const Sender: TObject; const ARequest: TURLRequest;
  const Certificate: TCertificate; var Accepted: Boolean);
var
  SMTP1: TIdSMTP;
  Msg1: TIdMessage;
  Addr1: TIdEmailAddressItem;
  Html1: TIdMessageBuilderHtml;
  SMTPResult: WideString;
const
  WarningThreshold = 21;

begin
  mmInfo.Lines.Add('   Server Certificate Check...');
  mminfo.Lines.Add('   ...Certificate Name: '+Certificate.CertName);
  mmInfo.Lines.Add('   ...Certificate Issuer: '+StringReplace(Certificate.Issuer, chr(13)+chr(10), ' / ', [rfReplaceAll]));
  mmInfo.Lines.Add('   ...Not Valid Before: '+FormatDateTime('yyyy-MM-dd hh:nn:ss', Certificate.Start)+' UTC');
  mmInfo.Lines.Add('   ...Not Valid After: '+FormatDateTime('yyyy-MM-dd hh:nn:ss', Certificate.Expiry)+' UTC');

  if (DaysBetween(Certificate.Expiry, Now) > WarningThreshold) then
  begin
    mmInfo.Lines.Add('   ...Days Remaining: '+IntToStr(DaysBetween(Certificate.Expiry, Now)));
  end
  else
  begin
    mmInfo.Lines.Add('   ...WARNING: Days Remaining: '+IntToStr(DaysBetween(Certificate.Expiry, Now)));
    if not(MailServerAvailable) then
    begin
      mmInfo.Lines.Add('   ...WARNING: SSL Certificate warning e-mail not sent (Mail services not configured)');
    end
    else
    begin

      // Send warning email
      Msg1  := nil;
      Addr1 := nil;
      SMTP1 := TIdSMTP.Create(nil);
      SMTP1.Host     := MainForm.MailServerHost;
      SMTP1.Port     := MainForm.MailServerPort;
      SMTP1.Username := MainForm.MailServerUser;
      SMTP1.Password := MainForm.MailServerPass;

      try
        Html1 := TIdMessageBuilderHtml.Create;
        try
          Html1.Html.Add('<html>');
          Html1.Html.Add('<head>');
          Html1.Html.Add('</head>');
          Html1.Html.Add('<body><pre>');
          Html1.Html.Add('Server Certificate Check:');
          Html1.Html.Add('...Certificate Name: '+Certificate.CertName);
          Html1.Html.Add('...Certificate Issuer: '+StringReplace(Certificate.Issuer, chr(13)+chr(10), ' / ', [rfReplaceAll]));
          Html1.Html.Add('...Not Valid Before: '+FormatDateTime('yyyy-MM-dd hh:nn:ss', Certificate.Start)+' UTC');
          Html1.Html.Add('...Not Valid After: '+FormatDateTime('yyyy-MM-dd hh:nn:ss', Certificate.Expiry)+' UTC');
          Html1.Html.Add('...Days Remaining: '+IntToStr(DaysBetween(Certificate.Expiry, Now))+' days');
          Html1.Html.Add('...Warning Threshold: '+IntToStr(WarningThreshold)+' days');
          Html1.Html.Add('</pre></body>');
          Html1.Html.Add('</html>');
          Html1.HtmlCharSet := 'utf-8';

          Msg1 := Html1.NewMessage(nil);
          Msg1.Subject := 'SSL Certificate Expiration Warning: '+Certificate.CertName+' expires in '+IntToStr(DaysBetween(Certificate.Expiry, now))+' days';
          Msg1.From.Text := MainForm.MailServerFrom;
          Msg1.From.Name := MainForm.MailServerName;

          Addr1 := Msg1.Recipients.Add;
          Addr1.Address := MainForm.MailserverFrom;

          SMTP1.Connect;
          try
            try
              SMTP1.Send(Msg1);
            except on E: Exception do
              begin
                SMTPResult := SMTPResult+'[ '+E.ClassName+' ] '+E.Message+Chr(10);
              end;
            end;
          finally
            SMTP1.Disconnect();
          end;
        finally
          Addr1.Free;
          Msg1.Free;
          Html1.Free;
        end;
      except on E: Exception do
        begin
          SMTPResult := SMTPResult+'[ '+E.ClassName+' ] '+E.Message+Chr(10);
        end;
      end;
      SMTP1.Free;

      if SMTPResult = ''
      then mmInfo.Lines.Add('   ...WARNING: SSL Certificate warning e-mail sent to '+MailServerName+' <'+MailServerFrom+'>')
      else
      begin
        mmInfo.Lines.Add('   ...WARNING: SSL Certificate warning e-mail to '+MailServerName+' <'+MailServerFrom+'> FAILED.');
        mmInfo.Lines.Add('   ...WARNING: SMTP Error: '+SMTPResult);
      end;
    end;
  end;
end;

procedure TMainForm.tmrInitTimer(Sender: TObject);
var
  i: Integer;
  ConfigFile: TStringList;
begin
  tmrInit.Enabled := False;

  // Let's use these internally for consistency
  FormatSettings.DateSeparator   := '-';
  FormatSettings.ShortDateFormat := 'yyyy-mm-dd';
  FormatSettings.TimeSeparator   := ':';
  FormatSettings.ShortTimeFormat := 'hh:nn:ss';

  // Get System Values
  AppName := GetAppName;
  AppVersion := GetAppVersion;
  AppRelease := GetAppRelease;
  AppReleaseUTC := GetAppReleaseUTC;
  AppFileName := GetAppFileName;
  AppFileSize := GetAppFileSize;
  AppTimeZone := GetAppTimeZone;
  AppTimeZoneOffset := GetAppTimeZoneOffset;

  // List of App Parameters
  AppParameters := TStringList.Create;
  AppParameters.QuoteChar := ' ';
  GetAppParameters(AppParameters);

  // List of IP Addresses
  IPAddresses := TStringList.Create;
  IPAddresses.QuoteChar := ' ';
  GetIPAddresses(IPAddresses);

  // Load JSON Configuration
  mmINfo.Lines.Add('Loading Configuration ...');
  AppConfigFile := StringReplace(ExtractFileName(ParamStr(0)),'exe','json',[]);
  i := 0;
  while i < AppParameters.Count do
  begin
    if Pos('"CONFIG=',UpperCase(AppParameters[i])) = 1
    then AppConfigFile  := Copy(AppParameters[i],9,length(AppParameters[i])-9);
    i := i + 1;
  end;
  ConfigFile := TStringList.Create;
  if FileExists(AppConfigFile) then
  begin
    try
      ConfigFile.LoadFromFile(AppConfigFile);
      mmInfo.Lines.Add('...Configuration File Loaded: '+AppConfigFile);
      AppConfiguration := TJSONObject.ParseJSONValue(ConfigFile.Text) as TJSONObject;
    except on E: Exception do
      begin
        mmInfo.Lines.Add('...Configuration File Error: '+AppConfigFile);
        mmInfo.Lines.Add('...['+E.ClassName+'] '+E.Message);
      end;
    end;
  end
  else // File doesn't exist
  begin
    mmInfo.Lines.Add('...Configuration File Not Found: '+AppConfigFile);
  end;
  ConfigFile.Free;
  Application.ProcessMessages;

  if Appconfiguration = nil then
  begin
    // Create an empty AppConfiguration
    mmInfo.Lines.Add('...Using Default Configuration');
    AppConfiguration := TJSONObject.Create;
    AppConfiguration.AddPair('BaseURL','http://+:44444/tms/xdata');
  end;

  if AppConfiguration.GetValue('BaseURL') <> nil
  then ServerContainer.XDataServer.BaseURL := (AppConfiguration.getValue('BaseURL') as TJSONString).Value
  else ServerContainer.XDataServer.BaseURL := 'http://+:44444/tms/xdata';
  mmInfo.Lines.Add('...Server BaseURL: '+ServerContainer.XDataServer.BaseURL);

  // Get Mail Configuration
  MailServerAvailable := False;
  if AppConfiguration.GetValue('Mail Services') <> nil then
  begin
    MailServerAvailable := True;
    MailServerHost := ((AppConfiguration.GetValue('Mail Services') as TJSONObject).GetValue('SMTP Host') as TJSONString).Value;
    MailServerPort := ((AppConfiguration.GetValue('Mail Services') as TJSONObject).GetValue('SMTP Port') as TJSONNumber).AsInt;
    MailServerUser := ((AppConfiguration.GetValue('Mail Services') as TJSONObject).GetValue('SMTP User') as TJSONString).Value;
    MailServerPass := ((AppConfiguration.GetValue('Mail Services') as TJSONObject).GetValue('SMTP Pass') as TJSONString).Value;
    MailServerFrom := ((AppConfiguration.GetValue('Mail Services') as TJSONObject).GetValue('SMTP From') as TJSONString).Value;
    MailServerName := ((AppConfiguration.GetValue('Mail Services') as TJSONObject).GetValue('SMTP Name') as TJSONString).Value;
    mmInfo.Lines.Add('...SMTP Mail Server: '+MailServerHost+' / '+IntToStr(MailServerPort));
  end
  else
  begin
    mmInfo.Lines.Add('...SMTP Mail Server: Unavailable');
  end;

  mmInfo.Lines.Add('Done.');
  mmInfo.Lines.Add('');
  Application.ProcessMessages;


  tmrStart.Enabled := True;

end;

procedure TMainForm.tmrStartTimer(Sender: TObject);
var
  i: Integer;
  ImageFile: TStringList;
  TableName: String;
  CacheFolderDirs: String;
  CacheFolderFiles: String;
  CacheFolderSize: Double;
  CacheFolderList: TStringDynArray;
  IconFiles: TStringDynArray;
  IconFile: TStringList;
  IconJSON: TJSONObject;
  IconSets: TJSONArray;
  IconWidth: Integer;
  IconHeight: Integer;
  IconCount: Integer;
  IconTotal: Integer;
  Response: IHTTPResponse;
  SMTP1: TIdSMTP;
  LogErrors: Integer;
begin

  tmrStart.Enabled := False;

  // This is (potentially) used when populating the photo table
  ImageFile := TStringList.Create;

  // FDConnection component dropped on form - DBConn
  // FDQuery component dropped on form - Query1
  //
  // FDPhysSQLiteDriverLink component droppoed on form
  // support for other databases should do the same
  //
  // DatabaseName is a Form Variable
  // DatabaseEngine is a Form Variable
  // DatabaseUsername is a Form Variable
  // DatabasePassword is a Form Variable

  mmInfo.Lines.Add('Initializing Database...');

  DatabaseEngine := 'sqlite';
  DatabaseName := 'DemoData.sqlite';
  DatabaseAlias := 'DemoData';
  DatabaseUsername := 'dbuser';
  DatabasePassword := 'dbpass';
  DatabaseConfig := '';

  i := 1;
  while i <= ParamCount do
  begin
    if Pos('DBNAME=',Uppercase(ParamStr(i))) = 1
    then DatabaseName := Copy(ParamStr(i),8,length(ParamStr(i)));

    if Pos('DBALIAS=',Uppercase(ParamStr(i))) = 1
    then DatabaseAlias := Copy(ParamStr(i),8,length(ParamStr(i)));

    if Pos('DBENGINE=',Uppercase(ParamStr(i))) = 1
    then DatabaseEngine := Lowercase(Copy(ParamStr(i),10,length(ParamStr(i))));

    if Pos('DBUSER=',Uppercase(ParamStr(i))) = 1
    then DatabaseUsername := Copy(ParamStr(i),8,length(ParamStr(i)));

    if Pos('DBPASS=',Uppercase(ParamStr(i))) = 1
    then DatabasePassword := Copy(ParamStr(i),8,length(ParamStr(i)));

    if Pos('DBCONFIG=',Uppercase(ParamStr(i))) = 1
    then DatabaseConfig := Copy(ParamStr(i),8,length(ParamStr(i)));

    i := i + 1;
  end;

  FDManager.Open;
  DBConn.Params.Clear;

  if (DatabaseEngine = 'sqlite') then
  begin
    // This creates the database if it doesn't already exist
    DBConn.Params.DriverID := 'SQLite';
    DBConn.Params.Database := DatabaseName;
    DBConn.Params.Add('DateTimeFormat=String');
    DBConn.Params.Add('Synchronous=Full');
    DBConn.Params.Add('LockingMode=Normal');
    DBConn.Params.Add('SharedCache=False');
    DBConn.Params.Add('UpdateOptions.LockWait=True');
    DBConn.Params.Add('BusyTimeout=10000');
    DBConn.Params.Add('SQLiteAdvanced=page_size=4096');
    // Extras
    with DBConn.FormatOptions do
    begin
      OwnMapRules := True;
      StrsEmpty2Null := True;
      with MapRules.Add do begin
        SourceDataType := dtWideMemo;
        TargetDataType := dtWideString;
      end;
    end;
  end;

  DBConn.Open;
  Query1.Connection := DBConn;
  mmInfo.Lines.Add('...['+DatabaseEngine+'] '+DatabaseName);

  Application.ProcessMessages;

  // Create and populate tables
  {$Include ddl\person\person.inc}
  Application.ProcessMessages;
  {$Include ddl\role\role.inc}
  Application.ProcessMessages;
  {$Include ddl\person_role\person_role.inc}
  Application.ProcessMessages;
  {$Include ddl\api_key\api_key.inc}
  Application.ProcessMessages;
  {$Include ddl\contact\contact.inc}
  Application.ProcessMessages;
  {$Include ddl\endpoint_history\endpoint_history.inc}
  Application.ProcessMessages;
  {$Include ddl\ip_allow\ip_allow.inc}
  Application.ProcessMessages;
  {$Include ddl\ip_block\ip_block.inc}
  Application.ProcessMessages;
  {$Include ddl\list\list.inc}
  Application.ProcessMessages;
  {$Include ddl\login_fail\login_fail.inc}
  Application.ProcessMessages;
  {$Include ddl\login_history\login_history.inc}
  Application.ProcessMessages;
  {$Include ddl\token\token.inc}
  Application.ProcessMessages;
  {$Include ddl\photo\photo.inc}
  Application.ProcessMessages;
  {$Include ddl\action_history\action_history.inc}
  Application.ProcessMessages;
  {$Include ddl\chatai_history\chatai_history.inc}
  Application.ProcessMessages;
  {$Include ddl\imageai_history\imageai_history.inc}
  Application.ProcessMessages;

  // Tables added for Blaugment project
  {$Include ddl\authcode\authcode.inc}
  Application.ProcessMessages;
  {$Include ddl\period\period.inc}
  Application.ProcessMessages;


  mmInfo.Lines.Add('Done.');
  mmInfo.Lines.Add('');

  // Cache Folder
  Application.ProcessMessages;
  if (AppConfiguration.GetValue('Cache Folder') <> nil)
  then AppCacheFolder := (AppConfiguration.GetValue('Cache Folder') as TJSONString).Value
  else AppCacheFolder := GetCurrentDir+'/cache';
  if RightStr(AppCacheFolder,1) <> '/'
  then AppCacheFolder := AppCacheFolder + '/';

  if not(ForceDirectories(AppCacheFolder))
  then mmInfo.Lines.Add('ERROR Initializing Cache Folder: '+AppCacheFolder);
  if not(ForceDirectories(AppCacheFolder+'images'))
  then mmInfo.Lines.Add('ERROR Initializing Cache Folder: '+AppCacheFolder+'images');
  if not(ForceDirectories(AppCacheFolder+'images/ai'))
  then mmInfo.Lines.Add('ERROR Initializing Cache Folder: '+AppCacheFolder+'images/ai');
  if not(ForceDirectories(AppCacheFolder+'images/people'))
  then mmInfo.Lines.Add('ERROR Initializing Cache Folder: '+AppCacheFolder+'images/people');

  CacheFolderDirs  := FloatToStrF(Length(TDirectory.GetDirectories(AppCacheFolder,'*',TsearchOption.soAllDirectories)),ffNumber,8,0);
  CacheFolderList := TDirectory.GetFiles(AppCacheFolder,'*.*',TsearchOption.soAllDirectories);
  CacheFolderFiles := FloatToStrF(Length(CacheFolderList),ffNumber,8,0);
  CacheFolderSize := 0;
  for i := 0 to Length(CacheFolderList)-1 do
    CacheFolderSize := CacheFolderSize + (FileSizeByName(CacheFolderList[i]) / 1024 / 1024);

  // Display System Values
  Application.ProcessMessages;
  mmInfo.Lines.Add('Configuring Server...');
  mmInfo.Lines.Add('...App Name: '+AppName);
  mmInfo.Lines.Add('...Version: '+AppVersion);
  mmInfo.Lines.Add('...Release: '+FormatDateTime('yyyy-mmm-dd (ddd) hh:nn:ss', AppRelease));
  mmInfo.Lines.Add('...Release UTC: '+FormatDateTime('yyyy-mm-dd hh:nn:ss.zzz', AppReleaseUTC));
  mmInfo.Lines.Add('...Server Time: '+FormatDateTime('yyyy-mmm-dd (ddd) hh:nn:ss', Now));
  mmInfo.Lines.Add('...TimeZone: '+AppTimeZone);
  mmInfo.Lines.Add('...TimeZone Offset: '+IntToStr(AppTimeZoneOffset)+'m');
  mmInfo.Lines.Add('...Base URL: '+ServerContainer.XDataServer.BaseURL);
  mmInfo.Lines.Add('...File Name: '+AppFileName);
  mmInfo.Lines.Add('...File Size: '+Format('%.1n',[AppFileSize / 1024 / 1024])+' MB');
  mmInfo.Lines.Add('...Cache Folder: '+AppCacheFolder);
  mmInfo.Lines.Add('...Cache Statistics: '+CacheFolderDirs+' Folders, '+CacheFolderFiles+' Files, '+FloatToStrF(CacheFolderSize,ffNumber,8,1)+' MB');
  mmInfo.Lines.Add('...Memory Usage: '+Format('%.1n',[GetMemoryUsage / 1024 / 1024])+' MB');

  mmInfo.Lines.Add('...Parameters:');
  Application.ProcessMessages;
  i := 0;
  while i < AppParameters.Count do
  begin
    mmInfo.Lines.Add('        '+StringReplace(AppParameters[i],'"','',[rfReplaceAll]));
    i := i + 1;
  end;

  mmInfo.Lines.Add('...IP Addresses:');
  Application.ProcessMessages;
  i := 0;
  while i < IPAddresses.Count do
  begin
    mmInfo.Lines.Add('        '+StringReplace(IPAddresses[i],'"','',[rfReplaceAll]));
    i := i + 1;
  end;


  // Are chat services avialable?
  Application.ProcessMessages;
  if (AppConfiguration.GetValue('Chat Interface') as TJSONArray) = nil
  then mmInfo.Lines.Add('...Chat: UNAVAILABLE')
  else
  begin
    mmInfo.Lines.Add('...Chat:');
    i := 0;
    while i < (AppConfiguration.GetValue('Chat Interface') as TJSONArray).Count do
    begin;
      mmInfo.Lines.Add('        '+(((AppConfiguration.GetValue('Chat Interface') as TJSONArray).items[i] as TJSONObject).getValue('Name') as TJSONString).Value);
      i := i + 1;
    end;
  end;


  // Load up Icon Sets
  Application.ProcessMessages;
  if (AppConfiguration.GetValue('Icons') <> nil)
  then AppIconsFolder := (AppConfiguration.GetValue('Icons') as TJSONString).Value
  else AppIconsFolder := GetCurrentDir+'/icon-sets';
  if RightStr(AppIconsFolder,1) <> '/'
  then AppIconsFolder := AppIconsFolder + '/';
  ForceDirectories(AppIconsFolder);
  IconFiles := TDirectory.GetFiles(AppIconsFolder,'*.json',TsearchOption.soAllDirectories);

  AppIcons := TJSONArray.Create;
  IconSets := TJSONArray.Create;
  IconTotal := 0;

  if length(IconFiles) = 0 then
  begin
    mmInfo.Lines.Add('...No Icon Sets Loaded: None Found.');
  end
  else
  begin
    mmInfo.Lines.Add('...Loading '+IntToStr(Length(IconFiles))+' Icon Sets:');
    IconFile := TStringList.Create;

    for i := 0 to Length(IconFiles)-1 do
    begin
      // Load JSON File
      Application.ProcessMessages;
      IconFile.LoadFromFile(IconFiles[i], TEncoding.UTF8);
      IconJSON := TJSONObject.ParseJSONValue(IconFile.Text) as TJSONObject;
      AppIcons.Add(IconJSON);

      // Get Icon Count information
      IconCount := (IconJSON.GetValue('icons') as TJSONObject).Count;
      IconTotal := IconTotal + IconCount;

      // Log what we're doing
      mmInfo.Lines.Add('        ['+TPath.GetFileName(IconFiles[i])+'] '+
        ((IconJSON.GetValue('info') as TJSONObject).GetValue('name') as TJSONString).Value+' - '+
        IntToStr(IconCount)+' Icons');

      // Sort out the default width and height.  This is either from the width and height properties
      // found in the root of the JSON object, or in the info element, or perhaps not at all in the
      // the case of the width property, in which case we'll assume it is the same as the height.
      // We're doing this now as we're not passing back this information to the client, just the
      // name, license, and icons, so the client will need this to properly generate the SVG data.
      IconHeight := 0;
      IconWidth := 0;
      if IconJSON.GetValue('height') <> nil
      then IconHeight := (IconJSON.GetValue('height') as TJSONNumber).AsInt
      else if (IconJSON.GetValue('info') as TJSONObject).GetValue('height') <> nil
           then IconHeight := ((IconJSON.GetValue('info') as TJSONObject).GetValue('height') as TJSONNumber).AsInt;
      if IconJSON.GetValue('width') <> nil
      then IconWidth := (IconJSON.GetValue('width') as TJSONNumber).AsInt
      else if (IconJSON.GetValue('info') as TJSONObject).GetValue('width') <> nil
           then IconWidth := ((IconJSON.GetValue('info') as TJSONObject).GetValue('width') as TJSONNumber).AsInt;
      if IconWidth = 0 then IconWidth := IconHeight;

      // Here we're building the JSON that we'll pass to the client telling them what icon sets are
      // available, along with the other data they will need that is at the icon-set level
      IconSets.add(TJSONObject.ParseJSONValue('{'+
        '"name":"'+((IconJSON.GetValue('info') as TJSONObject).GetValue('name') as TJSONString).Value+'",'+
        '"license":"'+(((IconJSON.GetValue('info') as TJSONObject).GetValue('license') as TJSONObject).GetValue('title') as TJSONString).Value+'",'+
        '"width":'+IntToStr(IconWidth)+','+
        '"height":'+IntToStr(IconHeight)+','+
        '"count":'+IntToStr(IconCount)+','+
        '"library":'+IntToStr(i)+
        '}') as TJSONObject);
    end;
    IconFile.Free;
  end;
  mmInfo.Lines.Add('        Icons Loaded: '+FloatToStrF(IconTotal,ffNumber,10,0));

  // We don't need to do anything else with this, so we'll store it as a string and
  // then return just that when asked for this ata.
  AppIconSets := IconSets.ToString;

  // This is added at the end to be sure to capture the icon memory usage, which can be rather significant.
  mmInfo.Lines.Add('...Memory Usage: '+Format('%.1n',[GetMemoryUsage / 1024 / 1024])+' MB');
  mmInfo.Lines.Add('Done.');


  // If any of the above have somehow failed, we won't get to here, so a bit of a sanity check.
  mmInfo.Lines.Add('');
  mmInfo.Lines.Add('Initialization complete: Starting Server.');
  mmInfo.Lines.Add('');

  // Start Server
  ServerContainer.SparkleHttpSysDispatcher.Active := True;
  UpdateGUI;
  Application.ProcessMessages;

   // Check expiration date of SSL certificate for wherever this server is running.
  // Be sure to include any directly related systems, like mail servers for example.
  // Perform Site Checks based on sites listed in the configuration JSON
  if (AppConfiguration.GetValue('Site Checks') = nil) then
  begin
    mmInfo.Lines.Add('');
    mmInfo.Lines.Add('Site Checks: No site checks configured.');
    mmInfo.Lines.Add('');
  end
  else
  begin
    mmInfo.Lines.Add('');
    for i := 0 to (AppConfiguration.GetValue('Site Checks') as TJSONArray).Count - 1 do
    begin
      Application.ProcessMessages;
      SiteCheckSuccess := True;
      sitecheckName := (((AppConfiguration.GetValue('Site Checks') as TJSONArray).Items[I] as TJSONObject).GetValue('Server') as TJSONString).Value;
      SiteCheckURL := (((AppConfiguration.GetValue('Site Checks') as TJSONArray).Items[I] as TJSONObject).GetValue('URL') as TJSONString).Value;

      mmInfo.Lines.Add(SiteCheckName+': '+SiteCheckURL);
      if Pos('http', SiteCheckURL) = 1 then
      begin
        // Regular HTTP(S) Check
        Response := NetHTTPClient1.Get(SiteCheckURL) as THTTPResponse;
      end
      else
      begin
        // SMTP Check
        SiteCheckSuccess := False;
        SMTP1 := TIdSMTP.Create(nil);
        SMTP1.Host := Copy(SiteCheckURL,1,Pos(':',SiteCheckURL)-1);
        SMTP1.Port := StrToInt(Trim(Copy(SiteCheckURL,Pos(':',SiteCheckURL)+1,5)));
        SMTP1.ConnectTimeout := 10000;
        try
          SMTP1.Connect;
        except on E:Exception do
          begin
            NetHTTPClient1RequestError(nil, '['+E.ClassName+'] '+E.Message);
          end;
        end;
        if SMTP1.Connected then
        begin
          SMTP1.Disconnect;
          SiteCheckSuccess := True;
          mmInfo.Lines.Add('   SMTP Connection Successful.');
          Response := NetHTTPClient1.Get('https://'+SMTP1.Host) as THTTPResponse;
        end
        else
        begin
          mmInfo.Lines.Add('   SMTP Connection Failed.');
        end;
        SMTP1.Free;
      end;

      if (SiteCheckSuccess = False) then
      begin
        mmInfo.Lines.Add('   Server Offline.');
      end
      else
      begin
        mmInfo.Lines.Add('   Server Online.');
      end;
      mmInfo.Lines.Add('Done.');
      mmInfo.Lines.Add(' ');
    end;
  end;


  // Store a copy of the log in the database
  Application.ProcessMessages;
  mmInfo.Lines.Add('Logging startup.');
  LogErrors := DBSupport.Occurrences(' fail',LowerCase(mmInfo.Lines.Text));
  try
    {$Include sql\system\action_history_insert\action_history_insert.inc}
    Query1.ParamByName('PERSONID').AsInteger := 0;
    Query1.ParamByName('IPADDRESS').AsString := 'localhost';
    Query1.ParamByName('APPLICATION').AsString := AppName;
    Query1.ParamByName('VERSION').AsString := AppVersion;
    Query1.ParamByName('SESSIONID').AsString := 'server';
    Query1.ParamByName('SESSIONSTART').AsDateTime := Now;
    Query1.ParamByName('SESSIONRECORDED').AsDateTime := TTimeZone.local.ToUniversalTime(Now);
    Query1.ParamByName('LOGSTATUS').AsInteger := 0;
    Query1.ParamByName('LOGERRORS').AsInteger := LogErrors;
    Query1.ParamByName('LOGEVENTS').AsInteger := 0;
    Query1.ParamByName('LOGSTART').AsInteger := 0;
    Query1.ParamByName('LOGCHANGES').AsInteger := 0;
    Query1.ParamByName('ACTIONS').AsString := mmInfo.Lines.Text;
    Query1.ExecSQL;
  except on E: Exception do
    begin
      DBSupport.DisconnectQuery(DBConn, Query1);
      mmInfo.Lines.Add('['+E.Classname+'] '+E.Message);
    end;
  end;
  mmInfo.Lines.Add('Startup complete.');
  mmInfo.Lines.Add('');

  // Cleanup
  ImageFile.Free;
  try
    DBSupport.DisconnectQuery(DBConn, Query1);
  except on E: Exception do
    begin
      mmInfo.Lines.Add('['+E.Classname+'] '+E.Message);
    end;
  end;
end;

procedure TMainForm.UpdateGUI;
const
  cHttp = '://+';
  cHttpLocalhost = '://localhost';
begin
  btStart.Enabled := not ServerContainer.SparkleHttpSysDispatcher.Active;
  btStop.Enabled := not btStart.Enabled;
  if ServerContainer.SparkleHttpSysDispatcher.Active then
  begin
    mmInfo.Lines.Add('XData Server started at '+StringReplace( ServerContainer.XDataServer.BaseUrl, cHttp, cHttpLocalhost, [rfIgnoreCase]));
    mmInfo.Lines.Add('SwaggerUI started at '+StringReplace( ServerContainer.XDataServer.BaseUrl, cHttp, cHttpLocalhost, [rfIgnoreCase])+'/swaggerui');
  end
  else
  begin
    mmInfo.Lines.Add('XData Server stopped');
  end;
end;

end.
