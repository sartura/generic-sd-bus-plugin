/**
 * @file netconf.go
 * @authors Luka Paulic <luka.paulic@sartura.hr> Borna Blazevic <borna.blazevic@sartura.hr>
 *
 * @brief Implements tha main logic of the generic sd-bus plugin.
 *        Main functionalities include:
 *          + calling any available sd-bus method, and returning its response
 *
 * @copyright
 * Copyright (C) 2020 Deutsche Telekom AG.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *	http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

package main

import (
	"bytes"
	"errors"
	"fmt"
	"io/ioutil"
	"log"
	"os"
	"reflect"
	"strconv"
	"time"
	"encoding/xml"
	"os/exec"

	"golang.org/x/crypto/ssh"

	"github.com/BurntSushi/toml"
	"github.com/Juniper/go-netconf/netconf"
)

type config struct {
	Login login
	Test  []test
}

type login struct {
	Address  string
	Username string
	Password string
	Enabled  bool
}

type test struct {
	Message        string
	XMLRequestHead string
	XMLRequestBody string
	XMLRequestTail string
	XMLResponse    string
	Replace        [][]interface{}
	Setup          []test
	Teardown       []test
	Graph          bool
}

type Result struct {
	XMLName		xml.Name 		`xml:"sd-bus-result"`
    XMLNS    	string   		`xml:"xmlns,attr"`
    Method    	string   		`xml:"sd-bus-method"`
    Response    string   		`xml:"sd-bus-response"`
    Signature   string   		`xml:"sd-bus-signature"`
}

func fillList(replace *[][]interface{}) (*[][]interface{}, error) {
	total := 1
	for i := range *replace {
		total = total * len((*replace)[i])
	}

	list := make([][]interface{}, total)
	for i := range list {
		list[i] = make([]interface{}, len(*replace))
	}
	for i := range *replace {
		first := 1
		for j := i + 1; j < len(*replace); j++ {
			first = first * len((*replace)[j])
		}
		second := 1
		for j := 0; j < i; j++ {
			second = second * len((*replace)[j])
		}
		jump := 1
		for j := i; j < len(*replace); j++ {
			jump = jump * len((*replace)[j])
		}
		for x := 0; x < len((*replace)[i]); x++ {
			val, ok := (*replace)[i][x].(string)
			if !ok {
				return nil, errors.New("Not a string")
			}

			for z := 0; z < second; z++ {
				for y := 0; y < first; y++ {
					list[z*jump+x*first+y][i] = val
				}
			}
		}
	}

	return &list, nil
}

func transform(replace [][]interface{}) (*[][]interface{}, error) {
	if len(replace) == 0 {
		return nil, nil
	}

	for i := range replace {
		if "int64" == reflect.TypeOf(replace[i][0]).Name() {
			start, ok := replace[i][0].(int64)
			if !ok {
				return nil, errors.New("Not a number")
			}
			step, ok := replace[i][1].(int64)
			if !ok {
				return nil, errors.New("Not a number")
			}
			stop, ok := replace[i][2].(int64)
			if !ok {
				return nil, errors.New("Not a number")
			}

			replace[i] = nil
			for n := start; n <= stop; n = n + step {
				// speed up this
				replace[i] = append(replace[i], strconv.FormatInt(n, 10))
			}
		}
	}

	list, err := fillList(&replace)
	if err != nil {
		return nil, err
	}

	return list, nil
}

func parseConfig(configFile string) {

	fmt.Printf("Parsing config file: %s\n", configFile)

	var Config config
	_, err := toml.DecodeFile(configFile, &Config)
	if err != nil {
		fmt.Printf("Toml error: %v\n", err)
		os.Exit(1)
	}

	if false == Config.Login.Enabled {
		fmt.Printf("Config file: %s is disabled\n", configFile)
		return
	}

	auth := &ssh.ClientConfig{
		Config: ssh.Config{
			Ciphers: []string{"aes128-ctr", "aes128-cbc", "hmac-sha1"},
		},
		User:            Config.Login.Username,
		Auth:            []ssh.AuthMethod{ssh.Password(Config.Login.Password)},
		HostKeyCallback: ssh.InsecureIgnoreHostKey(),
		Timeout:         5 * time.Second,
	}

	s, err := netconf.DialSSH(Config.Login.Address, auth)
	if err != nil {
		log.Fatal(err)
	}
	defer s.Close()

	for i := range Config.Test {
		cfg := Config.Test[i]

		var buffer bytes.Buffer
		var buffers []bytes.Buffer

		if cfg.XMLRequestHead != "" {
			buffer.Write([]byte(cfg.XMLRequestHead))
		}

		list, err := transform(cfg.Replace)
		if err != nil {
			fmt.Println(err)
		}

		list_entries := 0
		if nil == list {
			if cfg.XMLRequestBody != "" {
				buffer.Write([]byte(cfg.XMLRequestBody))
			}
		} else {
			for el := range *list {
				list_entries = list_entries + 1
				xml := fmt.Sprintf(cfg.XMLRequestBody, (*list)[el]...)
				if "" == cfg.XMLRequestHead && "" == cfg.XMLRequestTail {
					var item bytes.Buffer
					item.Write([]byte(xml))
					buffers = append(buffers, item)
				} else {
					buffer.Write([]byte(xml))
				}
			}
		}

		if cfg.XMLRequestTail != "" {
			buffer.Write([]byte(cfg.XMLRequestTail))
		}

		if nil == buffers {
			buffers = append(buffers, buffer)
		}

		start := time.Now()
		fmt.Printf("NETCONF requests %d\n", len(buffers))
		fmt.Printf("NETCONF list entries %d\n", list_entries)
		fmt.Printf("%s\n", Config.Test[i].Message)
		for item := range buffers {
			reply, err := s.Exec(netconf.RawMethod(buffers[item].String()))
			if err != nil {
				fmt.Println(buffers[item].String())
				fmt.Printf("ERROR: %s\n", err)
				fmt.Printf("Fail for test %d\n", i)
			}
			if reply == nil {
				fmt.Printf("ERROR no reply from server\n")
			} else if cfg.XMLResponse != "" && reply.Data != cfg.XMLResponse {
				result := Result{}
				xml.Unmarshal([]byte(cfg.XMLResponse), &result)
				o, _ := xml.Marshal(result);
				if string(reply.Data) == string(o) {
					fmt.Printf("Sucess for test %d\n", i+1)
				}

			} else {
				fmt.Printf("Sucess for test %d\n", i+1)
			}
		}
		finish := time.Now()
		fmt.Printf("time elapsed %f\n", finish.Sub(start).Seconds())
		fmt.Printf("speed is %f entries/sec\n\n", (float64(list_entries) / finish.Sub(start).Seconds()))
		fmt.Printf("\n")
	}
}

func main() {

	cmd := exec.Command("../build/tests/test_service")
	if cmd == nil {
		log.Fatal("failed to start test_service, check if tests are built")
		os.Exit(1)
	}
	if err := cmd.Start(); err != nil {
		log.Fatal("failed to start test_service, check if tests are built: ", err)
		os.Exit(1)
	}

	time.Sleep(time.Second)

	dir := "./test-files/"

	files, _ := ioutil.ReadDir(dir)
	for _, file := range files {
		parseConfig(dir + file.Name())
	}

	if err := cmd.Process.Kill(); err != nil {
		log.Fatal("failed to kill test_service: ", err)
	}
}