object MainForm: TMainForm
  Left = 0
  Top = 0
  Caption = 'SrcDest XData Server'
  ClientHeight = 502
  ClientWidth = 743
  Color = clBtnFace
  Font.Charset = DEFAULT_CHARSET
  Font.Color = clWindowText
  Font.Height = -11
  Font.Name = 'Tahoma'
  Font.Style = []
  OldCreateOrder = False
  OnCreate = FormCreate
  OnShow = FormShow
  DesignSize = (
    743
    502)
  PixelsPerInch = 96
  TextHeight = 13
  object mmInfo: TMemo
    Left = 8
    Top = 40
    Width = 727
    Height = 454
    Anchors = [akLeft, akTop, akRight, akBottom]
    DoubleBuffered = True
    ParentDoubleBuffered = False
    ReadOnly = True
    ScrollBars = ssVertical
    TabOrder = 0
  end
  object btStart: TButton
    Left = 8
    Top = 8
    Width = 75
    Height = 25
    Caption = 'Start'
    TabOrder = 1
    OnClick = btStartClick
  end
  object btStop: TButton
    Left = 90
    Top = 8
    Width = 75
    Height = 25
    Caption = 'Stop'
    TabOrder = 2
    OnClick = btStopClick
  end
  object btSwagger: TButton
    Left = 171
    Top = 8
    Width = 75
    Height = 25
    Caption = 'Swagger'
    TabOrder = 3
    OnClick = btSwaggerClick
  end
  object btRedoc: TButton
    Left = 252
    Top = 8
    Width = 75
    Height = 25
    Caption = 'Redoc'
    TabOrder = 4
    OnClick = btRedocClick
  end
  object DBConn: TFDConnection
    FormatOptions.AssignedValues = [fvMaxStringSize]
    Left = 520
    Top = 48
  end
  object FDPhysSQLiteDriverLink1: TFDPhysSQLiteDriverLink
    Left = 520
    Top = 160
  end
  object Query1: TFDQuery
    Connection = DBConn
    Left = 520
    Top = 104
  end
  object tmrStart: TTimer
    Enabled = False
    OnTimer = tmrStartTimer
    Left = 440
    Top = 240
  end
  object tmrInit: TTimer
    Enabled = False
    OnTimer = tmrInitTimer
    Left = 352
    Top = 240
  end
  object NetHTTPClient1: TNetHTTPClient
    Asynchronous = False
    ConnectionTimeout = 10000
    ResponseTimeout = 10000
    HandleRedirects = False
    AllowCookies = False
    UserAgent = 'Embarcadero URI Client/1.0'
    SecureProtocols = [SSL3, TLS12]
    OnValidateServerCertificate = NetHTTPClient1ValidateServerCertificate
    OnRequestError = NetHTTPClient1RequestError
    Left = 184
    Top = 424
  end
end
