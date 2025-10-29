; ModuleID = 'my_module'
source_filename = "my_module"

@0 = private unnamed_addr constant [4 x i8] c"%d\0A\00", align 1

define i32 @main() {
entry:
  %y = alloca i32, align 4
  %x = alloca i32, align 4
  store i32 5, ptr %x, align 4
  store i32 10, ptr %y, align 4
  %x1 = load i32, ptr %x, align 4
  %y2 = load i32, ptr %y, align 4
  %addtmp = add i32 %x1, %y2
  %0 = call i32 (ptr, ...) @printf(ptr @0, i32 %addtmp)
  ret i32 0
}

declare i32 @printf(ptr, ...)
