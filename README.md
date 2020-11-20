# Sysrepo Generic sd-bus plugin (DT)

## Introduction

This Sysrepo plugin is responsible for bridging systemd's lightweight D-Bus IPC
bus —
[**sd-bus**](https://www.freedesktop.org/software/systemd/man/sd-bus.html), and
Sysrepo/YANG via the RPC mechanism.

Generic sd-bus plugin enables RPC for implementing the sd-bus method call of an
sd-bus service. Detailed description of the RPC calls, how to issue them and RPC
responses are provided in the following sections. Test example data is provided
in the plugin repository. For detailed explanation, read the dedicated test
`README.md`.

## Development Setup

Setup the development environment using the provided
[`setup-dev-sysrepo`](https://github.com/sartura/setup-dev-sysrepo) scripts.
This will build all the necessary components and initialize a sparse OpenWrt
filesystem.

Subsequent rebuilds of the plugin may be done by navigating to the plugin source
directory and executing:

```shell
$ export SYSREPO_DIR=${HOME}/code/sysrepofs
$ cd ${SYSREPO_DIR}/repositories/plugins/generic-sd-bus-plugin

$ rm -rf ./build && mkdir ./build && cd ./build
$ cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
		-DCMAKE_PREFIX_PATH=${SYSREPO_DIR} \
		-DCMAKE_INSTALL_PREFIX=${SYSREPO_DIR} \
		-DCMAKE_BUILD_TYPE=Debug \
		..
-- The C compiler identification is GNU 9.3.0
-- Check for working C compiler: /usr/bin/cc
-- Check for working C compiler: /usr/bin/cc -- works
[...]
-- Configuring done
-- Generating done
-- Build files have been written to: ${SYSREPO_DIR}/repositories/plugins/generic-sd-bus-plugin/build

$ make && make install
[...]
[100%] Linking C executable sysrepo-plugin-dt-generic-sdbus
[100%] Built target sysrepo-plugin-dt-generic-sdbus
[100%] Built target sysrepo-plugin-dt-generic-sdbus
Install the project...
-- Install configuration: "Debug"
-- Installing: ${SYSREPO_DIR}/bin/sysrepo-plugin-dt-generic-sdbus
-- Set runtime path of "${SYSREPO_DIR}/bin/sysrepo-plugin-dt-generic-sdbus" to ""

$ cd ..
```

Before using the plugin it is necessary to install relevant YANG modules. For
this particular plugin, the following commands need to be invoked:

```shell
$ cd ${SYSREPO_DIR}/repositories/plugins/generic-sd-bus-plugin
$ export LD_LIBRARY_PATH="${SYSREPO_DIR}/lib64;${SYSREPO_DIR}/lib"
$ export PATH="${SYSREPO_DIR}/bin:${PATH}"

$ sysrepoctl -i ./yang/generic-sd-bus@2020-09-14.yang
```

## YANG Overview

The `generic-sd-bus` YANG module with the `ts-gsb` prefix consists of the
following `rpc` call endpoints:

* `/generic-sd-bus:sd-bus-call` — sd-bus call mechanism for sd-bus objects

The RPC enables executing a sd-bus call command for a specific sd-bus service
and its method with all of the necessary fields. YANG definition for the sd-bus
call RPC is listed below:

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
sd-bus `type` (SYSTEM or USER) name, sd-bus `service`, sd-bus object `path`, sd-bus
`interface`, sd-bus `method`, sd-bus method `signature` and `arguments`. All of these
message components are documented in the [official sd-bus documentation](https://www.freedesktop.org/software/systemd/man/sd_bus_call_method.html).

The `sd-bus-method-arguments` field is a string that contains a list of method
arguments. The arguments are ordered the same way `busctl` would accept them
with the exception that `strings`, `signatures` and `object-paths` HAVE to be enclosed
with quotation marks ("..."). In case of the need for quotation marks in the
`string/object-path` they can be escaped like this \". Further argument examples
and explanations can be found in the `./test` directory and on the [official busctl
documentation](https://www.freedesktop.org/software/systemd/man/busctl.html).

The output of the YANG RPC statement contains a list of sd-bus results denoted
in a YANG list `sd-bus-result`. The list contains two YANG leaf elements: (1)
`sd-bus-method`, a string that contains the sd-bus method call command that was
executed, (2) `sd-bus-response`, a string in containing the sd-bus call response
arguments and (3) `sd-bus-signature` - signature of the response arguments.

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

## Running and Examples

This plugin is installed as the `sysrepo-plugin-dt-generic-sdbus` binary to
`${SYSREPO_DIR}/bin/` directory path. Simply invoke this binary, making sure
that the environment variables are set correctly:

```shell
$ sysrepo-plugin-dt-generic-sdbus
[INF]: Applying scheduled changes.
[INF]: File "generic-sd-bus@2020-09-14.yang" was installed.
[INF]: Module "generic-sd-bus" was installed.
[INF]: Scheduled changes applied.
[INF]: Session 1 (user "...") created.
[INF]: plugin: sr_plugin_init_cb
[INF]: plugin: Subscribing to sd-bus call rpc
[INF]: plugin: Succesfull init
[...]
```

Output from the plugin is expected as the plugin has no UCI configuration to
load from the device into either the `startup` or the `running` datastore. This
can be show by invoking the following commands:

```shell
$ sysrepocfg -X -d startup -f json -m 'generic-sd-bus'
{

}

$ sysrepocfg -X -d running -f json -m 'generic-sd-bus'
{

}
```

Below is a simple example of how to make a YANG RPC call on the sd-bus object
named `object1` invoking the sd-bus method `method1` with no method message:

```xml
<sd-bus-call xmlns="https://terastream/ns/yang/generic-sd-bus">
    <sd-bus-message>
        <sd-bus>SYSTEM</sd-bus>
        <sd-bus-service>net.sysrepo.SDBUSTest</sd-bus-service>
        <sd-bus-object-path>/net/sysrepo/SDBUSTest</sd-bus-object-path>
        <sd-bus-interface>net.sysrepo.SDBUSTest</sd-bus-interface>
        <sd-bus-method>Test1</sd-bus-method>
        <sd-bus-method-signature>s</sd-bus-method-signature>
        <sd-bus-method-arguments>"str_arg"</sd-bus-method-arguments>
    </sd-bus-message>
</sd-bus-call>
```

An example response for the above RPC call:

```xml
<sd-bus-result  xmlns="https://terastream/ns/yang/generic-sd-bus">
    <sd-bus-method>Test1</sd-bus-method>
    <sd-bus-response>0</sd-bus-response>
    <sd-bus-signature>x</sd-bus-signature>
</sd-bus-result>
```

In case of an error while invoking the sd-bus call, NETCONF will report the error.

Introspection of the remote bus objects exposes information which can be used
to craft RPC calls:

```shell
$ gdbus introspect --system \
	--dest org.freedesktop.network1 \
	--object-path /org/freedesktop/network1

node /org/freedesktop/network1 {
  interface org.freedesktop.DBus.Peer { ... };
  interface org.freedesktop.DBus.Introspectable { ... };
  interface org.freedesktop.DBus.Properties { ... };
  interface org.freedesktop.network1.Manager {
    methods:
      ListLinks(out a(iso) arg_0);
      GetLinkByName(in  s arg_0,
                    out i arg_1,
                    out o arg_2);
      ...
      ForceRenewLink(in  i arg_0);
      ReconfigureLink(in  i arg_0);
      Reload();
    signals:
    properties:
      readonly s OperationalState = 'routable';
      readonly s CarrierState = 'carrier';
      readonly s AddressState = 'routable';
  };
  node link { ... };
  node network { ... };
};
```

From the provided data, we can craft and invoke `ListLinks` method call using
the following commands:

```shell
$ cat > ./test/test-rpc.xml << EOF
<sd-bus-call xmlns="https://terastream/ns/yang/generic-sd-bus">
    <sd-bus-message>
        <sd-bus>SYSTEM</sd-bus>
        <sd-bus-service>org.freedesktop.network1</sd-bus-service>
        <sd-bus-object-path>/org/freedesktop/network1</sd-bus-object-path>
        <sd-bus-interface>org.freedesktop.network1.Manager</sd-bus-interface>
        <sd-bus-method>ListLinks</sd-bus-method>
        <sd-bus-method-signature></sd-bus-method-signature>
        <sd-bus-method-arguments></sd-bus-method-arguments>
    </sd-bus-message>
</sd-bus-call>
EOF

$ sysrepocfg --rpc=./test/test-rpc.xml -f xml -m 'generic-sd-bus'

<sd-bus-call xmlns="https://terastream/ns/yang/generic-sd-bus">
  <sd-bus-result>
    <sd-bus-method>ListLinks</sd-bus-method>
    <sd-bus-response>5 3 "wg0" "/org/freedesktop/network1/link/_33" 2 "enp0s31f6" "/org/freedesktop/network1/link/_32" 1 "lo" "/org/freedesktop/network1/link/_31" 4 "wlp0s20f3" "/org/freedesktop/network1/link/_34" 5 "docker0" "/org/freedesktop/network1/link/_35"</sd-bus-response>
    <sd-bus-signature>a(iso)</sd-bus-signature>
  </sd-bus-result>
</sd-bus-call>
```

More complicated examples include passing arguments to method calls, such as
`GetLinkByName`:

```shell
$ cat > ./test/test-rpc.xml << EOF
<sd-bus-call xmlns="https://terastream/ns/yang/generic-sd-bus">
    <sd-bus-message>
        <sd-bus>SYSTEM</sd-bus>
        <sd-bus-service>org.freedesktop.network1</sd-bus-service>
        <sd-bus-object-path>/org/freedesktop/network1</sd-bus-object-path>
        <sd-bus-interface>org.freedesktop.network1.Manager</sd-bus-interface>
        <sd-bus-method>GetLinkByName</sd-bus-method>
        <sd-bus-method-signature>s</sd-bus-method-signature>
        <sd-bus-method-arguments>wg0</sd-bus-method-arguments>
    </sd-bus-message>
</sd-bus-call>
EOF

$ sysrepocfg --rpc=./test/test-rpc.xml -f xml -m 'generic-sd-bus'

<sd-bus-call xmlns="https://terastream/ns/yang/generic-sd-bus">
  <sd-bus-result>
    <sd-bus-method>GetLinkByName</sd-bus-method>
    <sd-bus-response>3 "/org/freedesktop/network1/link/_33"</sd-bus-response>
    <sd-bus-signature>io</sd-bus-signature>
  </sd-bus-result>
</sd-bus-call>
```

For more information and examples, please consult the [official sd-bus
documentation](https://www.freedesktop.org/software/systemd/man/sd_bus_call_method.html).
