# Building an MCP Server on ESP32: Connecting AI Assistants to Real-World Devices  

### Connecting AI to the Physical World with Model Context Protocol

As detailed in [StickyMCP: Notes That Stick, Even in the Cloud](https://www.bitfabrik.io/blog/index.php?id=253), MCP servers open the door for AI systems to interact with real-world tools far beyond their usual diet of static training data and existential boredom.  

This project brings together two cutting-edge technologies: the Model Context Protocol (MCP) and Arduino Microcontroller. The result is an MCP server running directly on an Arduino Nano ESP32, allowing AI assistants control physical hardware in real-time.
<br />

![DSC_1032](https://github.com/user-attachments/assets/505da7b4-4b12-482f-af9b-63dba4a2b079)

<br />

Video:  
(Some prompts were typed a bit wrong because I had the camera in the way, but Copilot Chat did not mind at all)  

[![Video](https://www.bitfabrik.io/blog/dateien/esp32-mcp/ESP32-mcp-video.jpg)](https://www.bitfabrik.io/blog/dateien/esp32-mcp/ESP32-mcp-2.mp4)

<br />

![ESP32-mcp](https://github.com/user-attachments/assets/1abc0813-3d2a-4a55-a85f-9174ea4552a1)

<br />

<img width="2496" height="1570" alt="vsc-mcp" src="https://github.com/user-attachments/assets/4dbd5c5f-7ccd-481d-bcf5-d5297dea1014" />

<br />
<br />

Implemented a MCP SDK for C++ (none exists officially) optimized for embedded systems. Features automatic JSON schema generation, registry-based tool discovery, and memory-safe execution.  
Architecture mirrors official SDKs (TypeScript/Python) while addressing embedded constraints. Demonstrates full MCP 2024-11-05 protocol compliance with hardware control tools. I kept it small and readable, and made it work to run simple tools like switching lights on and off.

<br />

### What is it?

This project implements a fully-compliant JSON-RPC 2.0 MCP server on an Arduino Nano ESP32 microcontroller. It exposes hardware controls (LEDs in this case) as MCP "tools" that can be invoked by AI assistants through natural language commands.

### Key Features

- **MCP Protocol Support**: Implements the MCP 2024-11-05 specification with proper initialization, tool listing, and tool execution
- **JSON-RPC 2.0 Compliance**: Standard protocol interface for reliable communication
- **OLED Display with QR Code**: Shows server URL as scannable QR code for easy mobile access
- **WiFi-Enabled**: Runs a web server on port 8000, making it accessible over the network
- **Server-Sent Events (SSE)**: Real-time notifications and logging stream for monitoring
- **Multiple LED Controls**: Manages built-in LED plus RGB LEDs (red, green, blue) independently
- **Echo Tool**: Simple text echo for testing and demonstration

### Technical Implementation

The server uses the ESPAsyncWebServer library for handling HTTP requests and ArduinoJson for JSON parsing/serialization. It exposes two main endpoints:
- `POST /mcp` - Main JSON-RPC 2.0 endpoint for all MCP methods
- `GET /sse` - Server-Sent Events stream for real-time notifications

Each LED is implemented as an MCP tool with a simple boolean parameter to turn it on or off. The server handles all the MCP lifecycle methods including initialization, capability negotiation, and tool invocation.

### Real-World Applications

This project demonstrates how AI assistants can seamlessly control physical devices. Instead of writing custom scripts or manual API calls, you can simply tell an AI assistant "turn on the red LED" and it happens. This opens up possibilities for:
- Smart home automation controlled by natural language
- Laboratory equipment control through AI assistants
- Educational demonstrations of AI-hardware integration
- Rapid prototyping of IoT devices with conversational interfaces

---

**Technical Stack:**
- Arduino Nano ESP32
- ESPAsyncWebServer
- ArduinoJson
- Model Context Protocol (MCP) 2024-11-05
- JSON-RPC 2.0

<br />

See also my [BitBlog](https://www.bitfabrik.io/blog/index.php?name=esp32-mcp)
