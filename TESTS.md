# MCP Server Test Scripts

This directory contains PowerShell test scripts to verify the Arduino MCP Server functionality.

## Prerequisites

1. Arduino Nano ESP32 running the MCP server code
2. Arduino connected to your WiFi network
3. PowerShell (Windows) or PowerShell Core (cross-platform)

## Configuration

Before running the tests, update the `$MCPUrl` variable in each script to match your Arduino's IP address:

```powershell
$MCPUrl = "http://192.168.1.125:8000/mcp"
```

To find your Arduino's IP address, check the Serial Monitor after the device boots.

## Test Scripts

### test-mcp-basic.ps1
**Purpose:** Test core MCP protocol functionality

**What it tests:**
- `initialize` - MCP server initialization
- `tools/list` - List all available tools
- `tools/call` (led) - Control built-in LED
- `tools/call` (echo) - Echo text test

**Usage:**
```powershell
.\test-mcp-basic.ps1
```

**Expected Output:**
- JSON-RPC 2.0 responses for each test
- Built-in LED should turn on during test 3
- Echo response should return "Hello from MCP!"

---

### test-mcp-rgb-leds.ps1
**Purpose:** Test all RGB LED controls with visual feedback

**What it tests:**
- `initialize` - Server initialization
- Red LED on/off
- Green LED on/off
- Blue LED on/off
- All LEDs simultaneously

**Usage:**
```powershell
.\test-mcp-rgb-leds.ps1
```

**Expected Behavior:**
1. Each LED turns on individually for 1 second, then off
2. All three LEDs turn on together for 2 seconds
3. All LEDs turn off at the end
4. JSON responses are formatted and displayed

---

## Expected Response Format

All responses follow JSON-RPC 2.0 format:

### Success Response
```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "result": {
    "content": [
      {
        "type": "text",
        "text": "LED turned on"
      }
    ]
  }
}
```

### Error Response
```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "error": {
    "code": -32601,
    "message": "Method not found"
  }
}
```

## Troubleshooting

**Connection refused:**
- Verify Arduino is powered on and connected to WiFi
- Check IP address in `$MCPUrl` matches Arduino's IP
- Ensure port 8000 is accessible (check firewall)

**No LED response:**
- Check LED pins are correctly connected (pins 14, 15, 16)
- Verify LEDs are active LOW (default configuration)
- Check Serial Monitor for error messages

**JSON parse errors:**
- Ensure Arduino has latest code deployed
- Verify ESPAsyncWebServer and ArduinoJson libraries are installed

## Advanced Usage

### Custom Tests

You can create custom test requests using the JSON-RPC 2.0 format:

```powershell
$MCPUrl = "http://192.168.1.125:8000/mcp"

$body = @{
    jsonrpc = "2.0"
    id = 1
    method = "tools/call"
    params = @{
        name = "red_led"
        arguments = @{
            on = $true
        }
    }
} | ConvertTo-Json -Compress -Depth 5

Invoke-WebRequest -Uri $MCPUrl -Method POST `
    -Headers @{"Content-Type"="application/json"} `
    -Body $body
```

## Available Tools

| Tool Name | Description | Parameters |
|-----------|-------------|------------|
| `led` | Control built-in LED | `on` (boolean) |
| `echo` | Echo text back | `text` (string) |
| `red_led` | Control red LED | `on` (boolean) |
| `green_led` | Control green LED | `on` (boolean) |
| `blue_led` | Control blue LED | `on` (boolean) |

## Contributing

Feel free to add more test scripts for:
- Error condition testing
- Performance/stress testing
- SSE (Server-Sent Events) endpoint testing
- Concurrent request handling
