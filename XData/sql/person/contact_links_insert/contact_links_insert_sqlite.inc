// [query] contact_links_insert

if (MainForm.DatabaseEngine = 'sqlite') then
begin

  with Query1 do
  begin

    SQL.Clear;
    SQL.Add('INSERT INTO '+
            '  contact (person_id, contact_id, last_modified, last_modifier, list_contact, value, preference, login_use  )'+
            'VALUES( '+
            '  :PERSONID, '+
            '  :CONTACTID, '+
            '  DateTime("now"), '+
            '  :LASTMODIFIER, '+
            '  10, '+
            '  :LINK, '+
            '  :CONTACTID, '+
            '  0 '+
            '); '
           );

  end;
end;
