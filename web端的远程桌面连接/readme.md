# **主流工具：guacamole和novnc**

novnc和guacamole均可以在web中对主机进行远程连接。

----------
## novnc ##
novnc主要通过vnc来进行连接，而novnc充当一个vnc client的效果，在web中对vnc server进行访问。novnc当中存在一个组件叫做`websockify`，可以支持vnc直接通过websocket进行通信。
> vnc（Virtual Network Console）分为server端和client端两部分，基于TCP的通信。

**优势**：


- novnc已经存在很久，性能较为稳定，可以直接通过浏览器去访问。
- novnc相比于传统vnc在安全上也进行了考虑。
- 相比于guacamole novnc部署较为简单。

**劣势**：

- 仅仅支持vnc协议，多样性不如guacamole。

----------
## guacamole ##
Apache Guacamole 是一个无客户端的远程桌面网关，它支持众多标准管理协议，例如 VNC，RDP，SSH 等等。可以支持http通信也支持websocket通信。

而Guacamole由`guacamole-server` 和 `guacamole-client` 组成

**guacamole-client：**是一个web app和web server的集合体，让用户通过此来访问web。

**guacamole-server：**接受并处理guacamole-client发送来的请求，其中包含核心组件`guacd`，用来处理不同的协议，例如FreeRDP，libssh2，LibVNC等。

**优势**：

- guacamole支持docker和正常部署两种方法，部署方法多样
- guacamole支持多种协议可以自己进行选择
- guacamole可以确保连接的安全

**劣势**：

- 部署较为复杂，性能开销会稍大