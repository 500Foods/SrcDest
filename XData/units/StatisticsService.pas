unit StatisticsService;

interface

uses
  System.Classes,
  XData.Security.Attributes,
  XData.Service.Common;

type
  [ServiceContract]
  IStatisticsService = interface(IInvokable)
    ['{E2E7896E-5ACC-4D2A-86AD-6241F9D10B88}']

    [Authorize] [HttpGet] function Today:TStream;
    [Authorize] [HttpGet] function StatQuery(Stat: String; Aggregate: Integer; PeriodStart: String; PeriodEnd: String):TStream;
  end;

implementation

initialization
  RegisterServiceType(TypeInfo(IStatisticsService));

end.
