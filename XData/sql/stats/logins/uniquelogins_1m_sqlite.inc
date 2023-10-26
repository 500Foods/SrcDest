// [query] stats/logins/unique_15m

if (MainForm.DatabaseEngine = 'sqlite') then
begin

  with Query1 do
  begin

    SQL.Clear;
    SQL.Add('SELECT '+
            '  strftime("%Y-%m-01", DateTime(session_start, :TZOFFSET || " minutes")) period, '+
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
