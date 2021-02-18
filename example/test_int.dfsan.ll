; ModuleID = 'test_int.dfsan.bc'
source_filename = "test_int.c"
target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

%struct.dfsan_label_info = type { i16, i16, i8*, float, float, i16, i32 }

@.str = private unnamed_addr constant [2 x i8] c"x\00", align 1
@.str.1 = private unnamed_addr constant [20 x i8] c"x label %d: %f, %f\0A\00", align 1
@.str.2 = private unnamed_addr constant [20 x i8] c"y label %d: %f, %f\0A\00", align 1
@.str.3 = private unnamed_addr constant [20 x i8] c"z label %d: %f, %f\0A\00", align 1
@.str.4 = private unnamed_addr constant [23 x i8] c"loop label %d: %f, %f\0A\00", align 1
@__dfsan_arg_tls = external thread_local(initialexec) global [64 x i16]
@__dfsan_retval_tls = external thread_local(initialexec) global i16
@__dfsan_shadow_ptr_mask = external global i64
@0 = private unnamed_addr constant [8 x i8] c"UNKNOWN\00"
@1 = private unnamed_addr constant [8 x i8] c"UNKNOWN\00"
@2 = private unnamed_addr constant [8 x i8] c"UNKNOWN\00"
@3 = private unnamed_addr constant [8 x i8] c"UNKNOWN\00"
@4 = private unnamed_addr constant [8 x i8] c"UNKNOWN\00"
@5 = private unnamed_addr constant [8 x i8] c"UNKNOWN\00"
@6 = private unnamed_addr constant [8 x i8] c"UNKNOWN\00"
@7 = private unnamed_addr constant [8 x i8] c"UNKNOWN\00"
@8 = private unnamed_addr constant [8 x i8] c"UNKNOWN\00"
@9 = private unnamed_addr constant [8 x i8] c"UNKNOWN\00"
@10 = private unnamed_addr constant [8 x i8] c"UNKNOWN\00"
@11 = private unnamed_addr constant [8 x i8] c"UNKNOWN\00"
@12 = private unnamed_addr constant [8 x i8] c"UNKNOWN\00"
@13 = private unnamed_addr constant [8 x i8] c"UNKNOWN\00"
@14 = private unnamed_addr constant [8 x i8] c"UNKNOWN\00"
@15 = private unnamed_addr constant [8 x i8] c"UNKNOWN\00"
@16 = private unnamed_addr constant [14 x i8] c"test_int.c:13\00"
@17 = private unnamed_addr constant [14 x i8] c"test_int.c:14\00"
@18 = private unnamed_addr constant [14 x i8] c"test_int.c:15\00"
@19 = private unnamed_addr constant [8 x i8] c"UNKNOWN\00"
@20 = private unnamed_addr constant [8 x i8] c"UNKNOWN\00"
@21 = private unnamed_addr constant [8 x i8] c"UNKNOWN\00"
@22 = private unnamed_addr constant [8 x i8] c"UNKNOWN\00"
@23 = private unnamed_addr constant [14 x i8] c"test_int.c:18\00"
@24 = private unnamed_addr constant [14 x i8] c"test_int.c:19\00"
@25 = private unnamed_addr constant [14 x i8] c"test_int.c:18\00"
@26 = private unnamed_addr constant [8 x i8] c"UNKNOWN\00"
@27 = private unnamed_addr constant [8 x i8] c"UNKNOWN\00"
@28 = private unnamed_addr constant [8 x i8] c"UNKNOWN\00"
@29 = private unnamed_addr constant [8 x i8] c"UNKNOWN\00"
@30 = private unnamed_addr constant [8 x i8] c"UNKNOWN\00"
@31 = private unnamed_addr constant [8 x i8] c"UNKNOWN\00"
@32 = private unnamed_addr constant [8 x i8] c"UNKNOWN\00"
@33 = private unnamed_addr constant [8 x i8] c"UNKNOWN\00"
@34 = private unnamed_addr constant [8 x i8] c"UNKNOWN\00"
@35 = private unnamed_addr constant [8 x i8] c"UNKNOWN\00"
@36 = private unnamed_addr constant [8 x i8] c"UNKNOWN\00"
@37 = private unnamed_addr constant [8 x i8] c"UNKNOWN\00"
@38 = private unnamed_addr constant [8 x i8] c"UNKNOWN\00"
@39 = private unnamed_addr constant [8 x i8] c"UNKNOWN\00"
@40 = private unnamed_addr constant [8 x i8] c"UNKNOWN\00"
@41 = private unnamed_addr constant [8 x i8] c"UNKNOWN\00"
@42 = private unnamed_addr constant [8 x i8] c"UNKNOWN\00"
@43 = private unnamed_addr constant [8 x i8] c"UNKNOWN\00"
@44 = private unnamed_addr constant [8 x i8] c"UNKNOWN\00"
@45 = private unnamed_addr constant [8 x i8] c"UNKNOWN\00"
@46 = private unnamed_addr constant [8 x i8] c"UNKNOWN\00"
@47 = private unnamed_addr constant [8 x i8] c"UNKNOWN\00"
@48 = private unnamed_addr constant [8 x i8] c"UNKNOWN\00"
@49 = private unnamed_addr constant [8 x i8] c"UNKNOWN\00"
@50 = private unnamed_addr constant [8 x i8] c"UNKNOWN\00"
@51 = private unnamed_addr constant [8 x i8] c"UNKNOWN\00"
@52 = private unnamed_addr constant [8 x i8] c"UNKNOWN\00"
@53 = private unnamed_addr constant [8 x i8] c"UNKNOWN\00"
@54 = private unnamed_addr constant [8 x i8] c"UNKNOWN\00"
@55 = private unnamed_addr constant [8 x i8] c"UNKNOWN\00"
@56 = private unnamed_addr constant [8 x i8] c"UNKNOWN\00"
@57 = private unnamed_addr constant [8 x i8] c"UNKNOWN\00"
@58 = private unnamed_addr constant [8 x i8] c"UNKNOWN\00"
@59 = private unnamed_addr constant [8 x i8] c"UNKNOWN\00"
@60 = private unnamed_addr constant [8 x i8] c"UNKNOWN\00"
@61 = private unnamed_addr constant [8 x i8] c"UNKNOWN\00"
@62 = private unnamed_addr constant [8 x i8] c"UNKNOWN\00"

; Function Attrs: noinline nounwind optnone uwtable
define dso_local i32 @main() #0 !dbg !9 {
entry:
  %labelreturn = alloca i16
  %0 = alloca i16
  %retval = alloca i32, align 4
  %x = alloca i32, align 4
  %1 = alloca i16
  %y = alloca i32, align 4
  %2 = alloca i16
  %z = alloca i32, align 4
  %3 = alloca i16
  %loop = alloca i32, align 4
  %4 = alloca i16
  %x_label = alloca i16, align 2
  %5 = alloca i16
  %i = alloca i32, align 4
  %6 = alloca i16
  %y_label = alloca i16, align 2
  %7 = alloca i16
  %z_label = alloca i16, align 2
  %8 = alloca i16
  %loop_label = alloca i16, align 2
  %9 = alloca i16
  %x_info = alloca %struct.dfsan_label_info*, align 8
  %10 = alloca i16
  %y_info = alloca %struct.dfsan_label_info*, align 8
  %11 = alloca i16
  %z_info = alloca %struct.dfsan_label_info*, align 8
  %12 = alloca i16
  %loop_info = alloca %struct.dfsan_label_info*, align 8
  store i16 0, i16* %0
  store i32 0, i32* %retval, align 4
  %13 = call zeroext i16 @__dfsan_union(i16 zeroext 0, i16 zeroext 0, i32 0, i32 0, i64 0, i16 54, i8* getelementptr inbounds ([8 x i8], [8 x i8]* @0, i32 0, i32 0)), !dbg !13
  %14 = call zeroext i16 @__dfsan_union(i16 zeroext %13, i16 zeroext 0, i32 0, i32 0, i64 0, i16 54, i8* getelementptr inbounds ([8 x i8], [8 x i8]* @1, i32 0, i32 0)), !dbg !13
  %15 = call zeroext i16 @__dfsan_union(i16 zeroext %14, i16 zeroext 0, i32 0, i32 0, i64 0, i16 54, i8* getelementptr inbounds ([8 x i8], [8 x i8]* @2, i32 0, i32 0)), !dbg !13
  call void @llvm.dbg.declare(metadata i32* %x, metadata !14, metadata !DIExpression()), !dbg !13
  %16 = call zeroext i16 @__dfsan_union(i16 zeroext 0, i16 zeroext 0, i32 0, i32 0, i64 0, i16 54, i8* getelementptr inbounds ([8 x i8], [8 x i8]* @3, i32 0, i32 0)), !dbg !15
  %17 = call zeroext i16 @__dfsan_union(i16 zeroext %16, i16 zeroext 0, i32 0, i32 0, i64 0, i16 54, i8* getelementptr inbounds ([8 x i8], [8 x i8]* @4, i32 0, i32 0)), !dbg !15
  %18 = call zeroext i16 @__dfsan_union(i16 zeroext %17, i16 zeroext 0, i32 0, i32 0, i64 0, i16 54, i8* getelementptr inbounds ([8 x i8], [8 x i8]* @5, i32 0, i32 0)), !dbg !15
  call void @llvm.dbg.declare(metadata i32* %y, metadata !16, metadata !DIExpression()), !dbg !15
  %19 = call zeroext i16 @__dfsan_union(i16 zeroext 0, i16 zeroext 0, i32 0, i32 0, i64 0, i16 54, i8* getelementptr inbounds ([8 x i8], [8 x i8]* @6, i32 0, i32 0)), !dbg !17
  %20 = call zeroext i16 @__dfsan_union(i16 zeroext %19, i16 zeroext 0, i32 0, i32 0, i64 0, i16 54, i8* getelementptr inbounds ([8 x i8], [8 x i8]* @7, i32 0, i32 0)), !dbg !17
  %21 = call zeroext i16 @__dfsan_union(i16 zeroext %20, i16 zeroext 0, i32 0, i32 0, i64 0, i16 54, i8* getelementptr inbounds ([8 x i8], [8 x i8]* @8, i32 0, i32 0)), !dbg !17
  call void @llvm.dbg.declare(metadata i32* %z, metadata !18, metadata !DIExpression()), !dbg !17
  %22 = call zeroext i16 @__dfsan_union(i16 zeroext 0, i16 zeroext 0, i32 0, i32 0, i64 0, i16 54, i8* getelementptr inbounds ([8 x i8], [8 x i8]* @9, i32 0, i32 0)), !dbg !19
  %23 = call zeroext i16 @__dfsan_union(i16 zeroext %22, i16 zeroext 0, i32 0, i32 0, i64 0, i16 54, i8* getelementptr inbounds ([8 x i8], [8 x i8]* @10, i32 0, i32 0)), !dbg !19
  %24 = call zeroext i16 @__dfsan_union(i16 zeroext %23, i16 zeroext 0, i32 0, i32 0, i64 0, i16 54, i8* getelementptr inbounds ([8 x i8], [8 x i8]* @11, i32 0, i32 0)), !dbg !19
  call void @llvm.dbg.declare(metadata i32* %loop, metadata !20, metadata !DIExpression()), !dbg !19
  %25 = ptrtoint i32* %x to i64, !dbg !21
  %26 = and i64 %25, -123145302310913, !dbg !21
  %27 = mul i64 %26, 2, !dbg !21
  %28 = inttoptr i64 %27 to i16*, !dbg !21
  %29 = bitcast i16* %28 to i64*, !dbg !21
  store i64 0, i64* %29, align 2, !dbg !21
  store i32 1, i32* %x, align 4, !dbg !21
  %30 = call zeroext i16 @__dfsan_union(i16 zeroext 0, i16 zeroext 0, i32 0, i32 0, i64 0, i16 54, i8* getelementptr inbounds ([8 x i8], [8 x i8]* @12, i32 0, i32 0)), !dbg !22
  %31 = call zeroext i16 @__dfsan_union(i16 zeroext %30, i16 zeroext 0, i32 0, i32 0, i64 0, i16 54, i8* getelementptr inbounds ([8 x i8], [8 x i8]* @13, i32 0, i32 0)), !dbg !22
  %32 = call zeroext i16 @__dfsan_union(i16 zeroext %31, i16 zeroext 0, i32 0, i32 0, i64 0, i16 54, i8* getelementptr inbounds ([8 x i8], [8 x i8]* @14, i32 0, i32 0)), !dbg !22
  call void @llvm.dbg.declare(metadata i16* %x_label, metadata !23, metadata !DIExpression()), !dbg !22
  %call = call zeroext i16 @dfsan_create_label(i8* getelementptr inbounds ([2 x i8], [2 x i8]* @.str, i32 0, i32 0)), !dbg !31
  store i16 0, i16* %4, !dbg !22
  store i16 %call, i16* %x_label, align 2, !dbg !22
  %33 = load i16, i16* %4, !dbg !32
  %34 = load i16, i16* %x_label, align 2, !dbg !32
  %35 = bitcast i32* %x to i8*, !dbg !33
  call void @dfsan_set_label(i16 zeroext %34, i8* %35, i64 4), !dbg !34
  %36 = ptrtoint i32* %x to i64, !dbg !35
  %37 = and i64 %36, -123145302310913, !dbg !35
  %38 = mul i64 %37, 2, !dbg !35
  %39 = inttoptr i64 %38 to i16*, !dbg !35
  %40 = load i16, i16* %39, align 2
  %41 = load i32, i32* %x, align 4, !dbg !35
  %42 = call zeroext i16 @__dfsan_union(i16 zeroext %40, i16 zeroext 0, i32 0, i32 0, i64 0, i16 51, i8* getelementptr inbounds ([8 x i8], [8 x i8]* @15, i32 0, i32 0)), !dbg !37
  %cmp = icmp sgt i32 %41, 0, !dbg !37
  call void @__branch_visitor_int(i16 zeroext %40, i16 zeroext 0, i32 %41, i32 0, i1 %cmp, i32 38, i64 438997255675854297, i64 0, i16 0, i8* getelementptr inbounds ([14 x i8], [14 x i8]* @16, i32 0, i32 0)), !dbg !38
  br i1 %cmp, label %if.then, label %if.end, !dbg !38

if.then:                                          ; preds = %entry
  %43 = ptrtoint i32* %x to i64, !dbg !39
  %44 = and i64 %43, -123145302310913, !dbg !39
  %45 = mul i64 %44, 2, !dbg !39
  %46 = inttoptr i64 %45 to i16*, !dbg !39
  %47 = load i16, i16* %46, align 2
  %48 = load i32, i32* %x, align 4, !dbg !39
  %49 = call zeroext i16 @__dfsan_union(i16 zeroext 0, i16 zeroext %47, i32 4, i32 %48, i64 0, i16 15, i8* getelementptr inbounds ([14 x i8], [14 x i8]* @17, i32 0, i32 0)), !dbg !41
  %mul = mul nsw i32 4, %48, !dbg !41
  store i16 %49, i16* %1, !dbg !42
  store i32 %mul, i32* %y, align 4, !dbg !42
  %50 = load i16, i16* %1, !dbg !43
  %51 = load i32, i32* %y, align 4, !dbg !43
  %52 = call zeroext i16 @__dfsan_union(i16 zeroext %50, i16 zeroext 0, i32 %51, i32 4, i64 0, i16 21, i8* getelementptr inbounds ([14 x i8], [14 x i8]* @18, i32 0, i32 0)), !dbg !44
  %rem = srem i32 %51, 4, !dbg !44
  store i16 %52, i16* %2, !dbg !45
  store i32 %rem, i32* %z, align 4, !dbg !45
  %53 = load i16, i16* %1, !dbg !46
  %54 = load i32, i32* %y, align 4, !dbg !46
  store i16 %53, i16* %3, !dbg !47
  store i32 %54, i32* %loop, align 4, !dbg !47
  %55 = call zeroext i16 @__dfsan_union(i16 zeroext 0, i16 zeroext 0, i32 0, i32 0, i64 0, i16 54, i8* getelementptr inbounds ([8 x i8], [8 x i8]* @19, i32 0, i32 0)), !dbg !48
  %56 = call zeroext i16 @__dfsan_union(i16 zeroext %55, i16 zeroext 0, i32 0, i32 0, i64 0, i16 54, i8* getelementptr inbounds ([8 x i8], [8 x i8]* @20, i32 0, i32 0)), !dbg !48
  %57 = call zeroext i16 @__dfsan_union(i16 zeroext %56, i16 zeroext 0, i32 0, i32 0, i64 0, i16 54, i8* getelementptr inbounds ([8 x i8], [8 x i8]* @21, i32 0, i32 0)), !dbg !48
  call void @llvm.dbg.declare(metadata i32* %i, metadata !50, metadata !DIExpression()), !dbg !48
  store i16 0, i16* %5, !dbg !48
  store i32 1, i32* %i, align 4, !dbg !48
  br label %for.cond, !dbg !51

for.cond:                                         ; preds = %for.inc, %if.then
  %58 = load i16, i16* %5, !dbg !52
  %59 = load i32, i32* %i, align 4, !dbg !52
  %60 = call zeroext i16 @__dfsan_union(i16 zeroext %58, i16 zeroext 0, i32 0, i32 0, i64 0, i16 51, i8* getelementptr inbounds ([8 x i8], [8 x i8]* @22, i32 0, i32 0)), !dbg !54
  %cmp1 = icmp slt i32 %59, 5, !dbg !54
  call void @__branch_visitor_int(i16 zeroext %58, i16 zeroext 0, i32 %59, i32 5, i1 %cmp1, i32 40, i64 438997255675854297, i64 1, i16 0, i8* getelementptr inbounds ([14 x i8], [14 x i8]* @23, i32 0, i32 0)), !dbg !55
  br i1 %cmp1, label %for.body, label %for.end, !dbg !55

for.body:                                         ; preds = %for.cond
  %61 = load i16, i16* %3, !dbg !56
  %62 = load i32, i32* %loop, align 4, !dbg !56
  %63 = load i16, i16* %5, !dbg !58
  %64 = load i32, i32* %i, align 4, !dbg !58
  %65 = call zeroext i16 @__dfsan_union(i16 zeroext %61, i16 zeroext %63, i32 %62, i32 %64, i64 0, i16 15, i8* getelementptr inbounds ([14 x i8], [14 x i8]* @24, i32 0, i32 0)), !dbg !59
  %mul2 = mul nsw i32 %62, %64, !dbg !59
  store i16 %65, i16* %3, !dbg !60
  store i32 %mul2, i32* %loop, align 4, !dbg !60
  br label %for.inc, !dbg !61

for.inc:                                          ; preds = %for.body
  %66 = load i16, i16* %5, !dbg !62
  %67 = load i32, i32* %i, align 4, !dbg !62
  %68 = call zeroext i16 @__dfsan_union(i16 zeroext %66, i16 zeroext 0, i32 %67, i32 1, i64 0, i16 11, i8* getelementptr inbounds ([14 x i8], [14 x i8]* @25, i32 0, i32 0)), !dbg !62
  %inc = add nsw i32 %67, 1, !dbg !62
  store i16 %68, i16* %5, !dbg !62
  store i32 %inc, i32* %i, align 4, !dbg !62
  br label %for.cond, !dbg !63, !llvm.loop !64

for.end:                                          ; preds = %for.cond
  br label %if.end, !dbg !66

if.end:                                           ; preds = %for.end, %entry
  %69 = call zeroext i16 @__dfsan_union(i16 zeroext 0, i16 zeroext 0, i32 0, i32 0, i64 0, i16 54, i8* getelementptr inbounds ([8 x i8], [8 x i8]* @26, i32 0, i32 0)), !dbg !67
  %70 = call zeroext i16 @__dfsan_union(i16 zeroext %69, i16 zeroext 0, i32 0, i32 0, i64 0, i16 54, i8* getelementptr inbounds ([8 x i8], [8 x i8]* @27, i32 0, i32 0)), !dbg !67
  %71 = call zeroext i16 @__dfsan_union(i16 zeroext %70, i16 zeroext 0, i32 0, i32 0, i64 0, i16 54, i8* getelementptr inbounds ([8 x i8], [8 x i8]* @28, i32 0, i32 0)), !dbg !67
  call void @llvm.dbg.declare(metadata i16* %y_label, metadata !68, metadata !DIExpression()), !dbg !67
  %72 = load i16, i16* %1, !dbg !69
  %73 = load i32, i32* %y, align 4, !dbg !69
  %conv = sext i32 %73 to i64, !dbg !69
  %74 = call zeroext i16 @__dfsw_dfsan_get_label(i64 %conv, i16 zeroext %72, i16* %labelreturn), !dbg !70
  %75 = load i16, i16* %labelreturn, !dbg !70
  store i16 %75, i16* %6, !dbg !67
  store i16 %74, i16* %y_label, align 2, !dbg !67
  %76 = call zeroext i16 @__dfsan_union(i16 zeroext 0, i16 zeroext 0, i32 0, i32 0, i64 0, i16 54, i8* getelementptr inbounds ([8 x i8], [8 x i8]* @29, i32 0, i32 0)), !dbg !71
  %77 = call zeroext i16 @__dfsan_union(i16 zeroext %76, i16 zeroext 0, i32 0, i32 0, i64 0, i16 54, i8* getelementptr inbounds ([8 x i8], [8 x i8]* @30, i32 0, i32 0)), !dbg !71
  %78 = call zeroext i16 @__dfsan_union(i16 zeroext %77, i16 zeroext 0, i32 0, i32 0, i64 0, i16 54, i8* getelementptr inbounds ([8 x i8], [8 x i8]* @31, i32 0, i32 0)), !dbg !71
  call void @llvm.dbg.declare(metadata i16* %z_label, metadata !72, metadata !DIExpression()), !dbg !71
  %79 = load i16, i16* %2, !dbg !73
  %80 = load i32, i32* %z, align 4, !dbg !73
  %conv4 = sext i32 %80 to i64, !dbg !73
  %81 = call zeroext i16 @__dfsw_dfsan_get_label(i64 %conv4, i16 zeroext %79, i16* %labelreturn), !dbg !74
  %82 = load i16, i16* %labelreturn, !dbg !74
  store i16 %82, i16* %7, !dbg !71
  store i16 %81, i16* %z_label, align 2, !dbg !71
  %83 = call zeroext i16 @__dfsan_union(i16 zeroext 0, i16 zeroext 0, i32 0, i32 0, i64 0, i16 54, i8* getelementptr inbounds ([8 x i8], [8 x i8]* @32, i32 0, i32 0)), !dbg !75
  %84 = call zeroext i16 @__dfsan_union(i16 zeroext %83, i16 zeroext 0, i32 0, i32 0, i64 0, i16 54, i8* getelementptr inbounds ([8 x i8], [8 x i8]* @33, i32 0, i32 0)), !dbg !75
  %85 = call zeroext i16 @__dfsan_union(i16 zeroext %84, i16 zeroext 0, i32 0, i32 0, i64 0, i16 54, i8* getelementptr inbounds ([8 x i8], [8 x i8]* @34, i32 0, i32 0)), !dbg !75
  call void @llvm.dbg.declare(metadata i16* %loop_label, metadata !76, metadata !DIExpression()), !dbg !75
  %86 = load i16, i16* %3, !dbg !77
  %87 = load i32, i32* %loop, align 4, !dbg !77
  %conv6 = sext i32 %87 to i64, !dbg !77
  %88 = call zeroext i16 @__dfsw_dfsan_get_label(i64 %conv6, i16 zeroext %86, i16* %labelreturn), !dbg !78
  %89 = load i16, i16* %labelreturn, !dbg !78
  store i16 %89, i16* %8, !dbg !75
  store i16 %88, i16* %loop_label, align 2, !dbg !75
  %90 = call zeroext i16 @__dfsan_union(i16 zeroext 0, i16 zeroext 0, i32 0, i32 0, i64 0, i16 54, i8* getelementptr inbounds ([8 x i8], [8 x i8]* @35, i32 0, i32 0)), !dbg !79
  %91 = call zeroext i16 @__dfsan_union(i16 zeroext %90, i16 zeroext 0, i32 0, i32 0, i64 0, i16 54, i8* getelementptr inbounds ([8 x i8], [8 x i8]* @36, i32 0, i32 0)), !dbg !79
  %92 = call zeroext i16 @__dfsan_union(i16 zeroext %91, i16 zeroext 0, i32 0, i32 0, i64 0, i16 54, i8* getelementptr inbounds ([8 x i8], [8 x i8]* @37, i32 0, i32 0)), !dbg !79
  call void @llvm.dbg.declare(metadata %struct.dfsan_label_info** %x_info, metadata !80, metadata !DIExpression()), !dbg !79
  %93 = load i16, i16* %4, !dbg !96
  %94 = load i16, i16* %x_label, align 2, !dbg !96
  %call8 = call %struct.dfsan_label_info* @dfsan_get_label_info(i16 zeroext %94), !dbg !97
  store i16 0, i16* %9, !dbg !79
  store %struct.dfsan_label_info* %call8, %struct.dfsan_label_info** %x_info, align 8, !dbg !79
  %95 = call zeroext i16 @__dfsan_union(i16 zeroext 0, i16 zeroext 0, i32 0, i32 0, i64 0, i16 54, i8* getelementptr inbounds ([8 x i8], [8 x i8]* @38, i32 0, i32 0)), !dbg !98
  %96 = call zeroext i16 @__dfsan_union(i16 zeroext %95, i16 zeroext 0, i32 0, i32 0, i64 0, i16 54, i8* getelementptr inbounds ([8 x i8], [8 x i8]* @39, i32 0, i32 0)), !dbg !98
  %97 = call zeroext i16 @__dfsan_union(i16 zeroext %96, i16 zeroext 0, i32 0, i32 0, i64 0, i16 54, i8* getelementptr inbounds ([8 x i8], [8 x i8]* @40, i32 0, i32 0)), !dbg !98
  call void @llvm.dbg.declare(metadata %struct.dfsan_label_info** %y_info, metadata !99, metadata !DIExpression()), !dbg !98
  %98 = load i16, i16* %6, !dbg !100
  %99 = load i16, i16* %y_label, align 2, !dbg !100
  %call9 = call %struct.dfsan_label_info* @dfsan_get_label_info(i16 zeroext %99), !dbg !101
  store i16 0, i16* %10, !dbg !98
  store %struct.dfsan_label_info* %call9, %struct.dfsan_label_info** %y_info, align 8, !dbg !98
  %100 = call zeroext i16 @__dfsan_union(i16 zeroext 0, i16 zeroext 0, i32 0, i32 0, i64 0, i16 54, i8* getelementptr inbounds ([8 x i8], [8 x i8]* @41, i32 0, i32 0)), !dbg !102
  %101 = call zeroext i16 @__dfsan_union(i16 zeroext %100, i16 zeroext 0, i32 0, i32 0, i64 0, i16 54, i8* getelementptr inbounds ([8 x i8], [8 x i8]* @42, i32 0, i32 0)), !dbg !102
  %102 = call zeroext i16 @__dfsan_union(i16 zeroext %101, i16 zeroext 0, i32 0, i32 0, i64 0, i16 54, i8* getelementptr inbounds ([8 x i8], [8 x i8]* @43, i32 0, i32 0)), !dbg !102
  call void @llvm.dbg.declare(metadata %struct.dfsan_label_info** %z_info, metadata !103, metadata !DIExpression()), !dbg !102
  %103 = load i16, i16* %7, !dbg !104
  %104 = load i16, i16* %z_label, align 2, !dbg !104
  %call10 = call %struct.dfsan_label_info* @dfsan_get_label_info(i16 zeroext %104), !dbg !105
  store i16 0, i16* %11, !dbg !102
  store %struct.dfsan_label_info* %call10, %struct.dfsan_label_info** %z_info, align 8, !dbg !102
  %105 = call zeroext i16 @__dfsan_union(i16 zeroext 0, i16 zeroext 0, i32 0, i32 0, i64 0, i16 54, i8* getelementptr inbounds ([8 x i8], [8 x i8]* @44, i32 0, i32 0)), !dbg !106
  %106 = call zeroext i16 @__dfsan_union(i16 zeroext %105, i16 zeroext 0, i32 0, i32 0, i64 0, i16 54, i8* getelementptr inbounds ([8 x i8], [8 x i8]* @45, i32 0, i32 0)), !dbg !106
  %107 = call zeroext i16 @__dfsan_union(i16 zeroext %106, i16 zeroext 0, i32 0, i32 0, i64 0, i16 54, i8* getelementptr inbounds ([8 x i8], [8 x i8]* @46, i32 0, i32 0)), !dbg !106
  call void @llvm.dbg.declare(metadata %struct.dfsan_label_info** %loop_info, metadata !107, metadata !DIExpression()), !dbg !106
  %108 = load i16, i16* %8, !dbg !108
  %109 = load i16, i16* %loop_label, align 2, !dbg !108
  %call11 = call %struct.dfsan_label_info* @dfsan_get_label_info(i16 zeroext %109), !dbg !109
  store i16 0, i16* %12, !dbg !106
  store %struct.dfsan_label_info* %call11, %struct.dfsan_label_info** %loop_info, align 8, !dbg !106
  %110 = load i16, i16* %4, !dbg !110
  %111 = load i16, i16* %x_label, align 2, !dbg !110
  %conv12 = zext i16 %111 to i32, !dbg !110
  %112 = load i16, i16* %9, !dbg !111
  %113 = load %struct.dfsan_label_info*, %struct.dfsan_label_info** %x_info, align 8, !dbg !111
  %114 = call zeroext i16 @__dfsan_union(i16 zeroext %112, i16 zeroext 0, i32 0, i32 0, i64 0, i16 32, i8* getelementptr inbounds ([8 x i8], [8 x i8]* @47, i32 0, i32 0)), !dbg !112
  %115 = call zeroext i16 @__dfsan_union(i16 zeroext %114, i16 zeroext 0, i32 0, i32 0, i64 0, i16 32, i8* getelementptr inbounds ([8 x i8], [8 x i8]* @48, i32 0, i32 0)), !dbg !112
  %neg_dydx = getelementptr inbounds %struct.dfsan_label_info, %struct.dfsan_label_info* %113, i32 0, i32 3, !dbg !112
  %116 = ptrtoint float* %neg_dydx to i64, !dbg !112
  %117 = and i64 %116, -123145302310913, !dbg !112
  %118 = mul i64 %117, 2, !dbg !112
  %119 = inttoptr i64 %118 to i16*, !dbg !112
  %120 = load i16, i16* %119, align 2
  %121 = load float, float* %neg_dydx, align 8, !dbg !112
  %conv13 = fpext float %121 to double, !dbg !111
  %122 = load i16, i16* %9, !dbg !113
  %123 = load %struct.dfsan_label_info*, %struct.dfsan_label_info** %x_info, align 8, !dbg !113
  %124 = call zeroext i16 @__dfsan_union(i16 zeroext %122, i16 zeroext 0, i32 0, i32 0, i64 0, i16 32, i8* getelementptr inbounds ([8 x i8], [8 x i8]* @49, i32 0, i32 0)), !dbg !114
  %125 = call zeroext i16 @__dfsan_union(i16 zeroext %124, i16 zeroext 0, i32 0, i32 0, i64 0, i16 32, i8* getelementptr inbounds ([8 x i8], [8 x i8]* @50, i32 0, i32 0)), !dbg !114
  %pos_dydx = getelementptr inbounds %struct.dfsan_label_info, %struct.dfsan_label_info* %123, i32 0, i32 4, !dbg !114
  %126 = ptrtoint float* %pos_dydx to i64, !dbg !114
  %127 = and i64 %126, -123145302310913, !dbg !114
  %128 = mul i64 %127, 2, !dbg !114
  %129 = inttoptr i64 %128 to i16*, !dbg !114
  %130 = load i16, i16* %129, align 2
  %131 = load float, float* %pos_dydx, align 4, !dbg !114
  %conv14 = fpext float %131 to double, !dbg !113
  %call15 = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([20 x i8], [20 x i8]* @.str.1, i32 0, i32 0), i32 %conv12, double %conv13, double %conv14), !dbg !115
  %132 = load i16, i16* %6, !dbg !116
  %133 = load i16, i16* %y_label, align 2, !dbg !116
  %conv16 = zext i16 %133 to i32, !dbg !116
  %134 = load i16, i16* %10, !dbg !117
  %135 = load %struct.dfsan_label_info*, %struct.dfsan_label_info** %y_info, align 8, !dbg !117
  %136 = call zeroext i16 @__dfsan_union(i16 zeroext %134, i16 zeroext 0, i32 0, i32 0, i64 0, i16 32, i8* getelementptr inbounds ([8 x i8], [8 x i8]* @51, i32 0, i32 0)), !dbg !118
  %137 = call zeroext i16 @__dfsan_union(i16 zeroext %136, i16 zeroext 0, i32 0, i32 0, i64 0, i16 32, i8* getelementptr inbounds ([8 x i8], [8 x i8]* @52, i32 0, i32 0)), !dbg !118
  %neg_dydx17 = getelementptr inbounds %struct.dfsan_label_info, %struct.dfsan_label_info* %135, i32 0, i32 3, !dbg !118
  %138 = ptrtoint float* %neg_dydx17 to i64, !dbg !118
  %139 = and i64 %138, -123145302310913, !dbg !118
  %140 = mul i64 %139, 2, !dbg !118
  %141 = inttoptr i64 %140 to i16*, !dbg !118
  %142 = load i16, i16* %141, align 2
  %143 = load float, float* %neg_dydx17, align 8, !dbg !118
  %conv18 = fpext float %143 to double, !dbg !117
  %144 = load i16, i16* %10, !dbg !119
  %145 = load %struct.dfsan_label_info*, %struct.dfsan_label_info** %y_info, align 8, !dbg !119
  %146 = call zeroext i16 @__dfsan_union(i16 zeroext %144, i16 zeroext 0, i32 0, i32 0, i64 0, i16 32, i8* getelementptr inbounds ([8 x i8], [8 x i8]* @53, i32 0, i32 0)), !dbg !120
  %147 = call zeroext i16 @__dfsan_union(i16 zeroext %146, i16 zeroext 0, i32 0, i32 0, i64 0, i16 32, i8* getelementptr inbounds ([8 x i8], [8 x i8]* @54, i32 0, i32 0)), !dbg !120
  %pos_dydx19 = getelementptr inbounds %struct.dfsan_label_info, %struct.dfsan_label_info* %145, i32 0, i32 4, !dbg !120
  %148 = ptrtoint float* %pos_dydx19 to i64, !dbg !120
  %149 = and i64 %148, -123145302310913, !dbg !120
  %150 = mul i64 %149, 2, !dbg !120
  %151 = inttoptr i64 %150 to i16*, !dbg !120
  %152 = load i16, i16* %151, align 2
  %153 = load float, float* %pos_dydx19, align 4, !dbg !120
  %conv20 = fpext float %153 to double, !dbg !119
  %call21 = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([20 x i8], [20 x i8]* @.str.2, i32 0, i32 0), i32 %conv16, double %conv18, double %conv20), !dbg !121
  %154 = load i16, i16* %7, !dbg !122
  %155 = load i16, i16* %z_label, align 2, !dbg !122
  %conv22 = zext i16 %155 to i32, !dbg !122
  %156 = load i16, i16* %11, !dbg !123
  %157 = load %struct.dfsan_label_info*, %struct.dfsan_label_info** %z_info, align 8, !dbg !123
  %158 = call zeroext i16 @__dfsan_union(i16 zeroext %156, i16 zeroext 0, i32 0, i32 0, i64 0, i16 32, i8* getelementptr inbounds ([8 x i8], [8 x i8]* @55, i32 0, i32 0)), !dbg !124
  %159 = call zeroext i16 @__dfsan_union(i16 zeroext %158, i16 zeroext 0, i32 0, i32 0, i64 0, i16 32, i8* getelementptr inbounds ([8 x i8], [8 x i8]* @56, i32 0, i32 0)), !dbg !124
  %neg_dydx23 = getelementptr inbounds %struct.dfsan_label_info, %struct.dfsan_label_info* %157, i32 0, i32 3, !dbg !124
  %160 = ptrtoint float* %neg_dydx23 to i64, !dbg !124
  %161 = and i64 %160, -123145302310913, !dbg !124
  %162 = mul i64 %161, 2, !dbg !124
  %163 = inttoptr i64 %162 to i16*, !dbg !124
  %164 = load i16, i16* %163, align 2
  %165 = load float, float* %neg_dydx23, align 8, !dbg !124
  %conv24 = fpext float %165 to double, !dbg !123
  %166 = load i16, i16* %11, !dbg !125
  %167 = load %struct.dfsan_label_info*, %struct.dfsan_label_info** %z_info, align 8, !dbg !125
  %168 = call zeroext i16 @__dfsan_union(i16 zeroext %166, i16 zeroext 0, i32 0, i32 0, i64 0, i16 32, i8* getelementptr inbounds ([8 x i8], [8 x i8]* @57, i32 0, i32 0)), !dbg !126
  %169 = call zeroext i16 @__dfsan_union(i16 zeroext %168, i16 zeroext 0, i32 0, i32 0, i64 0, i16 32, i8* getelementptr inbounds ([8 x i8], [8 x i8]* @58, i32 0, i32 0)), !dbg !126
  %pos_dydx25 = getelementptr inbounds %struct.dfsan_label_info, %struct.dfsan_label_info* %167, i32 0, i32 4, !dbg !126
  %170 = ptrtoint float* %pos_dydx25 to i64, !dbg !126
  %171 = and i64 %170, -123145302310913, !dbg !126
  %172 = mul i64 %171, 2, !dbg !126
  %173 = inttoptr i64 %172 to i16*, !dbg !126
  %174 = load i16, i16* %173, align 2
  %175 = load float, float* %pos_dydx25, align 4, !dbg !126
  %conv26 = fpext float %175 to double, !dbg !125
  %call27 = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([20 x i8], [20 x i8]* @.str.3, i32 0, i32 0), i32 %conv22, double %conv24, double %conv26), !dbg !127
  %176 = load i16, i16* %8, !dbg !128
  %177 = load i16, i16* %loop_label, align 2, !dbg !128
  %conv28 = zext i16 %177 to i32, !dbg !128
  %178 = load i16, i16* %12, !dbg !129
  %179 = load %struct.dfsan_label_info*, %struct.dfsan_label_info** %loop_info, align 8, !dbg !129
  %180 = call zeroext i16 @__dfsan_union(i16 zeroext %178, i16 zeroext 0, i32 0, i32 0, i64 0, i16 32, i8* getelementptr inbounds ([8 x i8], [8 x i8]* @59, i32 0, i32 0)), !dbg !130
  %181 = call zeroext i16 @__dfsan_union(i16 zeroext %180, i16 zeroext 0, i32 0, i32 0, i64 0, i16 32, i8* getelementptr inbounds ([8 x i8], [8 x i8]* @60, i32 0, i32 0)), !dbg !130
  %neg_dydx29 = getelementptr inbounds %struct.dfsan_label_info, %struct.dfsan_label_info* %179, i32 0, i32 3, !dbg !130
  %182 = ptrtoint float* %neg_dydx29 to i64, !dbg !130
  %183 = and i64 %182, -123145302310913, !dbg !130
  %184 = mul i64 %183, 2, !dbg !130
  %185 = inttoptr i64 %184 to i16*, !dbg !130
  %186 = load i16, i16* %185, align 2
  %187 = load float, float* %neg_dydx29, align 8, !dbg !130
  %conv30 = fpext float %187 to double, !dbg !129
  %188 = load i16, i16* %12, !dbg !131
  %189 = load %struct.dfsan_label_info*, %struct.dfsan_label_info** %loop_info, align 8, !dbg !131
  %190 = call zeroext i16 @__dfsan_union(i16 zeroext %188, i16 zeroext 0, i32 0, i32 0, i64 0, i16 32, i8* getelementptr inbounds ([8 x i8], [8 x i8]* @61, i32 0, i32 0)), !dbg !132
  %191 = call zeroext i16 @__dfsan_union(i16 zeroext %190, i16 zeroext 0, i32 0, i32 0, i64 0, i16 32, i8* getelementptr inbounds ([8 x i8], [8 x i8]* @62, i32 0, i32 0)), !dbg !132
  %pos_dydx31 = getelementptr inbounds %struct.dfsan_label_info, %struct.dfsan_label_info* %189, i32 0, i32 4, !dbg !132
  %192 = ptrtoint float* %pos_dydx31 to i64, !dbg !132
  %193 = and i64 %192, -123145302310913, !dbg !132
  %194 = mul i64 %193, 2, !dbg !132
  %195 = inttoptr i64 %194 to i16*, !dbg !132
  %196 = load i16, i16* %195, align 2
  %197 = load float, float* %pos_dydx31, align 4, !dbg !132
  %conv32 = fpext float %197 to double, !dbg !131
  %call33 = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.4, i32 0, i32 0), i32 %conv28, double %conv30, double %conv32), !dbg !133
  ret i32 0, !dbg !134
}

; Function Attrs: nounwind readnone speculatable
declare void @llvm.dbg.declare(metadata, metadata, metadata) #1

declare zeroext i16 @dfsan_create_label(i8*) #2

declare void @dfsan_set_label(i16 zeroext, i8*, i64) #2

declare zeroext i16 @dfsan_get_label(i64) #2

declare %struct.dfsan_label_info* @dfsan_get_label_info(i16 zeroext) #2

declare i32 @printf(i8*, ...) #2

declare void @__memcpy(i8*, i8*, i64 zeroext, i16 zeroext, i16 zeroext, i16 zeroext, i8*)

declare void @__basicblock(i64, i64)

declare void @__branch_visitor_char(i16 zeroext, i16 zeroext, i8, i8, i1, i32, i64, i64, i16, i8*)

declare void @__branch_visitor_short(i16 zeroext, i16 zeroext, i16, i16, i1, i32, i64, i64, i16, i8*)

declare void @__branch_visitor_int(i16 zeroext, i16 zeroext, i32, i32, i1, i32, i64, i64, i16, i8*)

declare void @__branch_visitor_long(i16 zeroext, i16 zeroext, i64, i64, i1, i32, i64, i64, i16, i8*)

declare void @__branch_visitor_longlong(i16 zeroext, i16 zeroext, i128, i128, i1, i32, i64, i64, i16, i8*)

declare void @__branch_visitor_float(i16 zeroext, i16 zeroext, float, float, i1, i32, i64, i64, i16, i8*)

declare void @__branch_visitor_double(i16 zeroext, i16 zeroext, double, double, i1, i32, i64, i64, i16, i8*)

; Function Attrs: nounwind readnone
declare zeroext i16 @__dfsan_union_unsupported_type(i16 zeroext, i16 zeroext, i64, i16, i8*) #3

; Function Attrs: nounwind readnone
declare zeroext i16 @__dfsan_union(i16 zeroext, i16 zeroext, i32, i32, i64, i16, i8*) #3

; Function Attrs: nounwind readnone
declare zeroext i16 @__dfsan_union_long(i16 zeroext, i16 zeroext, i64, i64, i64, i16, i8*) #3

; Function Attrs: nounwind readnone
declare zeroext i16 @__dfsan_union_byte(i16 zeroext, i16 zeroext, i8, i8, i64, i16, i8*) #3

; Function Attrs: nounwind readnone
declare zeroext i16 @__dfsan_union_short(i16 zeroext, i16 zeroext, i16, i16, i64, i16, i8*) #3

; Function Attrs: nounwind readnone
declare zeroext i16 @__dfsan_union_float(i16 zeroext, i16 zeroext, float, float, i64, i16, i8*) #3

; Function Attrs: nounwind readnone
declare zeroext i16 @__dfsan_union_double(i16 zeroext, i16 zeroext, double, double, i64, i16, i8*) #3

; Function Attrs: nounwind readonly
declare zeroext i16 @__dfsan_union_load(i16*, i64) #4

declare void @__dfsan_unimplemented(i8*)

declare void @__dfsan_set_label(i16 zeroext, i8*, i64)

declare void @__dfsan_nonzero_label()

declare void @__dfsan_vararg_wrapper(i8*)

; Function Attrs: noinline nounwind optnone uwtable
define linkonce_odr dso_local i32 @"dfsw$main"() #0 {
entry:
  %0 = call i32 @main()
  store i16 0, i16* @__dfsan_retval_tls
  ret i32 %0
}

define linkonce_odr zeroext i16 @"dfsw$dfsan_create_label"(i8*) #2 {
entry:
  %1 = call i16 @dfsan_create_label(i8* %0)
  store i16 0, i16* @__dfsan_retval_tls
  ret i16 %1
}

define linkonce_odr void @"dfsw$dfsan_set_label"(i16 zeroext, i8*, i64) #2 {
entry:
  call void @dfsan_set_label(i16 %0, i8* %1, i64 %2)
  ret void
}

define linkonce_odr zeroext i16 @"dfsw$dfsan_get_label"(i64) #2 {
entry:
  %labelreturn = alloca i16
  %1 = load i16, i16* getelementptr inbounds ([64 x i16], [64 x i16]* @__dfsan_arg_tls, i64 0, i64 0)
  %2 = call i16 @__dfsw_dfsan_get_label(i64 %0, i16 zeroext %1, i16* %labelreturn)
  %3 = load i16, i16* %labelreturn
  store i16 %3, i16* @__dfsan_retval_tls
  ret i16 %2
}

define linkonce_odr %struct.dfsan_label_info* @"dfsw$dfsan_get_label_info"(i16 zeroext) #2 {
entry:
  %1 = call %struct.dfsan_label_info* @dfsan_get_label_info(i16 %0)
  store i16 0, i16* @__dfsan_retval_tls
  ret %struct.dfsan_label_info* %1
}

declare zeroext i16 @__dfsw_dfsan_get_label(i64, i16, i16*) #2

attributes #0 = { noinline nounwind optnone uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { nounwind readnone speculatable }
attributes #2 = { "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #3 = { nounwind readnone }
attributes #4 = { nounwind readonly }

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!3, !4, !5, !6, !7}
!llvm.ident = !{!8}

!0 = distinct !DICompileUnit(language: DW_LANG_C99, file: !1, producer: "clang version 7.0.0 ", isOptimized: false, runtimeVersion: 0, emissionKind: FullDebug, enums: !2)
!1 = !DIFile(filename: "test_int.c", directory: "/data/gryan/proxgrad/gradtest/artifacts/usenix2021_pga/example")
!2 = !{}
!3 = !{i32 2, !"Dwarf Version", i32 4}
!4 = !{i32 2, !"Debug Info Version", i32 3}
!5 = !{i32 1, !"wchar_size", i32 4}
!6 = !{i32 7, !"PIC Level", i32 2}
!7 = !{i32 7, !"PIE Level", i32 2}
!8 = !{!"clang version 7.0.0 "}
!9 = distinct !DISubprogram(name: "main", scope: !1, file: !1, line: 5, type: !10, isLocal: false, isDefinition: true, scopeLine: 5, flags: DIFlagPrototyped, isOptimized: false, unit: !0, retainedNodes: !2)
!10 = !DISubroutineType(types: !11)
!11 = !{!12}
!12 = !DIBasicType(name: "int", size: 32, encoding: DW_ATE_signed)
!13 = !DILocation(line: 7, column: 7, scope: !9)
!14 = !DILocalVariable(name: "x", scope: !9, file: !1, line: 7, type: !12)
!15 = !DILocation(line: 7, column: 10, scope: !9)
!16 = !DILocalVariable(name: "y", scope: !9, file: !1, line: 7, type: !12)
!17 = !DILocation(line: 7, column: 13, scope: !9)
!18 = !DILocalVariable(name: "z", scope: !9, file: !1, line: 7, type: !12)
!19 = !DILocation(line: 7, column: 16, scope: !9)
!20 = !DILocalVariable(name: "loop", scope: !9, file: !1, line: 7, type: !12)
!21 = !DILocation(line: 8, column: 5, scope: !9)
!22 = !DILocation(line: 10, column: 15, scope: !9)
!23 = !DILocalVariable(name: "x_label", scope: !9, file: !1, line: 10, type: !24)
!24 = !DIDerivedType(tag: DW_TAG_typedef, name: "dfsan_label", file: !25, line: 25, baseType: !26)
!25 = !DIFile(filename: "/data/gryan/proxgrad/gradtest/artifacts/usenix2021_pga/build/lib/clang/7.0.0/include/sanitizer/dfsan_interface.h", directory: "/data/gryan/proxgrad/gradtest/artifacts/usenix2021_pga/example")
!26 = !DIDerivedType(tag: DW_TAG_typedef, name: "uint16_t", file: !27, line: 25, baseType: !28)
!27 = !DIFile(filename: "/usr/include/x86_64-linux-gnu/bits/stdint-uintn.h", directory: "/data/gryan/proxgrad/gradtest/artifacts/usenix2021_pga/example")
!28 = !DIDerivedType(tag: DW_TAG_typedef, name: "__uint16_t", file: !29, line: 39, baseType: !30)
!29 = !DIFile(filename: "/usr/include/x86_64-linux-gnu/bits/types.h", directory: "/data/gryan/proxgrad/gradtest/artifacts/usenix2021_pga/example")
!30 = !DIBasicType(name: "unsigned short", size: 16, encoding: DW_ATE_unsigned)
!31 = !DILocation(line: 10, column: 25, scope: !9)
!32 = !DILocation(line: 11, column: 19, scope: !9)
!33 = !DILocation(line: 11, column: 28, scope: !9)
!34 = !DILocation(line: 11, column: 3, scope: !9)
!35 = !DILocation(line: 13, column: 7, scope: !36)
!36 = distinct !DILexicalBlock(scope: !9, file: !1, line: 13, column: 7)
!37 = !DILocation(line: 13, column: 9, scope: !36)
!38 = !DILocation(line: 13, column: 7, scope: !9)
!39 = !DILocation(line: 14, column: 13, scope: !40)
!40 = distinct !DILexicalBlock(scope: !36, file: !1, line: 13, column: 14)
!41 = !DILocation(line: 14, column: 11, scope: !40)
!42 = !DILocation(line: 14, column: 7, scope: !40)
!43 = !DILocation(line: 15, column: 9, scope: !40)
!44 = !DILocation(line: 15, column: 11, scope: !40)
!45 = !DILocation(line: 15, column: 7, scope: !40)
!46 = !DILocation(line: 17, column: 12, scope: !40)
!47 = !DILocation(line: 17, column: 10, scope: !40)
!48 = !DILocation(line: 18, column: 14, scope: !49)
!49 = distinct !DILexicalBlock(scope: !40, file: !1, line: 18, column: 5)
!50 = !DILocalVariable(name: "i", scope: !49, file: !1, line: 18, type: !12)
!51 = !DILocation(line: 18, column: 10, scope: !49)
!52 = !DILocation(line: 18, column: 19, scope: !53)
!53 = distinct !DILexicalBlock(scope: !49, file: !1, line: 18, column: 5)
!54 = !DILocation(line: 18, column: 20, scope: !53)
!55 = !DILocation(line: 18, column: 5, scope: !49)
!56 = !DILocation(line: 19, column: 14, scope: !57)
!57 = distinct !DILexicalBlock(scope: !53, file: !1, line: 18, column: 29)
!58 = !DILocation(line: 19, column: 21, scope: !57)
!59 = !DILocation(line: 19, column: 19, scope: !57)
!60 = !DILocation(line: 19, column: 12, scope: !57)
!61 = !DILocation(line: 20, column: 5, scope: !57)
!62 = !DILocation(line: 18, column: 25, scope: !53)
!63 = !DILocation(line: 18, column: 5, scope: !53)
!64 = distinct !{!64, !55, !65}
!65 = !DILocation(line: 20, column: 5, scope: !49)
!66 = !DILocation(line: 21, column: 3, scope: !40)
!67 = !DILocation(line: 24, column: 15, scope: !9)
!68 = !DILocalVariable(name: "y_label", scope: !9, file: !1, line: 24, type: !24)
!69 = !DILocation(line: 24, column: 41, scope: !9)
!70 = !DILocation(line: 24, column: 25, scope: !9)
!71 = !DILocation(line: 25, column: 15, scope: !9)
!72 = !DILocalVariable(name: "z_label", scope: !9, file: !1, line: 25, type: !24)
!73 = !DILocation(line: 25, column: 41, scope: !9)
!74 = !DILocation(line: 25, column: 25, scope: !9)
!75 = !DILocation(line: 26, column: 15, scope: !9)
!76 = !DILocalVariable(name: "loop_label", scope: !9, file: !1, line: 26, type: !24)
!77 = !DILocation(line: 26, column: 44, scope: !9)
!78 = !DILocation(line: 26, column: 28, scope: !9)
!79 = !DILocation(line: 28, column: 34, scope: !9)
!80 = !DILocalVariable(name: "x_info", scope: !9, file: !1, line: 28, type: !81)
!81 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !82, size: 64)
!82 = !DIDerivedType(tag: DW_TAG_const_type, baseType: !83)
!83 = distinct !DICompositeType(tag: DW_TAG_structure_type, name: "dfsan_label_info", file: !25, line: 33, size: 256, elements: !84)
!84 = !{!85, !86, !87, !91, !93, !94, !95}
!85 = !DIDerivedType(tag: DW_TAG_member, name: "l1", scope: !83, file: !25, line: 34, baseType: !24, size: 16)
!86 = !DIDerivedType(tag: DW_TAG_member, name: "l2", scope: !83, file: !25, line: 35, baseType: !24, size: 16, offset: 16)
!87 = !DIDerivedType(tag: DW_TAG_member, name: "loc", scope: !83, file: !25, line: 36, baseType: !88, size: 64, offset: 64)
!88 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !89, size: 64)
!89 = !DIDerivedType(tag: DW_TAG_const_type, baseType: !90)
!90 = !DIBasicType(name: "char", size: 8, encoding: DW_ATE_signed_char)
!91 = !DIDerivedType(tag: DW_TAG_member, name: "neg_dydx", scope: !83, file: !25, line: 37, baseType: !92, size: 32, offset: 128)
!92 = !DIBasicType(name: "float", size: 32, encoding: DW_ATE_float)
!93 = !DIDerivedType(tag: DW_TAG_member, name: "pos_dydx", scope: !83, file: !25, line: 38, baseType: !92, size: 32, offset: 160)
!94 = !DIDerivedType(tag: DW_TAG_member, name: "opcode", scope: !83, file: !25, line: 39, baseType: !24, size: 16, offset: 192)
!95 = !DIDerivedType(tag: DW_TAG_member, name: "f_val", scope: !83, file: !25, line: 40, baseType: !12, size: 32, offset: 224)
!96 = !DILocation(line: 28, column: 64, scope: !9)
!97 = !DILocation(line: 28, column: 43, scope: !9)
!98 = !DILocation(line: 29, column: 34, scope: !9)
!99 = !DILocalVariable(name: "y_info", scope: !9, file: !1, line: 29, type: !81)
!100 = !DILocation(line: 29, column: 64, scope: !9)
!101 = !DILocation(line: 29, column: 43, scope: !9)
!102 = !DILocation(line: 30, column: 34, scope: !9)
!103 = !DILocalVariable(name: "z_info", scope: !9, file: !1, line: 30, type: !81)
!104 = !DILocation(line: 30, column: 64, scope: !9)
!105 = !DILocation(line: 30, column: 43, scope: !9)
!106 = !DILocation(line: 31, column: 34, scope: !9)
!107 = !DILocalVariable(name: "loop_info", scope: !9, file: !1, line: 31, type: !81)
!108 = !DILocation(line: 31, column: 67, scope: !9)
!109 = !DILocation(line: 31, column: 46, scope: !9)
!110 = !DILocation(line: 33, column: 33, scope: !9)
!111 = !DILocation(line: 33, column: 42, scope: !9)
!112 = !DILocation(line: 33, column: 50, scope: !9)
!113 = !DILocation(line: 33, column: 60, scope: !9)
!114 = !DILocation(line: 33, column: 68, scope: !9)
!115 = !DILocation(line: 33, column: 3, scope: !9)
!116 = !DILocation(line: 34, column: 33, scope: !9)
!117 = !DILocation(line: 34, column: 42, scope: !9)
!118 = !DILocation(line: 34, column: 50, scope: !9)
!119 = !DILocation(line: 34, column: 60, scope: !9)
!120 = !DILocation(line: 34, column: 68, scope: !9)
!121 = !DILocation(line: 34, column: 3, scope: !9)
!122 = !DILocation(line: 35, column: 33, scope: !9)
!123 = !DILocation(line: 35, column: 42, scope: !9)
!124 = !DILocation(line: 35, column: 50, scope: !9)
!125 = !DILocation(line: 35, column: 60, scope: !9)
!126 = !DILocation(line: 35, column: 68, scope: !9)
!127 = !DILocation(line: 35, column: 3, scope: !9)
!128 = !DILocation(line: 36, column: 36, scope: !9)
!129 = !DILocation(line: 36, column: 48, scope: !9)
!130 = !DILocation(line: 36, column: 59, scope: !9)
!131 = !DILocation(line: 36, column: 69, scope: !9)
!132 = !DILocation(line: 36, column: 80, scope: !9)
!133 = !DILocation(line: 36, column: 3, scope: !9)
!134 = !DILocation(line: 41, column: 3, scope: !9)
