 // RUN: db-run %s %S/../../resources/data/test | FileCheck %s
 !test_table_tuple=type tuple<!db.nullable<!db.string>,!db.nullable<f32>,!db.nullable<f64>,!db.nullable<!db.decimal<5,2>>,!db.nullable<i32>,!db.nullable<i64>,!db.nullable<i1>,!db.nullable<!db.date<day>>,!db.nullable<!db.date<millisecond>>>
 module {
    //CHECK: string("str")
    //CHECK: float(1.1)
    //CHECK: float(1.1)
    //CHECK: decimal(1.10)
    //CHECK: int(1)
    //CHECK: int(1)
    //CHECK: bool(true)
    //CHECK: date(1996-01-02)
    //CHECK: date(1996-01-02)
    //CHECK: string("---------------")
    //CHECK: string(NULL)
    //CHECK: float(NULL)
    //CHECK: float(NULL)
    //CHECK: decimal(NULL)
    //CHECK: int(NULL)
    //CHECK: int(NULL)
    //CHECK: bool(NULL)
    //CHECK: date(NULL)
    //CHECK: date(NULL)
    //CHECK: string("---------------")
	func @main () {
			%str_const = db.constant ( "---------------" ) :!db.string
            %0 = db.scan_source "{ \"table\": \"test\", \"columns\": [\"str\",\"float32\",\"float64\",\"decimal\",\"int32\",\"int64\",\"bool\",\"date32\",\"date64\"] }" : !db.iterable<!db.iterable<!test_table_tuple,table_row_iterator>,table_chunk_iterator>
            db.for %table_chunk in %0 : !db.iterable<!db.iterable<!test_table_tuple,table_row_iterator>,table_chunk_iterator>{
				db.for %table_row in %table_chunk : !db.iterable<!test_table_tuple,table_row_iterator>{
					%1,%2,%3,%4,%5,%6,%7,%8,%9 = util.unpack %table_row : !test_table_tuple -> !db.nullable<!db.string>,!db.nullable<f32>,!db.nullable<f64>,!db.nullable<!db.decimal<5,2>>,!db.nullable<i32>,!db.nullable<i64>,!db.nullable<i1>,!db.nullable<!db.date<day>>,!db.nullable<!db.date<millisecond>>
					db.runtime_call "DumpValue" (%1) : (!db.nullable<!db.string>) -> ()
					db.runtime_call "DumpValue" (%2) : (!db.nullable<f32>) -> ()
					db.runtime_call "DumpValue" (%3) : (!db.nullable<f64>) -> ()
					db.runtime_call "DumpValue" (%4) : (!db.nullable<!db.decimal<5,2>>) -> ()
					db.runtime_call "DumpValue" (%5) : (!db.nullable<i32>) -> ()
					db.runtime_call "DumpValue" (%6) : (!db.nullable<i64>) -> ()
					db.runtime_call "DumpValue" (%7) : (!db.nullable<i1>) -> ()
					db.runtime_call "DumpValue" (%8) : (!db.nullable<!db.date<day>>) -> ()
					db.runtime_call "DumpValue" (%9) : (!db.nullable<!db.date<millisecond>>) -> ()

					db.runtime_call "DumpValue" (%str_const) : (!db.string) -> ()
				}
			}
		return
	}
 }