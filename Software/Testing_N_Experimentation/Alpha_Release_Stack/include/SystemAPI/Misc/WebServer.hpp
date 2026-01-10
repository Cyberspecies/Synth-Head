/*****************************************************************
 * @file WebServer.hpp
 * @brief Web Server Tools - Website hosting management utilities
 * 
 * Provides tools for:
 * - HTTP server management
 * - REST API creation
 * - WebSocket support
 * - Static file serving
 * - Template rendering
 * - CORS and security
 * - Request/Response handling
 * 
 * @author ARCOS
 * @version 1.0
 *****************************************************************/

#pragma once

#include <cstdint>
#include <cstring>
#include <functional>
#include <vector>

namespace SystemAPI {
namespace Web {

// ============================================================
// HTTP Types
// ============================================================

/**
 * @brief HTTP methods
 */
enum class Method : uint8_t {
  GET,
  POST,
  PUT,
  DELETE_,  // DELETE is reserved
  PATCH,
  OPTIONS,
  HEAD,
  ANY       // Match any method
};

inline const char* getMethodName(Method method) {
  switch (method) {
    case Method::GET:     return "GET";
    case Method::POST:    return "POST";
    case Method::PUT:     return "PUT";
    case Method::DELETE_: return "DELETE";
    case Method::PATCH:   return "PATCH";
    case Method::OPTIONS: return "OPTIONS";
    case Method::HEAD:    return "HEAD";
    case Method::ANY:     return "ANY";
    default:              return "UNKNOWN";
  }
}

/**
 * @brief HTTP status codes
 */
enum class Status : uint16_t {
  OK = 200,
  CREATED = 201,
  ACCEPTED = 202,
  NO_CONTENT = 204,
  MOVED_PERMANENTLY = 301,
  FOUND = 302,
  NOT_MODIFIED = 304,
  BAD_REQUEST = 400,
  UNAUTHORIZED = 401,
  FORBIDDEN = 403,
  NOT_FOUND = 404,
  METHOD_NOT_ALLOWED = 405,
  CONFLICT = 409,
  INTERNAL_ERROR = 500,
  NOT_IMPLEMENTED = 501,
  SERVICE_UNAVAILABLE = 503
};

/**
 * @brief Content types
 */
enum class ContentType : uint8_t {
  TEXT_PLAIN,
  TEXT_HTML,
  TEXT_CSS,
  TEXT_JAVASCRIPT,
  APPLICATION_JSON,
  APPLICATION_XML,
  APPLICATION_OCTET_STREAM,
  IMAGE_PNG,
  IMAGE_JPEG,
  IMAGE_GIF,
  IMAGE_SVG
};

inline const char* getContentTypeString(ContentType type) {
  switch (type) {
    case ContentType::TEXT_PLAIN:               return "text/plain";
    case ContentType::TEXT_HTML:                return "text/html";
    case ContentType::TEXT_CSS:                 return "text/css";
    case ContentType::TEXT_JAVASCRIPT:          return "application/javascript";
    case ContentType::APPLICATION_JSON:         return "application/json";
    case ContentType::APPLICATION_XML:          return "application/xml";
    case ContentType::APPLICATION_OCTET_STREAM: return "application/octet-stream";
    case ContentType::IMAGE_PNG:                return "image/png";
    case ContentType::IMAGE_JPEG:               return "image/jpeg";
    case ContentType::IMAGE_GIF:                return "image/gif";
    case ContentType::IMAGE_SVG:                return "image/svg+xml";
    default:                                    return "text/plain";
  }
}

// ============================================================
// Request/Response
// ============================================================

/**
 * @brief HTTP header
 */
struct Header {
  char name[32];
  char value[128];
};

/**
 * @brief Query parameter
 */
struct QueryParam {
  char name[32];
  char value[128];
};

/**
 * @brief HTTP request
 */
struct Request {
  Method method = Method::GET;
  char path[128] = "";
  char query[256] = "";
  Header headers[16];
  int headerCount = 0;
  QueryParam params[16];
  int paramCount = 0;
  const char* body = nullptr;
  int bodyLength = 0;
  char clientIP[16] = "";
  uint16_t clientPort = 0;
  
  /**
   * @brief Get header value by name
   */
  const char* getHeader(const char* name) const {
    for (int i = 0; i < headerCount; i++) {
      if (strcasecmp(headers[i].name, name) == 0) {
        return headers[i].value;
      }
    }
    return nullptr;
  }
  
  /**
   * @brief Get query parameter by name
   */
  const char* getParam(const char* name) const {
    for (int i = 0; i < paramCount; i++) {
      if (strcmp(params[i].name, name) == 0) {
        return params[i].value;
      }
    }
    return nullptr;
  }
  
  /**
   * @brief Check if request has body
   */
  bool hasBody() const { return body != nullptr && bodyLength > 0; }
};

/**
 * @brief HTTP response builder
 */
class Response {
public:
  Response& status(Status code) {
    statusCode_ = code;
    return *this;
  }
  
  Response& header(const char* name, const char* value) {
    if (headerCount_ < 16) {
      strncpy(headers_[headerCount_].name, name, sizeof(headers_[0].name) - 1);
      strncpy(headers_[headerCount_].value, value, sizeof(headers_[0].value) - 1);
      headerCount_++;
    }
    return *this;
  }
  
  Response& contentType(ContentType type) {
    return header("Content-Type", getContentTypeString(type));
  }
  
  Response& contentType(const char* type) {
    return header("Content-Type", type);
  }
  
  Response& body(const char* content) {
    body_ = content;
    bodyLength_ = content ? strlen(content) : 0;
    return *this;
  }
  
  Response& body(const char* content, int length) {
    body_ = content;
    bodyLength_ = length;
    return *this;
  }
  
  Response& json(const char* jsonContent) {
    contentType(ContentType::APPLICATION_JSON);
    return body(jsonContent);
  }
  
  Response& html(const char* htmlContent) {
    contentType(ContentType::TEXT_HTML);
    return body(htmlContent);
  }
  
  Response& redirect(const char* url, Status code = Status::FOUND) {
    status(code);
    return header("Location", url);
  }
  
  Response& cors(const char* origin = "*") {
    header("Access-Control-Allow-Origin", origin);
    header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
    header("Access-Control-Allow-Headers", "Content-Type, Authorization");
    return *this;
  }
  
  Response& cache(int maxAgeSeconds) {
    char value[32];
    snprintf(value, sizeof(value), "max-age=%d", maxAgeSeconds);
    return header("Cache-Control", value);
  }
  
  Response& noCache() {
    return header("Cache-Control", "no-cache, no-store, must-revalidate");
  }
  
  // Getters
  Status getStatus() const { return statusCode_; }
  const Header* getHeaders() const { return headers_; }
  int getHeaderCount() const { return headerCount_; }
  const char* getBody() const { return body_; }
  int getBodyLength() const { return bodyLength_; }
  
private:
  Status statusCode_ = Status::OK;
  Header headers_[16];
  int headerCount_ = 0;
  const char* body_ = nullptr;
  int bodyLength_ = 0;
};

// ============================================================
// Route Handling
// ============================================================

/**
 * @brief Request handler function type
 */
using RequestHandler = std::function<void(const Request&, Response&)>;

/**
 * @brief Middleware function type
 */
using Middleware = std::function<bool(const Request&, Response&)>;

/**
 * @brief Route definition
 */
struct Route {
  Method method = Method::GET;
  char path[128] = "";
  RequestHandler handler;
  bool requiresAuth = false;
};

// ============================================================
// WebSocket Types
// ============================================================

/**
 * @brief WebSocket message types
 */
enum class WSMessageType : uint8_t {
  TEXT,
  BINARY,
  PING,
  PONG,
  CLOSE
};

/**
 * @brief WebSocket client info
 */
struct WSClient {
  int id;
  char ip[16];
  uint16_t port;
  bool connected;
  uint32_t connectedAt;
};

/**
 * @brief WebSocket message
 */
struct WSMessage {
  WSMessageType type;
  const char* data;
  int length;
  int clientId;
};

/**
 * @brief WebSocket event handler
 */
using WSHandler = std::function<void(const WSMessage&)>;
using WSConnectHandler = std::function<void(const WSClient&)>;
using WSDisconnectHandler = std::function<void(const WSClient&)>;

// ============================================================
// Web Server
// ============================================================

/**
 * @brief Web server configuration
 */
struct ServerConfig {
  uint16_t port = 80;
  uint16_t wsPort = 81;
  int maxConnections = 4;
  int timeoutMs = 5000;
  bool enableCORS = true;
  bool enableWebSocket = true;
  const char* staticPath = "/www";
  const char* indexFile = "index.html";
};

/**
 * @brief Web server manager
 * 
 * @example
 * ```cpp
 * auto& server = Web::Server::instance();
 * 
 * // Configure
 * server.getConfig().port = 8080;
 * 
 * // Add routes
 * server.get("/", [](const Request& req, Response& res) {
 *   res.html("<h1>Hello World!</h1>");
 * });
 * 
 * server.get("/api/status", [](const Request& req, Response& res) {
 *   res.json("{\"status\": \"ok\"}");
 * });
 * 
 * server.post("/api/data", [](const Request& req, Response& res) {
 *   // Handle POST data
 *   res.status(Status::CREATED).json("{\"id\": 123}");
 * });
 * 
 * // Add middleware
 * server.use([](const Request& req, Response& res) {
 *   // Log all requests
 *   printf("%s %s\n", getMethodName(req.method), req.path);
 *   return true;  // Continue to handler
 * });
 * 
 * // WebSocket
 * server.onWSConnect([](const WSClient& client) {
 *   printf("Client %d connected\n", client.id);
 * });
 * 
 * server.onWSMessage([](const WSMessage& msg) {
 *   printf("Message: %s\n", msg.data);
 *   server.wsBroadcast(msg.data);  // Echo to all
 * });
 * 
 * // Start server
 * server.start();
 * ```
 */
class Server {
public:
  static Server& instance() {
    static Server inst;
    return inst;
  }
  
  // ---- Configuration ----
  
  ServerConfig& getConfig() { return config_; }
  const ServerConfig& getConfig() const { return config_; }
  
  // ---- Lifecycle ----
  
  bool start() {
    if (running_) return true;
    // Platform-specific server start would go here
    running_ = true;
    return true;
  }
  
  void stop() {
    running_ = false;
    // Platform-specific server stop
  }
  
  bool isRunning() const { return running_; }
  
  // ---- Route Registration ----
  
  void route(Method method, const char* path, RequestHandler handler, bool authRequired = false) {
    Route r;
    r.method = method;
    strncpy(r.path, path, sizeof(r.path) - 1);
    r.handler = handler;
    r.requiresAuth = authRequired;
    routes_.push_back(r);
  }
  
  void get(const char* path, RequestHandler handler) {
    route(Method::GET, path, handler);
  }
  
  void post(const char* path, RequestHandler handler) {
    route(Method::POST, path, handler);
  }
  
  void put(const char* path, RequestHandler handler) {
    route(Method::PUT, path, handler);
  }
  
  void del(const char* path, RequestHandler handler) {
    route(Method::DELETE_, path, handler);
  }
  
  void any(const char* path, RequestHandler handler) {
    route(Method::ANY, path, handler);
  }
  
  // ---- Middleware ----
  
  void use(Middleware middleware) {
    middlewares_.push_back(middleware);
  }
  
  // ---- WebSocket ----
  
  void onWSConnect(WSConnectHandler handler) {
    wsConnectHandler_ = handler;
  }
  
  void onWSDisconnect(WSDisconnectHandler handler) {
    wsDisconnectHandler_ = handler;
  }
  
  void onWSMessage(WSHandler handler) {
    wsMessageHandler_ = handler;
  }
  
  void wsSend(int clientId, const char* data, WSMessageType type = WSMessageType::TEXT) {
    // Platform-specific WebSocket send
    (void)clientId;
    (void)data;
    (void)type;
  }
  
  void wsBroadcast(const char* data, WSMessageType type = WSMessageType::TEXT) {
    // Platform-specific WebSocket broadcast
    (void)data;
    (void)type;
  }
  
  int getWSClientCount() const { return wsClientCount_; }
  
  // ---- Static Files ----
  
  void serveStatic(const char* urlPath, const char* fsPath) {
    // Register static file serving route
    (void)urlPath;
    (void)fsPath;
  }
  
  // ---- Statistics ----
  
  uint32_t getRequestCount() const { return requestCount_; }
  uint32_t getActiveConnections() const { return activeConnections_; }
  
private:
  Server() = default;
  Server(const Server&) = delete;
  Server& operator=(const Server&) = delete;
  
  bool running_ = false;
  ServerConfig config_;
  
  std::vector<Route> routes_;
  std::vector<Middleware> middlewares_;
  
  WSConnectHandler wsConnectHandler_;
  WSDisconnectHandler wsDisconnectHandler_;
  WSHandler wsMessageHandler_;
  int wsClientCount_ = 0;
  
  uint32_t requestCount_ = 0;
  uint32_t activeConnections_ = 0;
};

// ============================================================
// JSON Utilities
// ============================================================

/**
 * @brief Simple JSON builder for responses
 * 
 * @example
 * ```cpp
 * JsonBuilder json;
 * json.beginObject()
 *     .add("status", "ok")
 *     .add("count", 42)
 *     .add("active", true)
 *     .beginArray("items")
 *       .addValue("item1")
 *       .addValue("item2")
 *     .endArray()
 *   .endObject();
 * 
 * res.json(json.toString());
 * ```
 */
class JsonBuilder {
public:
  JsonBuilder& beginObject() {
    append("{");
    needComma_ = false;
    return *this;
  }
  
  JsonBuilder& endObject() {
    append("}");
    needComma_ = true;
    return *this;
  }
  
  JsonBuilder& beginArray(const char* name = nullptr) {
    if (name) {
      addKey(name);
    } else if (needComma_) {
      append(",");
    }
    append("[");
    needComma_ = false;
    return *this;
  }
  
  JsonBuilder& endArray() {
    append("]");
    needComma_ = true;
    return *this;
  }
  
  JsonBuilder& add(const char* key, const char* value) {
    addKey(key);
    append("\"");
    appendEscaped(value);
    append("\"");
    needComma_ = true;
    return *this;
  }
  
  JsonBuilder& add(const char* key, int value) {
    addKey(key);
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", value);
    append(buf);
    needComma_ = true;
    return *this;
  }
  
  JsonBuilder& add(const char* key, float value) {
    addKey(key);
    char buf[24];
    snprintf(buf, sizeof(buf), "%.2f", value);
    append(buf);
    needComma_ = true;
    return *this;
  }
  
  JsonBuilder& add(const char* key, bool value) {
    addKey(key);
    append(value ? "true" : "false");
    needComma_ = true;
    return *this;
  }
  
  JsonBuilder& addValue(const char* value) {
    if (needComma_) append(",");
    append("\"");
    appendEscaped(value);
    append("\"");
    needComma_ = true;
    return *this;
  }
  
  JsonBuilder& addValue(int value) {
    if (needComma_) append(",");
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", value);
    append(buf);
    needComma_ = true;
    return *this;
  }
  
  const char* toString() const { return buffer_; }
  int length() const { return pos_; }
  
  void clear() {
    pos_ = 0;
    buffer_[0] = '\0';
    needComma_ = false;
  }
  
private:
  static constexpr int MAX_SIZE = 1024;
  char buffer_[MAX_SIZE];
  int pos_ = 0;
  bool needComma_ = false;
  
  void append(const char* str) {
    int len = strlen(str);
    if (pos_ + len < MAX_SIZE) {
      strcpy(&buffer_[pos_], str);
      pos_ += len;
    }
  }
  
  void appendEscaped(const char* str) {
    while (*str && pos_ < MAX_SIZE - 2) {
      if (*str == '"' || *str == '\\') {
        buffer_[pos_++] = '\\';
      }
      buffer_[pos_++] = *str++;
    }
    buffer_[pos_] = '\0';
  }
  
  void addKey(const char* key) {
    if (needComma_) append(",");
    append("\"");
    append(key);
    append("\":");
  }
};

// ============================================================
// HTML Template Utilities
// ============================================================

/**
 * @brief Simple HTML template builder
 * 
 * @example
 * ```cpp
 * HtmlBuilder html;
 * html.doctype()
 *     .html()
 *       .head()
 *         .title("My Page")
 *         .style("body { font-family: sans-serif; }")
 *       .end()
 *       .body()
 *         .h1("Welcome!")
 *         .p("This is a paragraph.")
 *         .div().cls("container")
 *           .text("Content here")
 *         .end()
 *       .end()
 *     .end();
 * 
 * res.html(html.toString());
 * ```
 */
class HtmlBuilder {
public:
  HtmlBuilder& doctype() { return append("<!DOCTYPE html>\n"); }
  
  HtmlBuilder& html(const char* lang = "en") {
    return openTag("html", nullptr, lang ? (std::string("lang=\"") + lang + "\"").c_str() : nullptr);
  }
  
  HtmlBuilder& head() { return openTag("head"); }
  HtmlBuilder& body(const char* cls = nullptr) { return openTag("body", cls); }
  
  HtmlBuilder& title(const char* text) {
    openTag("title");
    append(text);
    return closeTag();
  }
  
  HtmlBuilder& meta(const char* name, const char* content) {
    char buf[128];
    snprintf(buf, sizeof(buf), "<meta name=\"%s\" content=\"%s\">\n", name, content);
    return append(buf);
  }
  
  HtmlBuilder& charset(const char* charset = "UTF-8") {
    char buf[64];
    snprintf(buf, sizeof(buf), "<meta charset=\"%s\">\n", charset);
    return append(buf);
  }
  
  HtmlBuilder& viewport(const char* content = "width=device-width, initial-scale=1") {
    return meta("viewport", content);
  }
  
  HtmlBuilder& style(const char* css) {
    openTag("style");
    append(css);
    return closeTag();
  }
  
  HtmlBuilder& script(const char* js) {
    openTag("script");
    append(js);
    return closeTag();
  }
  
  HtmlBuilder& scriptSrc(const char* src) {
    char buf[128];
    snprintf(buf, sizeof(buf), "<script src=\"%s\"></script>\n", src);
    return append(buf);
  }
  
  HtmlBuilder& link(const char* rel, const char* href) {
    char buf[128];
    snprintf(buf, sizeof(buf), "<link rel=\"%s\" href=\"%s\">\n", rel, href);
    return append(buf);
  }
  
  HtmlBuilder& div(const char* cls = nullptr) { return openTag("div", cls); }
  HtmlBuilder& span(const char* cls = nullptr) { return openTag("span", cls); }
  HtmlBuilder& p(const char* text = nullptr, const char* cls = nullptr) {
    openTag("p", cls);
    if (text) append(text);
    return text ? closeTag() : *this;
  }
  
  HtmlBuilder& h1(const char* text, const char* cls = nullptr) { return heading(1, text, cls); }
  HtmlBuilder& h2(const char* text, const char* cls = nullptr) { return heading(2, text, cls); }
  HtmlBuilder& h3(const char* text, const char* cls = nullptr) { return heading(3, text, cls); }
  
  HtmlBuilder& a(const char* href, const char* text, const char* cls = nullptr) {
    char buf[256];
    snprintf(buf, sizeof(buf), "<a href=\"%s\"%s%s\">%s</a>", 
             href, cls ? " class=\"" : "", cls ? cls : "", text);
    if (cls) append("\"");
    return append(buf);
  }
  
  HtmlBuilder& img(const char* src, const char* alt = "", const char* cls = nullptr) {
    char buf[256];
    snprintf(buf, sizeof(buf), "<img src=\"%s\" alt=\"%s\"%s%s%s>\n",
             src, alt, cls ? " class=\"" : "", cls ? cls : "", cls ? "\"" : "");
    return append(buf);
  }
  
  HtmlBuilder& ul(const char* cls = nullptr) { return openTag("ul", cls); }
  HtmlBuilder& ol(const char* cls = nullptr) { return openTag("ol", cls); }
  HtmlBuilder& li(const char* text = nullptr, const char* cls = nullptr) {
    openTag("li", cls);
    if (text) append(text);
    return text ? closeTag() : *this;
  }
  
  HtmlBuilder& table(const char* cls = nullptr) { return openTag("table", cls); }
  HtmlBuilder& tr(const char* cls = nullptr) { return openTag("tr", cls); }
  HtmlBuilder& th(const char* text, const char* cls = nullptr) {
    openTag("th", cls);
    append(text);
    return closeTag();
  }
  HtmlBuilder& td(const char* text, const char* cls = nullptr) {
    openTag("td", cls);
    append(text);
    return closeTag();
  }
  
  HtmlBuilder& form(const char* action, const char* method = "POST", const char* cls = nullptr) {
    char buf[256];
    snprintf(buf, sizeof(buf), "<form action=\"%s\" method=\"%s\"%s%s%s>\n",
             action, method, cls ? " class=\"" : "", cls ? cls : "", cls ? "\"" : "");
    tagStack_[stackPos_++] = "form";
    return append(buf);
  }
  
  HtmlBuilder& input(const char* type, const char* name, const char* value = nullptr,
                      const char* placeholder = nullptr, const char* cls = nullptr) {
    char buf[256];
    int pos = snprintf(buf, sizeof(buf), "<input type=\"%s\" name=\"%s\"", type, name);
    if (value) pos += snprintf(buf + pos, sizeof(buf) - pos, " value=\"%s\"", value);
    if (placeholder) pos += snprintf(buf + pos, sizeof(buf) - pos, " placeholder=\"%s\"", placeholder);
    if (cls) pos += snprintf(buf + pos, sizeof(buf) - pos, " class=\"%s\"", cls);
    snprintf(buf + pos, sizeof(buf) - pos, ">\n");
    return append(buf);
  }
  
  HtmlBuilder& button(const char* text, const char* type = "submit", const char* cls = nullptr) {
    char buf[128];
    snprintf(buf, sizeof(buf), "<button type=\"%s\"%s%s%s>%s</button>\n",
             type, cls ? " class=\"" : "", cls ? cls : "", cls ? "\"" : "", text);
    return append(buf);
  }
  
  HtmlBuilder& textarea(const char* name, const char* content = "", const char* cls = nullptr) {
    char buf[128];
    snprintf(buf, sizeof(buf), "<textarea name=\"%s\"%s%s%s>",
             name, cls ? " class=\"" : "", cls ? cls : "", cls ? "\"" : "");
    append(buf);
    append(content);
    append("</textarea>\n");
    return *this;
  }
  
  HtmlBuilder& br() { return append("<br>\n"); }
  HtmlBuilder& hr() { return append("<hr>\n"); }
  
  HtmlBuilder& text(const char* t) { return append(t); }
  
  HtmlBuilder& cls(const char* className) {
    // Add class to previous tag (must be called right after opening tag)
    return *this;  // Simplified - would need buffer manipulation
  }
  
  HtmlBuilder& id(const char* idName) {
    return *this;  // Simplified
  }
  
  HtmlBuilder& end() { return closeTag(); }
  
  const char* toString() const { return buffer_; }
  int length() const { return pos_; }
  void clear() { pos_ = 0; buffer_[0] = '\0'; stackPos_ = 0; }
  
private:
  static constexpr int MAX_SIZE = 4096;
  static constexpr int MAX_DEPTH = 32;
  char buffer_[MAX_SIZE];
  int pos_ = 0;
  const char* tagStack_[MAX_DEPTH];
  int stackPos_ = 0;
  
  HtmlBuilder& append(const char* str) {
    int len = strlen(str);
    if (pos_ + len < MAX_SIZE) {
      strcpy(&buffer_[pos_], str);
      pos_ += len;
    }
    return *this;
  }
  
  HtmlBuilder& openTag(const char* tag, const char* cls = nullptr, const char* extra = nullptr) {
    char buf[128];
    if (cls && extra) {
      snprintf(buf, sizeof(buf), "<%s class=\"%s\" %s>\n", tag, cls, extra);
    } else if (cls) {
      snprintf(buf, sizeof(buf), "<%s class=\"%s\">\n", tag, cls);
    } else if (extra) {
      snprintf(buf, sizeof(buf), "<%s %s>\n", tag, extra);
    } else {
      snprintf(buf, sizeof(buf), "<%s>", tag);
    }
    tagStack_[stackPos_++] = tag;
    return append(buf);
  }
  
  HtmlBuilder& closeTag() {
    if (stackPos_ > 0) {
      char buf[32];
      snprintf(buf, sizeof(buf), "</%s>\n", tagStack_[--stackPos_]);
      append(buf);
    }
    return *this;
  }
  
  HtmlBuilder& heading(int level, const char* text, const char* cls) {
    char tag[4];
    snprintf(tag, sizeof(tag), "h%d", level);
    openTag(tag, cls);
    append(text);
    return closeTag();
  }
};

} // namespace Web
} // namespace SystemAPI
