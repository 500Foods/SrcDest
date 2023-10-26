// [query] actions_log

if (MainForm.DatabaseEngine = 'sqlite') then
begin

  with Query1 do
  begin

    SQL.Clear;
    SQL.Add('SELECT '+
            '  person_id, '+
            '  session_start, '+
            '  session_id, '+
            '  group_concat(actions, char(10)) as actions '+
            'FROM '+
            '  action_history '+
            'WHERE '+
            '  (person_id = :PERSONID) '+
            '  and (session_id = :SESSIONID) '+
            'GROUP BY '+
            '  person_id, '+
            '  session_id, '+
            '  session_start '+
            'ORDER BY '+
            '  session_recorded; '
           );

  end;
end;

