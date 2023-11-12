// [table] period
//

TableName := 'period';

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
                '  last_modified              text        NOT NULL, '+
                '  last_modifier              integer     NOT NULL, '+
                '  name_short                 text        NOT NULL, '+
                '  name_long                  text        NOT NULL, '+
                '  period_start               text        NOT NULL, '+
                '  period_end                 text        NOT NULL, '+
                '  category                   text        NOT NULL, '+
                '  adjustment                 text        NOT NULL, '+
                '  CONSTRAINT constraint_name PRIMARY KEY (name_short, period_start, period_end) '+
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

      // Weeks
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2023-W52", "2023 Week 52", "2023-12-24 00:00:00.000", "2023-12-30 23:59:59.999", "Weeks", "week" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2023-W51", "2023 Week 51", "2023-12-17 00:00:00.000", "2023-12-23 23:59:59.999", "Weeks", "week" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2023-W50", "2023 Week 50", "2023-12-10 00:00:00.000", "2023-12-16 23:59:59.999", "Weeks", "week" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2023-W49", "2023 Week 49", "2023-12-03 00:00:00.000", "2023-12-09 23:59:59.999", "Weeks", "week" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2023-W48", "2023 Week 48", "2023-11-26 00:00:00.000", "2023-12-02 23:59:59.999", "Weeks", "week" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2023-W47", "2023 Week 47", "2023-11-19 00:00:00.000", "2023-11-25 23:59:59.999", "Weeks", "week" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2023-W46", "2023 Week 46", "2023-11-12 00:00:00.000", "2023-11-18 23:59:59.999", "Weeks", "week" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2023-W45", "2023 Week 45", "2023-11-05 00:00:00.000", "2023-11-11 23:59:59.999", "Weeks", "week" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2023-W44", "2023 Week 44", "2023-10-29 00:00:00.000", "2023-11-04 23:59:59.999", "Weeks", "week" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2023-W43", "2023 Week 43", "2023-10-22 00:00:00.000", "2023-10-28 23:59:59.999", "Weeks", "week" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2023-W42", "2023 Week 42", "2023-10-15 00:00:00.000", "2023-10-21 23:59:59.999", "Weeks", "week" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2023-W41", "2023 Week 41", "2023-10-08 00:00:00.000", "2023-10-14 23:59:59.999", "Weeks", "week" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2023-W40", "2023 Week 40", "2023-10-01 00:00:00.000", "2023-10-07 23:59:59.999", "Weeks", "week" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2023-W39", "2023 Week 39", "2023-09-24 00:00:00.000", "2023-09-30 23:59:59.999", "Weeks", "week" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2023-W38", "2023 Week 38", "2023-09-17 00:00:00.000", "2023-09-23 23:59:59.999", "Weeks", "week" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2023-W37", "2023 Week 37", "2023-09-10 00:00:00.000", "2023-09-16 23:59:59.999", "Weeks", "week" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2023-W36", "2023 Week 36", "2023-09-03 00:00:00.000", "2023-09-09 23:59:59.999", "Weeks", "week" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2023-W35", "2023 Week 35", "2023-08-27 00:00:00.000", "2023-09-02 23:59:59.999", "Weeks", "week" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2023-W34", "2023 Week 34", "2023-08-20 00:00:00.000", "2023-08-26 23:59:59.999", "Weeks", "week" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2023-W33", "2023 Week 33", "2023-08-13 00:00:00.000", "2023-08-19 23:59:59.999", "Weeks", "week" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2023-W32", "2023 Week 32", "2023-08-06 00:00:00.000", "2023-08-12 23:59:59.999", "Weeks", "week" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2023-W31", "2023 Week 31", "2023-07-30 00:00:00.000", "2023-08-05 23:59:59.999", "Weeks", "week" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2023-W30", "2023 Week 30", "2023-07-23 00:00:00.000", "2023-07-29 23:59:59.999", "Weeks", "week" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2023-W29", "2023 Week 29", "2023-07-16 00:00:00.000", "2023-07-22 23:59:59.999", "Weeks", "week" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2023-W28", "2023 Week 28", "2023-07-09 00:00:00.000", "2023-07-15 23:59:59.999", "Weeks", "week" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2023-W27", "2023 Week 27", "2023-07-02 00:00:00.000", "2023-07-08 23:59:59.999", "Weeks", "week" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2023-W26", "2023 Week 26", "2023-06-25 00:00:00.000", "2023-07-01 23:59:59.999", "Weeks", "week" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2023-W25", "2023 Week 25", "2023-06-18 00:00:00.000", "2023-06-24 23:59:59.999", "Weeks", "week" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2023-W24", "2023 Week 24", "2023-06-11 00:00:00.000", "2023-06-17 23:59:59.999", "Weeks", "week" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2023-W23", "2023 Week 23", "2023-06-04 00:00:00.000", "2023-06-10 23:59:59.999", "Weeks", "week" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2023-W22", "2023 Week 22", "2023-05-28 00:00:00.000", "2023-06-03 23:59:59.999", "Weeks", "week" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2023-W21", "2023 Week 21", "2023-05-21 00:00:00.000", "2023-05-27 23:59:59.999", "Weeks", "week" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2023-W20", "2023 Week 20", "2023-05-14 00:00:00.000", "2023-05-20 23:59:59.999", "Weeks", "week" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2023-W19", "2023 Week 19", "2023-05-07 00:00:00.000", "2023-05-13 23:59:59.999", "Weeks", "week" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2023-W18", "2023 Week 18", "2023-04-30 00:00:00.000", "2023-05-06 23:59:59.999", "Weeks", "week" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2023-W17", "2023 Week 17", "2023-04-23 00:00:00.000", "2023-04-29 23:59:59.999", "Weeks", "week" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2023-W16", "2023 Week 16", "2023-04-16 00:00:00.000", "2023-04-22 23:59:59.999", "Weeks", "week" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2023-W15", "2023 Week 15", "2023-04-09 00:00:00.000", "2023-04-15 23:59:59.999", "Weeks", "week" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2023-W14", "2023 Week 14", "2023-04-02 00:00:00.000", "2023-04-08 23:59:59.999", "Weeks", "week" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2023-W13", "2023 Week 13", "2023-03-26 00:00:00.000", "2023-04-01 23:59:59.999", "Weeks", "week" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2023-W12", "2023 Week 12", "2023-03-19 00:00:00.000", "2023-03-25 23:59:59.999", "Weeks", "week" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2023-W11", "2023 Week 11", "2023-03-12 00:00:00.000", "2023-03-18 23:59:59.999", "Weeks", "week" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2023-W10", "2023 Week 10", "2023-03-05 00:00:00.000", "2023-03-11 23:59:59.999", "Weeks", "week" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2023-W09", "2023 Week 9",  "2023-02-26 00:00:00.000", "2023-03-04 23:59:59.999", "Weeks", "week" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2023-W08", "2023 Week 8",  "2023-02-19 00:00:00.000", "2023-02-25 23:59:59.999", "Weeks", "week" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2023-W07", "2023 Week 7",  "2023-02-12 00:00:00.000", "2023-02-18 23:59:59.999", "Weeks", "week" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2023-W06", "2023 Week 6",  "2023-02-05 00:00:00.000", "2023-02-11 23:59:59.999", "Weeks", "week" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2023-W05", "2023 Week 5",  "2023-01-29 00:00:00.000", "2023-02-04 23:59:59.999", "Weeks", "week" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2023-W04", "2023 Week 4",  "2023-01-22 00:00:00.000", "2023-01-28 23:59:59.999", "Weeks", "week" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2023-W03", "2023 Week 3",  "2023-01-15 00:00:00.000", "2023-01-21 23:59:59.999", "Weeks", "week" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2023-W02", "2023 Week 2",  "2023-01-08 00:00:00.000", "2023-01-14 23:59:59.999", "Weeks", "week" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2023-W01", "2023 Week 1",  "2023-01-01 00:00:00.000", "2023-01-07 23:59:59.999", "Weeks", "week" );');

      // Months
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2024-Dec", "2024 December",  "2024-12-01 00:00:00.000", "2024-12-31 23:59:59.999", "Months", "month" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2024-Nov", "2024 November",  "2024-11-01 00:00:00.000", "2024-11-30 23:59:59.999", "Months", "month" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2024-Oct", "2024 October",   "2024-10-01 00:00:00.000", "2024-10-31 23:59:59.999", "Months", "month" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2024-Sep", "2024 September", "2024-09-01 00:00:00.000", "2024-09-30 23:59:59.999", "Months", "month" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2024-Aug", "2024 August",    "2024-08-01 00:00:00.000", "2024-08-31 23:59:59.999", "Months", "month" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2024-Jul", "2024 July",      "2024-07-01 00:00:00.000", "2024-07-31 23:59:59.999", "Months", "month" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2024-Jun", "2024 June",      "2024-06-01 00:00:00.000", "2024-06-30 23:59:59.999", "Months", "month" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2024-May", "2024 May",       "2024-05-01 00:00:00.000", "2024-05-31 23:59:59.999", "Months", "month" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2024-Apr", "2024 April",     "2024-04-01 00:00:00.000", "2024-04-30 23:59:59.999", "Months", "month" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2024-Mar", "2024 March",     "2024-03-01 00:00:00.000", "2024-03-31 23:59:59.999", "Months", "month" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2024-Feb", "2024 February",  "2024-02-01 00:00:00.000", "2024-02-29 23:59:59.999", "Months", "month" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2024-Jan", "2024 January",   "2024-01-01 00:00:00.000", "2024-01-31 23:59:59.999", "Months", "month" );');

      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2023-Dec", "2023 December",  "2023-12-01 00:00:00.000", "2023-12-31 23:59:59.999", "Months", "month" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2023-Nov", "2023 November",  "2023-11-01 00:00:00.000", "2023-11-30 23:59:59.999", "Months", "month" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2023-Oct", "2023 October",   "2023-10-01 00:00:00.000", "2023-10-31 23:59:59.999", "Months", "month" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2023-Sep", "2023 September", "2023-09-01 00:00:00.000", "2023-09-30 23:59:59.999", "Months", "month" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2023-Aug", "2023 August",    "2023-08-01 00:00:00.000", "2023-08-31 23:59:59.999", "Months", "month" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2023-Jul", "2023 July",      "2023-07-01 00:00:00.000", "2023-07-31 23:59:59.999", "Months", "month" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2023-Jun", "2023 June",      "2023-06-01 00:00:00.000", "2023-06-30 23:59:59.999", "Months", "month" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2023-May", "2023 May",       "2023-05-01 00:00:00.000", "2023-05-31 23:59:59.999", "Months", "month" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2023-Apr", "2023 April",     "2023-04-01 00:00:00.000", "2023-04-30 23:59:59.999", "Months", "month" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2023-Mar", "2023 March",     "2023-03-01 00:00:00.000", "2023-03-31 23:59:59.999", "Months", "month" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2023-Feb", "2023 February",  "2023-02-01 00:00:00.000", "2023-02-28 23:59:59.999", "Months", "month" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2023-Jan", "2023 January",   "2023-01-01 00:00:00.000", "2023-01-31 23:59:59.999", "Months", "month" );');

      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2022-Dec", "2022 December",  "2022-12-01 00:00:00.000", "2022-12-31 23:59:59.999", "Months", "month" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2022-Nov", "2022 November",  "2022-11-01 00:00:00.000", "2022-11-30 23:59:59.999", "Months", "month" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2022-Oct", "2022 October",   "2022-10-01 00:00:00.000", "2022-10-31 23:59:59.999", "Months", "month" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2022-Sep", "2022 September", "2022-09-01 00:00:00.000", "2022-09-30 23:59:59.999", "Months", "month" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2022-Aug", "2022 August",    "2022-08-01 00:00:00.000", "2022-08-31 23:59:59.999", "Months", "month" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2022-Jul", "2022 July",      "2022-07-01 00:00:00.000", "2022-07-31 23:59:59.999", "Months", "month" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2022-Jun", "2022 June",      "2022-06-01 00:00:00.000", "2022-06-30 23:59:59.999", "Months", "month" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2022-May", "2022 May",       "2022-05-01 00:00:00.000", "2022-05-31 23:59:59.999", "Months", "month" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2022-Apr", "2022 April",     "2022-04-01 00:00:00.000", "2022-04-30 23:59:59.999", "Months", "month" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2022-Mar", "2022 March",     "2022-03-01 00:00:00.000", "2022-03-31 23:59:59.999", "Months", "month" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2022-Feb", "2022 February",  "2022-02-01 00:00:00.000", "2022-02-28 23:59:59.999", "Months", "month" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2022-Jan", "2022 January",   "2022-01-01 00:00:00.000", "2022-01-31 23:59:59.999", "Months", "month" );');

      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2021-Dec", "2021 December",  "2021-12-01 00:00:00.000", "2021-12-31 23:59:59.999", "Months", "month" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2021-Nov", "2021 November",  "2021-11-01 00:00:00.000", "2021-11-30 23:59:59.999", "Months", "month" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2021-Oct", "2021 October",   "2021-10-01 00:00:00.000", "2021-10-31 23:59:59.999", "Months", "month" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2021-Sep", "2021 September", "2021-09-01 00:00:00.000", "2021-09-30 23:59:59.999", "Months", "month" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2021-Aug", "2021 August",    "2021-08-01 00:00:00.000", "2021-08-31 23:59:59.999", "Months", "month" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2021-Jul", "2021 July",      "2021-07-01 00:00:00.000", "2021-07-31 23:59:59.999", "Months", "month" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2021-Jun", "2021 June",      "2021-06-01 00:00:00.000", "2021-06-30 23:59:59.999", "Months", "month" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2021-May", "2021 May",       "2021-05-01 00:00:00.000", "2021-05-31 23:59:59.999", "Months", "month" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2021-Apr", "2021 April",     "2021-04-01 00:00:00.000", "2021-04-30 23:59:59.999", "Months", "month" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2021-Mar", "2021 March",     "2021-03-01 00:00:00.000", "2021-03-31 23:59:59.999", "Months", "month" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2021-Feb", "2021 February",  "2021-02-01 00:00:00.000", "2021-02-28 23:59:59.999", "Months", "month" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2021-Jan", "2021 January",   "2021-01-01 00:00:00.000", "2021-01-31 23:59:59.999", "Months", "month" );');

      // Quarters
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2024-Q1", "2024 Q1",   "2024-01-01 00:00:00.000", "2024-03-31 23:59:59.999", "Quarters", "quarter" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2023-Q1", "2023 Q1",   "2023-01-01 00:00:00.000", "2023-03-31 23:59:59.999", "Quarters", "quarter" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2022-Q1", "2022 Q1",   "2022-01-01 00:00:00.000", "2022-03-31 23:59:59.999", "Quarters", "quarter" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2021-Q1", "2021 Q1",   "2021-01-01 00:00:00.000", "2021-03-31 23:59:59.999", "Quarters", "quarter" );');

      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2024-Q2", "2024 Q2",   "2024-04-01 00:00:00.000", "2024-06-30 23:59:59.999", "Quarters", "quarter" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2023-Q2", "2023 Q2",   "2023-04-01 00:00:00.000", "2023-06-30 23:59:59.999", "Quarters", "quarter" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2022-Q2", "2022 Q2",   "2022-04-01 00:00:00.000", "2022-06-30 23:59:59.999", "Quarters", "quarter" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2021-Q2", "2021 Q2",   "2021-04-01 00:00:00.000", "2021-06-30 23:59:59.999", "Quarters", "quarter" );');

      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2024-Q3", "2024 Q3",   "2024-07-01 00:00:00.000", "2024-09-30 23:59:59.999", "Quarters", "quarter" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2023-Q3", "2023 Q3",   "2023-07-01 00:00:00.000", "2023-09-30 23:59:59.999", "Quarters", "quarter" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2022-Q3", "2022 Q3",   "2022-07-01 00:00:00.000", "2022-09-30 23:59:59.999", "Quarters", "quarter" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2021-Q3", "2021 Q3",   "2021-07-01 00:00:00.000", "2021-09-30 23:59:59.999", "Quarters", "quarter" );');

      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2024-Q4", "2024 Q4",   "2024-10-01 00:00:00.000", "2024-12-31 23:59:59.999", "Quarters", "quarter" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2023-Q4", "2023 Q4",   "2023-10-01 00:00:00.000", "2023-12-31 23:59:59.999", "Quarters", "quarter" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2022-Q4", "2022 Q4",   "2022-10-01 00:00:00.000", "2022-12-31 23:59:59.999", "Quarters", "quarter" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2021-Q4", "2021 Q4",   "2021-10-01 00:00:00.000", "2021-12-31 23:59:59.999", "Quarters", "quarter" );');

      // Years
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2024", "2024",   "2024-01-01 00:00:00.000", "2024-12-31 23:59:59.999", "Years", "year" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2023", "2023",   "2023-01-01 00:00:00.000", "2023-12-31 23:59:59.999", "Years", "year" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2022", "2022",   "2022-01-01 00:00:00.000", "2022-12-31 23:59:59.999", "Years", "year" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2021", "2021",   "2021-01-01 00:00:00.000", "2021-12-31 23:59:59.999", "Years", "year" );');

      // Custom Periods - Seasons in this case, likely payroll periods in others. Seasons are location-specific... these are for Vancouver, British Columbia, Canada
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2024-Sp", "2024 Spring", "2024-03-19 20:02:00.000", "2024-06-20 13:49:59.999", "Seasons", "period" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2023-Sp", "2023 Spring", "2023-03-20 14:24:00.000", "2023-06-21 07:56:59.999", "Seasons", "period" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2022-Sp", "2022 Spring", "2022-03-20 08:33:00.000", "2022-06-21 02:12:59.999", "Seasons", "period" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2021-Sp", "2021 Spring", "2021-03-20 02:27:00.000", "2021-06-20 20:31:59.999", "Seasons", "period" );');

      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2024-Su", "2024 Summer", "2024-06-20 13:50:00.000", "2024-09-22 05:42:59.999", "Seasons", "period" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2023-Su", "2023 Summer", "2023-06-21 07:57:00.000", "2023-09-22 23:49:59.999", "Seasons", "period" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2022-Su", "2022 Summer", "2022-06-21 02:13:00.000", "2022-09-22 18:02:59.999", "Seasons", "period" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2021-Su", "2021 Summer", "2021-06-20 20:32:00.000", "2021-09-22 12:20:59.999", "Seasons", "period" );');

      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2024-F",  "2024 Fall",   "2024-09-22 05:43:00.000", "2024-12-21 01:19:59.999", "Seasons", "period" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2023-F",  "2023 Fall",   "2023-09-22 23:50:00.000", "2023-12-21 19:26:59.999", "Seasons", "period" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2022-F",  "2022 Fall",   "2022-09-22 18:03:00.000", "2022-12-21 13:47:59.999", "Seasons", "period" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2021-F",  "2021 Fall",   "2021-09-22 12:21:00.000", "2021-12-21 07:58:59.999", "Seasons", "period" );');

      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2024-W",  "2024 Winter", "2024-12-21 01:20:00.000", "2025-03-20 14:00:59.999", "Seasons", "period" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2023-W",  "2023 Winter", "2023-12-21 19:27:00.000", "2024-03-19 20:01:59.999", "Seasons", "period" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2022-W",  "2022 Winter", "2022-12-21 13:48:00.000", "2023-03-20 14:23:59.999", "Seasons", "period" );');
      SQL.Add('insert into '+TableName+' values( Datetime("now"), 0, "2021-W",  "2021 Winter", "2021-12-21 07:59:00.000", "2022-03-20 08:32:59.999", "Seasons", "period" );');

      ExecSQL;
    end;
  end;
end;

