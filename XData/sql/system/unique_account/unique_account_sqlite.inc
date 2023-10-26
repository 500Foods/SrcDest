// [query] unique_account

if (DatabaseEngine = 'sqlite') then
begin

  with Query1 do
  begin

    SQL.Clear;
    SQL.Add('select '+
            '  sum(matches) matches '+
            'from ( '+
            '  select '+
            '    count(*) matches '+
            '  from '+
            '   contact '+
            '  where '+
            '    (contact_id = 0) and '+
            '    (value = :UNIQUEACCOUNT) '+
            'UNION ALL '+
            '  select '+
            '    count(*) matches '+
            '  from '+
            '   list '+
            '  where '+
            '    (list_id = 5) and '+
            '    (value = :UNIQUEACCOUNT) '+
            ');'
           );

  end;
end;