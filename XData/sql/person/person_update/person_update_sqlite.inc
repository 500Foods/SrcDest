// [query] person_update

if (MainForm.DatabaseEngine = 'sqlite') then
begin

  with Query1 do
  begin

    SQL.Clear;
    SQL.Add('UPDATE '+
            '  person '+
            'SET '+
            '  first_name = :FIRSTNAME, '+
            '  middle_name = :MIDDLENAME, '+
            '  last_name = :LASTNAME, '+
            '  description = :DESCRIPTION, '+
            '  birthdate = :BIRTHDATE, '+
            '  last_modifier = :LASTMODIFIER, '+
            '  last_modified = DateTime("now") '+
            'WHERE '+
            '  person_id = :PERSONID; '
           );

  end;
end;
