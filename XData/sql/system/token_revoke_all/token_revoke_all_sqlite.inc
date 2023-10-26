// [query] token_revoke_all

if (DatabaseEngine = 'sqlite') then
begin

  with Query1 do
  begin

    SQL.Clear;
    SQL.Add('delete from '+
            '  token '+
            'where '+
            '  person_id = :PERSONID '+
            ';'
           );

  end;
end;