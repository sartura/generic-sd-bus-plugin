# Content
* About the plugin
* Configuration xml
* sd-bus call RPC call

# About the plugin
Generic sd-bus plugin enables RPC for implementig the sd-bus method call of an sd-bus service.

Detailed description of the RPC calls, how to issue them and RPC responses are
provided in the following sections.

Test example data is provided in the plugin. For detailed explanation, read the
dedicated test README.md

# sd-bus call RPC call
This section covers the sd-bus call RPC and shows an example on how the RPC call
can be invoked and what responses to expect.

The RPC enables executing a sd-bus call command for a specific sd-bus service and its
method with all of the necessary fields. YANG definition for the sd-bus call RPC is listed
below:

```
rpc sd-bus-call {
          input {
               list sd-bus-message {
                    key "sd-bus sd-bus-service sd-bus-object-path sd-bus-interface sd-bus-method";
                    
                    leaf sd-bus {
                         mandatory true;
                         type enumeration {
                              enum SYSTEM;
                              enum USER;
                         }
                    }
                    leaf sd-bus-service {
                         mandatory true;
                         type string;
                    }
                    leaf sd-bus-object-path {
                         mandatory true;
                         type string;
                    }
                    leaf sd-bus-interface {
                         mandatory true;
                         type string;
                    }
                    leaf sd-bus-method {
                         mandatory true;
                         type string;
                    }
                    leaf sd-bus-method-signature {
                         mandatory true;
                         type string;
                    }
                    leaf sd-bus-method-arguments {
                         mandatory true;
                         type string;
                         
                    }
               }
          }
          output {
               list sd-bus-result {
                    key sd-bus-method;
                    leaf sd-bus-method {
                         type string;
                    }
                    leaf sd-bus-response {
                         type string;
                    }
                    leaf sd-bus-signature {
                         type string;
                    }
               }
          }
     }
}
```

The input of the YANG RPC statement contains a list of sd-bus method calls that
need to be called on the system. The `sd-bus-message` YANG list contains the
sd-bus type (SYSTEM or USER) name, sd-bus service, sd-bus object path, sd-bus interface, sd-bus method, sd-bus method signature and arguments. All of these message components are documented on the official (sd-bus documentation)[https://www.freedesktop.org/software/systemd/man/sd_bus_call_method.html]

The sd-bus-method-arguments field is a string that contains a list of method arguments. The arguments are ordered the same way (busctl)[] would accept them with the exception that strings, signatures and object-paths HAVE to be enclosed with quotation marks ("..."). In case of the need for quotation marks in the string/object-path they can be escaped like this \". Further argument examples and explenations can be found in the test folder and on the official (busctl documentation)[https://www.freedesktop.org/software/systemd/man/busctl.html]

The output of the YANG RPC statement contains a list of sd-bus results denoted
in a YANG list `sd-bus-result`. The list contains two YANG leaf elements: (1)
`sd-bus-method`, a string that contains the sd-bus method call command that was
executed, (2) `sd-bus-response`, a string in containing the sd-bus
call response arguments and (3) `sd-bus-signature` - signature of the response arguments.

The cardinality of the YANG RPC statement elements is as follows:

| YANG element              | cardinality |
|---------------------------|:-----------:|
| input                                   |
| sd-bus-message            |      0..n   |
| sd-bus                    |      1      |
| sd-bus-service            |      1      |
| sd-bus-object-path        |      1      |
| sd-bus-interface          |      1      |
| sd-bus-method             |      1      |
| sd-bus-method-signature   |      0..1   |
| sd-bus-method-arguments   |      0..1   |
| output                                  |
| sd-bus-result             |      0..n   |
| sd-bus-method             |      1      |
| sd-bus-response           |      1      |
| sd-bus-signature          |      1      |


Below is a simple example of how to make a YANG RPC call on the sd-bus object
named `object1` invoking the sd-bus method `method1` with no method message:

```
<sd-bus-call xmlns="https://terastream/ns/yang/generic-sd-bus">
    <sd-bus-message>
        <sd-bus>USER</sd-bus>
        <sd-bus-service>net.sysrepo.SDBUSTest</sd-bus-service>
        <sd-bus-object-path>/net/sysrepo/SDBUSTest</sd-bus-object-path>
            <sd-bus-interface>net.sysrepo.SDBUSTest</sd-bus-interface>
        <sd-bus-method>Test1</sd-bus-method>
        <sd-bus-method-signature>s</sd-bus-method-signature>
        <sd-bus-method-arguments>[ "str_arg" ]</sd-bus-method-arguments>
    </sd-bus-message>
</sd-bus-call>
```

An example response for the above RPC call:

```
<sd-bus-result  xmlns="https://terastream/ns/yang/generic-sd-bus">
    <sd-bus-method>Test1</sd-bus-method>
    <sd-bus-response>[ 0 ]</sd-bus-response>
    <sd-bus-signature>x</sd-bus-signature>
</sd-bus-result>
```

In case of an error while invoking the sd-bus call, NETCONF will report the error.

