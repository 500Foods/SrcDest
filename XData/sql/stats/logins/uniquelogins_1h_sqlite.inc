// [query] stats/logins/unique_1h

if (MainForm.DatabaseEngine = 'sqlite') then
begin

  with Query1 do
  begin

    SQL.Clear;
    SQL.Add('SELECT '+
            '  strftime("%Y-%m-%d %H:00:00", DateTime(session_start, :TZOFFSET || " minutes")) period, '+
            '  count(distinct person_id) logins '+
            'FROM '+
            '  action_history '+
            'WHERE '+
            '  (session_start >= :SESSIONSTART) '+
            '  and (session_start <= :SESSIONEND) '+
            'GROUP BY '+
            '  period '+
            'ORDER BY '+
            '  period;'
           );

  end;
end;
