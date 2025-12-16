
/*
  Nano ESP32 MCP Server (JSON-RPC 2.0)
  Endpoints:
    POST /mcp    -> JSON-RPC 2.0 endpoint for all MCP methods
    GET  /sse    -> SSE stream for notifications/logging

  MCP Methods:
    - initialize: Server initialization with protocol version negotiation
    - tools/list: List available tools
    - tools/call: Invoke a tool
    - resources/list: List available resources
    - resources/read: Read resource data
    - resources/subscribe: Subscribe to resource updates
    - resources/unsubscribe: Unsubscribe from resource updates

  Dependencies (Library Manager):
    - ESPAsyncWebServer
    - ArduinoJson
    - Adafruit SSD1306
    - Adafruit GFX Library
    - QRCodeGFX
*/

#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <QRCodeGFX.h>

#ifndef LED_BUILTIN
#define LED_BUILTIN 13
#endif

// RGB LED pins
#define LED_RED 14     // Pin 14 for red
#define LED_GREEN 15   // Pin 15 for green
#define LED_BLUE 16    // Pin 16 for blue

// ------- OLED Display Config -------
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ------- WiFi config -------
#include "arduino_secrets.h"
const char* WIFI_SSID = SECRET_SSID;
const char* WIFI_PSK  = SECRET_PASS;

// ------- Server config -------
const int SERVER_PORT = 8000;  // Konfigurierbar: Ändere hier den Port
AsyncWebServer server(SERVER_PORT);
AsyncEventSource events("/sse");   // SSE endpoint for MCP notifications

// ------- Buffers (tune for your payload sizes) -------
static const size_t JSON_SMALL = 512;
static const size_t JSON_MED   = 1024;
static const size_t JSON_LARGE = 2048;

// ------- Server Info -------
const char* SERVER_NAME = "arduino-mcp";
const char* SERVER_VERSION = "1.0.0";

// ------- Timing Constants -------
const unsigned long LED_INDICATOR_DELAY_MS = 100;
const unsigned long INTRO_DISPLAY_DURATION_MS = 3000;
const unsigned long WIFI_STATUS_DISPLAY_MS = 2000;
const unsigned long WIFI_CONNECT_RETRY_MS = 500;
const unsigned long I2C_INIT_DELAY_MS = 100;
const unsigned long NOTIFICATION_COOLDOWN_MS = 50;  // Debounce notifications

// ------- Utilities -------
unsigned long bootMillis;
bool displayAvailable = false;  // Flag if display is available

// Forward declarations
extern Adafruit_SSD1306 display;
extern bool displayAvailable;

// RAII class for indicator on MCP requests
class McpRequestIndicator {
public:
  McpRequestIndicator() {
    if (displayAvailable) {
      // Draw indicator dot in top right corner
      display.fillCircle(122, 3, 2, SSD1306_WHITE);
      display.display();
    }
  }
  
  ~McpRequestIndicator() {
    delay(LED_INDICATOR_DELAY_MS);  // Keep visible briefly
    if (displayAvailable) {
      // Clear indicator dot
      display.fillCircle(122, 3, 2, SSD1306_BLACK);
      display.display();
    }
  }
};

// ------- MCP Tool Framework -------

// Base class for MCP tools
class McpTool {
public:
  virtual ~McpTool() {}  // Virtual destructor for proper cleanup
  
  virtual const char* getName() const = 0;
  virtual const char* getDescription() const = 0;
  
  // Build the tool's JSON schema for tools/list
  virtual void buildSchema(JsonObject& tool) const {
    tool["name"] = getName();
    tool["description"] = getDescription();
    JsonObject schema = tool.createNestedObject("inputSchema");
    schema["type"] = "object";
    addSchemaProperties(schema);
  }
  
  // Execute the tool with given arguments
  virtual bool execute(const JsonVariantConst args, JsonArray& content, String& error) = 0;
  
protected:
  // Override to add custom schema properties
  virtual void addSchemaProperties(JsonObject& schema) const = 0;
  
  // Helper to add a boolean parameter to schema
  void addBoolParam(JsonObject& schema, const char* name, const char* desc, bool required = true) const {
    JsonObject props = schema["properties"].isNull() ? 
                       schema.createNestedObject("properties") : 
                       schema["properties"].as<JsonObject>();
    JsonObject param = props.createNestedObject(name);
    param["type"] = "boolean";
    param["description"] = desc;
    
    if (required) {
      JsonArray req = schema["required"].isNull() ? 
                      schema.createNestedArray("required") : 
                      schema["required"].as<JsonArray>();
      req.add(name);
    }
  }
  
  // Helper to add a string parameter to schema
  void addStringParam(JsonObject& schema, const char* name, const char* desc, bool required = true) const {
    JsonObject props = schema["properties"].isNull() ? 
                       schema.createNestedObject("properties") : 
                       schema["properties"].as<JsonObject>();
    JsonObject param = props.createNestedObject(name);
    param["type"] = "string";
    param["description"] = desc;
    
    if (required) {
      JsonArray req = schema["required"].isNull() ? 
                      schema.createNestedArray("required") : 
                      schema["required"].as<JsonArray>();
      req.add(name);
    }
  }
  
  // Helper to validate and extract bool argument
  bool getBoolArg(const JsonVariantConst args, const char* name, bool& value, String& error) const {
    if (!args.is<JsonObjectConst>()) {
      error = "arguments_must_be_object";
      return false;
    }
    
    JsonObjectConst obj = args.as<JsonObjectConst>();
    if (!obj.containsKey(name)) {
      error = String("missing_argument_") + name;
      return false;
    }
    
    value = obj[name].as<bool>();
    return true;
  }
  
  // Helper to validate and extract string argument
  bool getStringArg(const JsonVariantConst args, const char* name, const char*& value, String& error) const {
    if (!args.is<JsonObjectConst>()) {
      error = "arguments_must_be_object";
      return false;
    }
    
    JsonObjectConst obj = args.as<JsonObjectConst>();
    if (!obj.containsKey(name)) {
      error = String("missing_argument_") + name;
      return false;
    }
    
    value = obj[name].as<const char*>();
    return true;
  }
  
  // Helper to add text content to response
  void addTextContent(JsonArray& content, const char* text) const {
    JsonObject textContent = content.createNestedObject();
    textContent["type"] = "text";
    textContent["text"] = text;
  }
};

// Tool Registry
class ToolRegistry {
private:
  static const int MAX_TOOLS = 10;
  McpTool* tools[MAX_TOOLS];
  int toolCount = 0;
  
public:
  void registerTool(McpTool* tool) {
    if (toolCount < MAX_TOOLS) {
      tools[toolCount++] = tool;
    } else {
      Serial.printf("FATAL: Tool registry overflow! Cannot register '%s'\n", tool->getName());
    }
  }
  
  McpTool* findTool(const char* name) {
    for (int i = 0; i < toolCount; i++) {
      if (strcmp(tools[i]->getName(), name) == 0) {
        return tools[i];
      }
    }
    return nullptr;
  }
  
  void buildToolsList(JsonArray& toolsArray) {
    for (int i = 0; i < toolCount; i++) {
      JsonObject tool = toolsArray.createNestedObject();
      tools[i]->buildSchema(tool);
    }
  }
  
  int getToolCount() const { return toolCount; }
};

// Global registry
ToolRegistry toolRegistry;

// ------- MCP Resource Framework -------

// Base class for MCP resources
class McpResource {
public:
  virtual ~McpResource() {}
  
  virtual const char* getUri() const = 0;
  virtual const char* getName() const = 0;
  virtual const char* getDescription() const = 0;
  virtual const char* getMimeType() const { return "application/json"; }
  
  // Build the resource info for resources/list
  virtual void buildInfo(JsonObject& resource) const {
    resource["uri"] = getUri();
    resource["name"] = getName();
    resource["description"] = getDescription();
    resource["mimeType"] = getMimeType();
  }
  
  // Read the resource data
  virtual void read(JsonArray& content) = 0;
  
protected:
  // Helper to add JSON content to response
  void addJsonContent(JsonArray& content, const char* jsonText) const {
    JsonObject textContent = content.createNestedObject();
    textContent["uri"] = getUri();
    textContent["mimeType"] = getMimeType();
    textContent["text"] = jsonText;
  }
};

// Resource Registry
class ResourceRegistry {
private:
  static const int MAX_RESOURCES = 10;
  McpResource* resources[MAX_RESOURCES];
  int resourceCount = 0;
  
public:
  void registerResource(McpResource* resource) {
    if (resourceCount < MAX_RESOURCES) {
      resources[resourceCount++] = resource;
    } else {
      Serial.printf("FATAL: Resource registry overflow! Cannot register '%s'\n", resource->getUri());
    }
  }
  
  McpResource* findResource(const char* uri) {
    for (int i = 0; i < resourceCount; i++) {
      if (strcmp(resources[i]->getUri(), uri) == 0) {
        return resources[i];
      }
    }
    return nullptr;
  }
  
  void buildResourcesList(JsonArray& resourcesArray) {
    for (int i = 0; i < resourceCount; i++) {
      JsonObject resource = resourcesArray.createNestedObject();
      resources[i]->buildInfo(resource);
    }
  }
  
  int getResourceCount() const { return resourceCount; }
};

// Global resource registry
ResourceRegistry resourceRegistry;

// ------- Subscription Manager -------

#include <mutex>

class SubscriptionManager {
private:
  static const int MAX_SUBSCRIPTIONS = 20;
  String subscriptions[MAX_SUBSCRIPTIONS];
  int count = 0;
  mutable std::mutex mtx;  // Thread-safe access
  
public:
  bool subscribe(const char* uri) {
    std::lock_guard<std::mutex> lock(mtx);
    
    // Check if already subscribed
    for (int i = 0; i < count; i++) {
      if (subscriptions[i] == uri) {
        return true;  // Already subscribed
      }
    }
    
    if (count < MAX_SUBSCRIPTIONS) {
      subscriptions[count++] = String(uri);
      return true;
    }
    return false;
  }
  
  bool unsubscribe(const char* uri) {
    std::lock_guard<std::mutex> lock(mtx);
    
    for (int i = 0; i < count; i++) {
      if (subscriptions[i] == uri) {
        // Shift remaining items
        for (int j = i; j < count - 1; j++) {
          subscriptions[j] = subscriptions[j + 1];
        }
        count--;
        return true;
      }
    }
    return false;
  }
  
  bool isSubscribed(const char* uri) const {
    std::lock_guard<std::mutex> lock(mtx);
    
    for (int i = 0; i < count; i++) {
      if (subscriptions[i] == uri) {
        return true;
      }
    }
    return false;
  }
  
  int getSubscriptionCount() const { 
    std::lock_guard<std::mutex> lock(mtx);
    return count; 
  }
  
  void clear() {
    std::lock_guard<std::mutex> lock(mtx);
    count = 0;
  }
};

// Global subscription manager
SubscriptionManager subscriptionManager;

// Helper to send resource update notification via SSE (with debouncing)
void sendResourceUpdateNotification(const char* uri) {
  static unsigned long lastNotificationTime = 0;
  unsigned long now = millis();
  
  // Debounce notifications
  if (now - lastNotificationTime < NOTIFICATION_COOLDOWN_MS) {
    return;  // Too soon, prevent notification spam
  }
  
  if (!subscriptionManager.isSubscribed(uri)) {
    return;  // No one subscribed
  }
  
  lastNotificationTime = now;
  StaticJsonDocument<JSON_MED> doc;
  doc["jsonrpc"] = "2.0";
  doc["method"] = "notifications/resources/updated";
  JsonObject params = doc.createNestedObject("params");
  params["uri"] = uri;
  
  String out;
  serializeJson(doc, out);
  events.send(out.c_str(), "notification", millis());
  
  Serial.println(">>> NOTIFICATION SENT:");
  serializeJsonPretty(doc, Serial);
  Serial.println();
}

// ------- Concrete Tool Implementations -------

class LedTool : public McpTool {
public:
  const char* getName() const override { return "led"; }
  const char* getDescription() const override { return "Control the built-in LED"; }
  
protected:
  void addSchemaProperties(JsonObject& schema) const override {
    addBoolParam(schema, "on", "Turn LED on (true) or off (false)");
  }
  
public:
  bool execute(const JsonVariantConst args, JsonArray& content, String& error) override {
    bool on;
    if (!getBoolArg(args, "on", on, error)) return false;
    
    digitalWrite(LED_BUILTIN, on ? HIGH : LOW);
    addTextContent(content, on ? "LED turned on" : "LED turned off");
    
    // Notify subscribers that LED status changed
    sendResourceUpdateNotification("led://status");
    
    return true;
  }
};

class EchoTool : public McpTool {
public:
  const char* getName() const override { return "echo"; }
  const char* getDescription() const override { return "Echo back the provided text"; }
  
protected:
  void addSchemaProperties(JsonObject& schema) const override {
    addStringParam(schema, "text", "Text to echo back");
  }
  
public:
  bool execute(const JsonVariantConst args, JsonArray& content, String& error) override {
    const char* text;
    if (!getStringArg(args, "text", text, error)) return false;
    
    addTextContent(content, text);
    return true;
  }
};

class RgbLedTool : public McpTool {
private:
  const char* name;
  const char* description;
  int pin;
  const char* colorName;
  char messageBuffer[64];  // Buffer for dynamic messages
  
public:
  RgbLedTool(const char* n, const char* desc, int p, const char* color) 
    : name(n), description(desc), pin(p), colorName(color) {
    messageBuffer[0] = '\0';
  }
  
  const char* getName() const override { return name; }
  const char* getDescription() const override { return description; }
  
protected:
  void addSchemaProperties(JsonObject& schema) const override {
    // Build description directly in JsonObject to avoid temporary string issues
    char desc[80];
    snprintf(desc, sizeof(desc), "Turn %s LED on (true) or off (false)", colorName);
    addBoolParam(schema, "on", desc);
  }
  
public:
  bool execute(const JsonVariantConst args, JsonArray& content, String& error) override {
    bool on;
    if (!getBoolArg(args, "on", on, error)) return false;
    
    digitalWrite(pin, on ? LOW : HIGH);  // Active LOW
    
    // Use instance buffer to avoid temporary string issues
    snprintf(messageBuffer, sizeof(messageBuffer), "%s LED turned %s", colorName, on ? "on" : "off");
    addTextContent(content, messageBuffer);
    
    // Notify subscribers that LED status changed
    sendResourceUpdateNotification("led://status");
    
    return true;
  }
};

// Tool instances
LedTool ledTool;
EchoTool echoTool;
RgbLedTool redLedTool("red_led", "Control the red LED", LED_RED, "Red");
RgbLedTool greenLedTool("green_led", "Control the green LED", LED_GREEN, "Green");
RgbLedTool blueLedTool("blue_led", "Control the blue LED", LED_BLUE, "Blue");

// ------- Concrete Resource Implementations -------

class LedStatusResource : public McpResource {
public:
  const char* getUri() const override { return "led://status"; }
  const char* getName() const override { return "LED Status"; }
  const char* getDescription() const override { return "Current status of all LEDs"; }
  
  void read(JsonArray& content) override {
    StaticJsonDocument<JSON_SMALL> doc;
    doc["builtin"] = digitalRead(LED_BUILTIN) == HIGH;
    doc["red"] = digitalRead(LED_RED) == LOW;      // Active LOW
    doc["green"] = digitalRead(LED_GREEN) == LOW;  // Active LOW
    doc["blue"] = digitalRead(LED_BLUE) == LOW;    // Active LOW
    
    String jsonText;
    serializeJson(doc, jsonText);
    addJsonContent(content, jsonText.c_str());
  }
};

class SystemInfoResource : public McpResource {
public:
  const char* getUri() const override { return "system://info"; }
  const char* getName() const override { return "System Information"; }
  const char* getDescription() const override { return "Arduino system information"; }
  
  void read(JsonArray& content) override {
    StaticJsonDocument<JSON_SMALL> doc;
    doc["uptime_ms"] = millis() - bootMillis;
    doc["uptime_sec"] = (millis() - bootMillis) / 1000;
    doc["free_heap"] = ESP.getFreeHeap();
    doc["wifi_rssi"] = WiFi.RSSI();
    doc["wifi_connected"] = WiFi.status() == WL_CONNECTED;
    doc["subscriptions"] = subscriptionManager.getSubscriptionCount();
    
    String jsonText;
    serializeJson(doc, jsonText);
    addJsonContent(content, jsonText.c_str());
  }
};

class WifiStatusResource : public McpResource {
public:
  const char* getUri() const override { return "wifi://status"; }
  const char* getName() const override { return "WiFi Status"; }
  const char* getDescription() const override { return "WiFi connection status and signal strength"; }
  
  void read(JsonArray& content) override {
    StaticJsonDocument<JSON_SMALL> doc;
    doc["connected"] = WiFi.status() == WL_CONNECTED;
    doc["ssid"] = WiFi.SSID();
    doc["ip"] = WiFi.localIP().toString();
    doc["rssi"] = WiFi.RSSI();
    doc["signal_quality"] = map(WiFi.RSSI(), -100, -50, 0, 100);
    
    String jsonText;
    serializeJson(doc, jsonText);
    addJsonContent(content, jsonText.c_str());
  }
};

// Resource instances
LedStatusResource ledStatusResource;
SystemInfoResource systemInfoResource;
WifiStatusResource wifiStatusResource;

void sseLog(const char* msg) {
  StaticJsonDocument<JSON_MED> doc;
  doc["jsonrpc"] = "2.0";
  doc["method"] = "notifications/message";
  JsonObject params = doc.createNestedObject("params");
  params["level"] = "info";
  params["message"] = msg;
  
  String out;
  serializeJson(doc, out);
  events.send(out.c_str(), "message", millis());
}

void sseResult(const char* msg) {
  events.send(msg, "result", millis());
}

// ------- HTTP Handlers -------

// Handle MCP JSON-RPC 2.0 requests
void handleMCP(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
  McpRequestIndicator indicator;  // LED on when created, off when function exits
  
  StaticJsonDocument<JSON_LARGE> reqDoc;
  DeserializationError err = deserializeJson(reqDoc, data, len);
  
  if (err) {
    Serial.printf("JSON parse failed: %s\n", err.c_str());
    Serial.printf("Raw data (%d bytes): %.*s\n", len, len, data);
    StaticJsonDocument<JSON_SMALL> errResp;
    errResp["jsonrpc"] = "2.0";
    JsonObject error = errResp.createNestedObject("error");
    error["code"] = -32700;
    error["message"] = "Parse error";
    errResp["id"] = nullptr;
    
    String out;
    serializeJson(errResp, out);
    request->send(400, "application/json", out);
    return;
  }

  JsonVariant idVar = reqDoc["id"];
  const char* jsonrpc = reqDoc["jsonrpc"] | "";
  const char* method = reqDoc["method"] | "";
  JsonVariantConst params = reqDoc["params"];
  
  Serial.println("\n>>> REQUEST:");
  serializeJsonPretty(reqDoc, Serial);
  Serial.println();

  // Validate JSON-RPC 2.0
  if (strcmp(jsonrpc, "2.0") != 0) {
    StaticJsonDocument<JSON_SMALL> errResp;
    errResp["jsonrpc"] = "2.0";
    JsonObject error = errResp.createNestedObject("error");
    error["code"] = -32600;
    error["message"] = "Invalid Request";
    errResp["id"] = idVar;
    
    String out;
    serializeJson(errResp, out);
    request->send(400, "application/json", out);
    return;
  }

  if (!method || strlen(method) == 0) {
    StaticJsonDocument<JSON_SMALL> errResp;
    errResp["jsonrpc"] = "2.0";
    JsonObject error = errResp.createNestedObject("error");
    error["code"] = -32600;
    error["message"] = "Missing method";
    errResp["id"] = idVar;
    
    String out;
    serializeJson(errResp, out);
    request->send(400, "application/json", out);
    return;
  }

  StaticJsonDocument<JSON_LARGE> response;
  response["jsonrpc"] = "2.0";
  response["id"] = idVar;

  // Handle initialize
  if (strcmp(method, "initialize") == 0) {
    JsonObject result = response.createNestedObject("result");
    
    // Use client's protocol version if provided, otherwise use latest
    const char* clientVersion = params["protocolVersion"] | "2025-06-18";
    result["protocolVersion"] = clientVersion;
    
    JsonObject serverInfo = result.createNestedObject("serverInfo");
    serverInfo["name"] = SERVER_NAME;
    serverInfo["version"] = SERVER_VERSION;
    
    JsonObject capabilities = result.createNestedObject("capabilities");
    
    // Tools capability (empty object for 2025+ protocol)
    JsonObject toolsCap = capabilities.createNestedObject("tools");
    
    // Resources capability
    JsonObject resourcesCap = capabilities.createNestedObject("resources");
    resourcesCap["subscribe"] = true;
    resourcesCap["listChanged"] = false;  // Not implemented yet
    
    String out;
    serializeJson(response, out);
    Serial.println("<<< RESPONSE:");
    serializeJsonPretty(response, Serial);
    Serial.println();
    request->send(200, "application/json", out);
    return;
  }

  // Handle notifications/initialized (notification - no response needed)
  if (strcmp(method, "notifications/initialized") == 0) {
    request->send(200, "application/json", "{}");
    return;
  }

  // Handle ping
  if (strcmp(method, "ping") == 0) {
    JsonObject result = response.createNestedObject("result");
    // Empty result object
    
    String out;
    serializeJson(response, out);
    request->send(200, "application/json", out);
    return;
  }

  // Handle tools/list
  if (strcmp(method, "tools/list") == 0) {
    JsonObject result = response.createNestedObject("result");
    JsonArray tools = result.createNestedArray("tools");
    
    // Use registry to build tools list
    toolRegistry.buildToolsList(tools);
    
    String out;
    serializeJson(response, out);
    Serial.println("<<< RESPONSE:");
    serializeJsonPretty(response, Serial);
    Serial.println();
    request->send(200, "application/json", out);
    return;
  }

  // Handle tools/call
  if (strcmp(method, "tools/call") == 0) {
    if (!params.containsKey("name")) {
      JsonObject error = response.createNestedObject("error");
      error["code"] = -32602;
      error["message"] = "Missing tool name";
      
      String out;
      serializeJson(response, out);
      request->send(400, "application/json", out);
      return;
    }
    
    const char* toolName = params["name"];
    JsonVariantConst arguments = params["arguments"];
    
    // Find tool in registry
    McpTool* tool = toolRegistry.findTool(toolName);
    
    if (!tool) {
      JsonObject error = response.createNestedObject("error");
      error["code"] = -32601;
      error["message"] = "Unknown tool";
      
      String out;
      serializeJson(response, out);
      request->send(400, "application/json", out);
      return;
    }
    
    // Execute tool
    JsonObject result = response.createNestedObject("result");
    JsonArray content = result.createNestedArray("content");
    String errorMsg;
    
    if (!tool->execute(arguments, content, errorMsg)) {
      // Tool execution failed
      response.remove("result");
      JsonObject error = response.createNestedObject("error");
      error["code"] = -32603;
      error["message"] = errorMsg;
      
      String out;
      serializeJson(response, out);
      request->send(400, "application/json", out);
      return;
    }
    
    // Success
    String out;
    serializeJson(response, out);
    Serial.println("<<< RESPONSE:");
    serializeJsonPretty(response, Serial);
    Serial.println();
    request->send(200, "application/json", out);
    sseResult(out.c_str());
    return;
  }

  // Handle resources/list
  if (strcmp(method, "resources/list") == 0) {
    JsonObject result = response.createNestedObject("result");
    JsonArray resources = result.createNestedArray("resources");
    
    resourceRegistry.buildResourcesList(resources);
    
    String out;
    serializeJson(response, out);
    Serial.println("<<< RESPONSE:");
    serializeJsonPretty(response, Serial);
    Serial.println();
    request->send(200, "application/json", out);
    return;
  }

  // Handle resources/read
  if (strcmp(method, "resources/read") == 0) {
    if (!params.containsKey("uri")) {
      JsonObject error = response.createNestedObject("error");
      error["code"] = -32602;
      error["message"] = "Missing resource uri";
      
      String out;
      serializeJson(response, out);
      request->send(400, "application/json", out);
      return;
    }
    
    const char* uri = params["uri"];
    McpResource* resource = resourceRegistry.findResource(uri);
    
    if (!resource) {
      JsonObject error = response.createNestedObject("error");
      error["code"] = -32602;
      error["message"] = "Unknown resource";
      
      String out;
      serializeJson(response, out);
      request->send(400, "application/json", out);
      return;
    }
    
    JsonObject result = response.createNestedObject("result");
    JsonArray contents = result.createNestedArray("contents");
    resource->read(contents);
    
    String out;
    serializeJson(response, out);
    Serial.println("<<< RESPONSE:");
    serializeJsonPretty(response, Serial);
    Serial.println();
    request->send(200, "application/json", out);
    return;
  }

  // Handle resources/subscribe
  if (strcmp(method, "resources/subscribe") == 0) {
    if (!params.containsKey("uri")) {
      JsonObject error = response.createNestedObject("error");
      error["code"] = -32602;
      error["message"] = "Missing resource uri";
      
      String out;
      serializeJson(response, out);
      request->send(400, "application/json", out);
      return;
    }
    
    const char* uri = params["uri"];
    
    // Verify resource exists
    McpResource* resource = resourceRegistry.findResource(uri);
    if (!resource) {
      JsonObject error = response.createNestedObject("error");
      error["code"] = -32602;
      error["message"] = "Unknown resource";
      
      String out;
      serializeJson(response, out);
      request->send(400, "application/json", out);
      return;
    }
    
    // Add to subscriptions
    if (!subscriptionManager.subscribe(uri)) {
      JsonObject error = response.createNestedObject("error");
      error["code"] = -32603;
      error["message"] = "Subscription limit reached";
      
      String out;
      serializeJson(response, out);
      request->send(400, "application/json", out);
      return;
    }
    
    JsonObject result = response.createNestedObject("result");
    // Empty result object for success
    
    String out;
    serializeJson(response, out);
    Serial.println("<<< RESPONSE:");
    serializeJsonPretty(response, Serial);
    Serial.println();
    request->send(200, "application/json", out);
    return;
  }

  // Handle resources/unsubscribe
  if (strcmp(method, "resources/unsubscribe") == 0) {
    if (!params.containsKey("uri")) {
      JsonObject error = response.createNestedObject("error");
      error["code"] = -32602;
      error["message"] = "Missing resource uri";
      
      String out;
      serializeJson(response, out);
      request->send(400, "application/json", out);
      return;
    }
    
    const char* uri = params["uri"];
    subscriptionManager.unsubscribe(uri);
    
    JsonObject result = response.createNestedObject("result");
    // Empty result object for success
    
    String out;
    serializeJson(response, out);
    Serial.println("<<< RESPONSE:");
    serializeJsonPretty(response, Serial);
    Serial.println();
    request->send(200, "application/json", out);
    return;
  }

  // Method not found
  JsonObject error = response.createNestedObject("error");
  error["code"] = -32601;
  error["message"] = "Method not found";
  
  String out;
  serializeJson(response, out);
  request->send(404, "application/json", out);
}
void handleStatus(AsyncWebServerRequest* request) {
  StaticJsonDocument<JSON_SMALL> doc;
  doc["server"]    = SERVER_NAME;
  doc["version"]   = SERVER_VERSION;
  doc["uptime_ms"] = millis() - bootMillis;
  String out;
  serializeJson(doc, out);
  request->send(200, "application/json", out);
}

void handleTools(AsyncWebServerRequest* request) {
  // Legacy endpoint - now uses registry for consistency
  StaticJsonDocument<JSON_LARGE> doc;
  JsonArray tools = doc.createNestedArray("tools");
  
  // Use registry to populate all tools
  toolRegistry.buildToolsList(tools);

  String out;
  serializeJson(doc, out);
  request->send(200, "application/json", out);
}

void handleInvoke(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
  // Legacy endpoint - use new tool system
  StaticJsonDocument<JSON_MED> doc;
  DeserializationError err = deserializeJson(doc, data, len);
  if (err) {
    request->send(400, "application/json", "{\"error\":\"invalid_json\"}");
    return;
  }

  const char* toolName = doc["tool"] | nullptr;
  JsonVariantConst args = doc["args"];

  if (!toolName) {
    request->send(400, "application/json", "{\"error\":\"missing_tool\"}");
    return;
  }

  sseLog("invoke received");

  // Find and execute tool
  McpTool* tool = toolRegistry.findTool(toolName);
  if (!tool) {
    request->send(400, "application/json", "{\"error\":\"unknown_tool\"}");
    return;
  }
  
  StaticJsonDocument<JSON_MED> resp;
  JsonArray content = resp.createNestedArray("content");
  String errorMsg;
  
  if (!tool->execute(args, content, errorMsg)) {
    resp.clear();
    resp["error"] = errorMsg;
  }
  
  String out;
  serializeJson(resp, out);
  request->send(200, "application/json", out);
  sseResult(out.c_str());
}

// ------- Display Helper Function -------
void showQRCode() {
  if (!displayAvailable) return;
  
  // Show intro animation
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(52, 10);
  display.print("mcp");
  
  display.setTextSize(3);
  display.setCursor(16, 28);
  display.print("IDEAS");
  display.display();
  Serial.println("Intro displayed");
  delay(INTRO_DISPLAY_DURATION_MS);
  
  // Show QR code
  String ipAddress = WiFi.localIP().toString();
  String qrText = "http://" + ipAddress + ":" + String(SERVER_PORT) + "/mcp";
  String subText = ipAddress + ":" + String(SERVER_PORT);
  Serial.printf("Showing QR code for: %s\n", qrText.c_str());
  
  display.clearDisplay();
  QRCodeGFX qrcode(display);
  qrcode.setScale(2);
  qrcode.generateData(qrText.c_str());
  
  int qrWidth = 58;
  int xPos = (SCREEN_WIDTH - qrWidth) / 2;
  qrcode.draw(xPos, -3);
  
  display.setCursor(0, 57);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.print(subText.c_str());
  display.display();
  Serial.println("QR code displayed");
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);
  
  digitalWrite(LED_BUILTIN, LOW);
  digitalWrite(LED_RED, HIGH);    // OFF (active LOW)
  digitalWrite(LED_GREEN, HIGH);  // OFF (active LOW)
  digitalWrite(LED_BLUE, HIGH);   // OFF (active LOW)
  
  bootMillis = millis();

  Serial.begin(115200);
  delay(1000);

  Serial.println("ARDUINO NANO ESP32 - MCP SERVER v1.0.0");
  
  // Register all tools
  toolRegistry.registerTool(&ledTool);
  toolRegistry.registerTool(&echoTool);
  toolRegistry.registerTool(&redLedTool);
  toolRegistry.registerTool(&greenLedTool);
  toolRegistry.registerTool(&blueLedTool);
  Serial.printf("Registered %d tools\n", toolRegistry.getToolCount());

  // Register all resources
  resourceRegistry.registerResource(&ledStatusResource);
  resourceRegistry.registerResource(&systemInfoResource);
  resourceRegistry.registerResource(&wifiStatusResource);
  Serial.printf("Registered %d resources\n", resourceRegistry.getResourceCount());

  // Initialize I2C and display
  Wire.begin();
  Serial.println("I2C initialized");
  delay(I2C_INIT_DELAY_MS);
  
  // I2C scan to verify display is physically connected
  Serial.println("Scanning I2C bus...");
  Wire.beginTransmission(SCREEN_ADDRESS);
  byte error = Wire.endTransmission();
  
  if (error == 0) {
    Serial.printf("✓ I2C device found at address 0x%02X\n", SCREEN_ADDRESS);
    
    // Now try to initialize display
    if(display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
      displayAvailable = true;
      Serial.println("✓ Display successfully initialized");
      
      display.clearDisplay();
      display.display();
      delay(100);
      
      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(SSD1306_WHITE);
      display.setCursor(0, 20);
      display.println("Connecting WiFi");
      display.setCursor(0, 35);
      display.print(WIFI_SSID);
      display.display();
    } else {
      displayAvailable = false;
      Serial.println("✗ Display found via I2C but init failed!");
      Serial.println("MCP Server continues without display.");
    }
  } else {
    displayAvailable = false;
    Serial.printf("✗ No I2C device at address 0x%02X (error: %d)\n", SCREEN_ADDRESS, error);
    Serial.println("MCP Server continues without display.");
  }

  // Connect WiFi with progress indicator
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PSK);
  Serial.printf("Connecting to %s", WIFI_SSID);
  
  int dotCount = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(WIFI_CONNECT_RETRY_MS);
    Serial.print(".");
    
    if (displayAvailable) {
      display.fillRect(0, 50, 128, 14, SSD1306_BLACK);
      display.setCursor(0, 50);
      for(int i = 0; i < dotCount; i++) {
        display.print(".");
      }
      display.display();
      dotCount = (dotCount + 1) % 10;
    }
  }
  
  Serial.println();
  Serial.printf("Connected - IP: %s\n", WiFi.localIP().toString().c_str());
  
  // WiFi connected message
  if (displayAvailable) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 20);
    display.println("WiFi connected!");
    display.setCursor(0, 35);
    display.print(WiFi.localIP().toString());
    display.display();
    delay(WIFI_STATUS_DISPLAY_MS);
  }

  // Enable CORS for web browser access
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "Content-Type");
  
  // HTTP routes
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/tools",  HTTP_GET, handleTools);  // Legacy endpoint

  // POST /mcp - Main MCP JSON-RPC endpoint
  server.on("/mcp", HTTP_POST, 
    [](AsyncWebServerRequest* request) {}, 
    NULL,
    [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
      handleMCP(request, data, len, index, total);
    }
  );
  
  // POST /invoke - Legacy endpoint
  server.on("/invoke", HTTP_POST,
    [](AsyncWebServerRequest* request) {}, 
    NULL,
    [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
      handleInvoke(request, data, len, index, total);
    }
  );

  // SSE
  server.addHandler(&events);
  events.onConnect([](AsyncEventSourceClient* client){
    StaticJsonDocument<JSON_SMALL> doc;
    doc["jsonrpc"] = "2.0";
    doc["method"] = "notifications/message";
    JsonObject params = doc.createNestedObject("params");
    params["level"] = "info";
    params["message"] = "SSE connected";
    
    String out;
    serializeJson(doc, out);
    client->send(out.c_str(), "message", millis());
  });

  server.begin();
  Serial.printf("Server ready on port %d\n", SERVER_PORT);
  
  // Show QR code (stays permanent)
  showQRCode();
}

void loop() {
  // Nothing here; AsyncWebServer handles all requests
}
