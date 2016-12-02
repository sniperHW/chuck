
##basic
[select](#user-content-select-index-)

##垃圾收集

lua是一种自动管理内存的语言。通过运行一个垃圾收集器回收不可再被访问的对象所占用的内存。





###assert (v [, message])

如果v为nil或false,用message作为参数调用[error](),没有message则缺省使用"assertion failed!"。否则返回传递给assert的所有参数。示例:

```lua
	print(assert(nil,"test error"))
	>>>
	lua: test.lua:1: test error
	stack traceback:
	        [C]: in function 'assert'
	        test.lua:1: in main chunk
	        [C]: ?

	print(assert(1,"test error"))
	>>>
	1       test error

```

###collectgarbage ([opt [, arg]])

lua垃圾回收的通用接口函数，根据参数opt的值执行不同的操作：

*	"collect":执行一次全量垃圾回收。这是默认选项(无参数调用collect)。


*	"stop":停止自动垃圾回收(只有手动执行collect才进行垃圾回收)。


*	"restart":重新启动自动垃圾回收。


*	"count":返回lua虚拟机当前使用的内存(单位Kbytes)。


###select (index, ···)

如果index是一个正整数，返回非固定参数从左边数index开始的所有参数，例如：

`print(select(2,1,2,3,4,5))  -> 2 3 4 5`

如果index是一个负整数，返回非固定参数中从右边数-index开始的所有参数，例如：

`print(select(-2,1,2,3,4,5)) -> 4 5`

否则index必须是字符串"#",返回非固定参数的数量，例如：

`print(select("#","a","b","c","d","e")) -> 5`

