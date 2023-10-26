unit PersonService;

interface

uses
  System.Classes,
  XData.Security.Attributes,
  XData.Service.Common;

type
  [ServiceContract]
  IPersonService = interface(IInvokable)
    ['{B3F998DC-587F-442D-8101-97819329D6C9}']

    ///  <summary>
    ///    Return directory dataset.
    ///  </summary>
    ///  <remarks>
    ///    All person records are returned, along with their contact information.
    ///  </remarks>
    ///  <param name="format">
    ///     Valid dataset formats for this request include the following.
    ///    - CSV - Comma Separated Values Format, includes header row
    ///    - PLAIN - No delimiters or header row
    ///    - FIREDAC: FireDAC JSON Format
    ///    - BINARY: FireDAC Binary Format
    ///    - XML: FireDAC XML Format
    ///    - JSON: Simple JSON Format
    ///  </param>
    [Authorize] [HttpGet] function Directory(Format: String): TStream;

    ///  <summary>
    ///    Return user profile information.
    ///  </summary>
    ///  <remarks>
    ///    The profile information is returned as JSONbased on the person that is
    ///    logged in - the JWT is used to determine the person_id.
    ///  </remarks>
    [Authorize] [HttpGet] function Profile: TStream;

    ///  <summary>
    ///    Return a specific Action Log.
    ///  </summary>
    ///  <remarks>
    ///    This returns the composite action log for a given person and session.
    ///    The session id can be found in the Profile data.  Composite refers to
    ///    how the action_history table stores multiple records for each session.
    ///    This endpoint returns them combined into one text block.
    ///    Note that if the Person doesn't match the JWT, then access may be
    ///    denied unless a role permitting such access has been granted.
    ///  </remarks>
    [Authorize] [HttpGet] function ActionLog(Person: Integer; Session: String): TStream;

    ///  <summary>
    ///    Update basic person information, including names, birthdate, and description fields.
    ///  </summary>
    ///  <remarks>
    ///    Normally this would be called only by the person themselves, but accommmodations
    ///    have been made via the role mechanism to accept changes from another account.
    ///  </remarks>

    [Authorize] [HttpPost] function UpdatePerson(PersonID: Integer; FirstName, MiddleName, LastName, Birthdate, Description: String):String;


    ///  <summary>
    ///    Updates the user-supplied list of links.  This maps directly into the contacts table.
    ///  </summary>
    ///  <remarks>
    ///    Normally this would be called only by the person themselves, but accommmodations
    ///    have been made via the role mechanism to accept changes from another account.
    ///  </remarks>

    [Authorize] [HttpPost] function UpdatePersonLinks(PersonID: Integer; Links: String):String;

    ///  <summary>
    ///    Update basic person information, including names, birthdate, and description fields.
    ///  </summary>
    ///  <remarks>
    ///    Normally this would be called only by the person themselves, but accommmodations
    ///    have been made via the role mechanism to accept changes from another account.
    ///  </remarks>

    [Authorize] [HttpPost] function UpdatePersonPhoto(PersonID: Integer; Photo: String):String;


  end;


implementation

initialization
  RegisterServiceType(TypeInfo(IPersonService));

end.
