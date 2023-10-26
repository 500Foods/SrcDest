// [query] unique_email

if (DatabaseEngine = 'sqlite') then
begin

  with Query1 do
  begin

    SQL.Clear;
    SQL.Add('select '+
            '  count(*) matches '+
            'from '+
            ' contact '+
            'where '+
            '  (contact_id = 1) '+
            '  and (upper(value) = :UNIQUEEMAIL); '
           );

  end;
end;