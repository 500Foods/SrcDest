// [query] contact_links_delete
//
// This gets rid of all of a person's contact records corresponding to "list_contact = 10" which is
// sort of reserved for Social Medial Links. Be aware that this might not hold for all projects.

if (MainForm.DatabaseEngine = 'sqlite') then
begin

  with Query1 do
  begin

    SQL.Clear;
    SQL.Add('DELETE FROM '+
            '  contact '+
            'WHERE '+
            '  (person_id = :PERSONID) '+
            '  and (list_contact = 10); '
           );

  end;
end;
