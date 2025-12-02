
/*
  Nano ESP32 MCP Server (JSON-RPC 2.0)
  Endpoints:
    POST /mcp    -> JSON-RPC 2.0 endpoint for all MCP methods
    GET  /sse    -> SSE stream for notifications/logging

  MCP Methods:
    - initialize: Server initialization
    - tools/list: List available tools
    - tools/call: Invoke a tool

  Dependencies (Library Manager):
    - ESPAsyncWebServer
    - ArduinoJson
*/

#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

#ifndef LED_BUILTIN
#define LED_BUILTIN 13
#endif

// RGB LED pins
#define LED_RED 14     // Pin 14 for red
#define LED_GREEN 15   // Pin 15 for green
#define LED_BLUE 16    // Pin 16 for blue

// ------- WiFi config -------
#include "arduino_secrets.h"
const char* WIFI_SSID = SECRET_SSID;
const char* WIFI_PSK  = SECRET_PASS;

// ------- Server on port 8000 -------
AsyncWebServer server(8000);
AsyncEventSource events("/sse");   // SSE endpoint for MCP notifications

// ------- Buffers (tune for your payload sizes) -------
static const size_t JSON_SMALL = 512;
static const size_t JSON_MED   = 1024;
static const size_t JSON_LARGE = 2048;

// ------- Server Info -------
const char* SERVER_NAME = "arduino-mcp";
const char* SERVER_VERSION = "1.0.0";

// ------- Utilities -------
unsigned long bootMillis;

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

// ------- Tool: LED -------
StaticJsonDocument<JSON_SMALL> ledCall(const JsonVariantConst args) {
  StaticJsonDocument<JSON_SMALL> resp;
  bool on = false;

  if (args.is<JsonObjectConst>()) {
    JsonObjectConst obj = args.as<JsonObjectConst>();
    if (obj.containsKey("on")) {
      on = obj["on"].as<bool>();
    } else {
      resp["error"] = "missing_argument_on";
      return resp;
    }
  } else {
    resp["error"] = "arguments_must_be_object";
    return resp;
  }

  digitalWrite(LED_BUILTIN, on ? HIGH : LOW);
  
  // Return content in MCP format
  JsonArray content = resp.createNestedArray("content");
  JsonObject textContent = content.createNestedObject();
  textContent["type"] = "text";
  textContent["text"] = on ? "LED turned on" : "LED turned off";
  
  return resp;
}

// ------- Tool: ECHO -------
StaticJsonDocument<JSON_MED> echoCall(const JsonVariantConst args) {
  StaticJsonDocument<JSON_MED> resp;
  const char* text = "";
  
  if (args.is<JsonObjectConst>() && args.as<JsonObjectConst>().containsKey("text")) {
    text = args["text"].as<const char*>();
  } else {
    resp["error"] = "missing_argument_text";
    return resp;
  }
  
  // Return content in MCP format
  JsonArray content = resp.createNestedArray("content");
  JsonObject textContent = content.createNestedObject();
  textContent["type"] = "text";
  textContent["text"] = text;
  
  return resp;
}

// ------- Tool: RED LED -------
StaticJsonDocument<JSON_SMALL> redLedCall(const JsonVariantConst args) {
  StaticJsonDocument<JSON_SMALL> resp;
  bool on = false;

  if (args.is<JsonObjectConst>()) {
    JsonObjectConst obj = args.as<JsonObjectConst>();
    if (obj.containsKey("on")) {
      on = obj["on"].as<bool>();
    } else {
      resp["error"] = "missing_argument_on";
      return resp;
    }
  } else {
    resp["error"] = "arguments_must_be_object";
    return resp;
  }

  digitalWrite(LED_RED, on ? LOW : HIGH);  // Active LOW
  
  JsonArray content = resp.createNestedArray("content");
  JsonObject textContent = content.createNestedObject();
  textContent["type"] = "text";
  textContent["text"] = on ? "Red LED turned on" : "Red LED turned off";
  
  return resp;
}

// ------- Tool: GREEN LED -------
StaticJsonDocument<JSON_SMALL> greenLedCall(const JsonVariantConst args) {
  StaticJsonDocument<JSON_SMALL> resp;
  bool on = false;

  if (args.is<JsonObjectConst>()) {
    JsonObjectConst obj = args.as<JsonObjectConst>();
    if (obj.containsKey("on")) {
      on = obj["on"].as<bool>();
    } else {
      resp["error"] = "missing_argument_on";
      return resp;
    }
  } else {
    resp["error"] = "arguments_must_be_object";
    return resp;
  }

  digitalWrite(LED_GREEN, on ? LOW : HIGH);  // Active LOW
  
  JsonArray content = resp.createNestedArray("content");
  JsonObject textContent = content.createNestedObject();
  textContent["type"] = "text";
  textContent["text"] = on ? "Green LED turned on" : "Green LED turned off";
  
  return resp;
}

// ------- Tool: BLUE LED -------
StaticJsonDocument<JSON_SMALL> blueLedCall(const JsonVariantConst args) {
  StaticJsonDocument<JSON_SMALL> resp;
  bool on = false;

  if (args.is<JsonObjectConst>()) {
    JsonObjectConst obj = args.as<JsonObjectConst>();
    if (obj.containsKey("on")) {
      on = obj["on"].as<bool>();
    } else {
      resp["error"] = "missing_argument_on";
      return resp;
    }
  } else {
    resp["error"] = "arguments_must_be_object";
    return resp;
  }

  digitalWrite(LED_BLUE, on ? LOW : HIGH);  // Active LOW
  
  JsonArray content = resp.createNestedArray("content");
  JsonObject textContent = content.createNestedObject();
  textContent["type"] = "text";
  textContent["text"] = on ? "Blue LED turned on" : "Blue LED turned off";
  
  return resp;
}

// ------- HTTP Handlers -------

// Handle MCP JSON-RPC 2.0 requests
void handleMCP(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
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
    result["protocolVersion"] = "2024-11-05";
    
    JsonObject serverInfo = result.createNestedObject("serverInfo");
    serverInfo["name"] = SERVER_NAME;
    serverInfo["version"] = SERVER_VERSION;
    
    JsonObject capabilities = result.createNestedObject("capabilities");
    capabilities["tools"] = true;
    
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

  // Handle tools/list
  if (strcmp(method, "tools/list") == 0) {
    JsonObject result = response.createNestedObject("result");
    JsonArray tools = result.createNestedArray("tools");
    
    // LED tool
    JsonObject ledTool = tools.createNestedObject();
    ledTool["name"] = "led";
    ledTool["description"] = "Control the built-in LED";
    JsonObject ledSchema = ledTool.createNestedObject("inputSchema");
    ledSchema["type"] = "object";
    JsonObject ledProps = ledSchema.createNestedObject("properties");
    JsonObject ledOnProp = ledProps.createNestedObject("on");
    ledOnProp["type"] = "boolean";
    ledOnProp["description"] = "Turn LED on (true) or off (false)";
    JsonArray ledReq = ledSchema.createNestedArray("required");
    ledReq.add("on");
    
    // Echo tool
    JsonObject echoTool = tools.createNestedObject();
    echoTool["name"] = "echo";
    echoTool["description"] = "Echo back the provided text";
    JsonObject echoSchema = echoTool.createNestedObject("inputSchema");
    echoSchema["type"] = "object";
    JsonObject echoProps = echoSchema.createNestedObject("properties");
    JsonObject echoTextProp = echoProps.createNestedObject("text");
    echoTextProp["type"] = "string";
    echoTextProp["description"] = "Text to echo back";
    JsonArray echoReq = echoSchema.createNestedArray("required");
    echoReq.add("text");
    
    // Red LED tool
    JsonObject redTool = tools.createNestedObject();
    redTool["name"] = "red_led";
    redTool["description"] = "Control the red LED";
    JsonObject redSchema = redTool.createNestedObject("inputSchema");
    redSchema["type"] = "object";
    JsonObject redProps = redSchema.createNestedObject("properties");
    JsonObject redOnProp = redProps.createNestedObject("on");
    redOnProp["type"] = "boolean";
    redOnProp["description"] = "Turn red LED on (true) or off (false)";
    JsonArray redReq = redSchema.createNestedArray("required");
    redReq.add("on");
    
    // Green LED tool
    JsonObject greenTool = tools.createNestedObject();
    greenTool["name"] = "green_led";
    greenTool["description"] = "Control the green LED";
    JsonObject greenSchema = greenTool.createNestedObject("inputSchema");
    greenSchema["type"] = "object";
    JsonObject greenProps = greenSchema.createNestedObject("properties");
    JsonObject greenOnProp = greenProps.createNestedObject("on");
    greenOnProp["type"] = "boolean";
    greenOnProp["description"] = "Turn green LED on (true) or off (false)";
    JsonArray greenReq = greenSchema.createNestedArray("required");
    greenReq.add("on");
    
    // Blue LED tool
    JsonObject blueTool = tools.createNestedObject();
    blueTool["name"] = "blue_led";
    blueTool["description"] = "Control the blue LED";
    JsonObject blueSchema = blueTool.createNestedObject("inputSchema");
    blueSchema["type"] = "object";
    JsonObject blueProps = blueSchema.createNestedObject("properties");
    JsonObject blueOnProp = blueProps.createNestedObject("on");
    blueOnProp["type"] = "boolean";
    blueOnProp["description"] = "Turn blue LED on (true) or off (false)";
    JsonArray blueReq = blueSchema.createNestedArray("required");
    blueReq.add("on");
    
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
    
    if (strcmp(toolName, "led") == 0) {
      auto toolResult = ledCall(arguments);
      
      // Check if tool returned error
      if (toolResult.containsKey("error")) {
        JsonObject error = response.createNestedObject("error");
        error["code"] = -32603;
        error["message"] = toolResult["error"].as<String>();
        
        String out;
        serializeJson(response, out);
        request->send(400, "application/json", out);
        return;
      }
      
      // Success - return tool result
      response["result"] = toolResult;
      
      String out;
      serializeJson(response, out);
      Serial.println("<<< RESPONSE:");
      serializeJsonPretty(response, Serial);
      Serial.println();
      request->send(200, "application/json", out);
      sseResult(out.c_str());
      return;
      
    } else if (strcmp(toolName, "echo") == 0) {
      auto toolResult = echoCall(arguments);
      
      // Check if tool returned error
      if (toolResult.containsKey("error")) {
        JsonObject error = response.createNestedObject("error");
        error["code"] = -32603;
        error["message"] = toolResult["error"].as<String>();
        
        String out;
        serializeJson(response, out);
        request->send(400, "application/json", out);
        return;
      }
      
      // Success - return tool result
      response["result"] = toolResult;
      
      String out;
      serializeJson(response, out);
      Serial.println("<<< RESPONSE:");
      serializeJsonPretty(response, Serial);
      Serial.println();
      request->send(200, "application/json", out);
      sseResult(out.c_str());
      return;
      
    } else if (strcmp(toolName, "red_led") == 0) {
      auto toolResult = redLedCall(arguments);
      
      if (toolResult.containsKey("error")) {
        JsonObject error = response.createNestedObject("error");
        error["code"] = -32603;
        error["message"] = toolResult["error"].as<String>();
        
        String out;
        serializeJson(response, out);
        request->send(400, "application/json", out);
        return;
      }
      
      response["result"] = toolResult;
      String out;
      serializeJson(response, out);
      Serial.println("<<< RESPONSE:");
      serializeJsonPretty(response, Serial);
      Serial.println();
      request->send(200, "application/json", out);
      sseResult(out.c_str());
      return;
      
    } else if (strcmp(toolName, "green_led") == 0) {
      auto toolResult = greenLedCall(arguments);
      
      if (toolResult.containsKey("error")) {
        JsonObject error = response.createNestedObject("error");
        error["code"] = -32603;
        error["message"] = toolResult["error"].as<String>();
        
        String out;
        serializeJson(response, out);
        request->send(400, "application/json", out);
        return;
      }
      
      response["result"] = toolResult;
      String out;
      serializeJson(response, out);
      Serial.println("<<< RESPONSE:");
      serializeJsonPretty(response, Serial);
      Serial.println();
      request->send(200, "application/json", out);
      sseResult(out.c_str());
      return;
      
    } else if (strcmp(toolName, "blue_led") == 0) {
      auto toolResult = blueLedCall(arguments);
      
      if (toolResult.containsKey("error")) {
        JsonObject error = response.createNestedObject("error");
        error["code"] = -32603;
        error["message"] = toolResult["error"].as<String>();
        
        String out;
        serializeJson(response, out);
        request->send(400, "application/json", out);
        return;
      }
      
      response["result"] = toolResult;
      String out;
      serializeJson(response, out);
      Serial.println("<<< RESPONSE:");
      serializeJsonPretty(response, Serial);
      Serial.println();
      request->send(200, "application/json", out);
      sseResult(out.c_str());
      return;
      
    } else {
      JsonObject error = response.createNestedObject("error");
      error["code"] = -32601;
      error["message"] = "Unknown tool";
      
      String out;
      serializeJson(response, out);
      request->send(400, "application/json", out);
      return;
    }
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
  StaticJsonDocument<JSON_MED> doc;
  JsonArray tools = doc.createNestedArray("tools");

  JsonObject t1 = tools.createNestedObject();
  t1["name"] = "led";
  JsonArray t1args = t1.createNestedArray("args");
  t1args.add("on");

  JsonObject t2 = tools.createNestedObject();
  t2["name"] = "echo";
  JsonArray t2args = t2.createNestedArray("args");
  t2args.add("text");

  String out;
  serializeJson(doc, out);
  request->send(200, "application/json", out);
}

void handleInvoke(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
  // Accumulate body (AsyncWebServer buffers it for us)
  StaticJsonDocument<JSON_MED> doc;
  DeserializationError err = deserializeJson(doc, data, len);
  if (err) {
    request->send(400, "application/json", "{\"error\":\"invalid_json\"}");
    return;
  }

  const char* tool = doc["tool"] | nullptr;
  JsonVariantConst args = doc["args"];

  if (!tool) {
    request->send(400, "application/json", "{\"error\":\"missing_tool\"}");
    return;
  }

  sseLog("invoke received");

  String out;
  if (strcmp(tool, "led") == 0) {
    auto resp = ledCall(args);
    serializeJson(resp, out);
    request->send(200, "application/json", out);
    sseResult(out.c_str());
    return;
  } else if (strcmp(tool, "echo") == 0) {
    auto resp = echoCall(args);
    serializeJson(resp, out);
    request->send(200, "application/json", out);
    sseResult(out.c_str());
    return;
  } else {
    request->send(400, "application/json", "{\"error\":\"unknown_tool\"}");
    return;
  }
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

  // Wi-Fi
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PSK);
  //Serial.printf("Connecting to %s", WIFI_SSID);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.printf("Connected - IP: %s\n", WiFi.localIP().toString().c_str());

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
  Serial.println("Server ready on port 8000");
}

void loop() {
  // Nothing here; AsyncWebServer handles all requests
}
