// [query] authcode_verify

if (DatabaseEngine = 'sqlite') then
begin

  with Query1 do
  begin

    SQL.Clear;
    SQL.Add('select '+
            '  authcode '+
            'from '+
            '  authcode '+
            'where '+
            '  (authcode = :AUTHCODE) '+
            '  and (valid_after < DateTime("now")) '+
            '  and (valid_until > DateTime("now")) '+
            '  and (destination = :DESTINATION) '+
            '  and (application = :APPLICATION) '+
            '  and (version = :VERSION) '+
            '  and (ip_address = :IPADDRESS)'
           );

  end;
end;

