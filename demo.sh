#!/bin/bash
set -e

# Color definitions
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Helper functions
print_separator() {
    echo -e "${BLUE}--------------------------------------------------${NC}"
}

wait_for_enter() {
    read -p "Press Enter to continue..."
    echo
}

print_step() {
    echo -e "${GREEN}[STEP]${NC} $1"
}

print_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Check if executables exist
if [ ! -f "./bin/manager" ] || [ ! -f "./bin/client" ]; then
    print_error "Manager or client executables not found in ./bin directory."
    print_info "Make sure you've built the project with ./build.sh"
    exit 1
fi

clear
print_separator
print_step "Running D-Bus Configuration Manager Demo"
print_separator
print_info "This demo will guide you through the functionality of the D-Bus Configuration Manager."
print_info "1. First, open a terminal and run: ${YELLOW}./bin/manager${NC}"
print_info "2. Then open another terminal and run: ${YELLOW}./bin/client${NC}"
print_info "3. Follow the instructions in this terminal"
print_separator
wait_for_enter

clear
print_separator
print_step "Observing Initial Behavior"
print_separator
print_info "Look at the client terminal to see the initial behavior:"
print_info "- The client is using its default configuration"
print_info "- It displays messages at a regular interval (default: 1000ms)"
print_info "- The message text is the default ('Hey')"
print_separator
wait_for_enter

clear
print_separator
print_step "Changing Message Text Configuration"
print_separator
print_info "Now I will execute this command to change the message text:"
echo
echo -e "${YELLOW}gdbus call -e -d com.system.configurationManager \\"
echo "            -o /com/system/configurationManager/Application/confManagerApplication1 \\"
echo "            -m com.system.configurationManager.Application.Configuration.ChangeConfiguration \\"
echo "            \"TimeoutPhrase\" \"<'Please stop me'>${NC}"
echo
print_info "This command uses D-Bus to modify the 'TimeoutPhrase' configuration parameter."
print_separator
wait_for_enter

gdbus call -e -d com.system.configurationManager \
            -o /com/system/configurationManager/Application/confManagerApplication1 \
            -m com.system.configurationManager.Application.Configuration.ChangeConfiguration \
            "TimeoutPhrase" "<'Please stop me'>"

# Check the command result
if [ $? -eq 0 ]; then
    print_info "Command executed successfully!"
else
    print_warn "Command returned non-zero exit code. Check if manager is running."
fi

echo
print_separator
print_step "Observing Configuration Change Effect"
print_separator
print_info "Look back at the client terminal - you should see the new message appearing"
print_info "The client automatically receives and applies configuration changes in real-time"
print_info "This happens through D-Bus signals without requiring client restart"
print_separator
wait_for_enter

clear
print_separator
print_step "Changing Time Interval Configuration"
print_separator
print_info "Now I will execute another command to change the time interval:"
echo
echo -e "${YELLOW}gdbus call -e -d com.system.configurationManager \\"
echo "            -o /com/system/configurationManager/Application/confManagerApplication1 \\"
echo "            -m com.system.configurationManager.Application.Configuration.ChangeConfiguration \\"
echo "            \"Timeout\" \"<int64 500>${NC}"
echo
print_info "This command modifies the 'Timeout' configuration parameter to 500ms (was 1000ms)."
print_separator
wait_for_enter

gdbus call -e -d com.system.configurationManager \
            -o /com/system/configurationManager/Application/confManagerApplication1 \
            -m com.system.configurationManager.Application.Configuration.ChangeConfiguration \
            "Timeout" "<int64 500>"

# Check the command result
if [ $? -eq 0 ]; then
    print_info "Command executed successfully!"
else
    print_warn "Command returned non-zero exit code. Check if manager is running."
fi

echo
print_separator
print_step "Observing Timeout Change Effect"
print_separator
print_info "Look back at the client terminal - the messages should now appear twice as fast"
print_info "The client is now using the updated 500ms interval instead of 1000ms"
print_info "Again, this change is applied in real-time without requiring restart"
print_separator
wait_for_enter

clear
print_separator
print_step "Retrieving Complete Configuration"
print_separator
print_info "Now I will execute this command to get the complete current configuration:"
echo
echo -e "${YELLOW}gdbus call -e -d com.system.configurationManager \\"
echo "            -o /com/system/configurationManager/Application/confManagerApplication1 \\"
echo "            -m com.system.configurationManager.Application.Configuration.GetConfiguration${NC}"
echo
print_info "This retrieves all configuration parameters and their current values"
print_separator
wait_for_enter

gdbus call -e -d com.system.configurationManager \
            -o /com/system/configurationManager/Application/confManagerApplication1 \
            -m com.system.configurationManager.Application.Configuration.GetConfiguration

# Check the command result
if [ $? -eq 0 ]; then
    print_info "Command executed successfully!"
else
    print_warn "Command returned non-zero exit code. Check if manager is running."
fi

echo
print_separator
print_step "Demo Completed"
print_separator
print_info "You've successfully completed the D-Bus Configuration Manager demo!"
print_info "In this demo, you've seen:"
print_info "1. Real-time configuration changes via D-Bus"
print_info "2. Multiple parameter types (string, integer)"
print_info "3. Configuration retrieval functionality"
echo
print_warn "You can now exit both the manager and client applications by pressing Ctrl+C in their terminals."
print_separator

# Optional: Ask if user wants to terminate the applications
read -p "Would you like to terminate the manager and client applications now? (y/n): " TERMINATE
if [[ "$TERMINATE" == "y" || "$TERMINATE" == "Y" ]]; then
    print_info "Attempting to terminate manager and client processes..."
    pkill -f "./bin/manager" 2>/dev/null || true
    pkill -f "./bin/client" 2>/dev/null || true
    print_info "Done. Thank you for trying the D-Bus Configuration Manager!"
else
    print_info "Remember to manually terminate the applications. Thank you for trying the demo!"
fi
print_separator