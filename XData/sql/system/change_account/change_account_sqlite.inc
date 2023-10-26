// [query] change_account

if (DatabaseEngine = 'sqlite') then
begin

  with Query1 do
  begin

    SQL.Clear;
    SQL.Add('update '+
            '  contact '+
            'set '+
            '  value = :ACCOUNTNAME '+
            'where '+
            '  (person_id = :PERSONID) '+
            '  and (contact_id = 0);'
           );

    SQL.Add('update '+
            '  person '+
            'set '+
            '  account_name = :ACCOUNTNAME '+
            'where '+
            '  (person_id = :PERSONID); '
           );

  end;
end;