// [query] stats/today_changes

if (MainForm.DatabaseEngine = 'sqlite') then
begin

  with Query1 do
  begin

    SQL.Clear;
    SQL.Add('SELECT '+
            '  coalesce(sum(log_changes),0) changes '+
            'FROM '+
            '  action_history '+
            'WHERE '+
            '  (session_start >= :SESSIONSTART); '
           );

  end;
end;
