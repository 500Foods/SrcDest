// [query] stats/today_events

if (MainForm.DatabaseEngine = 'sqlite') then
begin

  with Query1 do
  begin

    SQL.Clear;
    SQL.Add('SELECT '+
            '  coalesce(sum(log_events),0) events '+
            'FROM '+
            '  action_history '+
            'WHERE '+
            '  (session_start >= :SESSIONSTART); '
           );

  end;
end;
