/*
 * @swordstick update this file at 20170201
 * Durationlog 用于记录与慢查询对应的各操作，更为具体的耗时情况。
 * 记录每个主要函数的耗时调用
 * 上限时间，日志数量复用慢查询的选项决定
 * 可以使用 CONFIG SET/GET 命令来设置/获取这个选项的值。
 * 提供与SLOWLOG一样查询及reset能力
 *
 * 耗时日志保存在内存而不是文件中，
 * 这确保了耗时日志本身不会成为速度的瓶颈。
 *
 * 耗时日志，并非记录所有函数内容，根据不同命令特点，划分几个核心调用，并记录其动作和耗时
 *
 *
 *
 * Copyright (c) 2009-2012, Salvatore Sanfilippo <antirez at gmail dot com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Redis nor the names of its contributors may be used
 *     to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */


#include "redis.h"
#include "durationlog.h"


/*Create a new durationlog entry.
 *
 * 创建一条新的durationlog日志
 *
 *
 * 函数负责增加所有记录对象的引用计数 */

durationlogEntry *durationlogCreateEntry(robj **argv, int argc, long long duration_all) {

/* 从client的slow标签获取相应id和all time，从新增的durariontlist中获取完整的耗时细节记录*/

durationlogEntry *se = zmalloc(sizeof(*se));
    int j, slargc = argc;

    if (slargc > SLOWLOG_ENTRY_MAX_ARGC) slargc = SLOWLOG_ENTRY_MAX_ARGC;

    se->argc = slargc;

    se->argv = zmalloc(sizeof(robj*)*slargc);
    for (j = 0; j < slargc; j++) {

        if (slargc != argc && j == slargc-1) {
            se->argv[j] = createObject(REDIS_STRING,
                sdscatprintf(sdsempty(),"... (%d more arguments)",
                argc-slargc+1));
        } else {

            if (argv[j]->type == REDIS_STRING &&
                sdsEncodedObject(argv[j]) &&
                sdslen(argv[j]->ptr) > SLOWLOG_ENTRY_MAX_STRING)
            {
                sds s = sdsnewlen(argv[j]->ptr, SLOWLOG_ENTRY_MAX_STRING);

                s = sdscatprintf(s,"... (%lu more bytes)",
                    (unsigned long)
                    sdslen(argv[j]->ptr) - SLOWLOG_ENTRY_MAX_STRING);

                se->argv[j] = createObject(REDIS_STRING,s);
            } else {
                se->argv[j] = argv[j];
                incrRefCount(argv[j]);
            }
        }
    }


    se->time = time(NULL);


    se->duration_all = duration_all;


    se->id = server.slowlog_entry_id-1;

    se->duration = server.duration;

    return se;
}

/*Create a new duration entry.
 *
 * 创建一条新的单位耗时记录
 *
 *
 * 函数负责增加所有记录对象的引用计数*/

durationEntry *durationCreateEntry(char *modulename,long long duration) {

/* 初始化一个单位耗时记录*/
durationEntry *se = zmalloc(sizeof(*se));
se->modulename = modulename;
se->duration = duration;
return se;

}

void durationlistReset(list *duration) {
    while (listLength(duration) > 0)
        listDelNode(duration,listLast(duration));
}


/* Remove all the entries from the current durationlog.
 *
 * 删除所有duration日志,应该伴随着slowlog的reset操作被调用*/

void durationlogReset(void) {
    while (listLength(server.durationlog) > 0)
        listDelNode(server.durationlog,listLast(server.durationlog));
}


/*
 * 释放给定的耗时日志
 */

void durationlogFreeEntry(void *septr) {
    durationlogEntry *se = septr;
    int j;

    for (j = 0; j < se->argc; j++)
        decrRefCount(se->argv[j]);

    zfree(se->argv);

    durationlistReset(se->duration);

    zfree(se);
}

/*
 * 释放给定的单位耗时
 */

void durationFreeEntry(void *septr) {
durationEntry *se = septr;
zfree(se);
}







/* Initialize the duration log. This function should be called a single time
 * at server startup.
 *
 * 初始化服务器耗时记录功能。
 *
 * 这个函数只应该在服务器启动时执行一次。*/

void durationlogInit(void) {

    server.durationlog = listCreate();

    listSetFreeMethod(server.durationlog,durationlogFreeEntry);
}

/* Initialize the durationEntry . This function should be called a single time
 * at server startup.
 *
 * 初始化客户端耗时记录功能。
 *
 * 这个函数在server启动及每次写入一份durationlog后执行一次。*/

void durationInit(void) {

    server.duration = listCreate();

    listSetFreeMethod(server.duration,durationFreeEntry);

}




/* Push a new entry into the duration log.
 *
 *
 * This function will make sure to trim the duration log accordingly to the
 * configured max length.
*/

void durationlogPushEntryIfNeeded(robj **argv, int argc, long long duration) {

    if (server.slowlog_log_slower_than < 0) return; /* Slowlog disabled  so durationlog will disable too */

    if (listLength(server.duration) == 0)
        return;

    if (duration >= server.slowlog_log_slower_than )
        listAddNodeHead(server.durationlog,durationlogCreateEntry(argv,argc,duration));


    while (listLength(server.durationlog) > server.slowlog_max_len)
        listDelNode(server.durationlog,listLast(server.durationlog));

     durationInit();
}



/* The DURATION command. Implements all the subcommands needed to handle the
 * Redis duration log.
 *
 * DURATIONLOG 命令的实现，支持 GET / RESET 和 LEN 参数*/

void durationlogCommand(redisClient *c) {

    if (c->argc == 2 && !strcasecmp(c->argv[1]->ptr,"reset")) {
        durationlogReset();
        addReply(c,shared.ok);

    } else if (c->argc == 2 && !strcasecmp(c->argv[1]->ptr,"len")) {
        addReplyLongLong(c,listLength(server.durationlog));

    } else if ((c->argc == 2 || c->argc == 3) &&
               !strcasecmp(c->argv[1]->ptr,"get"))
    {
        long count = 10, sent = 0;
        listIter li;
        void *totentries;
        listNode *ln;
        durationlogEntry *se;
        durationEntry *sf;
        listIter ldi;
        listNode *ldn;


        if (c->argc == 3 &&
            getLongFromObjectOrReply(c,c->argv[2],&count,NULL) != REDIS_OK)
            return;

        listRewind(server.durationlog,&li);

        totentries = addDeferredMultiBulkLength(c);
        while(count-- && (ln = listNext(&li))) {
            int j;

            se = ln->value;
            addReplyMultiBulkLen(c,5);
            addReplyLongLong(c,se->id);
            addReplyLongLong(c,se->time);
            addReplyLongLong(c,se->duration_all);
            addReplyMultiBulkLen(c,se->argc);
            for (j = 0; j < se->argc; j++)
                addReplyBulk(c,se->argv[j]);
        // 遍历duration，取出所有内容
            addReplyMultiBulkLen(c,listLength(se->duration)*2);
            listRewind(se->duration,&ldi);
            while(ldn = listNext(&ldi)) {
            sf = ldn->value;
            addReplyBulkCString(c,sf->modulename);
            addReplyLongLong(c,sf->duration);
            }
            sent++;
        }
        setDeferredMultiBulkLength(c,totentries,sent);
    } else {
        addReplyError(c,
            "Unknown DURATIONLOG subcommand or wrong # of args. Try GET, RESET, LEN.");
    }
}


