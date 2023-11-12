// [table] role

TableName := 'role';

if (DatabaseEngine = 'sqlite') then
begin

  with Query1 do
  begin

    // Check if the table exists
    SQL.Clear;
    SQL.Add('select count(*) records from '+TableName+';');
    try
      Open;
      LogEvent('...'+TableName+' ('+IntToStr(FieldByName('records').AsInteger)+' records)');

    except on E:Exception do
      begin
        LogEvent('...'+TableName+' (CREATE)');
        SQL.Clear;
        SQL.Add('create table if not exists '+TableName+' ( '+
                '  role_id                    integer     NOT NULL, '+
                '  role_sort                  integer     NOT NULL, '+
                '  last_modified              text        NOT NULL, '+
                '  last_modifier              integer     NOT NULL, '+
                '  category                   text        NOT NULL, '+
                '  name                       text        NOT NULL, '+
                '  icon                       text        NOT NULL, '+
                '  CONSTRAINT constraint_name PRIMARY KEY (role_id), '+
                '  UNIQUE(name) '+
                ');'
               );
        ExecSQL;

        // Try it again
        SQL.Clear;
        SQL.Add('select count(*) records from '+TableName+';');
        Open;
      end;
    end;

    // Populate empty table with sample data
    if (FieldByName('records').AsInteger = 0) then
    begin
      LogEvent('...'+TableName+' (POPULATE)');
      SQL.Clear;

      // Default roles
      SQL.Add('insert into '+TableName+' values( 0, 0, Datetime("now"), 0, "System", "Login",               ":fa-person-to-portal" );');
      SQL.Add('insert into '+TableName+' values( 1, 1, Datetime("now"), 0, "System", "Change Password",     ":fa-shield-keyhole Swap" );');
      SQL.Add('insert into '+TableName+' values( 2, 2, Datetime("now"), 0, "System", "Change Name",         ":fa-cat" );');
      SQL.Add('insert into '+TableName+' values( 3, 3, Datetime("now"), 0, "System", "Change Account",      ":fa-shield-cat Swap" );');
      SQL.Add('insert into '+TableName+' values( 4, 4, Datetime("now"), 0, "System", "Change E-Mail",       ":fa-envelope Swap" );');
      SQL.Add('insert into '+TableName+' values( 5, 5, Datetime("now"), 0, "System", "Change Photo",        ":fa-camera" );');
      SQL.Add('insert into '+TableName+' values( 6, 6, Datetime("now"), 0, "System", "Change Description",  ":fa-scroll Swap" );');
      SQL.Add('insert into '+TableName+' values( 7, 7, Datetime("now"), 0, "System", "Change Links",        ":fa-link" );');

      SQL.Add('insert into '+TableName+' values( 100, 100, Datetime("now"), 0, "Manage", "Manage Accounts", ":fa-user-secret" );');
      SQL.Add('insert into '+TableName+' values( 110, 110, Datetime("now"), 0, "Manage", "View All Roles",  ":fa-globe Swap" );');

      SQL.Add('insert into '+TableName+' values( 200, 200, Datetime("now"), 0, "Statistics", "Statistics Access",  ":fa-chart-simple" );');

      ExecSQL;
    end;
  end;
end;
