# Test MCP Core Protocol
# Tests: initialize, tools/list, basic tool calls (LED, echo)

# Configuration
$MCPUrl = "http://192.168.1.125:8000/mcp"

Write-Host "=== MCP Basic Protocol Tests ===" -ForegroundColor Green
Write-Host ""

# Test 1: Initialize
Write-Host "[1/4] Testing initialize..." -ForegroundColor Cyan
$init = '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{}}'
$response = curl.exe -X POST $MCPUrl -H "Content-Type: application/json" -d $init
Write-Host $response -ForegroundColor Gray
Write-Host ""

# Test 2: Tools List
Write-Host "[2/4] Testing tools/list..." -ForegroundColor Cyan
$list = '{"jsonrpc":"2.0","id":2,"method":"tools/list","params":{}}'
$response = curl.exe -X POST $MCPUrl -H "Content-Type: application/json" -d $list
Write-Host $response -ForegroundColor Gray
Write-Host ""

# Test 3: LED Tool (on)
Write-Host "[3/4] Testing tools/call (LED on)..." -ForegroundColor Cyan
$call = '{"jsonrpc":"2.0","id":3,"method":"tools/call","params":{"name":"led","arguments":{"on":true}}}'
$response = curl.exe -X POST $MCPUrl -H "Content-Type: application/json" -d $call
Write-Host $response -ForegroundColor Gray
Write-Host ""

# Test 4: Echo Tool
Write-Host "[4/4] Testing tools/call (echo)..." -ForegroundColor Cyan
$echo = '{"jsonrpc":"2.0","id":4,"method":"tools/call","params":{"name":"echo","arguments":{"text":"Hello from MCP!"}}}'
$response = curl.exe -X POST $MCPUrl -H "Content-Type: application/json" -d $echo
Write-Host $response -ForegroundColor Gray
Write-Host ""

Write-Host "=== Tests Complete ===" -ForegroundColor Green
