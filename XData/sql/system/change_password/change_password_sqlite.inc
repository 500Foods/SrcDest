// [query] change_password

if (DatabaseEngine = 'sqlite') then
begin

  with Query1 do
  begin

    SQL.Clear;
    SQL.Add('update '+
            '  person '+
            'set '+
            '  password_hash = :PASSWORDHASH '+
            'where '+
            '  (person_id = :PERSONID) '
           );

  end;
end;