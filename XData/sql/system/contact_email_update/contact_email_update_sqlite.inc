// [query] contact_email

if (DatabaseEngine = 'sqlite') then
begin

  with Query1 do
  begin

    SQL.Clear;
    SQL.Add('update '+
            '  contact '+
            'set '+
            '  value = :EMAILADDRESS '+
            'where '+
            '  (person_id = :PERSONID) '+
            '  and (contact_id = 1); '
           );

  end;
end;
