unit SystemService;

interface

uses
  System.Classes,
  XData.Security.Attributes,
  XData.Service.Common;

type
  [ServiceContract]
  ISystemService = interface(IInvokable)
    ['{F9CC965F-B6F1-4D38-A50A-E271705E9FCB}']

    ///  <summary>
    ///    XData Application Information
    ///  </summary>
    ///  <remarks>
    ///    Returns JSON that includes information about the currently running application.
    ///  </remarks>
    ///  <param name="TZ">
    ///    The TimeZone of the connecting client.  This is used in determining what
    ///    adjustments to make when displaying dates and times on reports, or where
    ///    similar data needs to be converted to a local context.  This uses IANA
    ///    TimeZone names.  If an invalid TimeZone is provided, the JSON object for
    ///    Current Time (Client) will indicate as much.  Here are some examples.
    ///    - Pacific Standard Time
    ///    - America/New_York
    ///    - Europe/Paris
    ///    - EET
    ///    - UTC
    ///    - GMT
    ///    - EST
    ///    - PST8PDT
    ///  </param>
    [HttpGet] function Info(TZ: String):TStream;

    ///  <summary>
    ///    Login to XData Server
    ///  </summary>
    ///  <remarks>
    ///    If login is successful, a JWT will be returned.
    ///  </remarks>
    ///  <param name="Login_ID">
    ///    Login_ID can be any of the contact entries that have been marked as login_ok,
    ///    which would typically be just the email address but could also include phone
    ///    numbers or other values.
    ///  </param>
    ///  <param name="Password">
    ///    Password corresponding to the username.
    ///  </param>
    ///  <param name="API_Key">
    ///    An application-level API key that has been provided for your use.
    ///  </param>
    ///  <param name="TZ">
    ///    The TimeZone of the connecting client.  This is used in determining what
    ///    adjustments to make when displaying dates and times on reports, or where
    ///    similar data needs to be converted to a local context.  This uses IANA
    ///    TimeZone names.  If an invalid TimeZone is provided, the JSON object for
    ///    Current Time (Client) will indicate as much.  Here are some examples.
    ///    - Pacific Standard Time
    ///    - America/New_York
    ///    - Europe/Paris
    ///    - EET
    ///    - UTC
    ///    - GMT
    ///    - EST
    ///    - PST8PDT
    ///  </param>
    ///  <param name="IPAddress">
    ///    Client public IP Address.
    ///  </param>
    ///  <param name="IPLocation">
    ///    Client geolocation based on IP.
    ///  </param>
    ///  <param name="DeviceInfo">
    ///    UA-supplied client device.
    ///  </param>
    ///  <param name="BrowserInfo">
    ///    UA-supplied client browser.
    ///  </param>
    ///  <param name="ActionSession">
    ///    Session identifier unique to the user - just an encoded Unix timestamp.
    ///  </param>
    ///  <param name="ActionLog">
    ///    Client action log. Just a text log.
    ///  </param>

    [HttpPost] function Login(Login_ID, Password, API_Key, TZ, IPAddress, IPLocation, DeviceInfo, BrowserInfo, ActionSession, ActionLog: String):TStream;

    ///  <summary>
    ///    Logout - revoke the JWT.
    ///  </summary>
    ///  <remarks>
    ///    The JWT issued by the Login endpoint is good for a set period of time.  This will revoke
    ///    the JWT, making it invalid immediately rather than when it expires after a period of time.
    ///  </remarks>
    ///  <param name="ActionSession">
    ///    Session identifier unique to the user - just an encoded Unix timestamp.
    ///  </param>
    ///  <param name="ActionLog">
    ///    Client action log. Just a text log.
    ///  </param>
    [HttpPost] function Logout(ActionSession: String; ActionLog: String):TStream;

    ///  <summary>
    ///    Logout - revoke all tokens corresponding to the currently logged in account.
    ///  </summary>
    ///  <remarks>
    ///    This is used when the user wishes to logout of all accounts simultaneously.  This may
    ///    be useful if unauthorized access or other suspicious activity has been detected.
    ///  </remarks>
    ///  <param name="ActionSession">
    ///    Session identifier unique to the user - just an encoded Unix timestamp.
    ///  </param>
    ///  <param name="ActionLog">
    ///    Client action log. Just a text log.
    ///  </param>
    [HttpPost] function LogoutAll(ActionSession: String; ActionLog: String):TStream;

    ///  <summary>
    ///    Renew a previously issued JWT.
    ///  </summary>
    ///  <remarks>
    ///    The JWT issued by the Login endpoint is good for a set period of time.
    ///    This endpoint will re-issue a new JWT with the same claims for another period of time.
    ///  </remarks>
    ///  <param name="ActionSession">
    ///    Session identifier unique to the user - just an encoded Unix timestamp.
    ///  </param>
    ///  <param name="ActionLog">
    ///    Client action log. Just a text log.
    ///  </param>
    [Authorize] [HttpPost] function Renew(ActionSession: String; ActionLog: String):TStream;

    ///  <summary>
    ///    List of Icon Sets that are available for search and retrieval
    ///  </summary>
    ///  <remarks>
    ///    Returns a JSON array that includes the following.
    ///    - Name of Icon Set
    ///    - License Information
    ///    - Count of Icons included in Set
    ///    - Default Icon Width for Set
    ///    - Default Icon Height for Set
    ///
    ///    The order of the array should be used to identify which sets are to be included or excluded when a search is requested.
    ///  </remarks>
    [HttpGet] function AvailableIconSets:TStream;

    ///  <summary>
    ///    Performs a search for icons, returing whatever icons were found as a JSON array.
    ///  </summary>
    ///  <remarks>
    ///    The returned array is a JSON list of icons, including the SVG parts needed to build the icon.
    ///  </remarks>
    ///  <param name="SearchTerms">
    ///    Up to three terms will be used in conducting the search.  Any more that are passed in will be ignored.
    ///  </param>
    ///  <param name="SearchSets">
    ///    A comma-separated list of Icon Sets to search, where the number indicates the position in the array from AvailableIconSets.  A value of 'all' is also accepted, as this is likely the default search choice much of the time.
    ///  </param>
    ///  <param name="Results">
    ///    Indicates how many icons are to be returned.  If conducting searches while someone is typing, this should be a much smaller number than if a more deliberate search is being performed.
    ///  </param>
    [HttpGet] function SearchIconSets(SearchTerms: String; SearchSets:String; Results:Integer):TStream;


    [HttpGet] function SearchFontAwesome(Query: String):TStream;


    ///  <summary>
    ///    Allow user to change password
    ///  </summary>
    ///  <remarks>
    ///    Changes the password hash for the ID corresponding to the JWT
    ///  </remarks>
    ///  <param name="OldPassword">
    ///    Must match existing password on server.
    ///  </param>
    ///  <param name="NewPassword">
    ///    Complexity check done on client, so here just a character length check is done.
    ///  </param>
    ///  <param name="PasswordTest">
    ///    A calculation intended to discourage changing passwords via the Swagger interface.
    ///  </param>

    [Authorize] [HttpGet] function ChangePassword(OldPassword: String; NewPassword: String; PasswordTest: String):String;

    ///  <summary>
    ///    Allow user to change account name
    ///  </summary>
    ///  <remarks>
    ///    Changes the account name in the person and contact tables.
    ///  </remarks>
    ///  <param name="AccountName">
    ///    Must be uppercase.
    ///  </param>

    [Authorize] [HttpGet] function ChangeAccount(AccountName: String):String;

    ///  <summary>
    ///    Checks if an account is unique.  Used when registering or updating account information.
    ///  </summary>
    ///  <remarks>
    ///    Just returns true or false based on whether the account name is available.
    ///  </remarks>
    ///  <param name="UniqueAccount">
    ///    Typically between 4-50 characters, no symbols, etc.
    ///  </param>

    [HttpGet] function CheckUniqueAccount(UniqueAccount: String):Boolean;

    ///  <summary>
    ///    Checks if an e-mail address is unique.  Used when registering or updating account information.
    ///  </summary>
    ///  <remarks>
    ///    Just returns true or false based on whether the e-mail address is available.
    ///  </remarks>
    ///  <param name="UniqueEMail">
    ///    Should be valid e-mail (client checks this, not checked here).
    ///  </param>

    [HttpGet] function CheckUniqueEMail(UniqueEMail: String):Boolean;

    ///  <summary>
    ///    Sends a six-digit confirmation code to an E-Mail address to help with E-Mail validation.
    ///  </summary>
    ///  <remarks>
    ///    This requires no authorization so that it can be used also during registration validation.
    ///  </remarks>
    ///  <param name="Reason">
    ///    Why is this being sent?  Change E-Mail?  New Registration? Something else?
    ///  </param>
    ///  <param name="EMailAddress">
    ///    Where we're sending the confirmation
    ///  </param>
    ///  <param name="EMailSubject">
    ///    Where we're sending the confirmation
    ///  </param>
    ///  <param name="EMailBody">
    ///    Where we're sending the confirmation
    ///  </param>
    ///  <param name="SessionCode">
    ///    Used to generate a unique hash.  Must be the same code used to verify.
    ///  </param>
    ///  <param name="APIKey">
    ///    Intended to prevent casual use of this, where it could in theory be abused, sending out
    ///    confirmation E-Mails to random accounts.
    ///  </param>

    [HttpPost] function SendConfirmationCode(Reason, EMailAddress, EMailSubject, EMailBody, SessionCode, APIKey: String):String;

    ///  <summary>
    ///    Checks if the six-digit confirmation code is one that has been sent to the same e-mail address recently.
    ///  </summary>
    ///  <remarks>
    ///    This requires no authorization so that it can be used also during registration validation.
    ///  </remarks>
    ///  <param name="EMailAddress">
    ///    Where the confirmation code was sent.
    ///  </param>
    ///  <param name="SessionCode">
    ///    Used to generate a unique hash.  Must be the same code used to send request.
    ///  </param>
    ///  <param name="ConfirmationCode">
    ///    The code to check.
    ///  </param>
    ///  <param name="APIKey">
    ///    Intended to prevent casual use of this, where it could in theory be abused, sending out
    ///    confirmation E-Mails to random accounts.
    ///  </param>
    ///  <param name="Reason">
    ///    After confirmation, is there anything else we want to do immediately?
    ///      EMAIL-ID - Update email address for this ID
    ///  </param>


    [HttpGet] function VerifyConfirmationCode(EMailAddress, SessionCode, ConfirmationCode, APIKey, Reason: String):String;


    ///  <summary>
    ///    Sends an e-mail to the user's account
    ///  </summary>
    ///  <remarks>
    ///    Nothing fancy - e-mai is sent to the user's JWT-supplied e-mail, so not much chance of abuse here.
    ///  </remarks>
    ///  <param name="MailSubject">
    ///    Subject of E-Mail
    ///  </param>
    ///  <param name="MailBody">
    ///    Body of E-Mail
    ///  </param>
    ///  <param name="Reason">
    ///    Why is this email being sent?  Action log, chart, etc.
    ///  </param>

    [Authorize] [HttpPost] function SendEMail(MailSubject: String; MailBody: String; Reason: String):String;
  end;

implementation

initialization
  RegisterServiceType(TypeInfo(ISystemService));

end.
