# windows采集每个进程网络流量的方法 #

----------
## 采集每个进程网络流量方法考虑 ##

首先我希望使用程序来采集windows使用的每个进程中的网络流量使用情况。

- 使用windows api：自带的windows api当中仅能获取所有进程的网络流量。比如通过getIfEntry函数获取MIB_IFROW结构，读取当中的dwInOctets，dwOutOctets字段，可以获取自系统启动来接收的字节数和发送的字节数。但无法获取每个进程的网络流量情况。
- 使用perform计数器：通过调用pdh.lib中的计数器，添加一个PerformanceCounter来监视性能监视器中的数据，添加NetWork Interface字段中数据，可以获取系统总共的网络流量，但是无法获取获得单个进程的网络流量。
- 使用ETW：可以通过指定数据采集到每个进程的具体网络流量信息，以下是具体实现方法。（任务管理器中网络流量统计采取类似方法）

----------
## 什么是ETW ##
ETW全称(Event Tracing for Windows),又名Windows事件跟踪，是一种有效的内核级跟踪工具，允许你将内核或应用程序定义的事件记录到日志文件中。 可以实时使用事件或从日志文件使用事件，并使用它们调试应用程序或确定应用程序中发生性能问题的位置。

**可以获取到Perform等计数器中缺失的数据以及一些内核的数据。**

以下是ETW的架构图（从官方文档转载）

![image](pic\etw.png)
<p align="center">ETW架构</p>

----------
## ETW组成 ##
在windows中，ETW主要由以下三种成员组成

    控制器（Controllers）, 用于启动和停止事件跟踪会话并启动提供程序
    提供者（Providers）, 用于提供事件所需的程序
    消费者（Consumers）, 用于使用事件，读取事件内容的角色

在windows中，可以使用以下命令来获取系统中的提供者：

![image](pic\provider.png)
<p align="center">provider对象</p>

每种provider中均收集着不同的信息。在采集每个进程网络流量的时候，需要采集Microsoft-Windows-Kernel-Network的provider。其中根据查询etw-providers-docs的资料得到具体的opcode所对应的数据内容。

     <event value="10" symbol="KERNEL_NETWORK_TASK_TCPIPDatasent." version="0" task="KERNEL_NETWORK_TASK_TCPIP" opcode="Datasent." level="win:Informational" keywords="KERNEL_NETWORK_KEYWORD_IPV4" template="KERNEL_NETWORK_TASK_TCPIPDatasent.Args"/>
     <event value="11" symbol="KERNEL_NETWORK_TASK_TCPIPDatareceived." version="0" task="KERNEL_NETWORK_TASK_TCPIP" opcode="Datareceived." level="win:Informational" keywords="KERNEL_NETWORK_KEYWORD_IPV4" template="KERNEL_NETWORK_TASK_TCPIPDatareceived.Args"/>
     <event value="12" symbol="KERNEL_NETWORK_TASK_TCPIPConnectionattempted." version="0" task="KERNEL_NETWORK_TASK_TCPIP" opcode="Connectionattempted." level="win:Informational" keywords="KERNEL_NETWORK_KEYWORD_IPV4" template="KERNEL_NETWORK_TASK_TCPIPConnectionattempted.Args"/>
     <event value="13" symbol="KERNEL_NETWORK_TASK_TCPIPDisconnectissued." version="0" task="KERNEL_NETWORK_TASK_TCPIP" opcode="Disconnectissued." level="win:Informational" keywords="KERNEL_NETWORK_KEYWORD_IPV4" template="KERNEL_NETWORK_TASK_TCPIPDatareceived.Args"/>
     <event value="14" symbol="KERNEL_NETWORK_TASK_TCPIPDataretransmitted." version="0" task="KERNEL_NETWORK_TASK_TCPIP" opcode="Dataretransmitted." level="win:Informational" keywords="KERNEL_NETWORK_KEYWORD_IPV4" template="KERNEL_NETWORK_TASK_TCPIPDatareceived.Args"/>
     <event value="15" symbol="KERNEL_NETWORK_TASK_TCPIPConnectionaccepted." version="0" task="KERNEL_NETWORK_TASK_TCPIP" opcode="Connectionaccepted." level="win:Informational" keywords="KERNEL_NETWORK_KEYWORD_IPV4" template="KERNEL_NETWORK_TASK_TCPIPConnectionattempted.Args"/>
     <event value="16" symbol="KERNEL_NETWORK_TASK_TCPIPReconnectattempted." version="0" task="KERNEL_NETWORK_TASK_TCPIP" opcode="Reconnectattempted." level="win:Informational" keywords="KERNEL_NETWORK_KEYWORD_IPV4" template="KERNEL_NETWORK_TASK_TCPIPDatareceived.Args"/>
     <event value="17" symbol="KERNEL_NETWORK_TASK_TCPIPTCPconnectionattemptfailed." version="0" task="KERNEL_NETWORK_TASK_TCPIP" opcode="TCPconnectionattemptfailed." level="win:Informational" keywords="KERNEL_NETWORK_KEYWORD_IPV4 KERNEL_NETWORK_KEYWORD_IPV6" template="KERNEL_NETWORK_TASK_TCPIPTCPconnectionattemptfailed.Args"/>
     <event value="18" symbol="KERNEL_NETWORK_TASK_TCPIPProtocolcopieddataonbehalfofuser." version="0" task="KERNEL_NETWORK_TASK_TCPIP" opcode="Protocolcopieddataonbehalfofuser." level="win:Informational" keywords="KERNEL_NETWORK_KEYWORD_IPV4" template="KERNEL_NETWORK_TASK_TCPIPDatareceived.Args"/>
     <event value="26" symbol="KERNEL_NETWORK_TASK_TCPIPDatasent.26" version="0" task="KERNEL_NETWORK_TASK_TCPIP" opcode="Datasent." level="win:Informational" keywords="KERNEL_NETWORK_KEYWORD_IPV6" template="KERNEL_NETWORK_TASK_TCPIPDatasent.26Args"/>
     <event value="27" symbol="KERNEL_NETWORK_TASK_TCPIPDatareceived.27" version="0" task="KERNEL_NETWORK_TASK_TCPIP" opcode="Datareceived." level="win:Informational" keywords="KERNEL_NETWORK_KEYWORD_IPV6" template="KERNEL_NETWORK_TASK_TCPIPDatareceived.27Args"/>
     <event value="28" symbol="KERNEL_NETWORK_TASK_TCPIPConnectionattempted.28" version="0" task="KERNEL_NETWORK_TASK_TCPIP" opcode="Connectionattempted." level="win:Informational" keywords="KERNEL_NETWORK_KEYWORD_IPV6" template="KERNEL_NETWORK_TASK_TCPIPConnectionattempted.28Args"/>
     <event value="29" symbol="KERNEL_NETWORK_TASK_TCPIPDisconnectissued.29" version="0" task="KERNEL_NETWORK_TASK_TCPIP" opcode="Disconnectissued." level="win:Informational" keywords="KERNEL_NETWORK_KEYWORD_IPV6" template="KERNEL_NETWORK_TASK_TCPIPDatareceived.27Args"/>
     <event value="30" symbol="KERNEL_NETWORK_TASK_TCPIPDataretransmitted.30" version="0" task="KERNEL_NETWORK_TASK_TCPIP" opcode="Dataretransmitted." level="win:Informational" keywords="KERNEL_NETWORK_KEYWORD_IPV6" template="KERNEL_NETWORK_TASK_TCPIPDatareceived.27Args"/>
     <event value="31" symbol="KERNEL_NETWORK_TASK_TCPIPConnectionaccepted.31" version="0" task="KERNEL_NETWORK_TASK_TCPIP" opcode="Connectionaccepted." level="win:Informational" keywords="KERNEL_NETWORK_KEYWORD_IPV6" template="KERNEL_NETWORK_TASK_TCPIPConnectionattempted.28Args"/>
     <event value="32" symbol="KERNEL_NETWORK_TASK_TCPIPReconnectattempted.32" version="0" task="KERNEL_NETWORK_TASK_TCPIP" opcode="Reconnectattempted." level="win:Informational" keywords="KERNEL_NETWORK_KEYWORD_IPV6" template="KERNEL_NETWORK_TASK_TCPIPDatareceived.27Args"/>
     <event value="34" symbol="KERNEL_NETWORK_TASK_TCPIPProtocolcopieddataonbehalfofuser.34" version="0" task="KERNEL_NETWORK_TASK_TCPIP" opcode="Protocolcopieddataonbehalfofuser." level="win:Informational" keywords="KERNEL_NETWORK_KEYWORD_IPV6" template="KERNEL_NETWORK_TASK_TCPIPDatareceived.27Args"/>
     <event value="42" symbol="KERNEL_NETWORK_TASK_UDPIPDatasentoverUDPprotocol." version="0" task="KERNEL_NETWORK_TASK_UDPIP" opcode="DatasentoverUDPprotocol." level="win:Informational" keywords="KERNEL_NETWORK_KEYWORD_IPV4" template="KERNEL_NETWORK_TASK_TCPIPDatareceived.Args"/>
     <event value="43" symbol="KERNEL_NETWORK_TASK_UDPIPDatareceivedoverUDPprotocol." version="0" task="KERNEL_NETWORK_TASK_UDPIP" opcode="DatareceivedoverUDPprotocol." level="win:Informational" keywords="KERNEL_NETWORK_KEYWORD_IPV4" template="KERNEL_NETWORK_TASK_TCPIPDatareceived.Args"/>
     <event value="49" symbol="KERNEL_NETWORK_TASK_UDPIPUDPconnectionattemptfailed." version="0" task="KERNEL_NETWORK_TASK_UDPIP" opcode="UDPconnectionattemptfailed." level="win:Informational" keywords="KERNEL_NETWORK_KEYWORD_IPV4 KERNEL_NETWORK_KEYWORD_IPV6" template="KERNEL_NETWORK_TASK_TCPIPTCPconnectionattemptfailed.Args"/>
     <event value="58" symbol="KERNEL_NETWORK_TASK_UDPIPDatasentoverUDPprotocol.58" version="0" task="KERNEL_NETWORK_TASK_UDPIP" opcode="DatasentoverUDPprotocol." level="win:Informational" keywords="KERNEL_NETWORK_KEYWORD_IPV6" template="KERNEL_NETWORK_TASK_TCPIPDatareceived.27Args"/>
     <event value="59" symbol="KERNEL_NETWORK_TASK_UDPIPDatareceivedoverUDPprotocol.59" version="0" task="KERNEL_NETWORK_TASK_UDPIP" opcode="DatareceivedoverUDPprotocol." level="win:Informational" keywords="KERNEL_NETWORK_KEYWORD_IPV6" template="KERNEL_NETWORK_TASK_TCPIPDatareceived.27Args"/>


（参考链接：https://github.com/repnz/etw-providers-docs/）
其中，根据以上数据，自己创建对应的controller以及consumer进行具体流量的采集，请使用管理员权限执行程序。


