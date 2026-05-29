#pragma once

#include <cstdint>
#include <string>

// 通用头部
// Cache-Control	指定请求/响应缓存机制，如 no-cache、max-age=0 要求重新验证
// Connection	控制连接选项，如 keep-alive 保持连接，close 响应后关闭连接
// Date	消息发送的日期时间（部分请求也会带，但更多见响应）
// Trailer	声明消息尾部会出现的头部字段（分块传输编码时）
// Transfer-Encoding	消息体的传输编码，请求中常用 chunked（分块发送）
// Upgrade	请求升级到其他协议（如WebSocket），配合 Connection: upgrade
// Via	记录经过的代理服务器信息，用于追踪转发路径

// 请求专属头部
// Host	HTTP/1.1必填，指定请求的域名和端口。如 Host: www.example.com 用于虚拟主机区分
// User-Agent	标识客户端软件信息（浏览器、系统、引擎等），服务器据此做兼容处理或统计
// Accept	告知服务器客户端可处理的媒体类型（MIME），如 text/html, application/json，用于内容协商
// Accept-Charset	可接受的字符集（如 utf-8），现已较少使用
// Accept-Encoding	可接受的内容编码（压缩算法），如 gzip, deflate, br，服务器据此压缩响应体
// Accept-Language	期望的自然语言，如 zh-CN, en;q=0.9，用于国际化
// Authorization	携带身份验证凭据，通常为 Basic、Bearer、Digest 等认证方案的凭证
// Proxy-Authorization	用于向代理服务器认证的凭据
// Expect	客户端期待服务器的特定行为，最著名的是 100-continue，先询问再发送大请求体
// From	客户端用户的电子邮箱地址（极少使用，隐私问题）
// If-Match	条件请求：仅当资源的ETag匹配时才执行，用于防止更新冲突
// If-None-Match	与 If-Match 相反，ETag不匹配时才执行；常用于 GET 缓存验证（返回304）
// If-Modified-Since	如果资源在此日期时间之后修改过，则处理请求，常用于缓存（返回200或304）
// If-Unmodified-Since	资源在此时间之后未被修改时才处理，用于乐观锁
// If-Range	配合Range头，如果资源未改变则发送指定范围，否则返回完整实体
// Max-Forwards	限制请求经过的代理/网关跳数，主要用于 TRACE 和 OPTIONS
// Range	请求资源的某个字节范围，如 bytes=0-499，用于断点续传
// Referer	当前请求来源页面的URL，用于统计、防盗链等
// TE	告知服务器可接受的传输编码（除分块外的扩展），如 trailers
// Origin	发起跨源请求的来源（协议+域名+端口），用于CORS和POST等安全机制

// 实体头部（描述消息体）
// Content-Type	请求体的媒体类型，如 application/json、application/x-www-form-urlencoded、multipart/form-data
// Content-Length	请求体的长度（字节）。若不使用分块传输，则必须带此头
// Content-Encoding	请求体的内容编码（压缩），如 gzip，但请求中较少使用
// Content-Language	请求体所用的自然语言
// Content-Location	请求体在服务端的实际位置（用于内容协商等场景）
// Content-MD5	请求体的MD5校验值，用于完整性检查（已过时，多用TLS保证）

// 重要扩展及常用头部
// Cookie	将客户端保存的会话数据（cookies）发送给服务器，实现状态管理
// Set-Cookie ？不，那是响应头。请求中没有。
// DNT	Do Not Track，值为 1 时表示用户不希望被追踪（浏览器隐私设置）
// Sec-Fetch-Site、Sec-Fetch-Mode、Sec-Fetch-Dest	现代浏览器附加的安全上下文信息，帮助服务器防范CSRF等攻击
// Referer	(前面已有)
// X-Requested-With	传统标识Ajax请求，值为 XMLHttpRequest，常用于判断是否为异步请求
// X-Forwarded-For	由代理或负载均衡添加，记录原始客户端IP和经过的代理链
// X-Real-IP	类似X-Forwarded-For，一些代理单独传递真实IP

// 服务器与连接信息
// Server	声明服务器软件信息（如 nginx/1.21.6），帮助客户端了解后端环境
// Date	响应生成的日期时间，用于缓存计算、时钟同步等
// Connection	控制连接状态：keep-alive 保持连接，close 表示响应结束后关闭连接
// Upgrade	若服务器同意协议升级（如切换到 WebSocket），返回 Upgrade: websocket 配合 101 状态码

// 内容协商与元数据
// 描述响应体的类型、长度、语言等，辅助客户端正确解析。
// Content-Type	响应体的媒体类型及字符集，如 text/html; charset=UTF-8，直接决定浏览器如何渲染
// Content-Length	响应体的字节数。未使用分块传输时必须携带，帮助客户端界定消息边界
// Content-Encoding	响应体的压缩编码，如 gzip、deflate、br，客户端需要先解压再处理
// Content-Language	响应体使用的自然语言，如 zh-CN，辅助语音合成、翻译等
// Content-Location	返回资源副本的备用访问路径，常用于内容协商后的真实地址
// Content-Disposition	引导浏览器下载行为：inline（直接显示）或 attachment; filename="..."（强制下载）
// Content-Range	配合 206 Partial Content 状态码，指明返回的是资源的哪一部分（如 bytes 0-1023/2048）
// ETag	资源的实体标签（唯一标识），用于缓存验证和条件请求（若匹配，可返回 304）
// Last-Modified	资源最后修改的时间，与 If-Modified-Since 等配合实现缓存校验
// Vary	声明缓存时需要考虑哪些请求头差异，如 Vary: Accept-Encoding 告诉缓存不同编码要存不同版本

// 缓存控制
// Cache-Control	控制缓存行为，常用指令如 public、private、max-age=3600、no-cache、no-store、must-revalidate，优先级高
// Expires	指定资源的绝对过期时间（GMT格式），不如 Cache-Control: max-age 可靠，因需时钟同步
// Pragma	向后兼容，no-cache 常与 Cache-Control 一起使用
// Age	表示资源在缓存中已经存活的秒数，帮助计算新鲜度
// Warning	（已废弃）携带缓存状态警告信息，如 110 Response is Stale

// 重定向与位置
// 头部字段	作用
// Location	用于 3xx 重定向或 201 Created，告诉客户端应该跳转到的新 URL
// Refresh	（非标准但广泛支持）让浏览器在指定秒数后自动刷新或跳转到新URL，常用于旧页面跳转

// 安全与认证
// WWW-Authenticate	当返回 401 Unauthorized 时，告知客户端需要哪种认证方式（如 Basic realm="..."）
// Proxy-Authenticate	同 WWW-Authenticate，但针对代理服务器（返回 407 时）
// Set-Cookie	在客户端创建或更新 Cookie，可指定 Domain、Path、Expires、HttpOnly、Secure、SameSite 等属性
// Strict-Transport-Security (HSTS)	强制客户端在指定时间内仅用 HTTPS 访问该域名，防止降级攻击
// Content-Security-Policy	定义白名单，限制脚本、样式、图片等资源来源，防范 XSS
// X-Content-Type-Options	通常设为 nosniff，禁止浏览器进行 MIME 类型嗅探，避免执行伪装成图片的可执行脚本
// X-Frame-Options	控制页面能否被嵌入 <frame>/<iframe>，DENY 或 SAMEORIGIN 可防点击劫持
// X-XSS-Protection	（已大多由 CSP 取代）启用浏览器内置的反射型 XSS 过滤器，1; mode=block
// Referrer-Policy	控制 Referer 头在请求中的暴露程度，如 no-referrer、strict-origin 等
// Permissions-Policy	控制浏览器功能和 API 的使用权限（如摄像头、麦克风、定位）
// Cross-Origin-Resource-Policy	指示浏览器允许哪些站点加载该资源（same-origin 等）
// Cross-Origin-Opener-Policy	控制跨源窗口的 opener 关系，防范 window.opener 攻击
// Cross-Origin-Embedder-Policy	要求嵌入式资源必须明确许可，配合 require-corp 使用

// 请求与追踪
// Via	记录响应经过的代理链，每跳代理可追加信息
// X-Frame-Options	如前述
// X-Cache	非标准但常见，指示缓存服务器是否命中（如 HIT 或 MISS）
// X-Request-ID	为请求生成唯一标识，便于服务端追踪和日志关联
// Set-Cookie	已在安全类，实为状态管理核心
// Access-Control-Allow-Origin	CORS 头，指定允许发起跨域请求的源，若为 * 则允许所有，或具体域名
// Access-Control-Allow-Methods	允许的 HTTP 方法（跨域预检时返回）
// Access-Control-Allow-Headers	允许的请求头部（跨域预检）
// Access-Control-Expose-Headers	允许客户端在 XMLHttpRequest 中通过 getResponseHeader() 获取哪些响应头
// Access-Control-Max-Age	预检请求的缓存时间（秒）
// Timing-Allow-Origin	控制哪些源可以访问 Resource Timing API 的时间信息
// Server-Timing	传递后端处理各阶段的耗时信息（如 db;dur=53），供性能分析

#define GET     0
#define POST    1
#define PUT     2
#define DELETE  3
#define PATCH   4
#define HEAD    5
#define OPTIONS 6
#define TRACE   7
#define CONNECT 8

namespace http_util
{
static constexpr const char* METHODS[] = {"GET", "POST", "PUT", "DELETE", "PATCH", "HEAD", "OPTIONS", "TRACE", "CONNECT"};

std::string splice_request(const char* _method, const char* _source_url, const char* _version, std::string& _headers, std::string& _bodys);

void splice_response(const char* _version, const char* _code, const char* _sumary, std::string& _headers, std::string& _bodys);

bool parse_url_https(const std::string& url, std::string& host, uint16_t& port, std::string& path);
};  // namespace http_util