# Content
* About
* Usage

# About
The test samples and the test run code is provided with the plugin. The test
data is provided in the TOML file. The tests cover `sd-bus-call` RPC for methods with different signatures. 

# Usage
To run the tests it is necessary for the plugin to be compiled with the  cmake `ENABLE-TESTS` flag turned on.
Procedure to run the tests:

```
go get -v netconf.go
go run netconf.go
```
For the tests to be executed correctly Netopeer2-server needs to be running and listening and the TOML file needs to be properly configured for authentication.