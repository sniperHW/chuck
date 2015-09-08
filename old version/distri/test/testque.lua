local LinkQue =  require "distri.linkque"


local que = LinkQue.New()
que:Push({1})
print(que:Len())