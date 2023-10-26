// [query] action_history_insert

if (DatabaseEngine = 'sqlite') then
begin

  with Query1 do
  begin

    SQL.Clear;
    SQL.Add('insert into '+
            '  action_history '+
            '    (person_id, ip_address, application, version, session_id, session_start, session_recorded, log_status, log_events, log_changes, log_errors, log_start, actions) '+
            'values( '+
            '  :PERSONID, '+
            '  :IPADDRESS, '+
            '  :APPLICATION, '+
            '  :VERSION, '+
            '  :SESSIONID, '+
            '  :SESSIONSTART, '+
            '  :SESSIONRECORDED, '+
            '  :LOGSTATUS, '+
            '  :LOGEVENTS, '+
            '  :LOGCHANGES, '+
            '  :LOGERRORS, '+
            '  :LOGSTART, '+
            '  :ACTIONS '+
            ');'
           );

  end;
end;
