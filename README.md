# dbus-config-manager
A test task for OMP intership

## Running
```bash
./configuration.sh
./bin/manager
./bin/client
gdbus call -e -d com.system.configurationManager \
-o /com/system/configurationManager/Application/confManagerApplication1 \
-m com.system.configurationManager.Application.Configuration.ChangeConfiguration \
"TimeoutPhrase" "<'Please stop me'>"
```
