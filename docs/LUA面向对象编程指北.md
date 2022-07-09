## LUA面向对象设计指北

我一直对entity的对象创建方法不满意。就是在对象创建时会把所有父类都创建一遍。为什么会这样做呢？因为要模拟C++的对象创建方式。在我学习C++的时候，对于对象的创建描述是这样的。依次创建父类对象并调用建构函数。在使用时如果调用了子对象不存在的方法就依次查找父对象直到找到方法或变量。大概我画了一个图如下所示。

![](.\object1.png)

其中蓝色的路径是类继承的方向，绿色的路径是类对象调用的方向。红色是对象创建的过程。以对象D来说，对象D的创建需要分配4块内存。

分配4块内存本身没什么问题。问题在把这4块内存通过单向链表链接起来的方式。当在对象D中调用方法Fun c时。就要依照链接依次查询对象B>对象A>对象C。也就是这种方式在创建有多个父类对象时需要创建多块内存是非常低效的。在使用过程中调用方法时需要查询多块内存也是非常低效的。

以上内容是我印象中某个C++书里一笔带过的东西，在下面我会介绍现在C++内存分配的方式。现实中大部分普通人都没有机会去创建一个面向对象的模型。但了解这个模型对我们理解C++的运行效率很有帮助。尤其是多重继承模式下创建和使用效率的问题。这些问题通常隐藏在语言层面之下，很容易被人忽略。

#### 现在的C++创建对象是如何分配内存的？

因为我不相信现在C++是使用如此低效的对象创建方式所以简单的测试了一下C++对象创建的流程。

```c++
// test.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>

class Task {
public:
    Task() {
        std::cout << "ThreadTask.point" << this << std::endl;
    }

    void run() {
        std::cout << "ThreadTask.run" << std::endl;
    }
    int vart;
};

class P1 {
public:
    P1() {
        std::cout << "P1.point" << this << std::endl;
    }

    void func1() {
        std::cout << "P1.func1" << std::endl;
    }
    int var1;
};

class P2 : public Task {
public:

    P2() {
        std::cout << "P2.point" << this << std::endl;
    }

    void func2() {
        std::cout << "P2.func1" << std::endl;
    }

    int var2;
};

class C : public P1, public P2 {
public:
    C() {
        std::cout << "C.point" << this << std::endl;
    }
    void funt() {
        std::cout << "C.funt:" << var << std::endl;
    }
    int var;
};

int main()
{
    std::cout << "Hello World!\n";
    C* cObject = new C;

    getc(stdin);
}
```

这段代码的输出结果如下

```
Hello World!
P1.point01058E30
ThreadTask.point01058E34
P2.point01058E34
C.point01058E30
```

我们看到C++对象创建后的内存分配如下：

![](.\object2.png)

在多重继承的情况下还是会分配多块内存。只是按继承分支进行了优化。优先顺序是按照从左到右。我猜测大概流程就是计算所有变量和函数长度后统一分配内存。然后依次调用构建函数。这样会大幅减少对象创建和虚指针的查找过程。减少创建过程和执行过程的额外损坏。C++和lua的是完全不同的面向对象语言。对于C++来说这点损耗完全可以忽略。但对于lua来说就非常的致命了。

#### 继续看lua是如何分配对象内存的？

在[Programming in Lua : 16](http://www.lua.org/pil/16.html)中介绍了一种最原始的面向对象的创建方法。大部分的lua的面向对象都是基于这个改进的。

他的类代码是这样的

```lua
  -- look up for `k' in list of tables `plist'
    local function search (k, plist)
      for i=1, table.getn(plist) do
        local v = plist[i][k]     -- try `i'-th superclass
        if v then return v end
      end
    end
    
    function createClass (...)
      local c = {}        -- new class
    
      -- class will search for each method in the list of its
      -- parents (`arg' is the list of parents)
      setmetatable(c, {__index = function (t, k)
        return search(k, arg)
      end})
    
      -- prepare `c' to be the metatable of its instances
      c.__index = c
    
      -- define a new constructor for this new class
      function c:new (o)
        o = o or {}
        setmetatable(o, c)
        return o
      end
    
      -- return new class
      return c
    end
```

类的创建是这样的

```lua
 NamedAccount = createClass(Account, Named)
```

对象的创建是这样的

```lua
    account = NamedAccount:new{name = "Paul"}
    print(account:getname())     --> Paul
```

可以看出来还是非常简陋的，只能实现单方向的继承。对象的创建是基于类创建的。如果找不到就会在类里面沿着__index方向继续搜索。因为是个非常简化的版本，但我们看出来和C++的对象创建完全的不同。就是当在对象内搜索不到时，就会延申到类里面去搜索。这样当多个对象共享一个类的时候就可能会导致数据互相覆盖了。所以在luacluster里面我试图解决这个问题的方法是用C++的对象创建方式。为每个对象从头创建一套类内存来解决数据覆盖的问题。但很快我就意识到当对象继承越来越多后效率会快速下降。

#### middleclass是如何分配内存的？

看到middleclass的时候我知道我所有的困惑和疑虑都解除了。因为C++实在的太权威了，即使如此低效的对象创建方式，也很难让我彻底否定他。但middleclass让我在最后一刻彻底打破了C++对我的束缚。middleclass这个lua库很短，就让人很容易就忽略了。介绍的文章也不多，因为很少有人真的会去深究类对象的创建。他对象的创建非常的简洁大概如下所示。

![](.\object3.png)

就是在类的创建的过程中要把被继承的类数据复制过来。然后在对象创建的过程中直接复制数据创建对象。这个被创建出来的对象是唯一和完整的。不会再引用任何其他的数据也不会再具备创建的功能。虽然这样会导致因为继承的关系后面的子类会越来越大。但执行效率非常的高。也就是用空间换时间。也阐述了另外一个朴素的道理。妈妈生小孩那一刻，需要把爸爸爷爷奶奶姥姥都到齐才能生么？小孩出生这个事件点肯定只有妈妈是必须的。在打破对C++的束缚后我决定对luacluster进行改写。

#### 对middleclass的改进

在对luacluster改写过程中我发现了middleclass的一些不足。

1. new和class关键字不明确，作者建议使用local class的方式来定义关键字，我觉得这样不好。使用全局函数就可以简单的解决了。到处local class = require 'middleclass'实在不是一个好主意。

2. 是没有多重继承的问题，这个很简单加上就可以了，luacluster原来就支持多重继承。

3. 是类名字的问题，class关键字里面必须明确指出一个类的名字。这个看起来不是很优雅。定义出来的就是这个样子

   ```
   Class1 = class('Class1'):include(Mixin1, Mixin2)
   ```

   显得重复和耐受。

4. 继承的时候不能使用名字继承，这样对于每个父类要明确使用require进行引用。之后保存到变量才能通过class进行继承。就显得语法非常的啰嗦。

   ```lua
   Class2 = require("class2")
   Class3 = require("class3")
   Class1 = class('Class1', Class2, Class3):include(Mixin1, Mixin2)
   ```

   如果改成下面的样式就好看多了

   ```lua
   Class1 = class('Class2', 'Class3'):include(Mixin1, Mixin2)
   ```

   

基于上述的优化方案持续对luacluster进行改进。做开源不容易感兴趣的朋友给点个赞吧！

[surparallel/luacluster: Unity includes a 10,000 NPC scene (github.com)](https://github.com/surparallel/luacluster)

最近打算使用无缝大地图做一个2DMMO沙盒游戏。大概就是在一个12个小时才能走完的超大地图上，放入几千个玩家可以随意创建任何建筑物。这些建筑会攻击或产出。玩家制作各种随机的怪物那种。有感兴趣的制作人和公司可以联系我的邮箱isur@qq.com。