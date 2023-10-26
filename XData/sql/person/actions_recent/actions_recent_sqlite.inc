// [query] actions_recent

if (MainForm.DatabaseEngine = 'sqlite') then
begin

  with Query1 do
  begin

    SQL.Clear;
    SQL.Add('SELECT '+
            '  session_start, '+
            '  session_id, '+
            '  person_id, '+
            '  sum(log_status) log_status, '+
            '  sum(log_errors) log_errors, '+
            '  sum(log_events) log_events, '+
            '  sum(log_changes) log_changes, '+
            '  sum(log_start) log_start '+
            'FROM '+
            '  action_history '+
            'WHERE '+
            '  (person_id = :PERSONID) '+
            '  and (session_start > DateTime("now", "-30 days")) '+
            'GROUP BY '+
            '  person_id, '+
            '  session_id, '+
            '  session_start '+
            'ORDER BY '+
            '  session_start DESC '+
            'LIMIT 100;'
           );

  end;
end;
