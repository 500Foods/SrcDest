// [table] api_key

TableName := 'api_key';

if (DatabaseEngine = 'sqlite') then
begin

  with Query1 do
  begin

    // Check if the table exists
    SQL.Clear;
    SQL.Add('select count(*) records from '+TableName+';');
    try
      Open;
      mmInfo.Lines.Add('...'+TableName+' ('+IntToStr(FieldByName('records').AsInteger)+' records)');

    except on E:Exception do
      begin
        mmInfo.Lines.Add('...'+TableName+' (CREATE)');
        SQL.Clear;
        SQL.Add('create table if not exists '+TableName+' ( '+
                '  api_key                    text        NOT NULL, '+
                '  last_modified              text        NOT NULL, '+
                '  last_modifier              integer     NOT NULL, '+
                '  valid_after                text        NOT NULL, '+
                '  valid_until                text        NOT NULL, '+
                '  application                text        NOT NULL, '+
                '  CONSTRAINT constraint_name PRIMARY KEY (api_key) '+
                ');'
           );
        ExecSQL;

        // Try it again
        SQL.Clear;
        SQL.Add('select count(*) records from '+TableName+';');
        Open;
      end;
    end;

    // Populate empty table with sample data
    if (FieldByName('records').AsInteger = 0) then
    begin
      mmInfo.Lines.Add('...'+TableName+' (POPULATE)');
      SQL.Clear;

      // Default API Keys for testing - set to expire after three months
      SQL.Add('insert into '+TableName+' values( "testing", Datetime("now"), 0, Datetime("now"), Datetime("now", "+3 months"), "Testing XData Template Demo Data" );');
      SQL.Add('insert into '+TableName+' values( "{2DF239F0-8A9D-4531-9BC2-AB911CB40C4E}", Datetime("now"), 0, Datetime("now"), Datetime("now", "+10 years"), "blaugment" );');

      ExecSQL;
    end;
  end;
end;

