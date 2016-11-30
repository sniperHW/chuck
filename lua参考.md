[select](#user-content-select-index-)


###select (index, ···)

如果index是一个正整数，返回非固定参数从左边数index开始的所有参数，例如：

`print(select(2,1,2,3,4,5))  -> 2 3 4 5`

如果index是一个负整数，返回非固定参数中从右边数-index开始的所有参数，例如：

`print(select(-2,1,2,3,4,5)) -> 4 5`

否则index必须是字符串"#",返回非固定参数的数量，例如：

`print(select("#","a","b","c","d","e")) -> 5`

