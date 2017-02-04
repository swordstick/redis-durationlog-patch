# redis-durationlog-patch

redis补丁,增加名为durationlog的模块。<br>
用于收集进入slow日志的操作，底层调用过程中耗时情况。目标是填补slow + lantency 监控的盲区.<br>
目前补丁针对redis 3.0.7版本。

## 安装


* 下载官方3.0.7版本

```
$ wget http://download.redis.io/releases/redis-3.0.7.tar.gz
$ tar -xzvf redis-3.0.7.tar.gz
$ cd redis-3.0.7
```

* 打补丁并编译安装

```
$ cp /{path}/durationlog.h src/
$ cp /{path}/durationlog.c src/
$ patch -p1 < /{path}/redis-3.0.7.patch
$ make && make install
```


## 用法

### 配置文件参数

```
durationlog_status  = 1				//	1为打开，0为关闭，默认为1
slowlog-log-slower-than 1			//	时间阀值,复用slowlog参数，无需额外调整
slowlog-max-len 128					//	最大长度,复用slowlog参数，无需额外调整

```

### 在线开关DURATIONLOG功能

```
config set durationlog_status "1"	
config set durationlog_status "0"	
config get durationlog_status
```


### 命令支持

* 与slow用法完全一致

```
durationlog get 
durationlog get $len
durationlog reset
durationlog len

```

## 测试

* 为便于测试，设置为大于1微妙记录，注意调整的是slowlog的阀值
* durationlog返回值包含了slowlog全部内容

```
> config set slowlog-log-slower-than "1"
OK
> set key1 "value3"
OK
> get key1
"value3"
> durationlog get
1) 1) (integer) 12					//	该id编号与slowlog的id编号对应,含义同slowlog
   2) (integer) 1486139810			//	执行时间点,含义同slowlog
   3) (integer) 16					//	总消耗，含义同slowlog
   4) 1) "get"						//	command,含义同slowlog
      2) "key1"						//	command,含义同slowlog
   5) 1) "checktype_addReplay"		//	在 5)中记录调用链耗时，只显示已埋点的调用，执行顺序从高到底
      2) (integer) 3
      3) "lookupKeyReadOrReply"		//	该底层调用名称
      4) (integer) 5				//	该底层调用耗时
      5) "lookupKey"
      6) (integer) 2
      7) "expireIfNeeded"
      8) (integer) 1
...
```

## 性能

* durationlog原理与slowlog一致,仅在超过阀值才记录
* durationlog使用两层list作为存储结构,较slowlog复杂,但影响很小
* 可选择平时关闭durationlog功能,需要时在线开启

## 已支持跟踪命令及扩展埋点方法

参见：已支持跟踪命令及扩展埋点方法

## 版本

* v1.0

## 作者

swordstick

