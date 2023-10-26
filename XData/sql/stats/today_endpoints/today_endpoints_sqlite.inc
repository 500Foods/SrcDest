// [query] stats/today_endpoints

if (MainForm.DatabaseEngine = 'sqlite') then
begin

  with Query1 do
  begin

    SQL.Clear;
    SQL.Add('SELECT '+
            '  coalesce(count(*),0) endpoints '+
            'FROM '+
            '  endpoint_history '+
            'WHERE '+
            '  (accessed >= :SESSIONSTART); '
           );

  end;
end;
