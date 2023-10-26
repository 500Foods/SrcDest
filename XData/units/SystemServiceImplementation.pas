unit SystemServiceImplementation;

interface

uses
  System.Classes,
  System.SysUtils,
  System.JSON,
  System.DateUtils,
  System.StrUtils,

  System.Generics.Collections,

  System.Net.URLClient,
  System.Net.HttpClientComponent,
  System.Net.HttpClient,

  Sparkle.HttpSys.Server,
  Sparkle.Security,

  XData.Server.Module,
  XData.Service.Common,
  XData.Sys.Exceptions,

  Bcl.Jose.Core.JWT,
  Bcl.Jose.Core.Builder,

  IdStack,
  IdBaseComponent,
  IdComponent,
  IdTCPConnection,
  IdTCPClient,
  IdExplicitTLSClientServerBase,
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
  FireDAC.Stan.Param,
  FireDAC.Comp.Client,
  FireDAC.Comp.BatchMove,
  FireDAC.Comp.BatchMove.Dataset,
  FireDAC.Comp.BatchMove.JSON,

  SystemService;

type
  [ServiceImplementation]
  TSystemService = class(TInterfacedObject, ISystemService)
  private

    function Info(TZ: String):TStream;
    function Login(Login_ID: String; Password: String; API_Key: String; TZ: String; IPAddress: String; IPLocation: String; DeviceInfo: String; BrowserInfo: String; ActionSession: String; ActionLog: String):TStream;
    function Logout(ActionSession: String; ActionLog: String):TStream;
    function LogoutAll(ActionSession: String; ActionLog: String):TStream;
    function Renew(ActionSession: String; ActionLog: String):TStream;

    function ChangePassword(OldPassword: String; NewPassword: String; PasswordTest: String):String;
    function ChangeAccount(AccountName: String):String;
    function CheckUniqueAccount(UniqueAccount: String):Boolean;
    function CheckUniqueEMail(UniqueEMail: String):Boolean;

    function SendEMail(MailSubject: String; MailBody: String; Reason: String):String;
    function SendConfirmationCode(Reason, EMailAddress, EMailSubject, EMailBody, SessionCode, APIKey: String):String;
    function VerifyConfirmationCode(EMailAddress, SessionCode, ConfirmationCode, APIKey, Reason: String):String;

    function AvailableIconSets:TStream;
    function SearchIconSets(SearchTerms: String; SearchSets:String; Results:Integer):TStream;
    function SearchFontAwesome(Query: String):TStream;

  end;

implementation

uses Unit1, Unit2, Unit3, TZDB;

const
//  JWT_PERIOD = 2;  // How long a JWT is valid for, in minutes
  JWT_PERIOD = 15;  // How long a JWT is valid for, in minutes

function TSystemService.AvailableIconSets: TStream;
begin
  // Returning JSON, so flag it as such
  TXDataOperationContext.Current.Response.Headers.SetValue('content-type', 'application/json');

  Result := TStringStream.Create(MainForm.AppIconSets);
end;

function TSystemService.ChangeAccount(AccountName: String): String;
var
  DBConn: TFDConnection;
  Query1: TFDQuery;
  DatabaseName: String;
  DatabaseEngine: String;
  ElapsedTime: TDateTime;
  User: IUserIdentity;
  JWT: String;
  PersonID: Integer;
begin
  // Returning JSON, so flag it as such
  TXDataOperationContext.Current.Response.Headers.SetValue('content-type', 'application/json');

  // Time this event
  ElapsedTime := Now;

  // Get data from the JWT
  User := TXDataOperationContext.Current.Request.User;
  JWT := TXDataOperationContext.Current.Request.Headers.Get('Authorization');
  if (User = nil) then raise EXDataHttpUnauthorized.Create('Missing authentication');

  // Setup DB connection and query
  try
    DatabaseName := User.Claims.Find('dbn').AsString;
    DatabaseEngine := User.Claims.Find('dbe').AsString;
    DBSupport.ConnectQuery(DBConn, Query1, DatabaseName, DatabaseEngine);
  except on E: Exception do
    begin
      MainForm.mmInfo.Lines.Add('['+E.Classname+'] '+E.Message);
      raise EXDataHttpUnauthorized.Create('Internal Error: CQ');
    end;
  end;

  // Check if we've got a valid JWT (one that has not been revoked)
  try
    {$Include sql\system\token_check\token_check.inc}
    Query1.ParamByName('TOKENHASH').AsString := DBSupport.HashThis(JWT);
    Query1.ParamByName('IPADDRESS').AsString := TXDataOperationContext.Current.Request.RemoteIP;
    Query1.Open;
  except on E: Exception do
    begin
      MainForm.mmInfo.Lines.Add('['+E.Classname+'] '+E.Message);
      raise EXDataHttpUnauthorized.Create('Internal Error: TC');
    end;
  end;
  if Query1.RecordCount <> 1 then
  begin
    DBSupport.DisconnectQuery(DBConn, Query1);
    raise EXDataHttpUnauthorized.Create('JWT was not validated');
  end;


  // All good, let's update the account name
  PersonID := User.Claims.Find('usr').AsInteger;
  try
    {$Include sql\system\change_account\change_account.inc}
    Query1.ParamByName('PERSONID').AsInteger := PersonID;
    Query1.ParamByName('ACCOUNTNAME').AsString := AccountName;
    Query1.ExecSQL;
  except on E: Exception do
    begin
      DBSupport.DisconnectQuery(DBConn, Query1);
      MainForm.mmInfo.Lines.Add('['+E.Classname+'] '+E.Message);
      raise EXDataHttpUnauthorized.Create('Internal Error: CA');
    end;
  end;

  // All done.
  Result := 'Success';

  // Keep track of endpoint history
  try
    {$Include sql\system\endpoint_history_insert\endpoint_history_insert.inc}
    Query1.ParamByName('PERSONID').AsInteger := User.Claims.Find('usr').AsInteger;
    Query1.ParamByName('ENDPOINT').AsString := 'SystemService.ChangeAccount';
    Query1.ParamByName('ACCESSED').AsDateTime := TTimeZone.local.ToUniversalTime(ElapsedTime);
    Query1.ParamByName('IPADDRESS').AsString := TXDataOperationContext.Current.Request.RemoteIP;
    Query1.ParamByName('APPLICATION').AsString := User.Claims.Find('app').AsString;
    Query1.ParamByName('VERSION').AsString := MainForm.AppVersion;
    Query1.ParamByName('DATABASENAME').AsString := DatabaseName;
    Query1.ParamByName('DATABASEENGINE').AsString := DatabaseEngine;
    Query1.ParamByName('EXECUTIONMS').AsInteger := MillisecondsBetween(Now,ElapsedTime);
    Query1.ParamByName('DETAILS').AsString := '['+IntToStr(PersonID)+': '+AccountName+']';
    Query1.ExecSQL;
  except on E: Exception do
    begin
      MainForm.mmInfo.Lines.Add('['+E.Classname+'] '+E.Message);
      raise EXDataHttpUnauthorized.Create('Internal Error: EHI');
    end;
  end;

  // All Done
  try
    DBSupport.DisconnectQuery(DBConn, Query1);
  except on E: Exception do
    begin
      MainForm.mmInfo.Lines.Add('['+E.Classname+'] '+E.Message);
      raise EXDataHttpUnauthorized.Create('Internal Error: DQ');
    end;
  end;
end;

function TSystemService.ChangePassword(OldPassword, NewPassword, PasswordTest: String): String;
var
  DBConn: TFDConnection;
  Query1: TFDQuery;
  DatabaseName: String;
  DatabaseEngine: String;
  ElapsedTime: TDateTime;
  User: IUserIdentity;
  JWT: String;
  PersonID: Integer;
  PasswordHash: String;
begin
  // Returning JSON, so flag it as such
  TXDataOperationContext.Current.Response.Headers.SetValue('content-type', 'application/json');

  // Time this event
  ElapsedTime := Now;

  // See if request passes our test
  if PasswordTest <>  DBSupport.HashThis(OldPassword+'-TEST-'+NewPassword)
  then raise EXDataHttpUnauthorized.Create('Authentication Error: Test Failed');


  // Get data from the JWT
  User := TXDataOperationContext.Current.Request.User;
  JWT := TXDataOperationContext.Current.Request.Headers.Get('Authorization');
  if (User = nil) then raise EXDataHttpUnauthorized.Create('Missing authentication');

  // Setup DB connection and query
  try
    DatabaseName := User.Claims.Find('dbn').AsString;
    DatabaseEngine := User.Claims.Find('dbe').AsString;
    DBSupport.ConnectQuery(DBConn, Query1, DatabaseName, DatabaseEngine);
  except on E: Exception do
    begin
      MainForm.mmInfo.Lines.Add('['+E.Classname+'] '+E.Message);
      raise EXDataHttpUnauthorized.Create('Internal Error: CQ');
    end;
  end;

  // Check if we've got a valid JWT (one that has not been revoked)
  try
    {$Include sql\system\token_check\token_check.inc}
    Query1.ParamByName('TOKENHASH').AsString := DBSupport.HashThis(JWT);
    Query1.ParamByName('IPADDRESS').AsString := TXDataOperationContext.Current.Request.RemoteIP;
    Query1.Open;
  except on E: Exception do
    begin
      MainForm.mmInfo.Lines.Add('['+E.Classname+'] '+E.Message);
      raise EXDataHttpUnauthorized.Create('Internal Error: TC');
    end;
  end;
  if Query1.RecordCount <> 1 then
  begin
    DBSupport.DisconnectQuery(DBConn, Query1);
    raise EXDataHttpUnauthorized.Create('JWT was not validated');
  end;


  // Finally, let's check the current password matches
  PersonID := User.Claims.Find('usr').AsInteger;
  PasswordHash := DBSupport.HashThis('XData-Password:'+Trim(OldPassword));
  try
    {$Include sql\system\person_password_check\person_password_check.inc}
    Query1.ParamByName('PERSONID').AsInteger := PersonID;
    Query1.ParamByName('PASSWORDHASH').AsString := PasswordHash;
    Query1.Open;
  except on E: Exception do
    begin
      DBSupport.DisconnectQuery(DBConn, Query1);
      MainForm.mmInfo.Lines.Add('['+E.Classname+'] '+E.Message);
      raise EXDataHttpUnauthorized.Create('Internal Error: PPC');
    end;
  end;
  if Query1.RecordCount <> 1 then
  begin
    DBSupport.DisconnectQuery(DBConn, Query1);
    raise EXDataHttpUnauthorized.Create('Authentication error: Current Password does not match Server pasword');
  end;

  // All good, so let's update the password
  PasswordHash := DBSupport.HashThis('XData-Password:'+Trim(NewPassword));
  try
    {$Include sql\system\change_password\change_password.inc}
    Query1.ParamByName('PERSONID').AsInteger := PersonID;
    Query1.ParamByName('PASSWORDHASH').AsString := PasswordHash;
    Query1.ExecSQL;
  except on E: Exception do
    begin
      DBSupport.DisconnectQuery(DBConn, Query1);
      MainForm.mmInfo.Lines.Add('['+E.Classname+'] '+E.Message);
      raise EXDataHttpUnauthorized.Create('Internal Error: CP');
    end;
  end;
  if Query1.RowsAffected <> 1 then
  begin
    DBSupport.DisconnectQuery(DBConn, Query1);
    raise EXDataHttpUnauthorized.Create('Internal Error: CP');
  end;

  // All done.
  Result := 'Success';

  // Keep track of endpoint history
  try
    {$Include sql\system\endpoint_history_insert\endpoint_history_insert.inc}
    Query1.ParamByName('PERSONID').AsInteger := User.Claims.Find('usr').AsInteger;
    Query1.ParamByName('ENDPOINT').AsString := 'SystemService.ChangePassword';
    Query1.ParamByName('ACCESSED').AsDateTime := TTimeZone.local.ToUniversalTime(ElapsedTime);
    Query1.ParamByName('IPADDRESS').AsString := TXDataOperationContext.Current.Request.RemoteIP;
    Query1.ParamByName('APPLICATION').AsString := User.Claims.Find('app').AsString;
    Query1.ParamByName('VERSION').AsString := MainForm.AppVersion;
    Query1.ParamByName('DATABASENAME').AsString := DatabaseName;
    Query1.ParamByName('DATABASEENGINE').AsString := DatabaseEngine;
    Query1.ParamByName('EXECUTIONMS').AsInteger := MillisecondsBetween(Now,ElapsedTime);
    Query1.ParamByName('DETAILS').AsString := '['+IntToStr(PersonID)+': '+Result+']';
    Query1.ExecSQL;
  except on E: Exception do
    begin
      MainForm.mmInfo.Lines.Add('['+E.Classname+'] '+E.Message);
      raise EXDataHttpUnauthorized.Create('Internal Error: EHI');
    end;
  end;

  // All Done
  try
    DBSupport.DisconnectQuery(DBConn, Query1);
  except on E: Exception do
    begin
      MainForm.mmInfo.Lines.Add('['+E.Classname+'] '+E.Message);
      raise EXDataHttpUnauthorized.Create('Internal Error: DQ');
    end;
  end;
end;

function TSystemService.CheckUniqueAccount(UniqueAccount: String): Boolean;
var
  DBConn: TFDConnection;
  Query1: TFDQuery;
  DatabaseName: String;
  DatabaseEngine: String;
  ElapsedTime: TDateTime;
begin
  // Time this event
  ElapsedTime := Now;

  // Returning JSON, so flag it as such
  TXDataOperationContext.Current.Response.Headers.SetValue('content-type', 'application/json');

  // Setup DB connection and query
  try
    DatabaseName := MainForm.DatabaseName;
    DatabaseEngine := MainForm.DatabaseEngine;
    DBSupport.ConnectQuery(DBConn, Query1, DatabaseName, DatabaseEngine);
  except on E: Exception do
    begin
      MainForm.mmInfo.Lines.Add('['+E.Classname+'] '+E.Message);
      raise EXDataHttpUnauthorized.Create('Internal Error: CQ');
    end;
  end;

  // All good, so let's see if it is unique
    try
    {$Include sql\system\unique_account\unique_account.inc}
    Query1.ParamByName('UNIQUEACCOUNT').ASString := Uppercase(Trim(Copy(UniqueAccount,1,50)));
    Query1.Open;
  except on E: Exception do
    begin
      DBSupport.DisconnectQuery(DBConn, Query1);
      MainForm.mmInfo.Lines.Add('['+E.Classname+'] '+E.Message);
      raise EXDataHttpUnauthorized.Create('Internal Error: UA');
    end;
  end;
  if Query1.RowsAffected <> 1 then
  begin
    DBSupport.DisconnectQuery(DBConn, Query1);
    raise EXDataHttpUnauthorized.Create('Internal Error: UA');
  end;

  // All done.
  Result := (Query1.FieldByName('matches').AsInteger = 0);

  // Keep track of endpoint history
  try
    {$Include sql\system\endpoint_history_insert\endpoint_history_insert.inc}
    Query1.ParamByName('PERSONID').AsInteger := 1;
    Query1.ParamByName('ENDPOINT').AsString := 'SystemService.CheckUniqueAccount';
    Query1.ParamByName('ACCESSED').AsDateTime := TTimeZone.local.ToUniversalTime(ElapsedTime);
    Query1.ParamByName('IPADDRESS').AsString := TXDataOperationContext.Current.Request.RemoteIP;
    Query1.ParamByName('APPLICATION').AsString := MainForm.AppName;
    Query1.ParamByName('VERSION').AsString := MainForm.AppVersion;
    Query1.ParamByName('DATABASENAME').AsString := DatabaseName;
    Query1.ParamByName('DATABASEENGINE').AsString := DatabaseEngine;
    Query1.ParamByName('EXECUTIONMS').AsInteger := MillisecondsBetween(Now,ElapsedTime);
    Query1.ParamByName('DETAILS').AsString := '['+UniqueAccount+': '+Result.ToString(True)+']';
    Query1.ExecSQL;
  except on E: Exception do
    begin
      MainForm.mmInfo.Lines.Add('['+E.Classname+'] '+E.Message);
      raise EXDataHttpUnauthorized.Create('Internal Error: EHI');
    end;
  end;

  // All Done
  try
    DBSupport.DisconnectQuery(DBConn, Query1);
  except on E: Exception do
    begin
      MainForm.mmInfo.Lines.Add('['+E.Classname+'] '+E.Message);
      raise EXDataHttpUnauthorized.Create('Internal Error: DQ');
    end;
  end;
end;

function TSystemService.CheckUniqueEMail(UniqueEMail: String): Boolean;
var
  DBConn: TFDConnection;
  Query1: TFDQuery;
  DatabaseName: String;
  DatabaseEngine: String;
  ElapsedTime: TDateTime;
begin
  // Time this event
  ElapsedTime := Now;

  // Returning JSON, so flag it as such
  TXDataOperationContext.Current.Response.Headers.SetValue('content-type', 'application/json');

  // Setup DB connection and query
  try
    DatabaseName := MainForm.DatabaseName;
    DatabaseEngine := MainForm.DatabaseEngine;
    DBSupport.ConnectQuery(DBConn, Query1, DatabaseName, DatabaseEngine);
  except on E: Exception do
    begin
      MainForm.mmInfo.Lines.Add('['+E.Classname+'] '+E.Message);
      raise EXDataHttpUnauthorized.Create('Internal Error: CQ');
    end;
  end;

  // All good, so let's see if it is unique
    try
    {$Include sql\system\unique_email\unique_email.inc}
    Query1.ParamByName('UNIQUEEMAIL').ASString := Uppercase(Trim(Copy(UniqueEMail,1,50)));
    Query1.Open;
  except on E: Exception do
    begin
      DBSupport.DisconnectQuery(DBConn, Query1);
      MainForm.mmInfo.Lines.Add('['+E.Classname+'] '+E.Message);
      raise EXDataHttpUnauthorized.Create('Internal Error: UE');
    end;
  end;
  if Query1.RowsAffected <> 1 then
  begin
    DBSupport.DisconnectQuery(DBConn, Query1);
    raise EXDataHttpUnauthorized.Create('Internal Error: UE');
  end;

  // All done.
  Result := (Query1.FieldByName('matches').AsInteger = 0);

  // Keep track of endpoint history
  try
    {$Include sql\system\endpoint_history_insert\endpoint_history_insert.inc}
//    Query1.ParamByName('PERSONID').AsInteger := User.Claims.Find('usr').AsInteger;
    Query1.ParamByName('PERSONID').AsInteger := 1;
    Query1.ParamByName('ENDPOINT').AsString := 'SystemService.CheckUniqueEMail';
    Query1.ParamByName('ACCESSED').AsDateTime := TTimeZone.local.ToUniversalTime(ElapsedTime);
    Query1.ParamByName('IPADDRESS').AsString := TXDataOperationContext.Current.Request.RemoteIP;
    Query1.ParamByName('APPLICATION').AsString := MainForm.AppName;
    Query1.ParamByName('VERSION').AsString := MainForm.AppVersion;
    Query1.ParamByName('DATABASENAME').AsString := DatabaseName;
    Query1.ParamByName('DATABASEENGINE').AsString := DatabaseEngine;
    Query1.ParamByName('EXECUTIONMS').AsInteger := MillisecondsBetween(Now,ElapsedTime);
    Query1.ParamByName('DETAILS').AsString := '['+UniqueEMail+': '+Result.ToString(True)+']';
    Query1.ExecSQL;
  except on E: Exception do
    begin
      MainForm.mmInfo.Lines.Add('['+E.Classname+'] '+E.Message);
      raise EXDataHttpUnauthorized.Create('Internal Error: EHI');
    end;
  end;

  // All Done
  try
    DBSupport.DisconnectQuery(DBConn, Query1);
  except on E: Exception do
    begin
      MainForm.mmInfo.Lines.Add('['+E.Classname+'] '+E.Message);
      raise EXDataHttpUnauthorized.Create('Internal Error: DQ');
    end;
  end;
end;

function TSystemService.Info(TZ: String):TStream;
var
  DBConn: TFDConnection;
  Query1: TFDQuery;
  DatabaseName: String;
  DatabaseEngine: String;

  ResultJSON: TJSONObject;
  ResultArray: TJSONArray;
  ServerIPArray: TJSONArray;
  ParametersArray: TJSONArray;
  ClientTimeZone: TBundledTimeZone;
  ValidTimeZone: Boolean;

  ElapsedTime: TDateTime;
begin
  // Time this event
  ElapsedTime := Now;

  // Returning JSON, so flag it as such
  TXDataOperationContext.Current.Response.Headers.SetValue('content-type', 'application/json');

  // Figure out if we have a valid TZ
  ClientTimeZone := TBundledTimeZone.GetTimeZone('America/Vancouver');
  try
    ClientTimeZone := TBundledTimeZone.GetTimeZone(TZ);
    ValidTimeZone := True;
  except on E:Exception do
    begin
      if E.ClassName = 'ETimeZoneInvalid' then
      begin
        ValidTimeZone := False;
      end
      else
      begin
        ValidTimeZone := False;
        MainForm.mmInfo.Lines.Add('System Service Error: '+E.ClassName);
        MainForm.mmInfo.Lines.Add('System Service Error: '+E.Message);
      end;
    end;
  end;

  // Build our JSON Object
  ResultJSON := TJSONObject.Create;

  // This gets us a JSON Array of Parameters
  ParametersArray := TJSONObject.ParseJSONValue('['+Trim(MainForm.AppParameters.DelimitedText)+']') as TJSONArray;

  // This gets us a JSON Array of Server IP Addresses
  ServerIPArray := TJSONObject.ParseJSONValue('['+StringReplace(MainForm.IPAddresses.DelimitedText,'  ',' ',[rfReplaceAll])+']') as TJSONArray;

  ResultJSON.AddPair('Application Name',MainForm.AppName);
  ResultJSON.AddPair('Application Version',MainForm.AppVersion);
  ResultJSON.AddPair('Application Release',FormatDateTime('yyyy-mmm-dd (ddd) hh:nn:ss', MainForm.AppRelease));
  ResultJSON.AddPair('Application Release (UTC)',FormatDateTime('yyyy-mm-dd hh:nn:ss.zzz', MainForm.AppReleaseUTC));
  ResultJSON.AddPair('Application Parameters',ParametersArray);
  ResultJSON.AddPair('Application File Name',MainForm.AppFileName);
  ResultJSON.AddPair('Application File Size',TJSONNumber.Create(MainForm.AppFileSize));
  ResultJSON.AddPair('Application TimeZone',MainForm.AppTimeZone);
  ResultJSON.AddPair('Application TimeZone Offset',TJSONNumber.Create(MainForm.AppTimeZoneOffset));
  ResultJSON.AddPair('Application Memory',IntToStr(MainForm.GetMemoryUsage));
  ResultJSON.AddPair('IP Address (Server)',ServerIPArray);
  ResultJSON.AddPair('IP Address (Client)',TXDataOperationContext.Current.Request.RemoteIP);
  ResultJSON.AddPair('Current Time (Server)',FormatDateTime('yyyy-mm-dd hh:nn:ss.zzz', Now));
  ResultJSON.AddPair('Current Time (UTC)',FormatDateTime('yyyy-mm-dd hh:nn:ss.zzz', TTimeZone.Local.ToUniversalTime(Now)));
  if ValidTimeZone and Assigned(ClientTimeZone)
  then ResultJSON.AddPair('Current Time (Client)',FormatDateTime('yyyy-mm-dd hh:nn:ss.zzz', ClientTimeZone.ToLocalTime(TTimeZone.Local.ToUniversalTime(Now))))
  else ResultJSON.AddPair('current Time (Client)','Invalid Client TimeZone');

  // Setup DB connection and query
  try
    DatabaseName := MainForm.DatabaseName;
    DatabaseEngine := MainForm.DatabaseEngine;
    DBSupport.ConnectQuery(DBConn, Query1, DatabaseName, DatabaseEngine);
  except on E: Exception do
    begin
      MainForm.mmInfo.Lines.Add('['+E.Classname+'] '+E.Message);
      raise EXDataHttpUnauthorized.Create('Internal Error: CQ');
    end;
  end;

  // Get Role information
  try
    {$Include sql\person\role\role.inc}
    Query1.ParamByName('PERSONID').AsInteger := -1; // Nobody
    Query1.Open;
  except on E: Exception do
    begin
      MainForm.mmInfo.Lines.Add('['+E.Classname+'] '+E.Message);
      raise EXDataHttpUnauthorized.Create('Internal Error: R');
    end;
  end;
  ResultArray := TJSONObject.ParseJSONValue(DBSupport.QueryToJSON(Query1)) as TJSONArray;
  ResultJSON.AddPair('Roles', ResultArray);

  // Get Period information
  try
    {$Include sql\admin\period_past\period_past.inc}
    Query1.Open;
  except on E: Exception do
    begin
      MainForm.mmInfo.Lines.Add('['+E.Classname+'] '+E.Message);
      raise EXDataHttpUnauthorized.Create('Internal Error: PP');
    end;
  end;
  ResultArray := TJSONObject.ParseJSONValue(DBSupport.QueryToJSON(Query1)) as TJSONArray;
  ResultJSON.AddPair('Periods', ResultArray);

  // Not sure if there is another version of this that is more direct?
  Result := TStringStream.Create(ResultJSON.ToString);

  // Cleanup
  ResultJSON.Free;

  // Keep track of endpoint history
  try
    {$Include sql\system\endpoint_history_insert\endpoint_history_insert.inc}
    Query1.ParamByName('PERSONID').AsInteger := 1;
    Query1.ParamByName('ENDPOINT').AsString := 'SystemService.Info';
    Query1.ParamByName('ACCESSED').AsDateTime := TTimeZone.local.ToUniversalTime(ElapsedTime);
    Query1.ParamByName('IPADDRESS').AsString := TXDataOperationContext.Current.Request.RemoteIP;
    Query1.ParamByName('APPLICATION').AsString := MainForm.AppName;
    Query1.ParamByName('VERSION').AsString := MainForm.AppVersion;
    Query1.ParamByName('DATABASENAME').AsString := DatabaseName;
    Query1.ParamByName('DATABASEENGINE').AsString := DatabaseEngine;
    Query1.ParamByName('EXECUTIONMS').AsInteger := MillisecondsBetween(Now,ElapsedTime);
    Query1.ParamByName('DETAILS').AsString := '['+TZ+']';
    Query1.ExecSQL;
  except on E: Exception do
    begin
      MainForm.mmInfo.Lines.Add('['+E.Classname+'] '+E.Message);
      raise EXDataHttpUnauthorized.Create('Internal Error: EHI');
    end;
  end;

  // All Done
  try
    DBSupport.DisconnectQuery(DBConn, Query1);
  except on E: Exception do
    begin
      MainForm.mmInfo.Lines.Add('['+E.Classname+'] '+E.Message);
      raise EXDataHttpUnauthorized.Create('Internal Error: DQ');
    end;
  end;

end;



function TSystemService.Login(Login_ID, Password, API_Key, TZ, IPAddress, IPLocation, DeviceInfo, BrowserInfo, ActionSession, ActionLog: String): TStream;
var
  DBConn: TFDConnection;
  Query1: TFDQuery;
  DatabaseEngine: String;
  DatabaseName: String;
  ClientTimeZone: TBundledTimeZone;
  ValidTimeZone: Boolean;
  ElapsedTime: TDateTime;

  PersonID: Integer;
  ApplicationName: String;
  Roles: String;
  EMailAddress: String;
  PasswordHash: String;

  JWT: TJWT;
  JWTString: String;
  IssuedAt: TDateTime;
  ExpiresAt: TDateTime;

  LogStatus: Integer;
  LogEvents: Integer;
  LogErrors: Integer;
  LogStart: Integer;
  LogChanges: Integer;
begin
  // Returning JWT, so flag it as such
  TXDataOperationContext.Current.Response.Headers.SetValue('content-type', 'application/jwt');

  // Time this event
  ElapsedTime := Now;

  // We're creating a JWT now that is valid for JWT_PERIOD minutes
  IssuedAt := Now;
  ExpiresAt := IncMinute(IssuedAt, JWT_PERIOD);

  // Check that we've got values for all of the above.
  if Trim(Login_ID) = '' then raise EXDataHttpUnauthorized.Create('Username cannot be blank.');
  if Trim(Password) = '' then raise EXDataHttpUnauthorized.Create('Password cannot be blank.');
  if Trim(API_Key) = '' then raise EXDataHttpUnauthorized.Create('API Key cannot be blank.');
  if Trim(TZ) = '' then raise EXDataHttpUnauthorized.Create('Timezone cannot be blank.');

  // Figure out if we have a valid TZ
  ClientTimeZone := TBundledTimeZone.GetTimeZone('America/Vancouver');
  try
    ClientTimeZone := TBundledTimeZone.GetTimeZone(TZ);
    ValidTimeZone := True;
  except on E:Exception do
    begin
      if E.ClassName = 'ETimeZoneInvalid' then
      begin
        ValidTimeZone := False;
      end
      else
      begin
        ValidTimeZone := False;
        MainForm.mmInfo.Lines.Add('System Service Error: '+E.ClassName);
        MainForm.mmInfo.Lines.Add('System Service Error: '+E.Message);
      end;
    end;
  end;
  if not(ValidTimeZone) then raise EXDataHttpUnauthorized.Create('Invalid Timezone.');

  // Setup DB connection and query
  DatabaseName := MainForm.DatabaseName;
  DatabaseEngine := MainForm.DatabaseEngine;
  try
    DBSupport.ConnectQuery(DBConn, Query1, DatabaseName, DatabaseEngine);
  except on E: Exception do
    begin
      DBSupport.DisconnectQuery(DBConn, Query1);
      MainForm.mmInfo.Lines.Add('['+E.Classname+'] '+E.Message);
      raise EXDataHttpUnauthorized.Create('Internal Error: CQ');
    end;
  end;

  // Check if we've got a valid API_Key
  try
    {$Include sql\system\api_key_check\api_key_check.inc}
    Query1.ParamByName('APIKEY').AsString := LowerCase(API_Key);
    Query1.Open;
  except on E: Exception do
    begin
      DBSupport.DisconnectQuery(DBConn, Query1);
      MainForm.mmInfo.Lines.Add('['+E.Classname+'] '+E.Message);
      raise EXDataHttpUnauthorized.Create('Internal Error: AKC');
    end;
  end;
  if Query1.RecordCount = 0 then
  begin
    DBSupport.DisconnectQuery(DBConn, Query1);
    raise EXDataHttpUnauthorized.Create('API_Key was not validated.');
  end;
  ApplicationName := Query1.FieldByName('application').AsString;

  if not(Query1.FieldByName('valid_until').IsNull) and
     (ExpiresAt > TTimeZone.Local.ToLocalTime(Query1.FieldByName('valid_until').AsDateTime))
  then ExpiresAt := TTimeZone.Local.ToLocalTime(Query1.FieldByName('valid_until').AsDateTime);

  // Check if the IP Address is always allowed
  try
    {$Include sql\system\ip_allow_check\ip_allow_check.inc}
    Query1.ParamByName('IPADDRESS').AsString := TXDataOperationContext.Current.Request.RemoteIP;
    Query1.Open;
    if Query1.RecordCount = 0 then
    begin
      try
        {$Include sql\system\ip_block_check\ip_block_check.inc}
        Query1.ParamByName('IPADDRESS').AsString := TXDataOperationContext.Current.Request.RemoteIP;
        Query1.Open;
        if Query1.RecordCount > 0 then raise EXDataHttpUnauthorized.Create('IP Address has been blocked temporarily.')
      except on E: Exception do
        begin
    DBSupport.DisconnectQuery(DBConn, Query1);
          MainForm.mmInfo.Lines.Add('['+E.Classname+'] '+E.Message);
          raise EXDataHttpUnauthorized.Create('Internal Error: IBC');
        end;
      end;
    end;
  except on E: Exception do
    begin
      DBSupport.DisconnectQuery(DBConn, Query1);
      MainForm.mmInfo.Lines.Add('['+E.Classname+'] '+E.Message);
      raise EXDataHttpUnauthorized.Create('Internal Error: IAC');
    end;
  end;

  // IP Check passed.  Next up: Login attempts.  First we log the attempt.  Then we count them.
  try
    {$Include sql\system\login_fail_insert\login_fail_insert.inc}
    Query1.ParamByName('LOGINID').AsString := LowerCase(Login_ID);
    Query1.ParamByName('IPADDRESS').AsString := TXDataOperationContext.Current.Request.RemoteIP;
    Query1.Execute;
  except on E: Exception do
    begin
      DBSupport.DisconnectQuery(DBConn, Query1);
      MainForm.mmInfo.Lines.Add('['+E.Classname+'] '+E.Message);
      raise EXDataHttpUnauthorized.Create('Internal Error: LFI');
    end;
  end;
  try
    {$Include sql\system\login_fail_check\login_fail_check.inc}
    Query1.ParamByName('LOGINID').AsString := LowerCase(Login_ID);
    Query1.ParamByName('IPADDRESS').AsString := TXDataOperationContext.Current.Request.RemoteIP;
    Query1.Open;
  except on E: Exception do
    begin
      DBSupport.DisconnectQuery(DBConn, Query1);
      MainForm.mmInfo.Lines.Add('['+E.Classname+'] '+E.Message);
      raise EXDataHttpUnauthorized.Create('Internal Error: LFC');
    end;
  end;
  if Query1.FieldByNAme('attempts').AsInteger >= 5 then
  begin
    try
      {$Include sql\system\ip_block_insert\ip_block_insert.inc}
      Query1.ParamByName('IPADDRESS').AsString := TXDataOperationContext.Current.Request.RemoteIP;
      Query1.ParamByName('REASON').AsString := 'Too many failed login attempts.';
      Query1.ExecSQL;
    except on E: Exception do
      begin
        DBSupport.DisconnectQuery(DBConn, Query1);
        MainForm.mmInfo.Lines.Add('['+E.Classname+'] '+E.Message);
        raise EXDataHttpUnauthorized.Create('Internal Error: IBI');
      end;
    end;
    raise EXDataHttpUnauthorized.Create('Too many failed login attempts.  Please try again later.')
  end;

  // Alright, the login has passed all its initial checks.  Lets see if the Login_ID is known
  try
    {$Include sql\system\contact_search\contact_search.inc}
    Query1.ParamByName('LOGINID').AsString := LowerCase(Login_ID);
    Query1.Open;
  except on E: Exception do
    begin
      DBSupport.DisconnectQuery(DBConn, Query1);
      MainForm.mmInfo.Lines.Add('['+E.Classname+'] '+E.Message);
      raise EXDataHttpUnauthorized.Create('Internal Error: CS');
    end;
  end;
  if Query1.RecordCount = 0 then
  begin
    DBSupport.DisconnectQuery(DBConn, Query1);
    raise EXDataHttpUnauthorized.Create('Login not authenticated: Invalid Username.')
  end
  else if Query1.RecordCount > 1 then
  begin
    DBSupport.DisconnectQuery(DBConn, Query1);
    EXDataHttpUnauthorized.Create('Login not authenticated: Ambiguous Login.');
  end;

  // Got the Person ID
  PersonID := Query1.FieldByName('person_id').AsInteger;

  // Ok, we've got a person, let's see if they've got the required Login role
  try
    {$Include sql\system\person_role_check\person_role_check.inc}
    Query1.ParamByName('PERSONID').AsInteger := PersonID;
    Query1.Open;
  except on E: Exception do
    begin
      DBSupport.DisconnectQuery(DBConn, Query1);
      MainForm.mmInfo.Lines.Add('['+E.Classname+'] '+E.Message);
      raise EXDataHttpUnauthorized.Create('Internal Error: PRC');
    end;
  end;
  if Query1.FieldByName('role_id').AsInteger <> 0 then
  begin
    DBSupport.DisconnectQuery(DBConn, Query1);
    raise EXDataHttpUnauthorized.Create('Login not authorized for this Username.');
  end;

  // Login role is present, so let's make a note of the other roles
  Roles := '';
  while not(Query1.EOF) do
  begin
    Roles := Roles + Query1.FieldByName('role_id').AsString;

    // Invalid Date Conversin???
//    DStr := Query1.FieldByName('valid_until').AsString;
//    DDate := EncodeDateTime(
//      StrToInt(Copy(DStr,1,4)),
//      StrToInt(Copy(DStr,6,2)),
//      StrToInt(Copy(DStr,9,2)),
//      StrToInt(Copy(DStr,12,2)),
//      StrToInt(Copy(DStr,15,2)),
//      StrToInt(Copy(DStr,18,2)),
//      0
//    );
//    if not(Query1.FieldByName('valid_until').isNull) and
//       (ExpiresAt > TTimeZone.Local.ToLocalTIme(DDate))
//    then ExpiresAt := TTimeZone.Local.ToLocalTime(DDate);

    // Limit token validity of role expires before token expires
    if not(Query1.FieldByName('valid_until').isNull) and
       (ExpiresAt > TTimeZone.Local.ToLocalTIme(Query1.FieldByName('valid_until').AsDateTime))
    then ExpiresAt := TTimeZone.Local.ToLocalTime(Query1.FieldByName('valid_until').AsDateTime);

    Query1.Next;
    if not(Query1.EOF) then Roles := Roles + ',';
  end;

  // Get the first available EMail address if possible
  EMailAddress := 'unavailable';
  try
    {$Include sql\system\contact_email\contact_email.inc}
    Query1.ParamByName('PERSONID').AsInteger := PersonID;
    Query1.Open;
    if Query1.RecordCount > 0
    then EMailAddress := Query1.FieldByName('value').AsString;
  except on E: Exception do
    begin
      DBSupport.DisconnectQuery(DBConn, Query1);
      MainForm.mmInfo.Lines.Add('['+E.Classname+'] '+E.Message);
      raise EXDataHttpUnauthorized.Create('Internal Error: CE');
    end;
  end;

  // Finally, let's check the actual passowrd.
  PasswordHash := DBSupport.HashThis('XData-Password:'+Trim(Password));
  try
    {$Include sql\system\person_password_check\person_password_check.inc}
    Query1.ParamByName('PERSONID').AsInteger := PersonID;
    Query1.ParamByName('PASSWORDHASH').AsString := PasswordHash;
    Query1.Open;
  except on E: Exception do
    begin
      DBSupport.DisconnectQuery(DBConn, Query1);
      MainForm.mmInfo.Lines.Add('['+E.Classname+'] '+E.Message);
      raise EXDataHttpUnauthorized.Create('Internal Error: PPC');
    end;
  end;
  if Query1.RecordCount <> 1 then
  begin
    DBSupport.DisconnectQuery(DBConn, Query1);
    raise EXDataHttpUnauthorized.Create('Login not authenticated: Invalid Password');
  end;

  // Login has been authenticated and authorized.



  // Generate a new JWT
  JWT := TJWT.Create;
  try
    // Setup some Claims
    JWT.Claims.Issuer := MainForm.AppName;
    JWT.Claims.SetClaimOfType<string>( 'ver', MainForm.AppVersion );
    JWT.Claims.SetClaimOfType<string>( 'tzn', TZ );
    JWT.Claims.SetClaimOfType<Integer>( 'tzo', Trunc(ClientTimeZone.GetUTCOffset(Now).TotalMinutes));
    JWT.Claims.SetClaimOfType<integer>('usr', PersonID );
    JWT.Claims.SetClaimOfType<string>( 'app', ApplicationName );
    JWT.Claims.SetClaimOfType<string>( 'dbn', DatabaseName );
    JWT.Claims.SetClaimOfType<string>( 'dbe', DatabaseEngine );
    JWT.Claims.SetClaimOfType<string>( 'rol', Roles );
    JWT.Claims.SetClaimOfType<string>( 'eml', EMailAddress );
    JWT.Claims.SetClaimOfType<string>( 'fnm', Query1.FieldByName('first_name').AsString );
    JWT.Claims.SetClaimOfType<string>( 'mnm', Query1.FieldByName('middle_name').AsString );
    JWT.Claims.SetClaimOfType<string>( 'lnm', Query1.FieldByName('last_name').AsString );
    JWT.Claims.SetClaimOfType<string>( 'anm', Query1.FieldByName('account_name').AsString );
    JWT.Claims.SetClaimOfType<string>( 'net', TXDataOperationContext.Current.Request.RemoteIP );
    JWT.Claims.SetClaimOfType<string>( 'aft', FormatDateTime('yyyy-mm-dd hh:nn:ss.zzz',  TTimeZone.local.ToUniversalTime(IssuedAt))+' UTC');
    JWT.Claims.SetClaimOfType<string>( 'unt', FormatDateTime('yyyy-mm-dd hh:nn:ss.zzz',  TTimeZone.local.ToUniversalTime(ExpiresAt))+' UTC');
//    JWT.Claims.SetClaimOfType<int64>('iat', MillisecondsBetween(TTimeZone.local.ToUniversalTime(IssuedAt), EncodeDateTime(1970,1,1,0,0,0,0)));
//    JWT.Claims.SetClaimOfType<int64>('exp', MillisecondsBetween(TTimeZone.local.ToUniversalTime(ExpiresAt), EncodeDateTime(1970,1,1,0,0,0,0)));
    JWT.Claims.SetClaimOfType<integer>('iat', DateTimeToUnix(TTimeZone.local.ToUniversalTime(IssuedAt)));
    JWT.Claims.Expiration := ExpiresAt; // Gets converted to UTC automatically

    // Generate the actual JWT
    JWTSTring := 'Bearer '+TJOSE.SHA256CompactToken(ServerContainer.XDataServerJWT.Secret, JWT);
    Result := TStringStream.Create(JWTString);

  finally
    JWT.Free;
  end;

  // Add the JWT to a table that we'll use to help with expring tokens
  try
    {$Include sql\system\token_insert\token_insert.inc}
    Query1.ParamByName('TOKENHASH').AsString := DBSupport.HashThis(JWTString);
    Query1.ParamByName('PERSONID').AsInteger := PersonID;
    Query1.ParamByName('VALIDAFTER').AsDateTime := TTimeZone.local.ToUniversalTime(IssuedAt);
    Query1.ParamByName('VALIDUNTIL').AsDateTime := TTimeZone.local.ToUniversalTime(ExpiresAt);
    Query1.ParamByName('APPLICATION').AsString := ApplicationName;
    Query1.ParamByName('VERSION').AsString := MainForm.AppVersion;
    Query1.ParamByName('IPADDRESS').AsString := TXDataOperationContext.Current.Request.RemoteIP;
    Query1.ExecSQL;
  except on E: Exception do
    begin
      DBSupport.DisconnectQuery(DBConn, Query1);
      MainForm.mmInfo.Lines.Add('['+E.Classname+'] '+E.Message);
      raise EXDataHttpUnauthorized.Create('Internal Error: TI');
    end;
  end;

  // Keep track of login history
  try
    {$Include sql\system\login_history_insert\login_history_insert.inc}
    Query1.ParamByName('LOGGEDIN').AsDateTime := TTimeZone.local.ToUniversalTime(IssuedAt);
//    Query1.ParamByName('IPADDRESS').AsString := TXDataOperationContext.Current.Request.RemoteIP;
    Query1.ParamByName('IPADDRESS').AsString := IPAddress;
    Query1.ParamByName('PERSONID').AsInteger := PersonID;
    Query1.ParamByName('IPLOCATION').AsString := IPLocation;
    Query1.ParamByName('DEVICEINFO').AsString := DeviceInfo;
    Query1.ParamByName('BROWSERINFO').AsString := BrowserInfo;
    Query1.ParamByName('APPLICATION').AsString := ApplicationName;
    Query1.ParamByName('VERSION').AsString := MainForm.AppVersion;
    Query1.ExecSQL;
  except on E: Exception do
    begin
      DBSupport.DisconnectQuery(DBConn, Query1);
      MainForm.mmInfo.Lines.Add('['+E.Classname+'] '+E.Message);
      raise EXDataHttpUnauthorized.Create('Internal Error: LHI');
    end;
  end;

  // Cleanup after login
  try
    {$Include sql\system\login_cleanup\login_cleanup.inc}
    Query1.ParamByName('IPADDRESS').AsString := TXDataOperationContext.Current.Request.RemoteIP;
    Query1.ParamByName('LOGINID').AsString := LowerCase(Login_ID);
    Query1.ExecSQL;
  except on E: Exception do
    begin
      DBSupport.DisconnectQuery(DBConn, Query1);
      MainForm.mmInfo.Lines.Add('['+E.Classname+'] '+E.Message);
      raise EXDataHttpUnauthorized.Create('Internal Error: LCI');
    end;
  end;


  // Store ActionLog
  LogStatus := 0;
  if Pos('Browser Closed', ActionLog) > 0 then LogStatus := 1
  else if Pos('Logout: Normal', ActionLog) > 0 then LogStatus := 2
  else if Pos('Logout: Clean', ActionLog) > 0 then LogStatus := 3
  else if Pos('Logout: All', ActionLog) > 0 then LogStatus := 4;

  LogStart := 0;
  if Pos('Login Successful', ActionLog) > 0 then LogStart := 1
  else if Pos('AutoLogin - JWT Time Remaining', ActionLog) > 0 then LogStart := 2;

  LogEvents := DBSupport.Occurrences('[',ActionLog);
  LogChanges := DBSupport.Occurrences('<<',ActionLog);

  LogErrors := DBSupport.Occurrences('Failed:',ActionLog) +
               DBSupport.Occurrences('EXCEPTION:',ActionLog);

  // Record Action History
  try
    {$Include sql\system\action_history_insert\action_history_insert.inc}
    Query1.ParamByName('PERSONID').AsInteger := PersonID;
    Query1.ParamByName('IPADDRESS').AsString := TXDataOperationContext.Current.Request.RemoteIP;
    Query1.ParamByName('APPLICATION').AsString := ApplicationName;
    Query1.ParamByName('VERSION').AsString := MainForm.AppVersion;
    Query1.ParamByName('SESSIONID').AsString := ActionSession;
    Query1.ParamByName('SESSIONSTART').AsDateTime := DBSupport.DecodeSession(ActionSession);
    Query1.ParamByName('SESSIONRECORDED').AsDateTime := TTimeZone.local.ToUniversalTime(ElapsedTime);
    Query1.ParamByName('LOGSTATUS').AsInteger := LogStatus;
    Query1.ParamByName('LOGERRORS').AsInteger := LogErrors;
    Query1.ParamByName('LOGEVENTS').AsInteger := LogEvents;
    Query1.ParamByName('LOGCHANGES').AsInteger := LogChanges;
    Query1.ParamByName('LOGSTART').AsInteger := LogStart;
    Query1.ParamByName('ACTIONS').AsString := ActionLog;

    Query1.ExecSQL;
  except on E: Exception do
    begin
      DBSupport.DisconnectQuery(DBConn, Query1);
      MainForm.mmInfo.Lines.Add('['+E.Classname+'] '+E.Message);
      raise EXDataHttpUnauthorized.Create('Internal Error: AHI');
    end;
  end;

  // Keep track of endpoint history
  try
    {$Include sql\system\endpoint_history_insert\endpoint_history_insert.inc}
    Query1.ParamByName('PERSONID').AsInteger := PersonID;
    Query1.ParamByName('ENDPOINT').AsString := 'SystemService.Login';
    Query1.ParamByName('ACCESSED').AsDateTime := TTimeZone.local.ToUniversalTime(ElapsedTime);
    Query1.ParamByName('IPADDRESS').AsString := TXDataOperationContext.Current.Request.RemoteIP;
    Query1.ParamByName('APPLICATION').AsString := ApplicationName;
    Query1.ParamByName('VERSION').AsString := MainForm.AppVersion;
    Query1.ParamByName('DATABASENAME').AsString := DatabaseName;
    Query1.ParamByName('DATABASEENGINE').AsString := DatabaseEngine;
    Query1.ParamByName('EXECUTIONMS').AsInteger := MillisecondsBetween(Now,ElapsedTime);
    Query1.ParamByName('DETAILS').AsString := '['+Login_ID+'] [Passowrd] [API_Key] ['+TZ+']';
    Query1.ExecSQL;
  except on E: Exception do
    begin
      DBSupport.DisconnectQuery(DBConn, Query1);
      MainForm.mmInfo.Lines.Add('['+E.Classname+'] '+E.Message);
      raise EXDataHttpUnauthorized.Create('Internal Error: EHI');
    end;
  end;

  // All Done
  try  
    DBSupport.DisconnectQuery(DBConn, Query1);
  except on E: Exception do
    begin
      MainForm.mmInfo.Lines.Add('['+E.Classname+'] '+E.Message);
      raise EXDataHttpUnauthorized.Create('Internal Error: DQ');
    end;
  end;

end;

function TSystemService.Logout(ActionSession: String; ActionLog: String): TStream;
var
  DBConn: TFDConnection;
  Query1: TFDQuery;
  DatabaseEngine: String;
  DatabaseName: String;
  ElapsedTime: TDateTime;

  OldJWT: String;

  User: IUserIdentity;

  LogStatus: Integer;
  LogEvents: Integer;
  LogErrors: Integer;
  LogStart: Integer;
  LogChanges: Integer;
begin
  // Returning JWT, so flag it as such
  TXDataOperationContext.Current.Response.Headers.SetValue('content-type', 'application/json');

  // Time this event
  ElapsedTime := Now;

  // Get data from the JWT
  User := TXDataOperationContext.Current.Request.User;
  OldJWT := TXDataOperationContext.Current.Request.Headers.Get('Authorization');
  if (User = nil) then raise EXDataHttpUnauthorized.Create('Missing authentication');

  // Setup DB connection and query
  DatabaseName := MainForm.DatabaseName;
  DatabaseEngine := MainForm.DatabaseEngine;
  try
    DBSupport.ConnectQuery(DBConn, Query1, DatabaseName, DatabaseEngine);
  except on E: Exception do
    begin
      DBSupport.DisconnectQuery(DBConn, Query1);
      MainForm.mmInfo.Lines.Add('['+E.Classname+'] '+E.Message);
      raise EXDataHttpUnauthorized.Create('Internal Error: CQ');
    end;
  end;

  // Check if we've got a valid JWT (one that has not been revoked)
  try
    {$Include sql\system\token_check\token_check.inc}
    Query1.ParamByName('TOKENHASH').AsString := DBSupport.HashThis(OldJWT);
    Query1.ParamByName('IPADDRESS').AsString := TXDataOperationContext.Current.Request.RemoteIP;
    Query1.Open;
  except on E: Exception do
    begin
      MainForm.mmInfo.Lines.Add('['+E.Classname+'] '+E.Message);
      raise EXDataHttpUnauthorized.Create('Internal Error: TC');
    end;
  end;
  if Query1.RecordCount <> 1 then
  begin
    DBSupport.DisconnectQuery(DBConn, Query1);
    raise EXDataHttpUnauthorized.Create('JWT was not validated');
  end;

  // Revoke JWT
//  try
//    {$Include sql\system\token_revoke\token_revoke.inc}
//    Query1.ParamByName('TOKENHASH').AsString := DBSupport.HashThis(OldJWT);
//    Query1.ExecSQL;
//  except on E: Exception do
//    begin
//      MainForm.mmInfo.Lines.Add('['+E.Classname+'] '+E.Message);
//      raise EXDataHttpUnauthorized.Create('Internal Error: TR');
//    end;
//  end;

  LogStatus := 0;
  if Pos('Browser Closed', ActionLog) > 0 then LogStatus := 1
  else if Pos('Logout: Normal', ActionLog) > 0 then LogStatus := 2
  else if Pos('Logout: Clean', ActionLog) > 0 then LogStatus := 3
  else if Pos('Logout: All', ActionLog) > 0 then LogStatus := 4;

  LogStart := 0;
  if Pos('Login Successful', ActionLog) > 0 then LogStart := 1
  else if Pos('AutoLogin - JWT Time Remaining', ActionLog) > 0 then LogStart := 2;

  LogEvents := DBSupport.Occurrences('[',ActionLog);
  LogChanges := DBSupport.Occurrences('<<',ActionLog);

  LogErrors := DBSupport.Occurrences('Failed:',ActionLog) +
               DBSupport.Occurrences('EXCEPTION:',ActionLog);

  // Record Action History
  try
    {$Include sql\system\action_history_insert\action_history_insert.inc}
    Query1.ParamByName('PERSONID').AsInteger := User.Claims.Find('usr').asInteger;
    Query1.ParamByName('IPADDRESS').AsString := TXDataOperationContext.Current.Request.RemoteIP;
    Query1.ParamByName('APPLICATION').AsString := User.Claims.Find('app').asString;
    Query1.ParamByName('VERSION').AsString := User.Claims.Find('ver').asString;
    Query1.ParamByName('SESSIONID').AsString := ActionSession;
    Query1.ParamByName('SESSIONSTART').AsDateTime := DBSupport.DecodeSession(ActionSession);
    Query1.ParamByName('SESSIONRECORDED').AsDateTime := TTimeZone.local.ToUniversalTime(ElapsedTime);
    Query1.ParamByName('LOGSTATUS').AsInteger := LogStatus;
    Query1.ParamByName('LOGERRORS').AsInteger := LogErrors;
    Query1.ParamByName('LOGEVENTS').AsInteger := LogEvents;
    Query1.ParamByName('LOGCHANGES').AsInteger := LogChanges;
    Query1.ParamByName('LOGSTART').AsInteger := LogStart;
    Query1.ParamByName('ACTIONS').AsString := ActionLog;

    Query1.ExecSQL;
  except on E: Exception do
    begin
      DBSupport.DisconnectQuery(DBConn, Query1);
      MainForm.mmInfo.Lines.Add('['+E.Classname+'] '+E.Message);
      raise EXDataHttpUnauthorized.Create('Internal Error: AHI');
    end;
  end;

  // Keep track of endpoint history
  try
    {$Include sql\system\endpoint_history_insert\endpoint_history_insert.inc}
    Query1.ParamByName('PERSONID').AsInteger := User.Claims.Find('usr').asInteger;
    Query1.ParamByName('ENDPOINT').AsString := 'SystemService.Renew';
    Query1.ParamByName('ACCESSED').AsDateTime := TTimeZone.local.ToUniversalTime(ElapsedTime);
    Query1.ParamByName('IPADDRESS').AsString := TXDataOperationContext.Current.Request.RemoteIP;
    Query1.ParamByName('APPLICATION').AsString := User.Claims.Find('app').asString;
    Query1.ParamByName('VERSION').AsString := User.Claims.Find('ver').asString;
    Query1.ParamByName('DATABASENAME').AsString := User.Claims.Find('dbn').asString;
    Query1.ParamByName('DATABASEENGINE').AsString := User.Claims.Find('dbe').asString;
    Query1.ParamByName('EXECUTIONMS').AsInteger := MillisecondsBetween(Now,ElapsedTime);
    Query1.ParamByName('DETAILS').AsString := '['+User.Claims.Find('anm').asString+']';
    Query1.ExecSQL;
  except on E: Exception do
    begin
      DBSupport.DisconnectQuery(DBConn, Query1);
      MainForm.mmInfo.Lines.Add('['+E.Classname+'] '+E.Message);
      raise EXDataHttpUnauthorized.Create('Internal Error: EHI');
    end;
  end;

  // All Done
  try
    DBSupport.DisconnectQuery(DBConn, Query1);
  except on E: Exception do
    begin
      MainForm.mmInfo.Lines.Add('['+E.Classname+'] '+E.Message);
      raise EXDataHttpUnauthorized.Create('Internal Error: DQ');
    end;
  end;

  Result := TStringStream.Create('{"Message":"Logout Complete"}');
end;

function TSystemService.LogoutAll(ActionSession,
  ActionLog: String): TStream;
var
  DBConn: TFDConnection;
  Query1: TFDQuery;
  DatabaseEngine: String;
  DatabaseName: String;
  ElapsedTime: TDateTime;

  OldJWT: String;

  User: IUserIdentity;

  LogStatus: Integer;
  LogEvents: Integer;
  LogErrors: Integer;
  LogStart: Integer;
  LogChanges: Integer;
begin
  // Returning JWT, so flag it as such
  TXDataOperationContext.Current.Response.Headers.SetValue('content-type', 'application/json');

  // Time this event
  ElapsedTime := Now;

  // Get data from the JWT
  User := TXDataOperationContext.Current.Request.User;
  OldJWT := TXDataOperationContext.Current.Request.Headers.Get('Authorization');
  if (User = nil) then raise EXDataHttpUnauthorized.Create('Missing authentication');

  // Setup DB connection and query
  DatabaseName := MainForm.DatabaseName;
  DatabaseEngine := MainForm.DatabaseEngine;
  try
    DBSupport.ConnectQuery(DBConn, Query1, DatabaseName, DatabaseEngine);
  except on E: Exception do
    begin
      DBSupport.DisconnectQuery(DBConn, Query1);
      MainForm.mmInfo.Lines.Add('['+E.Classname+'] '+E.Message);
      raise EXDataHttpUnauthorized.Create('Internal Error: CQ');
    end;
  end;

  // Check if we've got a valid JWT (one that has not been revoked)
  try
    {$Include sql\system\token_check\token_check.inc}
    Query1.ParamByName('TOKENHASH').AsString := DBSupport.HashThis(OldJWT);
    Query1.ParamByName('IPADDRESS').AsString := TXDataOperationContext.Current.Request.RemoteIP;
    Query1.Open;
  except on E: Exception do
    begin
      MainForm.mmInfo.Lines.Add('['+E.Classname+'] '+E.Message);
      raise EXDataHttpUnauthorized.Create('Internal Error: TC');
    end;
  end;
  if Query1.RecordCount <> 1 then
  begin
    DBSupport.DisconnectQuery(DBConn, Query1);
    raise EXDataHttpUnauthorized.Create('JWT was not validated');
  end;

  // Revoke JWT
//  try
//    {$Include sql\system\token_revoke\token_revoke.inc}
//    Query1.ParamByName('TOKENHASH').AsString := DBSupport.HashThis(OldJWT);
//    Query1.ExecSQL;
//  except on E: Exception do
//    begin
//      MainForm.mmInfo.Lines.Add('['+E.Classname+'] '+E.Message);
//      raise EXDataHttpUnauthorized.Create('Internal Error: TR');
//    end;
//  end;

  // Revoke all JWT
  try
    {$Include sql\system\token_revoke_all\token_revoke_all.inc}
    Query1.ParamByName('PERSONID').AsInteger := User.Claims.Find('usr').asInteger;
    Query1.ExecSQL;
  except on E: Exception do
    begin
      MainForm.mmInfo.Lines.Add('['+E.Classname+'] '+E.Message);
      raise EXDataHttpUnauthorized.Create('Internal Error: TRA');
    end;
  end;

  LogStatus := 0;
  if Pos('Browser Closed', ActionLog) > 0 then LogStatus := 1
  else if Pos('Logout: Normal', ActionLog) > 0 then LogStatus := 2
  else if Pos('Logout: Clean', ActionLog) > 0 then LogStatus := 3
  else if Pos('Logout: All', ActionLog) > 0 then LogStatus := 4;

  LogStart := 0;
  if Pos('Login Successful', ActionLog) > 0 then LogStart := 1
  else if Pos('AutoLogin - JWT Time Remaining', ActionLog) > 0 then LogStart := 2;

  LogEvents := DBSupport.Occurrences('[',ActionLog);
  LogChanges := DBSupport.Occurrences('<<',ActionLog);

  LogErrors := DBSupport.Occurrences('Failed:',ActionLog) +
               DBSupport.Occurrences('EXCEPTION:',ActionLog);

  // Record Action History
  try
    {$Include sql\system\action_history_insert\action_history_insert.inc}
    Query1.ParamByName('PERSONID').AsInteger := User.Claims.Find('usr').asInteger;
    Query1.ParamByName('IPADDRESS').AsString := TXDataOperationContext.Current.Request.RemoteIP;
    Query1.ParamByName('APPLICATION').AsString := User.Claims.Find('app').asString;
    Query1.ParamByName('VERSION').AsString := User.Claims.Find('ver').asString;
    Query1.ParamByName('SESSIONID').AsString := ActionSession;
    Query1.ParamByName('SESSIONSTART').AsDateTime := DBSupport.DecodeSession(ActionSession);
    Query1.ParamByName('SESSIONRECORDED').AsDateTime := TTimeZone.local.ToUniversalTime(Now);
    Query1.ParamByName('LOGSTATUS').AsInteger := LogStatus;
    Query1.ParamByName('LOGERRORS').AsInteger := LogErrors;
    Query1.ParamByName('LOGEVENTS').AsInteger := LogEvents;
    Query1.ParamByName('LOGSTART').AsInteger := LogStart;
    Query1.ParamByName('LOGCHANGES').AsInteger := LogChanges;
    Query1.ParamByName('ACTIONS').AsString := ActionLog;
    Query1.ExecSQL;
  except on E: Exception do
    begin
      DBSupport.DisconnectQuery(DBConn, Query1);
      MainForm.mmInfo.Lines.Add('['+E.Classname+'] '+E.Message);
      raise EXDataHttpUnauthorized.Create('Internal Error: AHI');
    end;
  end;

  // Keep track of endpoint history
  try
    {$Include sql\system\endpoint_history_insert\endpoint_history_insert.inc}
    Query1.ParamByName('PERSONID').AsInteger := User.Claims.Find('usr').asInteger;
    Query1.ParamByName('ENDPOINT').AsString := 'SystemService.LogoutAll';
    Query1.ParamByName('ACCESSED').AsDateTime := TTimeZone.local.ToUniversalTime(ElapsedTime);
    Query1.ParamByName('IPADDRESS').AsString := TXDataOperationContext.Current.Request.RemoteIP;
    Query1.ParamByName('APPLICATION').AsString := User.Claims.Find('app').asString;
    Query1.ParamByName('VERSION').AsString := User.Claims.Find('ver').asString;
    Query1.ParamByName('DATABASENAME').AsString := User.Claims.Find('dbn').asString;
    Query1.ParamByName('DATABASEENGINE').AsString := User.Claims.Find('dbe').asString;
    Query1.ParamByName('EXECUTIONMS').AsInteger := MillisecondsBetween(Now,ElapsedTime);
    Query1.ParamByName('DETAILS').AsString := '['+User.Claims.Find('anm').asString+']';
    Query1.ExecSQL;
  except on E: Exception do
    begin
      DBSupport.DisconnectQuery(DBConn, Query1);
      MainForm.mmInfo.Lines.Add('['+E.Classname+'] '+E.Message);
      raise EXDataHttpUnauthorized.Create('Internal Error: EHI');
    end;
  end;

  // All Done
  try
    DBSupport.DisconnectQuery(DBConn, Query1);
  except on E: Exception do
    begin
      MainForm.mmInfo.Lines.Add('['+E.Classname+'] '+E.Message);
      raise EXDataHttpUnauthorized.Create('Internal Error: DQ');
    end;
  end;

  Result := TStringStream.Create('{"Message":"Logout Complete"}');
end;

function TSystemService.Renew(ActionSession: String; ActionLog: String): TStream;
var
  DBConn: TFDConnection;
  Query1: TFDQuery;
  DatabaseEngine: String;
  DatabaseName: String;
  ElapsedTime: TDateTime;

  OldJWT: String;
  JWT: TJWT;
  JWTString: String;
  IssuedAt: TDateTime;
  ExpiresAt: TDateTime;
  Roles: String;

  EMailAddress: String;

  User: IUserIdentity;

  LogStatus: Integer;
  LogEvents: Integer;
  LogErrors: Integer;
  LogStart: Integer;
  LogChanges: Integer;
begin
  // Returning JWT, so flag it as such
  TXDataOperationContext.Current.Response.Headers.SetValue('content-type', 'application/jwt');

  // Time this event
  ElapsedTime := Now;

  // We're creating a JWT now that is valid for JWT_PERIOD minutes
  IssuedAt := Now;
  ExpiresAt := IncMinute(IssuedAt, JWT_PERIOD);

  // Get data from the JWT
  User := TXDataOperationContext.Current.Request.User;
  OldJWT := TXDataOperationContext.Current.Request.Headers.Get('Authorization');
  if (User = nil) then raise EXDataHttpUnauthorized.Create('Missing authentication');

  // Setup DB connection and query
  DatabaseName := MainForm.DatabaseName;
  DatabaseEngine := MainForm.DatabaseEngine;
  try
    DBSupport.ConnectQuery(DBConn, Query1, DatabaseName, DatabaseEngine);
  except on E: Exception do
    begin
      DBSupport.DisconnectQuery(DBConn, Query1);
      MainForm.mmInfo.Lines.Add('['+E.Classname+'] '+E.Message);
      raise EXDataHttpUnauthorized.Create('Internal Error: CQ');
    end;
  end;

  // Check if we've got a valid JWT (one that has not been revoked)
  try
    {$Include sql\system\token_check\token_check.inc}
    Query1.ParamByName('TOKENHASH').AsString := DBSupport.HashThis(OldJWT);
    Query1.ParamByName('IPADDRESS').AsString := TXDataOperationContext.Current.Request.RemoteIP;
    Query1.Open;
  except on E: Exception do
    begin
      MainForm.mmInfo.Lines.Add('['+E.Classname+'] '+E.Message);
      raise EXDataHttpUnauthorized.Create('Internal Error: TC');
    end;
  end;
  if Query1.RecordCount <> 1 then
  begin
    DBSupport.DisconnectQuery(DBConn, Query1);
    raise EXDataHttpUnauthorized.Create('JWT was not validated');
  end;

  // Let's see if they've (still) got the required Login role
  try
    {$Include sql\system\person_role_check\person_role_check.inc}
    Query1.ParamByName('PERSONID').AsInteger := User.Claims.Find('usr').asInteger;
    Query1.Open;
  except on E: Exception do
    begin
      DBSupport.DisconnectQuery(DBConn, Query1);
      MainForm.mmInfo.Lines.Add('['+E.Classname+'] '+E.Message);
      raise EXDataHttpUnauthorized.Create('Internal Error: PRC');
    end;
  end;
  if Query1.FieldByName('role_id').AsInteger <> 0 then
  begin
    DBSupport.DisconnectQuery(DBConn, Query1);
    raise EXDataHttpUnauthorized.Create('Login not authorized');
  end;

  // Login role is present, so let's make a note of the other roles
  Roles := '';
  while not(Query1.EOF) do
  begin
    Roles := Roles + Query1.FieldByName('role_id').AsString;

    // Limit token validity of role expires before token expires
    if not(Query1.FieldByName('valid_until').isNull) and
       (ExpiresAt > TTimeZone.Local.ToLocalTIme(Query1.FieldByName('valid_until').AsDateTime))
    then ExpiresAt := TTimeZone.Local.ToLocalTime(Query1.FieldByName('valid_until').AsDateTime);

    Query1.Next;
    if not(Query1.EOF) then Roles := Roles + ',';
  end;

  // Check if we've got a valid JWT (one that has not been revoked)
  try
    {$Include sql\system\token_check\token_check.inc}
    Query1.ParamByName('TOKENHASH').AsString := DBSupport.HashThis(OldJWT);
    Query1.ParamByName('IPADDRESS').AsString := TXDataOperationContext.Current.Request.RemoteIP;
    Query1.Open;
  except on E: Exception do
    begin
      MainForm.mmInfo.Lines.Add('['+E.Classname+'] '+E.Message);
      raise EXDataHttpUnauthorized.Create('Internal Error: TC');
    end;
  end;
  if Query1.RecordCount <> 1 then
  begin
    DBSupport.DisconnectQuery(DBConn, Query1);
    raise EXDataHttpUnauthorized.Create('JWT was not validated');
  end;

  // Revoke JWT
//  try
//    {$Include sql\system\token_revoke\token_revoke.inc}
//    Query1.ParamByName('TOKENHASH').AsString := DBSupport.HashThis(OldJWT);
//    Query1.ExecSQL;
//  except on E: Exception do
//    begin
//      MainForm.mmInfo.Lines.Add('['+E.Classname+'] '+E.Message);
//      raise EXDataHttpUnauthorized.Create('Internal Error: TR');
//    end;
//  end;

  // Get the first available EMail address if possible
  EMailAddress := 'unavailable';
  try
    {$Include sql\system\contact_email\contact_email.inc}
    Query1.ParamByName('PERSONID').AsInteger :=  User.Claims.Find('usr').AsInteger;
    Query1.Open;
    if Query1.RecordCount > 0
    then EMailAddress := Query1.FieldByName('value').AsString;
  except on E: Exception do
    begin
      DBSupport.DisconnectQuery(DBConn, Query1);
      MainForm.mmInfo.Lines.Add('['+E.Classname+'] '+E.Message);
      raise EXDataHttpUnauthorized.Create('Internal Error: CE');
    end;
  end;

  // Get Profile information - Might have changed since logging in
  try
    {$Include sql\person\profile\profile.inc}
    Query1.ParamByName('PERSONID').AsInteger := User.Claims.Find('usr').AsInteger;
    Query1.Open;
  except on E: Exception do
    begin
      MainForm.mmInfo.Lines.Add('['+E.Classname+'] '+E.Message);
      raise EXDataHttpUnauthorized.Create('Internal Error: P');
    end;
  end;


  // Generate a new JWT
  JWT := TJWT.Create;
  try
    // Setup some Claims
    JWT.Claims.Issuer := MainForm.AppName;
    JWT.Claims.SetClaimOfType<string>( 'ver', User.Claims.Find('ver').asString );
    JWT.Claims.SetClaimOfType<string>( 'tzn', User.Claims.Find('tzn').asString );
    JWT.Claims.SetClaimOfType<integer>('tzo', User.Claims.Find('tzo').asInteger);
    JWT.Claims.SetClaimOfType<integer>('usr', User.Claims.Find('usr').asInteger);
    JWT.Claims.SetClaimOfType<string>( 'app', User.Claims.Find('app').asString );
    JWT.Claims.SetClaimOfType<string>( 'dbn', User.Claims.Find('dbn').asString );
    JWT.Claims.SetClaimOfType<string>( 'dbe', User.Claims.Find('dbe').asString );
    JWT.Claims.SetClaimOfType<string>( 'rol', Roles );

    JWT.Claims.SetClaimOfType<string>( 'eml', EMailAddress );
    JWT.Claims.SetClaimOfType<string>( 'fnm', Query1.FieldByName('first_name').AsString );
    JWT.Claims.SetClaimOfType<string>( 'mnm', Query1.FieldByName('middle_name').AsString );
    JWT.Claims.SetClaimOfType<string>( 'lnm', Query1.FieldByName('last_name').AsString );
    JWT.Claims.SetClaimOfType<string>( 'anm', Query1.FieldByName('account_name').AsString );

    JWT.Claims.SetClaimOfType<string>( 'net', User.Claims.Find('net').asString );
    JWT.Claims.SetClaimOfType<string>( 'aft', FormatDateTime('yyyy-mm-dd hh:nn:ss.zzz',  TTimeZone.local.ToUniversalTime(IssuedAt))+' UTC');
    JWT.Claims.SetClaimOfType<string>( 'unt', FormatDateTime('yyyy-mm-dd hh:nn:ss.zzz',  TTimeZone.local.ToUniversalTime(ExpiresAt))+' UTC');
    JWT.Claims.SetClaimOfType<integer>('iat', DateTimeToUnix(TTimeZone.local.ToUniversalTime(IssuedAt)));
    JWT.Claims.Expiration := ExpiresAt; // Gets converted to UTC automatically

    // Generate the actual JWT
    JWTSTring := 'Bearer '+TJOSE.SHA256CompactToken(ServerContainer.XDataServerJWT.Secret, JWT);
    Result := TStringStream.Create(JWTString);

  finally
    JWT.Free;
  end;

  // Add the JWT to a table that we'll use to help with expring tokens
  try
    {$Include sql\system\token_insert\token_insert.inc}
    Query1.ParamByName('TOKENHASH').AsString := DBSupport.HashThis(JWTString);
    Query1.ParamByName('PERSONID').AsInteger := User.Claims.Find('usr').asInteger;
    Query1.ParamByName('VALIDAFTER').AsDateTime := TTimeZone.local.ToUniversalTime(IssuedAt);
    Query1.ParamByName('VALIDUNTIL').AsDateTime := TTimeZone.local.ToUniversalTime(ExpiresAt);
    Query1.ParamByName('APPLICATION').AsString := User.Claims.Find('app').asString;
    Query1.ParamByName('VERSION').AsString := User.Claims.Find('ver').asString;
    Query1.ParamByName('IPADDRESS').AsString := TXDataOperationContext.Current.Request.RemoteIP;
    Query1.ExecSQL;
  except on E: Exception do
    begin
      DBSupport.DisconnectQuery(DBConn, Query1);
      MainForm.mmInfo.Lines.Add('['+E.Classname+'] '+E.Message);
      raise EXDataHttpUnauthorized.Create('Internal Error: TI');
    end;
  end;

  LogStatus := 0;
  if Pos('Browser Closed', ActionLog) > 0 then LogStatus := 1
  else if Pos('Logout: Normal', ActionLog) > 0 then LogStatus := 2
  else if Pos('Logout: Clean', ActionLog) > 0 then LogStatus := 3
  else if Pos('Logout: All', ActionLog) > 0 then LogStatus := 4;

  LogStart := 0;
  if Pos('Login Successful', ActionLog) > 0 then LogStart := 1
  else if Pos('AutoLogin - JWT Time Remaining', ActionLog) > 0 then LogStart := 2;

  LogEvents := DBSupport.Occurrences('[',ActionLog);
  LogChanges := DBSupport.Occurrences('<<',ActionLog);

  LogErrors := DBSupport.Occurrences('Failed:',ActionLog) +
               DBSupport.Occurrences('EXCEPTION:',ActionLog);

  // Record Action History
  try
    {$Include sql\system\action_history_insert\action_history_insert.inc}
    Query1.ParamByName('PERSONID').AsInteger := User.Claims.Find('usr').asInteger;
    Query1.ParamByName('IPADDRESS').AsString := TXDataOperationContext.Current.Request.RemoteIP;
    Query1.ParamByName('APPLICATION').AsString := User.Claims.Find('app').asString;
    Query1.ParamByName('VERSION').AsString := User.Claims.Find('ver').asString;
    Query1.ParamByName('SESSIONID').AsString := ActionSession;
    Query1.ParamByName('SESSIONSTART').AsDateTime := DBSupport.DecodeSession(ActionSession);
    Query1.ParamByName('SESSIONRECORDED').AsDateTime := TTimeZone.local.ToUniversalTime(Now);
    Query1.ParamByName('LOGSTATUS').AsInteger := LogStatus;
    Query1.ParamByName('LOGERRORS').AsInteger := LogErrors;
    Query1.ParamByName('LOGEVENTS').AsInteger := LogEvents;
    Query1.ParamByName('LOGCHANGES').AsInteger := LogChanges;
    Query1.ParamByName('LOGSTART').AsInteger := LogStart;
    Query1.ParamByName('ACTIONS').AsString := ActionLog;
    Query1.ExecSQL;
  except on E: Exception do
    begin
      DBSupport.DisconnectQuery(DBConn, Query1);
      MainForm.mmInfo.Lines.Add('['+E.Classname+'] '+E.Message);
      raise EXDataHttpUnauthorized.Create('Internal Error: AHI');
    end;
  end;

  // Keep track of endpoint history
  try
    {$Include sql\system\endpoint_history_insert\endpoint_history_insert.inc}
    Query1.ParamByName('PERSONID').AsInteger := User.Claims.Find('usr').asInteger;
    Query1.ParamByName('ENDPOINT').AsString := 'SystemService.Renew';
    Query1.ParamByName('ACCESSED').AsDateTime := TTimeZone.local.ToUniversalTime(ElapsedTime);
    Query1.ParamByName('IPADDRESS').AsString := TXDataOperationContext.Current.Request.RemoteIP;
    Query1.ParamByName('APPLICATION').AsString := User.Claims.Find('app').asString;
    Query1.ParamByName('VERSION').AsString := User.Claims.Find('ver').asString;
    Query1.ParamByName('DATABASENAME').AsString := User.Claims.Find('dbn').asString;
    Query1.ParamByName('DATABASEENGINE').AsString := User.Claims.Find('dbe').asString;
    Query1.ParamByName('EXECUTIONMS').AsInteger := MillisecondsBetween(Now,ElapsedTime);
    Query1.ParamByName('DETAILS').AsString := '['+User.Claims.Find('anm').asString+']';
    Query1.ExecSQL;
  except on E: Exception do
    begin
      DBSupport.DisconnectQuery(DBConn, Query1);
      MainForm.mmInfo.Lines.Add('['+E.Classname+'] '+E.Message);
      raise EXDataHttpUnauthorized.Create('Internal Error: EHI');
    end;
  end;

  // All Done
  try
    DBSupport.DisconnectQuery(DBConn, Query1);
  except on E: Exception do
    begin
      MainForm.mmInfo.Lines.Add('['+E.Classname+'] '+E.Message);
      raise EXDataHttpUnauthorized.Create('Internal Error: DQ');
    end;
  end;
end;

function TSystemService.SearchFontAwesome(Query: String): TStream;
var
  Client: TNetHTTPClient;
  QStream: TStringStream;
  Response: String;
begin
  QStream := TSTringStream.Create(Query);
  Client := TNetHTTPClient.Create(nil);
  Client.Asynchronous := False;
  Client.ContentType := 'application/json';
  Client.SecureProtocols := [THTTPSecureProtocol.SSL3, THTTPSecureProtocol.TLS12];
  Response := Client.Post('https://api.fontawesome.com',QStream).ContentAsString;
  Result := TStringStream.Create(Response);
  Client.Free;
  QStream.Free;
end;

function TSystemService.SearchIconSets(SearchTerms, SearchSets: String; Results: Integer): TStream;
var
  IconsFound: TJSONArray;
  IconSet: TJSONObject;
  IconSetList: TStringList;
  i: integer;
  j: integer;
  k: integer;
  IconName: String;
  Icon: TJSONArray;
  IconCount: Integer;
  Terms:TStringList;
  Matched: Boolean;
begin
  // Returning JSON, so flag it as such
  TXDataOperationContext.Current.Response.Headers.SetValue('content-type', 'application/json');

  // JSON Array we'll be returning
  IconsFound := TJSONArray.Create;
  IconCount := 0;

  // If all, will just iterate through the sets
  // Otherwise, we'll build a list and only iterate through the contents of that list
  IconSetList := TStringList.Create;
  if SearchSets = 'all' then
  begin
    k := Mainform.AppIcons.Count;
  end
  else
  begin
    IconSetList.CommaText := SearchSets;
    k := IconSetList.Count;
  end;

  // Sort out Search Terms
  Terms := TStringList.Create;
  Terms.CommaText := StringReplace(Trim(SearchTerms),' ',',',[rfReplaceAll]);

  i := 0;
  while (i < k) and (IconCount < Results) and (Terms.Count > 0) do
  begin

    // Load up an Icon Set to Search
    if SearchSets = 'all'
    then IconSet := (MainForm.AppIcons.Items[i] as TJSONObject).GetValue('icons') as TJSONObject
    else IconSet := (MainForm.AppIcons.Items[StrToInt(IconSetList[i])] as TJSONObject).GetValue('icons') as TJSONObject;

    // Search all the icons in the Set
    for j := 0 to IconSet.Count-1 do
    begin

      if (IconCount < Results) then
      begin

        IconName := (Iconset.Pairs[j].JSONString as TJSONString).Value;

        // See if there is a match using the number of terms we have
        if Terms.Count = 1
        then Matched := (Pos(Terms[0], IconName) > 0)
        else if Terms.Count = 2
             then Matched := (Pos(Terms[0], IconName) > 0) and (Pos(Terms[1], IconName) > 0)
             else Matched := (Pos(Terms[0], IconName) > 0) and (Pos(Terms[1], IconName) > 0) and (Pos(Terms[2], IconName) > 0);

        // Got a match
        if Matched then
        begin
          Icon := TJSONArray.Create;
          Icon.Add(IconName);

          // Need to know what set it is in so we get lookup default width, height, license, set name
          if SearchSets = 'all'
          then Icon.Add(i)
          else Icon.Add(IconSetList[i]);

          // Add in the icon data - the SVG and width/height overrides
          Icon.Add(IconSet.GetValue(IconName) as TJSONObject);

          // Save to our set that we're returning
          IconsFound.Add(Icon);
          IconCount := IconCount + 1;
        end;
      end;
    end;

    i := i + 1;
  end;

  // Return the array of results
  Result := TStringStream.Create(IconsFound.ToString);
end;

function TSystemService.SendConfirmationCode(Reason, EMailAddress, EMailSubject, EMailBody, SessionCode, APIKey: String): String;
var
  DBConn: TFDConnection;
  Query1: TFDQuery;
  DatabaseName: String;
  DatabaseEngine: String;
  ElapsedTime: TDateTime;

  AuthorizationCode: Integer;
  AuthorizationCodeString1: String;
  AuthorizationCodeString2: String;
  ExpiresAt: TDateTime;
  ApplicationName: String;

  SMTP1: TIdSMTP;
  Msg1: TIdMessage;
  Addr1: TIdEmailAddressItem;
  Html1: TIdMessageBuilderHtml;
  SMTPResult: WideString;

begin
  // Time this event
  ElapsedTime := Now;
  ExpiresAt := Now + (10 * 60) / 86400; // 10 minutes

  // Returning JSON, so flag it as such
  TXDataOperationContext.Current.Response.Headers.SetValue('content-type', 'application/json');

  if not(MainForm.MailServerAvailable)
  then raise EXDataHttpUnauthorized.Create('Mail Services Not Configured');

  // If the subject doesn't contain a place for the authorization code, then we can't really send an authorization message.
  if (Pos('{AUTHORIZATION_CODE}', EMailSubject) = 0)
  then raise EXDataHttpUnauthorized.Create('Invalid Subject: Missing {AUTHORIZATION_CODE}');

  // If the message doesn't contain a place for the authorization code, then we can't really send an authorization message.
  if (Pos('{AUTHORIZATION_CODE}', EMailBody) = 0)
  then raise EXDataHttpUnauthorized.Create('Invalid Body: Missing {AUTHORIZATION_CODE}');

  // Setup DB connection and query
  DatabaseName := MainForm.DatabaseName;
  DatabaseEngine := MainForm.DatabaseEngine;
  try
    DBSupport.ConnectQuery(DBConn, Query1, DatabaseName, DatabaseEngine);
  except on E: Exception do
    begin
      DBSupport.DisconnectQuery(DBConn, Query1);
      MainForm.mmInfo.Lines.Add('['+E.Classname+'] '+E.Message);
      raise EXDataHttpUnauthorized.Create('Internal Error: CQ');
    end;
  end;

  // Check if we've got a valid API_Key
  try
    {$Include sql\system\api_key_check\api_key_check.inc}
    Query1.ParamByName('APIKEY').AsString := LowerCase(APIKey);
    Query1.Open;
  except on E: Exception do
    begin
      DBSupport.DisconnectQuery(DBConn, Query1);
      MainForm.mmInfo.Lines.Add('['+E.Classname+'] '+E.Message);
      raise EXDataHttpUnauthorized.Create('Internal Error: AKC');
    end;
  end;
  if Query1.RecordCount = 0 then
  begin
    DBSupport.DisconnectQuery(DBConn, Query1);
    raise EXDataHttpUnauthorized.Create('API_Key was not validated.');
  end;
  ApplicationName := Query1.FieldByName('application').AsString;

  // Generate an authorization code (6 digits with a hyphen in the middle)
  AuthorizationCode := Random(999998)+1; // Don't want zero as a code!
  AuthorizationCodeString1 := RightStr('000000'+IntToStr(AuthorizationCode),6);
  AuthorizationCodeString1 := Copy(AuthorizationCodeString1,1,3)+' '+Copy(AuthorizationCodeString1,4,3);
  AuthorizationCodeString2 := RightStr('000000'+IntToStr(AuthorizationCode),6);
  AuthorizationCodeString2 := '<span>'+Copy(AuthorizationCodeString2,1,3)+'</span><span style="margin-left:5px">'+Copy(AuthorizationCodeString2,4,3)+'</span>';

  // Store the generated Authorization code for later
  try
    {$Include sql\system\authcode_insert\authcode_insert.inc}
    Query1.ParamByName('AUTHCODE').AsString := DBSupport.HashThis(IntToStr(AuthorizationCode)+EMailAddress+SessionCode);
    Query1.ParamByName('DESTINATION').AsString:= DBSupport.HashThis(EMailAddress+SessionCode);
    Query1.ParamByName('VALIDAFTER').AsDateTime := TTimeZone.local.ToUniversalTime(ElapsedTime);
    Query1.ParamByName('VALIDUNTIL').AsDateTime := TTimeZone.local.ToUniversalTime(ExpiresAt);
    Query1.ParamByName('APPLICATION').AsString := ApplicationName;
    Query1.ParamByName('VERSION').AsString := MainForm.AppVersion;
    Query1.ParamByName('IPADDRESS').AsString := TXDataOperationContext.Current.Request.RemoteIP;
    Query1.ExecSQL;
  except on E: Exception do
    begin
      DBSupport.DisconnectQuery(DBConn, Query1);
      MainForm.mmInfo.Lines.Add('['+E.Classname+'] '+E.Message);
      raise EXDataHttpUnauthorized.Create('Internal Error: ACI');
    end;
  end;

  // Finally, let's send the email
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
      Html1.Html.Add('<body>');
      Html1.Html.Add(StringReplace(EMailBody, '{AUTHORIZATION_CODE}', AuthorizationCodeString2, [rfReplaceAll, rfIgnoreCase]));
      Html1.Html.Add('</body>');
      Html1.Html.Add('</html>');
      Html1.HtmlCharSet := 'utf-8';

      Msg1 := Html1.NewMessage(nil);
      Msg1.Subject := StringReplace(EMailSUbject, '{AUTHORIZATION_CODE}', AuthorizationCodeString1, [rfReplaceAll, rfIgnoreCase]);
      Msg1.From.Text := MainForm.MailServerFrom;
      Msg1.From.Name := MainForm.MailServerName;

      Addr1 := Msg1.Recipients.Add;
      Addr1.Address := EMailAddress;

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
      Addr1.Free;  // Shouldn't have to free this?
      Msg1.Free;   // Shouldn't have to free this?
      Html1.Free;  // Should have to free this!
    end;
  except on E: Exception do
    begin
      SMTPResult := SMTPResult+'[ '+E.ClassName+' ] '+E.Message+Chr(10);
    end;
  end;
  SMTP1.Free; // Should have to free this!

  if SMTPResult = ''
  then Result := 'Sent'
  else Result := 'Send Failed: '+SMTPResult;

  // Keep track of endpoint history
  try
    {$Include sql\system\endpoint_history_insert\endpoint_history_insert.inc}
//    Query1.ParamByName('PERSONID').AsInteger := User.Claims.Find('usr').AsInteger;
    Query1.ParamByName('PERSONID').AsInteger := 1;
    Query1.ParamByName('ENDPOINT').AsString := 'SystemService.SendConfirmationCode';
    Query1.ParamByName('ACCESSED').AsDateTime := TTimeZone.local.ToUniversalTime(ElapsedTime);
    Query1.ParamByName('IPADDRESS').AsString := TXDataOperationContext.Current.Request.RemoteIP;
    Query1.ParamByName('APPLICATION').AsString := ApplicationName;
    Query1.ParamByName('VERSION').AsString := MainForm.AppVersion;
    Query1.ParamByName('DATABASENAME').AsString := DatabaseName;
    Query1.ParamByName('DATABASEENGINE').AsString := DatabaseEngine;
    Query1.ParamByName('EXECUTIONMS').AsInteger := MillisecondsBetween(Now,ElapsedTime);
    Query1.ParamByName('DETAILS').AsString := '['+Reason+': '+EMailAddress+']';
    Query1.ExecSQL;
  except on E: Exception do
    begin
      MainForm.mmInfo.Lines.Add('['+E.Classname+'] '+E.Message);
      raise EXDataHttpUnauthorized.Create('Internal Error: EHI');
    end;
  end;

  // All Done
  try
    DBSupport.DisconnectQuery(DBConn, Query1);
  except on E: Exception do
    begin
      MainForm.mmInfo.Lines.Add('['+E.Classname+'] '+E.Message);
      raise EXDataHttpUnauthorized.Create('Internal Error: DQ');
    end;
  end;
end;

function TSystemService.SendEMail(MailSubject, MailBody: String; Reason: String): String;
var
  DBConn: TFDConnection;
  Query1: TFDQuery;
  DatabaseName: String;
  DatabaseEngine: String;
  ElapsedTime: TDateTime;

  User: IUserIdentity;
  JWT: String;

  SMTP1: TIdSMTP;
  Msg1: TIdMessage;
  Addr1: TIdEmailAddressItem;
  Addr2: TidEMailAddressItem;
  Html1: TIdMessageBuilderHtml;
  SMTPResult: WideString;

begin
  // Time this event
  ElapsedTime := Now;

  // Returning JSON, so flag it as such
  TXDataOperationContext.Current.Response.Headers.SetValue('content-type', 'application/json');

  if not(MainForm.MailServerAvailable)
  then raise EXDataHttpUnauthorized.Create('Mail Services Not Configured');

  // Get data from the JWT
  User := TXDataOperationContext.Current.Request.User;
  JWT := TXDataOperationContext.Current.Request.Headers.Get('Authorization');
  if (User = nil) then raise EXDataHttpUnauthorized.Create('Missing authentication');

  // Setup DB connection and query
  DatabaseName := MainForm.DatabaseName;
  DatabaseEngine := MainForm.DatabaseEngine;
  try
    DBSupport.ConnectQuery(DBConn, Query1, DatabaseName, DatabaseEngine);
  except on E: Exception do
    begin
      DBSupport.DisconnectQuery(DBConn, Query1);
      MainForm.mmInfo.Lines.Add('['+E.Classname+'] '+E.Message);
      raise EXDataHttpUnauthorized.Create('Internal Error: CQ');
    end;
  end;

  // Send the email
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
      Html1.Html.Add(MailBody);
      Html1.HtmlCharSet := 'utf-8';

      Msg1 := Html1.NewMessage(nil);
      Msg1.Subject := MailSubject;
      Msg1.From.Text := MainForm.MailServerFrom;
      Msg1.From.Name := MainForm.MailServerName;

      Addr1 := Msg1.Recipients.Add;
      Addr1.Address := User.Claims.Find('eml').AsString;

      Addr2 := Msg1.BCCList.Add;
      Addr2.Address := MainForm.MailServerFrom;

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
      Addr1.Free;  // Shouldn't have to free this?
      Msg1.Free;   // Shouldn't have to free this?
      Html1.Free;  // Should have to free this!
    end;
  except on E: Exception do
    begin
      SMTPResult := SMTPResult+'[ '+E.ClassName+' ] '+E.Message+Chr(10);
    end;
  end;
  SMTP1.Free; // Should have to free this!

  if SMTPResult = ''
  then Result := 'Sent'
  else Result := 'Send Failed: '+SMTPResult;

  // Keep track of endpoint history
  try
    {$Include sql\system\endpoint_history_insert\endpoint_history_insert.inc}
    Query1.ParamByName('PERSONID').AsInteger := User.Claims.Find('usr').AsInteger;
    Query1.ParamByName('ENDPOINT').AsString := 'SystemService.SendEMail';
    Query1.ParamByName('ACCESSED').AsDateTime := TTimeZone.local.ToUniversalTime(ElapsedTime);
    Query1.ParamByName('IPADDRESS').AsString := TXDataOperationContext.Current.Request.RemoteIP;
    Query1.ParamByName('APPLICATION').AsString := User.Claims.Find('app').AsString;
    Query1.ParamByName('VERSION').AsString := MainForm.AppVersion;
    Query1.ParamByName('DATABASENAME').AsString := DatabaseName;
    Query1.ParamByName('DATABASEENGINE').AsString := DatabaseEngine;
    Query1.ParamByName('EXECUTIONMS').AsInteger := MillisecondsBetween(Now,ElapsedTime);
    Query1.ParamByName('DETAILS').AsString := '['+Reason+']';
    Query1.ExecSQL;
  except on E: Exception do
    begin
      MainForm.mmInfo.Lines.Add('['+E.Classname+'] '+E.Message);
      raise EXDataHttpUnauthorized.Create('Internal Error: EHI');
    end;
  end;

  // All Done
  try
    DBSupport.DisconnectQuery(DBConn, Query1);
  except on E: Exception do
    begin
      MainForm.mmInfo.Lines.Add('['+E.Classname+'] '+E.Message);
      raise EXDataHttpUnauthorized.Create('Internal Error: DQ');
    end;
  end;
end;


function TSystemService.VerifyConfirmationCode(EMailAddress, SessionCode, ConfirmationCode, APIKey, Reason: String): String;
var
  DBConn: TFDConnection;
  Query1: TFDQuery;
  DatabaseName: String;
  DatabaseEngine: String;
  ElapsedTime: TDateTime;
  ApplicationName: String;

begin
  // Time this event
  ElapsedTime := Now;

  // Returning JSON, so flag it as such
  TXDataOperationContext.Current.Response.Headers.SetValue('content-type', 'application/json');

  if not(MainForm.MailServerAvailable)
  then raise EXDataHttpUnauthorized.Create('Mail Services Not Configured');

  // Setup DB connection and query
  DatabaseName := MainForm.DatabaseName;
  DatabaseEngine := MainForm.DatabaseEngine;
  try
    DBSupport.ConnectQuery(DBConn, Query1, DatabaseName, DatabaseEngine);
  except on E: Exception do
    begin
      DBSupport.DisconnectQuery(DBConn, Query1);
      MainForm.mmInfo.Lines.Add('['+E.Classname+'] '+E.Message);
      raise EXDataHttpUnauthorized.Create('Internal Error: CQ');
    end;
  end;

  // Check if we've got a valid API_Key
  try
    {$Include sql\system\api_key_check\api_key_check.inc}
    Query1.ParamByName('APIKEY').AsString := LowerCase(APIKey);
    Query1.Open;
  except on E: Exception do
    begin
      DBSupport.DisconnectQuery(DBConn, Query1);
      MainForm.mmInfo.Lines.Add('['+E.Classname+'] '+E.Message);
      raise EXDataHttpUnauthorized.Create('Internal Error: AKC');
    end;
  end;
  if Query1.RecordCount = 0 then
  begin
    DBSupport.DisconnectQuery(DBConn, Query1);
    raise EXDataHttpUnauthorized.Create('API_Key was not validated.');
  end;
  ApplicationName := Query1.FieldByName('application').AsString;

  // See if we can find a match
  try
    {$Include sql\system\authcode_verify\authcode_verify.inc}
    Query1.ParamByName('AUTHCODE').AsString := DBSupport.HashThis(ConfirmationCode+EMailAddress+SessionCode);
    Query1.ParamByName('DESTINATION').AsString:= DBSupport.HashThis(EMailAddress+SessionCode);
    Query1.ParamByName('APPLICATION').AsString := ApplicationName;
    Query1.ParamByName('VERSION').AsString := MainForm.AppVersion;
    Query1.ParamByName('IPADDRESS').AsString := TXDataOperationContext.Current.Request.RemoteIP;
    Query1.Open;
  except on E: Exception do
    begin
      DBSupport.DisconnectQuery(DBConn, Query1);
      MainForm.mmInfo.Lines.Add('['+E.Classname+'] '+E.Message);
      raise EXDataHttpUnauthorized.Create('Internal Error: AKV');
    end;
  end;

  if Query1.RecordCount = 1
  then Result := 'Success'
  else Result := 'Fail';

  // Validated?  What else do we need to do?
  if (Result = 'Success') and (Copy(Reason,1,6) = 'EMAIL-') then
  begin
    try
      {$Include sql\system\contact_email_update\contact_email_update.inc}
      Query1.ParamByName('EMAILADDRESS').AsString := EMailAddress;
      Query1.ParamByName('PERSONID').AsInteger := StrToInt(Copy(Reason,7,length(Reason) - 6));
      Query1.ExecSQL;
    except on E: Exception do
      begin
        DBSupport.DisconnectQuery(DBConn, Query1);
        MainForm.mmInfo.Lines.Add('['+E.Classname+'] '+E.Message);
        raise EXDataHttpUnauthorized.Create('Internal Error: CEU');
      end;
    end;

    if Query1.RowsAffected <> 1 then
    begin
      DBSupport.DisconnectQuery(DBConn, Query1);
      raise EXDataHttpUnauthorized.Create('Internal Error: CEU');
    end;

  end;


  // Keep track of endpoint history
  try
    {$Include sql\system\endpoint_history_insert\endpoint_history_insert.inc}
//    Query1.ParamByName('PERSONID').AsInteger := User.Claims.Find('usr').AsInteger;
    Query1.ParamByName('PERSONID').AsInteger := 1;
    Query1.ParamByName('ENDPOINT').AsString := 'SystemService.VerifyConfirmationCode';
    Query1.ParamByName('ACCESSED').AsDateTime := TTimeZone.local.ToUniversalTime(ElapsedTime);
    Query1.ParamByName('IPADDRESS').AsString := TXDataOperationContext.Current.Request.RemoteIP;
    Query1.ParamByName('APPLICATION').AsString := ApplicationName;
    Query1.ParamByName('VERSION').AsString := MainForm.AppVersion;
    Query1.ParamByName('DATABASENAME').AsString := DatabaseName;
    Query1.ParamByName('DATABASEENGINE').AsString := DatabaseEngine;
    Query1.ParamByName('EXECUTIONMS').AsInteger := MillisecondsBetween(Now,ElapsedTime);
    Query1.ParamByName('DETAILS').AsString := '['+EMailAddress+': '+Result+']';
    Query1.ExecSQL;
  except on E: Exception do
    begin
      MainForm.mmInfo.Lines.Add('['+E.Classname+'] '+E.Message);
      raise EXDataHttpUnauthorized.Create('Internal Error: EHI');
    end;
  end;

  // All Done
  try
    DBSupport.DisconnectQuery(DBConn, Query1);
  except on E: Exception do
    begin
      MainForm.mmInfo.Lines.Add('['+E.Classname+'] '+E.Message);
      raise EXDataHttpUnauthorized.Create('Internal Error: DQ');
    end;
  end;

end;

initialization
  RegisterServiceType(TSystemService);

end.
