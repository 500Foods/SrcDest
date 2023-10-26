// [query] stats/today_logins

if (MainForm.DatabaseEngine = 'sqlite') then
begin

  with Query1 do
  begin

    SQL.Clear;
    SQL.Add('SELECT '+
            '  coalesce(count(distinct person_id),0) logins '+
            'FROM '+
            '  action_history '+
            'WHERE '+
//            '  ((log_start = 1) or (log_start = 2)) '+
            '  (session_start >= :SESSIONSTART); '
           );

  end;
end;
