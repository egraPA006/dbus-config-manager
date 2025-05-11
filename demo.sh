#!/bin/bash

print_separator() {
    echo "--------------------------------------------------"
}

wait_for_enter() {
    read -p "Press Enter to continue..."
    echo
}

clear
print_separator
echo "1. First, open a terminal and run: ./bin/manager"
echo "2. Then open another terminal and run: ./bin/client"
print_separator
wait_for_enter

clear
print_separator
echo "Look at the client output to see the initial behavior"
print_separator
wait_for_enter

clear
print_separator
echo "Now I will execute this command to change the configuration:"
echo
echo "gdbus call -e -d com.system.configurationManager \\"
echo "            -o /com/system/configurationManager/Application/confManagerApplication1 \\"
echo "            -m com.system.configurationManager.Application.Configuration.ChangeConfiguration \\"
echo "            \"TimeoutPhrase\" \"<'Please stop me'>\""
echo
print_separator
wait_for_enter

gdbus call -e -d com.system.configurationManager \
            -o /com/system/configurationManager/Application/confManagerApplication1 \
            -m com.system.configurationManager.Application.Configuration.ChangeConfiguration \
            "TimeoutPhrase" "<'Please stop me'>"

echo
print_separator
echo "Look back at the client output - you should see the new message appearing"
print_separator
wait_for_enter

clear
print_separator
echo "Now I will execute another command to change the configuration:"
echo
echo "gdbus call -e -d com.system.configurationManager \\"
echo "            -o /com/system/configurationManager/Application/confManagerApplication1 \\"
echo "            -m com.system.configurationManager.Application.Configuration.ChangeConfiguration \\"
echo "            \"Timeout\" \"<int64 500>\""
echo
print_separator
wait_for_enter

gdbus call -e -d com.system.configurationManager \
            -o /com/system/configurationManager/Application/confManagerApplication1 \
            -m com.system.configurationManager.Application.Configuration.ChangeConfiguration \
            "Timeout" "<int64 500>"

echo
print_separator
echo "Look back at the client output - you should see new speed of output as new timeout is applied"
print_separator
wait_for_enter

clear
print_separator
echo "Now I will execute this command to get the current configuration:"
echo
echo "gdbus call -e -d com.system.configurationManager \\"
echo "            -o /com/system/configurationManager/Application/confManagerApplication1 \\"
echo "            -m com.system.configurationManager.Application.Configuration.GetConfiguration"
echo
print_separator
wait_for_enter

gdbus call -e -d com.system.configurationManager \
            -o /com/system/configurationManager/Application/confManagerApplication1 \
            -m com.system.configurationManager.Application.Configuration.GetConfiguration

echo
print_separator
echo "Script completed. You can now exit both the manager and client applications."
print_separator