// [query] photo_insert

if (MainForm.DatabaseEngine = 'sqlite') then
begin

  with Query1 do
  begin

    SQL.Clear;
    SQL.Add('DELETE FROM '+
            '  photo '+
            'WHERE'+
            '  (person_id = :PERSONID) '+
            '  and (photo_id = 0) '+
            '  and (photo_type = 1) '+
            ';'
           );
    SQL.Add('INSERT INTO '+
            '  photo (person_id, photo_id, last_modified, last_modifier, photo_type, photo_datauri) '+
            'VALUES( '+
            '  :PERSONID, '+
            '  0, '+
            '  DateTime("now"), '+
            '  :LASTMODIFIER, '+
            '  1, '+
            '  :PHOTO '+
            ');'
           );

  end;
end;
