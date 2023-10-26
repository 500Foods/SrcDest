// [query] stats/today_errors

if (MainForm.DatabaseEngine = 'sqlite') then
begin

  with Query1 do
  begin

    SQL.Clear;
    SQL.Add('SELECT '+
            '  coalesce(sum(log_errors),0) errors '+
            'FROM '+
            '  action_history '+
            'WHERE '+
            '  (session_start >= :SESSIONSTART); '
           );

  end;
end;
