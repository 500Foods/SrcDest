// [query] role

if (MainForm.DatabaseEngine = 'sqlite') then
begin

  with Query1 do
  begin

    SQL.Clear;
    SQL.Add('SELECT '+
            '  r.role_id, '+
            '  r.role_sort, '+
            '  r.category, '+
            '  r.name, '+
            '  r.icon, '+
            '  pr.valid_after, '+
            '  pr.valid_until, '+
            '  pr.last_modified, '+
            '  pr.last_modifier, '+
            '  m.account_name last_modifier_account '+
            'FROM '+
            '  role r '+
            '    LEFT OUTER JOIN '+
            '      person_role pr '+
            '        on pr.role_id = r.role_id '+
            '           and pr.person_id = :PERSONID '+
            '    LEFT OUTER JOIN '+
            '      person m '+
            '        on pr.last_modifier = m.person_id '+
            'ORDER BY '+
            '  r.role_sort, '+
            '  r.category, '+
            '  r.name; '
           );

  end;
end;
