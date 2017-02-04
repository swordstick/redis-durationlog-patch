/*
 * @swordstick update this file at 20170201
 * 命令执行期间的耗时日志记录.h文件
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


/* 使用举例
robj *lookupKeyWrite(redisDb *db, robj *key) {
    INITDURATIONARG;
    STARTDURATION("expireIfNeeded");

    expireIfNeeded(db,key);
    LOGDURATION;

    //可以多次使用
    STARTDURATION("expireIfNeeded");

    命令行;
    LOGDURATION;

    return lookupKey(db,key);
}

*/


#define INITDURATIONARG \
char *modulename;\
long long  start, duration;\

#define STARTDURATION(_modulename) \
modulename = _modulename; \
start=ustime();

#define LOGDURATION \
duration = ustime()-start;\
if ( (server.duration_status == 1) && (listAddNodeHead(server.duration,durationCreateEntry(modulename,duration)) == NULL) )\
   redisLog(REDIS_WARNING,"Fatal: Can't CreateEntry to server.druation.");

#define SLOWLOG_ENTRY_MAX_ARGC 32
#define SLOWLOG_ENTRY_MAX_STRING 128





/* This structure defines an entry inside the duration log list */
/*
 * 单个命令完整耗时日志，只在slow确定输出前提下，才Create一份该slow命令的日志
 */
typedef struct durationlogEntry {

    robj **argv;

    int argc;

    long long id;       /* Unique entry identifier. */

    long long duration_all;

    time_t time;        /* Unix time at which the query was executed. */

    // 记录每个调用耗时
    list *duration;

} durationlogEntry;


/* This structure defines an entry inside the duration log entry */
/*
 * 单个调用耗时记录，只要打开耗时日志选项就将记录，但不一定写入日志列表
 */
typedef struct durationEntry {

    // 调用模块名称
    char *modulename;

    // 执行命令消耗的时间，以微秒为单位
    long long duration;

} durationEntry;

/* Exported API */
void durationlogInit(void);
void durationlogPushEntryIfNeeded(robj **argv, int argc, long long duration_all);
void durationInit(void);
durationEntry *durationCreateEntry(char* modulename,long long duration);

/* Exported commands */
void durationlogCommand(redisClient *c);