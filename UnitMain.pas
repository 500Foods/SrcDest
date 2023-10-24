unit UnitMain;

interface

uses
  System.SysUtils,
  System.Classes,

  JS,
  Web,

  WEBLib.Graphics,
  WEBLib.Controls,
  WEBLib.Forms,
  WEBLib.Miletus,
  WEBLib.Dialogs,
  Vcl.Controls,
  WEBLib.WebCtrls,
  Vcl.StdCtrls,
  WEBLib.StdCtrls;

type
  TFormMain = class(TMiletusForm)
    WebHTMLDiv1: TWebHTMLDiv;
    WebButton1: TWebButton;
  private
    { Private declarations }
  public
    { Public declarations }
  end;

var
  FormMain: TFormMain;

implementation

{$R *.dfm}

initialization
  RegisterClass(TFormMain);

end.