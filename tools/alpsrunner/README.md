# ALPSRunner 1.1.2

网表远程仿真与结果回传。Windows x64、C++17 命令行模块；本机通过 SSH 连接已配置的 Windows 服务器，再由服务器上的 ALPSRunnerHost 经 VMware Tools 调用 IC618 Linux 中的 ALPS。成功后将输入快照、原始仿真结果、日志、运行摘要和 SHA-256 清单保存到本机。

本交付采用 `docs_1.6.zip` 的模块接入约定：providerId 为 `alpsrunner`，单阶段为 `simulate`，首次定义的模块 `apiVersion` 为 `1`。客户端版本 `1.1.2` 与远端 `ALPSRunnerHost 1.1.0` 通过协议 `1` 兼容；产品版本号不要求与服务器程序版本号相同。

## 1. 交付范围与前置条件

模块安装目录只包含四个文件：

```text
alpsrunner/
├─ ALPSRunner.exe
├─ module.manifest.json
├─ README.md
└─ ERROR_CODES.md
```

案例与实际配置、授权私钥在独立的 `alpsrunner_cases_20261110` 案例交付中，不放进模块安装目录。此批按用户明确要求携带可用私钥，因此整个案例交付属于授权使用材料，不能当作无凭据的公开样例再次传播。私钥仅在客户端读取，不通过仿真协议上传到服务器。

客户端无需安装 ALPS、VMware、Python、Git、CMake、独立 SSH 客户端或编译器。程序静态链接非系统运行库，Windows 自身系统库/UCRT 仍由操作系统提供。ALPS 本体及许可证不随包。

运行前应满足：

- 本机是 Windows x64，能访问配置中的 Windows SSH 服务器。
- 服务器主机公钥指纹已经通过可信渠道核实，客户端公钥已被授权为固定 ALPSRunnerHost 命令。
- 服务器已经配置 `ALPSRunnerHost 1.1.0`、机器 DPAPI 保护的 Linux 凭据及所需权限。
- IC618 已开机、VMware Tools 正常、现有 ALPS 环境及许可证可用。
- 算例目录可读写；安装目录可以只读。程序不会自动开机、部署服务器、更改 VM 网络或启动许可证服务。

服务器本身是外部运行依赖；本批四件套不复制服务器上的机器 DPAPI 凭据，也不把 ALPSRunnerHost 当成本地仿真阶段启动。更换运行电脑只需调整客户端路径并保留服务器授权；更换服务器则须单独完成部署、凭据配置、公钥授权和主机指纹核验。

## 2. 标准案例与调用方式

假设当前 PowerShell 工作目录是本批外层交付目录 `ALPSRunner_1.1.2`，其中 `alpsrunner/` 与 `alpsrunner_cases_20261110/` 并列：

```powershell
& ".\alpsrunner\ALPSRunner.exe" `
  --input ".\alpsrunner_cases_20261110\thermal_netlist\Model\ThermalNetList.sp" `
  --output-dir ".\alpsrunner_cases_20261110\thermal_netlist\Result\alpsrunner\simulate" `
  --config ".\alpsrunner_cases_20261110\thermal_netlist\Setting\ALPSRunner.json" `
  --runtime-dir ".\alpsrunner_cases_20261110\thermal_netlist\Result\logs\alpsrunner" `
  --threads 2
```

JSON 的 `ssh.private_key_file` 为 `alpsrunner.key`，因此读取同一 `Setting/` 目录中的私钥，无需再传 `--ssh-key`。PowerShell 行末反引号是续行符，后面不能加空格或注释。`$LASTEXITCODE` 为本次进程退出码。

| 参数 | 规则 |
| --- | --- |
| `--input FILE.sp` | 必填；非空、自包含 `.sp` 网表，最大 1 GiB。原文件不被改写。 |
| `--output-dir DIRECTORY` | 必填；本机结果根，每次调用在其中创建独立任务子目录。 |
| `--config FILE.json` | 必填；严格 JSON 配置，不支持注释或密码字段。 |
| `--ssh-key FILE` | schema 2 的可选私钥路径覆盖；优先于 JSON 的 `ssh.private_key_file`。两者至少提供一个。本交付 manifest 使用 JSON 中的固定相对路径。 |
| `--runtime-dir DIRECTORY` | 可选；显式本机运行暂存根。本交付 profile 必须传入 `{runtimeDir}`；原生 CLI 省略时仍使用系统临时目录。 |
| `--threads N` | 可选；整数 1～256，默认 2，传递为 ALPS 的 `-p N`。 |
| `--help` | 单独使用；离线显示帮助，不连接服务器。 |
| `--version` | 单独使用；离线显示客户端版本，不连接服务器。 |

CLI 相对路径按启动进程的工作目录解析；JSON 内私钥相对路径按 JSON 所在目录解析，均不按 EXE 所在目录猜测。宿主应传绝对路径。当前只发送一个网表，拒绝外部 `.include`、`.inc` 和引用外部文件的 `.lib`；网表内 `.lib section` 分段可使用。逻辑行上限为 1 MiB。输入文件名须符合 Windows 文件名规则且不能以 `-` 开头。

## 3. 配置与认证

本交付 profile 使用 schema 2。manifest 的 `accept` 仅声明文件扩展名；配置字段、类型、取值范围及认证模式由客户端读取 JSON 后校验，不表示平台已完成这些深层校验。

| 字段 | 含义 |
| --- | --- |
| `schema_version: 2` | ALPSRunner 配置格式版本，字段使用下划线；不是通用 `schemaVersion`。 |
| `ssh.host`、`port`、`username` | 远端 Windows SSH 服务器地址、端口、登录账号。 |
| `ssh.host_key_sha256` | 已核实的服务器 RSA 主机公钥指纹；不匹配时拒绝连接，不自动接受新指纹。 |
| `ssh.auth_method: "publickey"` | 使用公钥认证；客户端用私钥签名，服务器用授权公钥验证。 |
| `ssh.private_key_file` | 本地私钥文件路径；本案例为 `alpsrunner.key`。 |
| `execution.timeout_seconds` | 仿真运行时限，1～604800 秒；案例设置 3600。上传、收集、回传另需时间。 |
| `execution.poll_interval_seconds` | 状态轮询间隔，1～60 秒；案例设置 2。 |
| `execution.connect_timeout_seconds` | SSH 建连及认证时限，1～120 秒；案例设置 15。 |

当前支持无口令 PKCS#1 RSA PEM 私钥，文件头为 `-----BEGIN RSA PRIVATE KEY-----`，大小大于 0 且不超过 1 MiB。OpenSSH 新容器、非 RSA 算法、加密私钥不受支持。schema 2 不读取本机密码环境变量或 Windows 凭据管理器，不提示两个登录密码，认证失败也不回退密码。

原生 CLI 的 schema 1 密码模式保持兼容，但本 manifest 不采用该模式；schema 1 不能同时使用 `--ssh-key`。本交付不提供配置服务器密码的脚本或明文密码。

无口令私钥相当于访问权限凭据。案例目录、解压后的私钥以及包含它的 ZIP 应仅对获授权人员可读。复制私钥表示共享同一个授权身份；服务器撤销对应公钥后，全部副本的新连接都会失效。

## 4. 运行暂存与只读安装

本 profile 的输出位置为 `<dataRoot>/Result/alpsrunner/simulate/`，运行暂存为 `<dataRoot>/Result/logs/alpsrunner/`。两者必须在程序可确认的同一实际磁盘卷上；仅盘符相同不足以证明同卷，不得通过符号链接、junction 或其他重解析路径指向另一位置。平台不应开放输入、配置、私钥、输出或 runtime 路径的任意覆盖；这些路径由模板固定。

显式 runtime 必须在正在执行的 EXE 安装目录树之外，可创建且可写。程序为每次调用创建独立暂存子目录，保存下载中的 ZIP；旧密码模式所需的助手/任务 JSON 也放在该子目录。1.1.2 在 runtime 与输出同卷时，还把摘要/清单替换文件和解压发布暂存放入本次 runtime 子目录，保持结果原子发布和失败回滚。

原生 CLI 为保持兼容，在 runtime 与输出跨卷，或无法取得卷 GUID、无法证明同卷时，将结果发布事务回退到输出所在卷，并在摘要中报告 `output_volume_fallback`。**此回退不属于本交付 profile 的 R2 合规运行方式**。不能仅凭传了 `--runtime-dir` 就推断所有暂存都使用了该根。

平台验收必须检查本次 `run_summary.json`：`transaction_staging_policy` 等于 `runtime_same_volume`，`transaction_staging_dir` 等于 `runtime_dir`，且该目录是 `<dataRoot>/Result/logs/alpsrunner/` 下本次调用的独立子目录。

程序只清理本次拥有的暂存文件和目录；强制结束进程、断电或文件占用可能留下可诊断的暂存，不会自动清扫其他任务。已完成的结果和远端任务目录保留，不自动删除历史。

客户端自身诊断输出到 stdout/stderr，由宿主采集；本模块不另写客户端诊断日志文件。结果中的 `console.log` 及 `.sim` 内日志是远端 ALPS 返回的原始仿真证据，随结果归档保留，不作为客户端 runtime 诊断日志迁移。本模块不读取或写入 task_layer 的 `Result/tasks/`。

## 5. 输出、完整性与失败恢复

```text
<dataRoot>/Result/alpsrunner/simulate/
└─ ThermalNetList_<UTC时间戳>_<唯一标识>/
   ├─ ThermalNetList.sp
   ├─ ThermalNetList.sp.sim/
   │  ├─ ThermalNetList.sp.log
   │  ├─ ThermalNetList.sp.op0
   │  ├─ ThermalNetList.sp.dcop0
   │  ├─ ThermalNetList.sp.ic0
   │  ├─ ThermalNetList.sp.warn
   │  └─ ...
   ├─ console.log
   ├─ run_summary.json
   └─ SHA256SUMS.txt
```

上述为主要文件示例，不是固定完整清单。`.op0` 是文本工作点结果；`.dcop0` 是二进制直流工作点结果，本热网表对应稳态结果；`.ic0` 包含节点初值/工作点数据。`.warn` 允许为空，线程相关文件及其他 ALPS 原始文件也会完整回传。其他分析类型、失败或取消不保证生成相同文件。

输入含中文、空格或其他不适合现有 ALPS 包装脚本的字符时，远端助手生成字节一致的 `ALPSRunner_input.sp` 兼容副本；原始名称和原始内容仍保留。使用 `run_summary.json.simulator_input_name` 和 `command_argv` 确认实际仿真名称，不总按原输入名推导 `.sim` 目录。

回传先校验 ZIP 长度、SHA-256、ZIP 成员路径/类型、CRC 及逐文件 SHA-256，再发布结果。最大解压总量为 10 GiB，最多 100000 个归档条目；拒绝符号链接、路径穿越及 Windows 名称别名。最终 `SHA256SUMS.txt` 覆盖输入、返回文件和运行摘要，不含清单自身。

成功要求进程退出 0，摘要 `status=succeeded`、`simulation_status=succeeded`、`download_status=verified`，并保留本次完整结果。出现 `result_scope=logs_only` 时只有诊断集，不代表完整仿真结果。失败时可能只存在部分文件；输入/配置早期无效时可能没有任务目录。

每次任务目录都是动态名称，控制台 `Output:`/`Results:` 行和本次摘要可以帮助直接调用者定位。manifest 只登记内部结果集合 `alpsrunner/run.bundle`，`artifacts.produced` 为空；没有固定公开 OP0、`latest`、通配符产物路由、`publishTo` 或自动向 `Model/thermal.op0` 复制的行为。下游热后处理集成须另行定义稳定工件及平台 publisher 规则。

`run_summary.json` 是本工具的运行诊断与复现信息，不是 task_layer 的调度状态文件，也没有冒充规范的 `stage_receipt.json`。网络中断后 `simulation_status=unknown` 表示远端结束情况未确认，应按摘要保存的任务 ID/远端路径检查；不要直接假定旧仿真已停止。

## 6. 退出码与正常取消

| 退出码 | 含义 |
| --- | --- |
| `0` | 仿真成功且结果回传/校验完成。 |
| `2` | 命令行、输入、配置或运行暂存预检错误。 |
| `3` | 凭据、连接、提交、VMware Tools 或通信问题。 |
| `4` | 仿真或远端结果收集失败。 |
| `5` | 下载、解压、完整性校验或结果发布失败。 |
| `124` | 超时。 |
| `130` | 用户取消。 |

详细触发条件、恢复方式及平台 QS 归一行为见同目录 `ERROR_CODES.md`。Ctrl+C 会请求停止本次任务并尽量收集日志，不终止其他人的 ALPS。强制结束客户端不能证明远端任务也已结束。

## 7. 原生 Qt 与平台接入

Qt 桌面应用使用 `QProcess`：程序路径和参数列表分开设置，每个参数和值分别传入，不拼接 shell 命令，不把 PowerShell 反引号传入 `QProcess`。设置明确的工作目录，监听 stdout/stderr 与进程退出状态；本工具不提供 GUI。密钥模式适合关闭 stdin 的非交互运行。

平台应把四件套部署到 `<MODULE_ROOT>/alpsrunner/`，注册 manifest，并由配置提供三个 `user/alpsrunner.*` 输入 ref 与一个 `simulate` 阶段。模板用 `{dataRoot}` 展开实际绝对路径，`{runtimeDir}` 按规范指向 `Result/logs/alpsrunner/`。CLI 使用散参数，manifest 不虚构 `--data-root`。

线程覆盖使用白名单 `--threads` 与 `{override:--threads}`。依据规范，未提供覆盖值时整对省略，客户端采用默认 2；提供时只能出现一对 `--threads`，值须为 1～256。本次不开放其他 CLI 参数覆盖。

JSON Schema 通过仅说明 manifest 结构合法，不证明平台实际注册、DAG、模板展开和端到端调度已完成。本交付不宣称已经在接收方的 task_layer 注册或完成 thermal 全链路联调；相关动作与验收应按根交接说明进行。阶段内部结果由 ALPSRunner 自检，平台没有被声明的固定外部产物可直接接给其他模块。

## 8. 本地自测与版本边界

先用 `--version`、`--help` 离线检查程序能否启动，再按独立案例 README 执行最小真实仿真。正常验证需要保持安装目录只读，确认结果与 runtime 位于同一案例数据根的实际同一卷上，读取进程退出码和本次摘要，并核验 SHA 清单。

当前 1.1.2 在 1.1.1 的可选 runtime 参数基础上完善同卷发布暂存。既有个人目录 `ALPSRunner_1.1.1` 与通用 1.1.0 包不被本模块包静默覆盖。此次是 docs_1.6 模块专用交付，不是整个 PEEC Studio 的新版集成包。实际验证结果、二进制和归档摘要记录在包外交接说明中；本 README 不替代平台验收回执。

## 9. 内部依赖与许可

| 组件 | 固定版本/来源 | 用途与许可 |
| --- | --- | --- |
| nlohmann/json | 3.12.0；提交 `55f93686c01528224f448c19128836e7df245f72` | JSON；MIT。 |
| miniz | 3.1.2 | ZIP 解压；MIT。 |
| libssh2 | 提交 `6125bfe552b3a8c7340c2fb3b82cdf65b0e16f9d` | SSH；BSD-3-Clause；WinCNG 后端，不捆绑 OpenSSL。 |
| GCC libgcc/libstdc++ | 本批 MinGW GCC 16.1.0 静态运行库 | GPL v3 加 GCC Runtime Library Exception 3.1。 |
| MinGW-w64 winpthreads | 本批工具链静态库；许可原文取自项目保留的 MinGW-w64 v14.0.0 COPYING | MIT 风格及 Lockless 派生部分 BSD 条款。 |
| Windows CNG、系统库/UCRT | 由目标 Windows 系统提供 | 加密、文件与系统功能；非本包附带 DLL。 |

Linux 任务助手以 Python 2.7/3 兼容源码嵌入程序，由已有 IC618 Python 执行；本机不需要 Python。SHA-256 使用项目自身实现。远端 ALPS 2025.03.sp2 和其许可证保持实验室现有安装，未随本包分发。

为保持模块四件套，发布脚本把以下六份许可证原文完整附在本 README 后部：nlohmann-json MIT、miniz LICENSE、libssh2 COPYING、GCC GPL-3.0、GCC Runtime Library Exception 3.1、winpthreads COPYING。它们是本交付 README 的组成部分；不得在再分发时只保留表格而删掉原文。

第三方许可证不等同于 ALPSRunner 自有源码或商业 ALPS 的许可，本交付没有为这两者授予新的开源授权。

<!-- THIRD_PARTY_LICENSES_APPENDED_BY_PACKAGER -->

# Third-party license texts

## nlohmann-json-MIT.txt

MIT License 

Copyright (c) 2013-2025 Niels Lohmann

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

## miniz-LICENSE.txt

Copyright 2013-2014 RAD Game Tools and Valve Software
Copyright 2010-2014 Rich Geldreich and Tenacious Software LLC

All Rights Reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.

## libssh2-COPYING.txt

/* Copyright (C) 2004-2007 Sara Golemon <sarag@libssh2.org>
 * Copyright (C) 2005,2006 Mikhail Gusarov <dottedmag@dottedmag.net>
 * Copyright (C) 2006-2007 The Written Word, Inc.
 * Copyright (C) 2007 Eli Fant <elifantu@mail.ru>
 * Copyright (C) 2009-2023 Daniel Stenberg
 * Copyright (C) 2008, 2009 Simon Josefsson
 * Copyright (C) 2000 Markus Friedl
 * Copyright (C) 2015 Microsoft Corp.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

## gcc-GPL-3.0.txt

                    GNU GENERAL PUBLIC LICENSE
                       Version 3, 29 June 2007

 Copyright (C) 2007 Free Software Foundation, Inc. <http://fsf.org/>
 Everyone is permitted to copy and distribute verbatim copies
 of this license document, but changing it is not allowed.

                            Preamble

  The GNU General Public License is a free, copyleft license for
software and other kinds of works.

  The licenses for most software and other practical works are designed
to take away your freedom to share and change the works.  By contrast,
the GNU General Public License is intended to guarantee your freedom to
share and change all versions of a program--to make sure it remains free
software for all its users.  We, the Free Software Foundation, use the
GNU General Public License for most of our software; it applies also to
any other work released this way by its authors.  You can apply it to
your programs, too.

  When we speak of free software, we are referring to freedom, not
price.  Our General Public Licenses are designed to make sure that you
have the freedom to distribute copies of free software (and charge for
them if you wish), that you receive source code or can get it if you
want it, that you can change the software or use pieces of it in new
free programs, and that you know you can do these things.

  To protect your rights, we need to prevent others from denying you
these rights or asking you to surrender the rights.  Therefore, you have
certain responsibilities if you distribute copies of the software, or if
you modify it: responsibilities to respect the freedom of others.

  For example, if you distribute copies of such a program, whether
gratis or for a fee, you must pass on to the recipients the same
freedoms that you received.  You must make sure that they, too, receive
or can get the source code.  And you must show them these terms so they
know their rights.

  Developers that use the GNU GPL protect your rights with two steps:
(1) assert copyright on the software, and (2) offer you this License
giving you legal permission to copy, distribute and/or modify it.

  For the developers' and authors' protection, the GPL clearly explains
that there is no warranty for this free software.  For both users' and
authors' sake, the GPL requires that modified versions be marked as
changed, so that their problems will not be attributed erroneously to
authors of previous versions.

  Some devices are designed to deny users access to install or run
modified versions of the software inside them, although the manufacturer
can do so.  This is fundamentally incompatible with the aim of
protecting users' freedom to change the software.  The systematic
pattern of such abuse occurs in the area of products for individuals to
use, which is precisely where it is most unacceptable.  Therefore, we
have designed this version of the GPL to prohibit the practice for those
products.  If such problems arise substantially in other domains, we
stand ready to extend this provision to those domains in future versions
of the GPL, as needed to protect the freedom of users.

  Finally, every program is threatened constantly by software patents.
States should not allow patents to restrict development and use of
software on general-purpose computers, but in those that do, we wish to
avoid the special danger that patents applied to a free program could
make it effectively proprietary.  To prevent this, the GPL assures that
patents cannot be used to render the program non-free.

  The precise terms and conditions for copying, distribution and
modification follow.

                       TERMS AND CONDITIONS

  0. Definitions.

  "This License" refers to version 3 of the GNU General Public License.

  "Copyright" also means copyright-like laws that apply to other kinds of
works, such as semiconductor masks.

  "The Program" refers to any copyrightable work licensed under this
License.  Each licensee is addressed as "you".  "Licensees" and
"recipients" may be individuals or organizations.

  To "modify" a work means to copy from or adapt all or part of the work
in a fashion requiring copyright permission, other than the making of an
exact copy.  The resulting work is called a "modified version" of the
earlier work or a work "based on" the earlier work.

  A "covered work" means either the unmodified Program or a work based
on the Program.

  To "propagate" a work means to do anything with it that, without
permission, would make you directly or secondarily liable for
infringement under applicable copyright law, except executing it on a
computer or modifying a private copy.  Propagation includes copying,
distribution (with or without modification), making available to the
public, and in some countries other activities as well.

  To "convey" a work means any kind of propagation that enables other
parties to make or receive copies.  Mere interaction with a user through
a computer network, with no transfer of a copy, is not conveying.

  An interactive user interface displays "Appropriate Legal Notices"
to the extent that it includes a convenient and prominently visible
feature that (1) displays an appropriate copyright notice, and (2)
tells the user that there is no warranty for the work (except to the
extent that warranties are provided), that licensees may convey the
work under this License, and how to view a copy of this License.  If
the interface presents a list of user commands or options, such as a
menu, a prominent item in the list meets this criterion.

  1. Source Code.

  The "source code" for a work means the preferred form of the work
for making modifications to it.  "Object code" means any non-source
form of a work.

  A "Standard Interface" means an interface that either is an official
standard defined by a recognized standards body, or, in the case of
interfaces specified for a particular programming language, one that
is widely used among developers working in that language.

  The "System Libraries" of an executable work include anything, other
than the work as a whole, that (a) is included in the normal form of
packaging a Major Component, but which is not part of that Major
Component, and (b) serves only to enable use of the work with that
Major Component, or to implement a Standard Interface for which an
implementation is available to the public in source code form.  A
"Major Component", in this context, means a major essential component
(kernel, window system, and so on) of the specific operating system
(if any) on which the executable work runs, or a compiler used to
produce the work, or an object code interpreter used to run it.

  The "Corresponding Source" for a work in object code form means all
the source code needed to generate, install, and (for an executable
work) run the object code and to modify the work, including scripts to
control those activities.  However, it does not include the work's
System Libraries, or general-purpose tools or generally available free
programs which are used unmodified in performing those activities but
which are not part of the work.  For example, Corresponding Source
includes interface definition files associated with source files for
the work, and the source code for shared libraries and dynamically
linked subprograms that the work is specifically designed to require,
such as by intimate data communication or control flow between those
subprograms and other parts of the work.

  The Corresponding Source need not include anything that users
can regenerate automatically from other parts of the Corresponding
Source.

  The Corresponding Source for a work in source code form is that
same work.

  2. Basic Permissions.

  All rights granted under this License are granted for the term of
copyright on the Program, and are irrevocable provided the stated
conditions are met.  This License explicitly affirms your unlimited
permission to run the unmodified Program.  The output from running a
covered work is covered by this License only if the output, given its
content, constitutes a covered work.  This License acknowledges your
rights of fair use or other equivalent, as provided by copyright law.

  You may make, run and propagate covered works that you do not
convey, without conditions so long as your license otherwise remains
in force.  You may convey covered works to others for the sole purpose
of having them make modifications exclusively for you, or provide you
with facilities for running those works, provided that you comply with
the terms of this License in conveying all material for which you do
not control copyright.  Those thus making or running the covered works
for you must do so exclusively on your behalf, under your direction
and control, on terms that prohibit them from making any copies of
your copyrighted material outside their relationship with you.

  Conveying under any other circumstances is permitted solely under
the conditions stated below.  Sublicensing is not allowed; section 10
makes it unnecessary.

  3. Protecting Users' Legal Rights From Anti-Circumvention Law.

  No covered work shall be deemed part of an effective technological
measure under any applicable law fulfilling obligations under article
11 of the WIPO copyright treaty adopted on 20 December 1996, or
similar laws prohibiting or restricting circumvention of such
measures.

  When you convey a covered work, you waive any legal power to forbid
circumvention of technological measures to the extent such circumvention
is effected by exercising rights under this License with respect to
the covered work, and you disclaim any intention to limit operation or
modification of the work as a means of enforcing, against the work's
users, your or third parties' legal rights to forbid circumvention of
technological measures.

  4. Conveying Verbatim Copies.

  You may convey verbatim copies of the Program's source code as you
receive it, in any medium, provided that you conspicuously and
appropriately publish on each copy an appropriate copyright notice;
keep intact all notices stating that this License and any
non-permissive terms added in accord with section 7 apply to the code;
keep intact all notices of the absence of any warranty; and give all
recipients a copy of this License along with the Program.

  You may charge any price or no price for each copy that you convey,
and you may offer support or warranty protection for a fee.

  5. Conveying Modified Source Versions.

  You may convey a work based on the Program, or the modifications to
produce it from the Program, in the form of source code under the
terms of section 4, provided that you also meet all of these conditions:

    a) The work must carry prominent notices stating that you modified
    it, and giving a relevant date.

    b) The work must carry prominent notices stating that it is
    released under this License and any conditions added under section
    7.  This requirement modifies the requirement in section 4 to
    "keep intact all notices".

    c) You must license the entire work, as a whole, under this
    License to anyone who comes into possession of a copy.  This
    License will therefore apply, along with any applicable section 7
    additional terms, to the whole of the work, and all its parts,
    regardless of how they are packaged.  This License gives no
    permission to license the work in any other way, but it does not
    invalidate such permission if you have separately received it.

    d) If the work has interactive user interfaces, each must display
    Appropriate Legal Notices; however, if the Program has interactive
    interfaces that do not display Appropriate Legal Notices, your
    work need not make them do so.

  A compilation of a covered work with other separate and independent
works, which are not by their nature extensions of the covered work,
and which are not combined with it such as to form a larger program,
in or on a volume of a storage or distribution medium, is called an
"aggregate" if the compilation and its resulting copyright are not
used to limit the access or legal rights of the compilation's users
beyond what the individual works permit.  Inclusion of a covered work
in an aggregate does not cause this License to apply to the other
parts of the aggregate.

  6. Conveying Non-Source Forms.

  You may convey a covered work in object code form under the terms
of sections 4 and 5, provided that you also convey the
machine-readable Corresponding Source under the terms of this License,
in one of these ways:

    a) Convey the object code in, or embodied in, a physical product
    (including a physical distribution medium), accompanied by the
    Corresponding Source fixed on a durable physical medium
    customarily used for software interchange.

    b) Convey the object code in, or embodied in, a physical product
    (including a physical distribution medium), accompanied by a
    written offer, valid for at least three years and valid for as
    long as you offer spare parts or customer support for that product
    model, to give anyone who possesses the object code either (1) a
    copy of the Corresponding Source for all the software in the
    product that is covered by this License, on a durable physical
    medium customarily used for software interchange, for a price no
    more than your reasonable cost of physically performing this
    conveying of source, or (2) access to copy the
    Corresponding Source from a network server at no charge.

    c) Convey individual copies of the object code with a copy of the
    written offer to provide the Corresponding Source.  This
    alternative is allowed only occasionally and noncommercially, and
    only if you received the object code with such an offer, in accord
    with subsection 6b.

    d) Convey the object code by offering access from a designated
    place (gratis or for a charge), and offer equivalent access to the
    Corresponding Source in the same way through the same place at no
    further charge.  You need not require recipients to copy the
    Corresponding Source along with the object code.  If the place to
    copy the object code is a network server, the Corresponding Source
    may be on a different server (operated by you or a third party)
    that supports equivalent copying facilities, provided you maintain
    clear directions next to the object code saying where to find the
    Corresponding Source.  Regardless of what server hosts the
    Corresponding Source, you remain obligated to ensure that it is
    available for as long as needed to satisfy these requirements.

    e) Convey the object code using peer-to-peer transmission, provided
    you inform other peers where the object code and Corresponding
    Source of the work are being offered to the general public at no
    charge under subsection 6d.

  A separable portion of the object code, whose source code is excluded
from the Corresponding Source as a System Library, need not be
included in conveying the object code work.

  A "User Product" is either (1) a "consumer product", which means any
tangible personal property which is normally used for personal, family,
or household purposes, or (2) anything designed or sold for incorporation
into a dwelling.  In determining whether a product is a consumer product,
doubtful cases shall be resolved in favor of coverage.  For a particular
product received by a particular user, "normally used" refers to a
typical or common use of that class of product, regardless of the status
of the particular user or of the way in which the particular user
actually uses, or expects or is expected to use, the product.  A product
is a consumer product regardless of whether the product has substantial
commercial, industrial or non-consumer uses, unless such uses represent
the only significant mode of use of the product.

  "Installation Information" for a User Product means any methods,
procedures, authorization keys, or other information required to install
and execute modified versions of a covered work in that User Product from
a modified version of its Corresponding Source.  The information must
suffice to ensure that the continued functioning of the modified object
code is in no case prevented or interfered with solely because
modification has been made.

  If you convey an object code work under this section in, or with, or
specifically for use in, a User Product, and the conveying occurs as
part of a transaction in which the right of possession and use of the
User Product is transferred to the recipient in perpetuity or for a
fixed term (regardless of how the transaction is characterized), the
Corresponding Source conveyed under this section must be accompanied
by the Installation Information.  But this requirement does not apply
if neither you nor any third party retains the ability to install
modified object code on the User Product (for example, the work has
been installed in ROM).

  The requirement to provide Installation Information does not include a
requirement to continue to provide support service, warranty, or updates
for a work that has been modified or installed by the recipient, or for
the User Product in which it has been modified or installed.  Access to a
network may be denied when the modification itself materially and
adversely affects the operation of the network or violates the rules and
protocols for communication across the network.

  Corresponding Source conveyed, and Installation Information provided,
in accord with this section must be in a format that is publicly
documented (and with an implementation available to the public in
source code form), and must require no special password or key for
unpacking, reading or copying.

  7. Additional Terms.

  "Additional permissions" are terms that supplement the terms of this
License by making exceptions from one or more of its conditions.
Additional permissions that are applicable to the entire Program shall
be treated as though they were included in this License, to the extent
that they are valid under applicable law.  If additional permissions
apply only to part of the Program, that part may be used separately
under those permissions, but the entire Program remains governed by
this License without regard to the additional permissions.

  When you convey a copy of a covered work, you may at your option
remove any additional permissions from that copy, or from any part of
it.  (Additional permissions may be written to require their own
removal in certain cases when you modify the work.)  You may place
additional permissions on material, added by you to a covered work,
for which you have or can give appropriate copyright permission.

  Notwithstanding any other provision of this License, for material you
add to a covered work, you may (if authorized by the copyright holders of
that material) supplement the terms of this License with terms:

    a) Disclaiming warranty or limiting liability differently from the
    terms of sections 15 and 16 of this License; or

    b) Requiring preservation of specified reasonable legal notices or
    author attributions in that material or in the Appropriate Legal
    Notices displayed by works containing it; or

    c) Prohibiting misrepresentation of the origin of that material, or
    requiring that modified versions of such material be marked in
    reasonable ways as different from the original version; or

    d) Limiting the use for publicity purposes of names of licensors or
    authors of the material; or

    e) Declining to grant rights under trademark law for use of some
    trade names, trademarks, or service marks; or

    f) Requiring indemnification of licensors and authors of that
    material by anyone who conveys the material (or modified versions of
    it) with contractual assumptions of liability to the recipient, for
    any liability that these contractual assumptions directly impose on
    those licensors and authors.

  All other non-permissive additional terms are considered "further
restrictions" within the meaning of section 10.  If the Program as you
received it, or any part of it, contains a notice stating that it is
governed by this License along with a term that is a further
restriction, you may remove that term.  If a license document contains
a further restriction but permits relicensing or conveying under this
License, you may add to a covered work material governed by the terms
of that license document, provided that the further restriction does
not survive such relicensing or conveying.

  If you add terms to a covered work in accord with this section, you
must place, in the relevant source files, a statement of the
additional terms that apply to those files, or a notice indicating
where to find the applicable terms.

  Additional terms, permissive or non-permissive, may be stated in the
form of a separately written license, or stated as exceptions;
the above requirements apply either way.

  8. Termination.

  You may not propagate or modify a covered work except as expressly
provided under this License.  Any attempt otherwise to propagate or
modify it is void, and will automatically terminate your rights under
this License (including any patent licenses granted under the third
paragraph of section 11).

  However, if you cease all violation of this License, then your
license from a particular copyright holder is reinstated (a)
provisionally, unless and until the copyright holder explicitly and
finally terminates your license, and (b) permanently, if the copyright
holder fails to notify you of the violation by some reasonable means
prior to 60 days after the cessation.

  Moreover, your license from a particular copyright holder is
reinstated permanently if the copyright holder notifies you of the
violation by some reasonable means, this is the first time you have
received notice of violation of this License (for any work) from that
copyright holder, and you cure the violation prior to 30 days after
your receipt of the notice.

  Termination of your rights under this section does not terminate the
licenses of parties who have received copies or rights from you under
this License.  If your rights have been terminated and not permanently
reinstated, you do not qualify to receive new licenses for the same
material under section 10.

  9. Acceptance Not Required for Having Copies.

  You are not required to accept this License in order to receive or
run a copy of the Program.  Ancillary propagation of a covered work
occurring solely as a consequence of using peer-to-peer transmission
to receive a copy likewise does not require acceptance.  However,
nothing other than this License grants you permission to propagate or
modify any covered work.  These actions infringe copyright if you do
not accept this License.  Therefore, by modifying or propagating a
covered work, you indicate your acceptance of this License to do so.

  10. Automatic Licensing of Downstream Recipients.

  Each time you convey a covered work, the recipient automatically
receives a license from the original licensors, to run, modify and
propagate that work, subject to this License.  You are not responsible
for enforcing compliance by third parties with this License.

  An "entity transaction" is a transaction transferring control of an
organization, or substantially all assets of one, or subdividing an
organization, or merging organizations.  If propagation of a covered
work results from an entity transaction, each party to that
transaction who receives a copy of the work also receives whatever
licenses to the work the party's predecessor in interest had or could
give under the previous paragraph, plus a right to possession of the
Corresponding Source of the work from the predecessor in interest, if
the predecessor has it or can get it with reasonable efforts.

  You may not impose any further restrictions on the exercise of the
rights granted or affirmed under this License.  For example, you may
not impose a license fee, royalty, or other charge for exercise of
rights granted under this License, and you may not initiate litigation
(including a cross-claim or counterclaim in a lawsuit) alleging that
any patent claim is infringed by making, using, selling, offering for
sale, or importing the Program or any portion of it.

  11. Patents.

  A "contributor" is a copyright holder who authorizes use under this
License of the Program or a work on which the Program is based.  The
work thus licensed is called the contributor's "contributor version".

  A contributor's "essential patent claims" are all patent claims
owned or controlled by the contributor, whether already acquired or
hereafter acquired, that would be infringed by some manner, permitted
by this License, of making, using, or selling its contributor version,
but do not include claims that would be infringed only as a
consequence of further modification of the contributor version.  For
purposes of this definition, "control" includes the right to grant
patent sublicenses in a manner consistent with the requirements of
this License.

  Each contributor grants you a non-exclusive, worldwide, royalty-free
patent license under the contributor's essential patent claims, to
make, use, sell, offer for sale, import and otherwise run, modify and
propagate the contents of its contributor version.

  In the following three paragraphs, a "patent license" is any express
agreement or commitment, however denominated, not to enforce a patent
(such as an express permission to practice a patent or covenant not to
sue for patent infringement).  To "grant" such a patent license to a
party means to make such an agreement or commitment not to enforce a
patent against the party.

  If you convey a covered work, knowingly relying on a patent license,
and the Corresponding Source of the work is not available for anyone
to copy, free of charge and under the terms of this License, through a
publicly available network server or other readily accessible means,
then you must either (1) cause the Corresponding Source to be so
available, or (2) arrange to deprive yourself of the benefit of the
patent license for this particular work, or (3) arrange, in a manner
consistent with the requirements of this License, to extend the patent
license to downstream recipients.  "Knowingly relying" means you have
actual knowledge that, but for the patent license, your conveying the
covered work in a country, or your recipient's use of the covered work
in a country, would infringe one or more identifiable patents in that
country that you have reason to believe are valid.

  If, pursuant to or in connection with a single transaction or
arrangement, you convey, or propagate by procuring conveyance of, a
covered work, and grant a patent license to some of the parties
receiving the covered work authorizing them to use, propagate, modify
or convey a specific copy of the covered work, then the patent license
you grant is automatically extended to all recipients of the covered
work and works based on it.

  A patent license is "discriminatory" if it does not include within
the scope of its coverage, prohibits the exercise of, or is
conditioned on the non-exercise of one or more of the rights that are
specifically granted under this License.  You may not convey a covered
work if you are a party to an arrangement with a third party that is
in the business of distributing software, under which you make payment
to the third party based on the extent of your activity of conveying
the work, and under which the third party grants, to any of the
parties who would receive the covered work from you, a discriminatory
patent license (a) in connection with copies of the covered work
conveyed by you (or copies made from those copies), or (b) primarily
for and in connection with specific products or compilations that
contain the covered work, unless you entered into that arrangement,
or that patent license was granted, prior to 28 March 2007.

  Nothing in this License shall be construed as excluding or limiting
any implied license or other defenses to infringement that may
otherwise be available to you under applicable patent law.

  12. No Surrender of Others' Freedom.

  If conditions are imposed on you (whether by court order, agreement or
otherwise) that contradict the conditions of this License, they do not
excuse you from the conditions of this License.  If you cannot convey a
covered work so as to satisfy simultaneously your obligations under this
License and any other pertinent obligations, then as a consequence you may
not convey it at all.  For example, if you agree to terms that obligate you
to collect a royalty for further conveying from those to whom you convey
the Program, the only way you could satisfy both those terms and this
License would be to refrain entirely from conveying the Program.

  13. Use with the GNU Affero General Public License.

  Notwithstanding any other provision of this License, you have
permission to link or combine any covered work with a work licensed
under version 3 of the GNU Affero General Public License into a single
combined work, and to convey the resulting work.  The terms of this
License will continue to apply to the part which is the covered work,
but the special requirements of the GNU Affero General Public License,
section 13, concerning interaction through a network will apply to the
combination as such.

  14. Revised Versions of this License.

  The Free Software Foundation may publish revised and/or new versions of
the GNU General Public License from time to time.  Such new versions will
be similar in spirit to the present version, but may differ in detail to
address new problems or concerns.

  Each version is given a distinguishing version number.  If the
Program specifies that a certain numbered version of the GNU General
Public License "or any later version" applies to it, you have the
option of following the terms and conditions either of that numbered
version or of any later version published by the Free Software
Foundation.  If the Program does not specify a version number of the
GNU General Public License, you may choose any version ever published
by the Free Software Foundation.

  If the Program specifies that a proxy can decide which future
versions of the GNU General Public License can be used, that proxy's
public statement of acceptance of a version permanently authorizes you
to choose that version for the Program.

  Later license versions may give you additional or different
permissions.  However, no additional obligations are imposed on any
author or copyright holder as a result of your choosing to follow a
later version.

  15. Disclaimer of Warranty.

  THERE IS NO WARRANTY FOR THE PROGRAM, TO THE EXTENT PERMITTED BY
APPLICABLE LAW.  EXCEPT WHEN OTHERWISE STATED IN WRITING THE COPYRIGHT
HOLDERS AND/OR OTHER PARTIES PROVIDE THE PROGRAM "AS IS" WITHOUT WARRANTY
OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING, BUT NOT LIMITED TO,
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
PURPOSE.  THE ENTIRE RISK AS TO THE QUALITY AND PERFORMANCE OF THE PROGRAM
IS WITH YOU.  SHOULD THE PROGRAM PROVE DEFECTIVE, YOU ASSUME THE COST OF
ALL NECESSARY SERVICING, REPAIR OR CORRECTION.

  16. Limitation of Liability.

  IN NO EVENT UNLESS REQUIRED BY APPLICABLE LAW OR AGREED TO IN WRITING
WILL ANY COPYRIGHT HOLDER, OR ANY OTHER PARTY WHO MODIFIES AND/OR CONVEYS
THE PROGRAM AS PERMITTED ABOVE, BE LIABLE TO YOU FOR DAMAGES, INCLUDING ANY
GENERAL, SPECIAL, INCIDENTAL OR CONSEQUENTIAL DAMAGES ARISING OUT OF THE
USE OR INABILITY TO USE THE PROGRAM (INCLUDING BUT NOT LIMITED TO LOSS OF
DATA OR DATA BEING RENDERED INACCURATE OR LOSSES SUSTAINED BY YOU OR THIRD
PARTIES OR A FAILURE OF THE PROGRAM TO OPERATE WITH ANY OTHER PROGRAMS),
EVEN IF SUCH HOLDER OR OTHER PARTY HAS BEEN ADVISED OF THE POSSIBILITY OF
SUCH DAMAGES.

  17. Interpretation of Sections 15 and 16.

  If the disclaimer of warranty and limitation of liability provided
above cannot be given local legal effect according to their terms,
reviewing courts shall apply local law that most closely approximates
an absolute waiver of all civil liability in connection with the
Program, unless a warranty or assumption of liability accompanies a
copy of the Program in return for a fee.

                     END OF TERMS AND CONDITIONS

            How to Apply These Terms to Your New Programs

  If you develop a new program, and you want it to be of the greatest
possible use to the public, the best way to achieve this is to make it
free software which everyone can redistribute and change under these terms.

  To do so, attach the following notices to the program.  It is safest
to attach them to the start of each source file to most effectively
state the exclusion of warranty; and each file should have at least
the "copyright" line and a pointer to where the full notice is found.

    <one line to give the program's name and a brief idea of what it does.>
    Copyright (C) <year>  <name of author>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

Also add information on how to contact you by electronic and paper mail.

  If the program does terminal interaction, make it output a short
notice like this when it starts in an interactive mode:

    <program>  Copyright (C) <year>  <name of author>
    This program comes with ABSOLUTELY NO WARRANTY; for details type `show w'.
    This is free software, and you are welcome to redistribute it
    under certain conditions; type `show c' for details.

The hypothetical commands `show w' and `show c' should show the appropriate
parts of the General Public License.  Of course, your program's commands
might be different; for a GUI interface, you would use an "about box".

  You should also get your employer (if you work as a programmer) or school,
if any, to sign a "copyright disclaimer" for the program, if necessary.
For more information on this, and how to apply and follow the GNU GPL, see
<http://www.gnu.org/licenses/>.

  The GNU General Public License does not permit incorporating your program
into proprietary programs.  If your program is a subroutine library, you
may consider it more useful to permit linking proprietary applications with
the library.  If this is what you want to do, use the GNU Lesser General
Public License instead of this License.  But first, please read
<http://www.gnu.org/philosophy/why-not-lgpl.html>.

## gcc-runtime-exception-3.1.txt

GCC RUNTIME LIBRARY EXCEPTION

Version 3.1, 31 March 2009

Copyright (C) 2009 Free Software Foundation, Inc. <http://fsf.org/>

Everyone is permitted to copy and distribute verbatim copies of this
license document, but changing it is not allowed.

This GCC Runtime Library Exception ("Exception") is an additional
permission under section 7 of the GNU General Public License, version
3 ("GPLv3"). It applies to a given file (the "Runtime Library") that
bears a notice placed by the copyright holder of the file stating that
the file is governed by GPLv3 along with this Exception.

When you use GCC to compile a program, GCC may combine portions of
certain GCC header files and runtime libraries with the compiled
program. The purpose of this Exception is to allow compilation of
non-GPL (including proprietary) programs to use, in this way, the
header files and runtime libraries covered by this Exception.

0. Definitions.

A file is an "Independent Module" if it either requires the Runtime
Library for execution after a Compilation Process, or makes use of an
interface provided by the Runtime Library, but is not otherwise based
on the Runtime Library.

"GCC" means a version of the GNU Compiler Collection, with or without
modifications, governed by version 3 (or a specified later version) of
the GNU General Public License (GPL) with the option of using any
subsequent versions published by the FSF.

"GPL-compatible Software" is software whose conditions of propagation,
modification and use would permit combination with GCC in accord with
the license of GCC.

"Target Code" refers to output from any compiler for a real or virtual
target processor architecture, in executable form or suitable for
input to an assembler, loader, linker and/or execution
phase. Notwithstanding that, Target Code does not include data in any
format that is used as a compiler intermediate representation, or used
for producing a compiler intermediate representation.

The "Compilation Process" transforms code entirely represented in
non-intermediate languages designed for human-written code, and/or in
Java Virtual Machine byte code, into Target Code. Thus, for example,
use of source code generators and preprocessors need not be considered
part of the Compilation Process, since the Compilation Process can be
understood as starting with the output of the generators or
preprocessors.

A Compilation Process is "Eligible" if it is done using GCC, alone or
with other GPL-compatible software, or if it is done without using any
work based on GCC. For example, using non-GPL-compatible Software to
optimize any GCC intermediate representations would not qualify as an
Eligible Compilation Process.

1. Grant of Additional Permission.

You have permission to propagate a work of Target Code formed by
combining the Runtime Library with Independent Modules, even if such
propagation would otherwise violate the terms of GPLv3, provided that
all Target Code was generated by Eligible Compilation Processes. You
may then convey such a combination under terms of your choice,
consistent with the licensing of the Independent Modules.

2. No Weakening of GCC Copyleft.

The availability of this Exception does not imply any general
presumption that third-party software is unaffected by the copyleft
requirements of the license of GCC.


## winpthreads-COPYING.txt

Copyright (c) 2011 mingw-w64 project

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.


/*
 * Parts of this library are derived by:
 *
 * Posix Threads library for Microsoft Windows
 *
 * Use at own risk, there is no implied warranty to this code.
 * It uses undocumented features of Microsoft Windows that can change
 * at any time in the future.
 *
 * (C) 2010 Lockless Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *  * Neither the name of Lockless Inc. nor the names of its contributors may be
 *    used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AN
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */
