# Custom Output Format

本文档说明客户端 `-o` 自定义输出框架的设计、语法和当前接入范围。

## 目标

`bjobs`、`bhosts`、`bqueues` 和 `lshosts` 都需要支持用户指定字段、列宽、对齐方式和分隔符的文本输出。公共框架集中处理格式串解析、字段匹配、列宽计算和表格渲染；各命令只负责把自己的数据结构转换为字段值。

当前实现会在客户端请求中携带解析后的 canonical field list。`bjobs`、`bhosts` 和 `bqueues` 通过 batch 查询请求把字段列表发送给 `mbatchd`；`lshosts` 通过 `LIM_GET_HOSTINFO` 请求把字段列表发送给 `lim`。服务端在 trace/comm 日志中可见该字段 metadata，命令输出仍由客户端基于原有回复结构渲染。

该协议扩展使用 `VOLCLAVA_VERSION 23`；新 daemon 在 decode 时会按请求版本判断是否存在字段列表，因此仍可接收旧版本客户端没有该字段的请求。当前实现尚未裁剪服务器返回内容，后续如果要减少网络载荷，需要继续在回复结构和打包逻辑中按字段选择剪裁。

## 语法

`-o` 接收一个空白分隔的格式串：

```text
field_name[:[-]output_width] ... [delimiter='character']
field_name[%[-]output_width] ... [delimiter='character']
```

规则如下：

- 字段名和别名大小写不敏感。
- 未指定宽度时使用字段默认宽度。
- `field:12` 和 `field%12` 表示宽度为 12 的右对齐输出。
- `field:-12` 和 `field%-12` 表示宽度为 12 的左对齐输出。
- `delimiter='|'`、`delimiter=','` 或 `delimiter=' '` 指定单字符分隔符；分隔符可以用单引号或双引号包裹。
- 指定 delimiter 且字段没有显式宽度时，字段之间只输出分隔符，不使用默认宽度补空格；字段带显式宽度时仍按该宽度对齐。
- `-o` 不能与传统格式参数 `-w`、`-l` 同时使用；`bqueues -o` 也不能与 `-r` 同时使用。
- 字段值长于列宽时不截断，避免破坏脚本解析。

## 示例

```bash
bjobs -o "jobid user stat queue exec_host job_name"
bjobs -o "jobid:10 stat:-8 exit_code:9 delimiter=','" -u all -a
bhosts -o "hname stat max run available_mem reserved_mem total_mem"
bqueues -o "qname prio stat max pend run susp res_req"
lshosts -o "hname type model ncpus maxmem maxswp res"
lshosts -o "host_name:-24 ncpus:6 maxmem:10 delimiter='|'"
```

## 已接入字段

### bjobs

`JOBID`、`JOB_IDX`、`USER`、`STAT`、`QUEUE`、`FROM_HOST`、`EXEC_HOST`、`JOB_NAME`、`SUBMIT_TIME`、`PROJ_NAME`、`CPU_USED`、`MEM`、`SWAP`、`PIDS`、`START_TIME`、`FINISH_TIME`、`EXIT_CODE`。

`bjobs -o ... -json` 继续可用。JSON 输出复用同一套字段解析和字段取值逻辑，并跳过重复字段。

### bhosts

`HOST_NAME` (`HNAME`)、`STATUS` (`STAT`)、`JL_U` (`JLU`)、`MAX`、`NJOBS`、`RUN`、`SSUSP`、`USUSP`、`RSV`、`DISPATCH_WINDOW` (`DISPWIN`)、`AVAILABLE_MEM`、`RESERVED_MEM`、`TOTAL_MEM`。

内存字段复用 `bhosts -l` 的 `CURRENT LOAD USED FOR SCHEDULING` 中 `mem` load index。`TOTAL_MEM` 对应该表的 `Total` 行，`RESERVED_MEM` 对应 `Reserved` 行，`AVAILABLE_MEM` 按 `TOTAL_MEM - RESERVED_MEM` 计算。

### bqueues

`QUEUE_NAME` (`QNAME`)、`DESCRIPTION` (`DESC`)、`PRIORITY` (`PRIO`)、`STATUS` (`STAT`)、`MAX`、`JL_U` (`JLU`)、`JL_P` (`JLP`)、`JL_H` (`JLH`)、`NJOBS`、`PEND`、`RUN`、`SUSP`、`RSV`、`USUSP`、`SSUSP`、`NICE`、`HOSTS`、`RES_REQ`、`MAX_CORELIMIT` (`CORELIMIT`)、`MAX_CPULIMIT` (`CPULIMIT`)、`DEFAULT_CPULIMIT` (`DEF_CPULIMIT`)、`MAX_DATALIMIT` (`DATALIMIT`)、`DEFAULT_DATALIMIT` (`DEF_DATALIMIT`)、`MAX_FILELIMIT` (`FILELIMIT`)、`MAX_MEMLIMIT` (`MEMLIMIT`)、`DEFAULT_MEMLIMIT` (`DEF_MEMLIMIT`)、`MAX_PROCESSLIMIT` (`PROCESSLIMIT`)、`DEFAULT_PROCESSLIMIT` (`DEF_PROCESSLIMIT`)、`MAX_STACKLIMIT` (`STACKLIMIT`)、`MAX_SWAPLIMIT` (`SWAPLIMIT`)、`MAX_TASKLIMIT` (`TASKLIMIT`)、`MIN_TASKLIMIT`、`DEFAULT_TASKLIMIT` (`DEF_TASKLIMIT`)、`MAX_THREADLIMIT` (`THREADLIMIT`)、`DEFAULT_THREADLIMIT` (`DEF_THREADLIMIT`)。

当前 `queueInfoEnt` 没有单独的 thread limit 字段，因此 `MAX_THREADLIMIT` 和 `DEFAULT_THREADLIMIT` 输出 `-`。

### lshosts

`HOST_NAME` (`HNAME`)、`TYPE`、`MODEL`、`CPUF`、`NCPUS`、`MAXMEM`、`MAXSWP`、`SERVER`、`RESOURCES` (`RES`)、`MAXTMP`、`NPROCS`、`NCORES`、`NTHREADS`、`RUN_WINDOWS` (`RUNWIN`)。

`MAXMEM`、`MAXSWP`、`MAXTMP` 根据 `LSF_UNIT_FOR_LIMITS` 显示单位；默认单位为 `M`。当前 `hostInfo` 没有单独的 core/thread 字段，因此 `NCORES` 和 `NTHREADS` 输出 `-`；`NPROCS` 使用 `maxCpus`。

## 代码结构

公共框架位于：

```text
lsf/intlib/fmt_output.h
lsf/intlib/fmt_output.c
```

核心结构：

- `struct fmt_field_def`：命令字段定义，包含标准名、别名、表头和默认宽度。
- `struct fmt_request`：解析后的用户请求，包含列顺序、列宽、对齐和分隔符。
- `fmt_output_parse()`：解析格式串并匹配字段定义。
- `fmt_output_fields_string()`：导出 canonical field list，用于客户端请求携带字段选择 metadata。
- `fmt_output_print_header()` / `fmt_output_print_value()`：统一输出表头和值。

命令适配层保留在各命令源文件中：

- `lsbatch/cmd/bjobs.c`
- `lsbatch/cmd/bhosts.c`
- `lsbatch/cmd/bqueues.c`
- `lsf/lstools/lshosts.c`

新增命令接入 `-o` 时，只需要定义字段表、解析 `-o` 参数、实现字段取值函数，再调用公共渲染函数。

协议字段选择状态位于：

```text
lsbatch/lib/lsb.output.c
lsf/lib/lib.output.c
```

命令在发起查询前设置字段列表，查询返回后立即清空，避免一个命令中的后续 API 调用复用错误的字段集合。服务端当前只记录并保留该 metadata，尚未据此裁剪回复。

命令手册也包含 `-o` 的语法、字段表和示例：

- `lsbatch/man1/bjobs.1`
- `lsbatch/man1/bhosts.1`
- `lsbatch/man1/bqueues.1`
- `lsf/man/man1/lshosts.1`

## 构建验证

在当前开发环境中，仓库需要先运行 `./bootstrap.sh` 生成 Autotools 文件。GCC 15 默认 C 标准会触发旧代码的原型错误，使用 GNU89 模式可完成构建：

```bash
./bootstrap.sh
make -j4 CFLAGS='-g -O2 -Wall -fPIC -Wno-error=format-security -std=gnu89'
```

公共 parser 的单元测试可以通过以下命令运行：

```bash
make -C lsf/intlib check CFLAGS='-g -O2 -Wall -fPIC -Wno-error=format-security -std=gnu89'
```

端到端 smoke test 可以安装到临时 prefix，启动用户态单节点 daemon，再运行四个命令的 `-o` 查询。不要直接调用 `config/volclava start`，该脚本会先执行全局 `killall`。

```bash
./configure --prefix="$PWD/.test-install" volclavaadmin="$USER" \
  CFLAGS='-g -O2 -Wall -fPIC -Wno-error=format-security -std=gnu89'
make -j4 install

# 在 .test-install/etc/lsf.cluster.volclava 中把当前 hostname 加入 Begin Host 段。
env LSF_ENVDIR="$PWD/.test-install/etc" LSF_SERVERDIR="$PWD/.test-install/sbin" \
  PATH="$PWD/.test-install/bin:$PWD/.test-install/sbin:$PATH" \
  .test-install/sbin/lim -1
env LSF_ENVDIR="$PWD/.test-install/etc" LSF_SERVERDIR="$PWD/.test-install/sbin" \
  PATH="$PWD/.test-install/bin:$PWD/.test-install/sbin:$PATH" \
  .test-install/sbin/res -1
env LSF_ENVDIR="$PWD/.test-install/etc" LSF_SERVERDIR="$PWD/.test-install/sbin" \
  PATH="$PWD/.test-install/bin:$PWD/.test-install/sbin:$PATH" \
  .test-install/sbin/sbatchd -1

env LSF_ENVDIR="$PWD/.test-install/etc" PATH="$PWD/.test-install/bin:$PWD/.test-install/sbin:$PATH" \
  .test-install/bin/lshosts -o "hname type model ncpus maxmem maxtmp runwin delimiter='|'"
env LSF_ENVDIR="$PWD/.test-install/etc" PATH="$PWD/.test-install/bin:$PWD/.test-install/sbin:$PATH" \
  .test-install/bin/bhosts -o "hname stat max run ssusp ususp rsv dispwin delimiter=','"
env LSF_ENVDIR="$PWD/.test-install/etc" PATH="$PWD/.test-install/bin:$PWD/.test-install/sbin:$PATH" \
  .test-install/bin/bqueues -o "qname prio stat max pend run susp res_req delimiter='|'"
env LSF_ENVDIR="$PWD/.test-install/etc" PATH="$PWD/.test-install/bin:$PWD/.test-install/sbin:$PATH" \
  .test-install/bin/bjobs -a -o "jobid stat queue exec_host job_name exit_code delimiter='|'"
```
