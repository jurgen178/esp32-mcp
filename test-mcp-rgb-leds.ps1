# Test MCP RGB LED Controls
# Tests: All RGB LED tool calls (red, green, blue)

# Configuration
$MCPUrl = "http://192.168.1.125:8000/mcp"

$headers = @{
    "Content-Type" = "application/json"
}

Write-Host "=== MCP RGB LED Tests ===" -ForegroundColor Green
Write-Host ""

# Test 1: Initialize (required first)
Write-Host "[1/8] Testing initialize..." -ForegroundColor Cyan
$body = @{
    jsonrpc = "2.0"
    id = 1
    method = "initialize"
    params = @{}
} | ConvertTo-Json -Compress
$response = Invoke-WebRequest -Uri $MCPUrl -Method POST -Headers $headers -Body $body
Write-Host ($response.Content | ConvertFrom-Json | ConvertTo-Json -Depth 10) -ForegroundColor Gray
Write-Host ""

# Test 2: Red LED On
Write-Host "[2/8] Testing Red LED ON..." -ForegroundColor Cyan
$body = @{
    jsonrpc = "2.0"
    id = 2
    method = "tools/call"
    params = @{
        name = "red_led"
        arguments = @{
            on = $true
        }
    }
} | ConvertTo-Json -Compress -Depth 5
$response = Invoke-WebRequest -Uri $MCPUrl -Method POST -Headers $headers -Body $body
Write-Host ($response.Content | ConvertFrom-Json | ConvertTo-Json -Depth 10) -ForegroundColor Gray
Start-Sleep -Seconds 1

# Test 3: Red LED Off
Write-Host "[3/8] Testing Red LED OFF..." -ForegroundColor Cyan
$body = @{
    jsonrpc = "2.0"
    id = 3
    method = "tools/call"
    params = @{
        name = "red_led"
        arguments = @{
            on = $false
        }
    }
} | ConvertTo-Json -Compress -Depth 5
$response = Invoke-WebRequest -Uri $MCPUrl -Method POST -Headers $headers -Body $body
Write-Host ($response.Content | ConvertFrom-Json | ConvertTo-Json -Depth 10) -ForegroundColor Gray
Write-Host ""

# Test 4: Green LED On
Write-Host "[4/8] Testing Green LED ON..." -ForegroundColor Cyan
$body = @{
    jsonrpc = "2.0"
    id = 4
    method = "tools/call"
    params = @{
        name = "green_led"
        arguments = @{
            on = $true
        }
    }
} | ConvertTo-Json -Compress -Depth 5
$response = Invoke-WebRequest -Uri $MCPUrl -Method POST -Headers $headers -Body $body
Write-Host ($response.Content | ConvertFrom-Json | ConvertTo-Json -Depth 10) -ForegroundColor Gray
Start-Sleep -Seconds 1

# Test 5: Green LED Off
Write-Host "[5/8] Testing Green LED OFF..." -ForegroundColor Cyan
$body = @{
    jsonrpc = "2.0"
    id = 5
    method = "tools/call"
    params = @{
        name = "green_led"
        arguments = @{
            on = $false
        }
    }
} | ConvertTo-Json -Compress -Depth 5
$response = Invoke-WebRequest -Uri $MCPUrl -Method POST -Headers $headers -Body $body
Write-Host ($response.Content | ConvertFrom-Json | ConvertTo-Json -Depth 10) -ForegroundColor Gray
Write-Host ""

# Test 6: Blue LED On
Write-Host "[6/8] Testing Blue LED ON..." -ForegroundColor Cyan
$body = @{
    jsonrpc = "2.0"
    id = 6
    method = "tools/call"
    params = @{
        name = "blue_led"
        arguments = @{
            on = $true
        }
    }
} | ConvertTo-Json -Compress -Depth 5
$response = Invoke-WebRequest -Uri $MCPUrl -Method POST -Headers $headers -Body $body
Write-Host ($response.Content | ConvertFrom-Json | ConvertTo-Json -Depth 10) -ForegroundColor Gray
Start-Sleep -Seconds 1

# Test 7: Blue LED Off
Write-Host "[7/8] Testing Blue LED OFF..." -ForegroundColor Cyan
$body = @{
    jsonrpc = "2.0"
    id = 7
    method = "tools/call"
    params = @{
        name = "blue_led"
        arguments = @{
            on = $false
        }
    }
} | ConvertTo-Json -Compress -Depth 5
$response = Invoke-WebRequest -Uri $MCPUrl -Method POST -Headers $headers -Body $body
Write-Host ($response.Content | ConvertFrom-Json | ConvertTo-Json -Depth 10) -ForegroundColor Gray
Write-Host ""

# Test 8: All LEDs On (demonstration)
Write-Host "[8/8] Testing All LEDs ON (simultaneous)..." -ForegroundColor Cyan
Invoke-WebRequest -Uri $MCPUrl -Method POST -Headers $headers -Body (@{
    jsonrpc = "2.0"; id = 8; method = "tools/call"
    params = @{ name = "red_led"; arguments = @{ on = $true } }
} | ConvertTo-Json -Compress -Depth 5) | Out-Null

Invoke-WebRequest -Uri $MCPUrl -Method POST -Headers $headers -Body (@{
    jsonrpc = "2.0"; id = 9; method = "tools/call"
    params = @{ name = "green_led"; arguments = @{ on = $true } }
} | ConvertTo-Json -Compress -Depth 5) | Out-Null

Invoke-WebRequest -Uri $MCPUrl -Method POST -Headers $headers -Body (@{
    jsonrpc = "2.0"; id = 10; method = "tools/call"
    params = @{ name = "blue_led"; arguments = @{ on = $true } }
} | ConvertTo-Json -Compress -Depth 5) | Out-Null

Write-Host "All RGB LEDs should now be ON" -ForegroundColor Yellow
Start-Sleep -Seconds 2

# Turn all off
Invoke-WebRequest -Uri $MCPUrl -Method POST -Headers $headers -Body (@{
    jsonrpc = "2.0"; id = 11; method = "tools/call"
    params = @{ name = "red_led"; arguments = @{ on = $false } }
} | ConvertTo-Json -Compress -Depth 5) | Out-Null

Invoke-WebRequest -Uri $MCPUrl -Method POST -Headers $headers -Body (@{
    jsonrpc = "2.0"; id = 12; method = "tools/call"
    params = @{ name = "green_led"; arguments = @{ on = $false } }
} | ConvertTo-Json -Compress -Depth 5) | Out-Null

Invoke-WebRequest -Uri $MCPUrl -Method POST -Headers $headers -Body (@{
    jsonrpc = "2.0"; id = 13; method = "tools/call"
    params = @{ name = "blue_led"; arguments = @{ on = $false } }
} | ConvertTo-Json -Compress -Depth 5) | Out-Null

Write-Host "All RGB LEDs turned OFF" -ForegroundColor Yellow
Write-Host ""

Write-Host "=== RGB LED Tests Complete ===" -ForegroundColor Green
