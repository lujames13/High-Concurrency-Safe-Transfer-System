#!/bin/bash

# ============================================================================
# HSTS - Build and Test Script (從 tests/ 目錄執行)
# ============================================================================

set -e  # Exit on error

# --- Color Definitions ---
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# --- Helper Functions ---
print_step() {
    echo -e "\n${GREEN}[STEP]${NC} $1"
}

print_success() {
    echo -e "${GREEN}✓${NC} $1"
}

print_error() {
    echo -e "${RED}✗${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}⚠${NC} $1"
}

# --- Get Project Root ---
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

echo -e "${BLUE}========================================${NC}"
echo -e "${CYAN}HSTS - Build and Test${NC}"
echo -e "${BLUE}========================================${NC}"
echo "Script location: $SCRIPT_DIR"
echo "Project root: $PROJECT_ROOT"
echo ""

# Step 1: Navigate to Project Root
print_step "Navigating to project root..."
cd "$PROJECT_ROOT"
print_success "Current directory: $(pwd)"

# Step 2: Clean Old Build
print_step "Cleaning old build artifacts..."
rm -rf build bin lib
print_success "Clean complete"

# Step 3: Create Build Directory
print_step "Creating build directory..."
mkdir -p build
cd build
print_success "Build directory ready"

# Step 4: Run CMake
print_step "Running CMake configuration..."
if cmake .. > /dev/null 2>&1; then
    print_success "CMake configuration successful"
else
    print_error "CMake configuration failed"
    cmake ..  # Show error details
    exit 1
fi

# Step 5: Compile
print_step "Compiling project..."
if make -j$(nproc) 2>&1 | tee /tmp/hsts_build.log | grep -E "error:" > /dev/null; then
    print_error "Compilation failed. See errors above."
    exit 1
else
    print_success "Compilation successful"
fi

# Step 6: Verify Executables
cd "$PROJECT_ROOT"
print_step "Verifying executables..."

EXECUTABLES=("bin/server" "bin/client" "bin/test_bank" "bin/test_logger" "bin/test_monitor")
ALL_EXIST=true

for exec in "${EXECUTABLES[@]}"; do
    if [[ -f "$exec" ]]; then
        print_success "Found: $exec"
    else
        print_error "Missing: $exec"
        ALL_EXIST=false
    fi
done

if [[ "$ALL_EXIST" == false ]]; then
    print_error "Some executables are missing"
    exit 1
fi

# Step 7: Run Unit Tests
echo ""
echo -e "${BLUE}========================================${NC}"
echo -e "${CYAN}Running Unit Tests${NC}"
echo -e "${BLUE}========================================${NC}"

print_step "Testing Bank Core..."
if ./bin/test_bank; then
    print_success "Bank Core tests passed"
else
    print_error "Bank Core tests failed"
    exit 1
fi

print_step "Testing Logger..."
if ./bin/test_logger; then
    print_success "Logger tests passed"
else
    print_error "Logger tests failed"
    exit 1
fi

# Step 8: Interactive Mode Selection
echo ""
echo -e "${BLUE}========================================${NC}"
echo -e "${CYAN}Integration Test Options${NC}"
echo -e "${BLUE}========================================${NC}"
echo "1. Start Server (background) + Stress Test"
echo "2. Start Server (background) + Interactive Client"
echo "3. Start Server Only (foreground)"
echo "4. Skip Integration Tests"
echo ""
read -p "Select option [1-4]: " choice

case $choice in
    1)
        # Stress Test
        print_step "Cleaning IPC resources..."
        ipcrm -a 2>/dev/null || true
        
        print_step "Starting server in background..."
        ./bin/server > /tmp/hsts_server.log 2>&1 &
        SERVER_PID=$!
        print_success "Server started (PID: $SERVER_PID)"
        
        sleep 2
        
        if ! kill -0 $SERVER_PID 2>/dev/null; then
            print_error "Server failed to start"
            cat /tmp/hsts_server.log
            exit 1
        fi
        
        print_step "Running stress test..."
        ./bin/client --stress
        
        print_step "Stopping server..."
        kill -INT $SERVER_PID 2>/dev/null || true
        wait $SERVER_PID 2>/dev/null || true
        print_success "Server stopped"
        ;;
        
    2)
        # Interactive Client
        print_step "Cleaning IPC resources..."
        ipcrm -a 2>/dev/null || true
        
        print_step "Starting server in background..."
        ./bin/server > /tmp/hsts_server.log 2>&1 &
        SERVER_PID=$!
        print_success "Server started (PID: $SERVER_PID)"
        
        sleep 2
        
        if ! kill -0 $SERVER_PID 2>/dev/null; then
            print_error "Server failed to start"
            cat /tmp/hsts_server.log
            exit 1
        fi
        
        print_warning "Starting interactive client..."
        echo "Press Ctrl+C to stop client, then run: kill $SERVER_PID"
        ./bin/client
        
        print_step "Stopping server..."
        kill -INT $SERVER_PID 2>/dev/null || true
        wait $SERVER_PID 2>/dev/null || true
        print_success "Server stopped"
        ;;
        
    3)
        # Server Only
        print_step "Cleaning IPC resources..."
        ipcrm -a 2>/dev/null || true
        
        print_warning "Starting server in foreground..."
        print_warning "Press Ctrl+C to stop"
        ./bin/server
        ;;
        
    4)
        print_warning "Skipping integration tests"
        ;;
        
    *)
        print_error "Invalid option"
        exit 1
        ;;
esac

# Cleanup
print_step "Final cleanup..."
ipcrm -a 2>/dev/null || true
print_success "IPC resources cleaned"

echo ""
echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}✓ All tests completed!${NC}"
echo -e "${GREEN}========================================${NC}" 