// [query] stats/today_sessions

if (MainForm.DatabaseEngine = 'sqlite') then
begin

  with Query1 do
  begin

    SQL.Clear;
    SQL.Add('SELECT '+
            '  coalesce(count(distinct session_id),0) sessions '+
            'FROM '+
            '  action_history '+
            'WHERE '+
            '  ((log_start = 1) or (log_start = 2)) '+
            '  and (session_start >= :SESSIONSTART); '
           );

  end;
end;
