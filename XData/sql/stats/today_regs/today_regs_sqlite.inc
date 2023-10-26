// [query] stats/today_registrations

if (MainForm.DatabaseEngine = 'sqlite') then
begin

  with Query1 do
  begin

    SQL.Clear;
    SQL.Add('SELECT '+
            '  coalesce(count(*),0) registrations '+
            'FROM '+
            '  person '+
            'WHERE '+
            '  (member_since >= :SESSIONSTART); '
           );

  end;
end;
