// [query] stats/logins/logins_1w

if (MainForm.DatabaseEngine = 'sqlite') then
begin

  with Query1 do
  begin

    SQL.Clear;
    SQL.Add('SELECT '+
            '  strftime("%Y-W", DateTime(session_start, :TZOFFSET || " minutes")) || substr("00" || (cast(strftime("%W", DateTime(session_start, :TZOFFSET || " minutes")) as int) +0), -2) period, '+
            '  count(distinct session_id) logins '+
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
