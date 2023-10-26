// [query] admin/period_past

if (MainForm.DatabaseEngine = 'sqlite') then
begin

  with Query1 do
  begin

    SQL.Clear;
    SQL.Add('SELECT '+
            '  category, '+
            '  name_short, '+
            '  name_long, '+
            '  period_start, '+
            '  period_end, '+
            '  adjustment '+
            'FROM '+
            '  period '+
            'WHERE '+
            '  (period_start < DateTime("now", "+1 days")) '+
            'ORDER BY '+
            '  category, '+
            '  period_start DESC; '
           );

  end;
end;
