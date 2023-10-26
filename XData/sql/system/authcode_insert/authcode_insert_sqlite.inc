// [query] authcode_insert

if (DatabaseEngine = 'sqlite') then
begin

  with Query1 do
  begin

    SQL.Clear;
    SQL.Add('insert into '+
            '  authcode '+
            '    (authcode, valid_after, valid_until, destination, application, version, ip_address) '+
            'values( '+
            '  :AUTHCODE, '+
            '  :VALIDAFTER, '+
            '  :VALIDUNTIL, '+
            '  :DESTINATION, '+
            '  :APPLICATION, '+
            '  :VERSION, '+
            '  :IPADDRESS '+
            ');'
           );

  end;
end;

