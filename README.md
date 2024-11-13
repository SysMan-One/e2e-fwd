
### 	What is it ?
 E2E-FWD - a yet another Ethernet port to Ethernet port forwarder application. It's supposed to be used
 for test & debug purpose. The ports are set to PROMISC mode to performs forwarding frames AS-IS w\o any changes.

### 	Local deploy


#### 	Prerequisites
```
$ apt install cmake gcc

```



#### 	Build from sources

```
$ git clone  https://github.com/SysMan-One/e2e-fwd
$ cd e2e-fwd
$ git submodule update --init --recursive


$ mkdir build
$ cd build
$ cmake ../CMakeLists.txt -B ./

	or

$ cmake ../CMakeLists.txt -DCMAKE_BUILD_TYPE=Debug -B ./
$ make -s
```





#### 	Configuration options
A set of options are supposed to be used to control a behaviour of FW  instance.
Prefixes "/" or "-" can be used from command line and in the configuration file.
Option wich is added after "/CONFIG" option will override corresponding options from the configuration file.


##### 	General options
 
| Option\Keyword    | Description                                                                       |
| --------------    | --------------------------------------------------------------------------------- |
| CONFIG=\<fspec\>  | A file with configuration options |
| TRACE             | Enable extensible diagnostic output if binary is build with Debug                 |
| LOGFILE=\<fspec\> | Direct logging to specified file                                                  |
| LOGSIZE=\<number\>| Set limit (in octets) of the log file                                             |



##### 	Option list for FW

| Option\Keyword   | Description                                                                       |
| --------------   | --------------------------------------------------------------------------------- |
| IF1=\<devspec\>  | First network device |
| IF2=\<devspec\>  | Second netwoprk device |


#### 	An example of quick start session

```

[root@altservaarch64 ~]# cd /root/Works/e2e-fwd/build/
[root@altservaarch64 build]# ./e2e-fwd /if1=eth1 /if2=eth0
13-11-2024 13:10:58.190  12594 [E2E-FWD\main:312] %E2E-FWD-I:  Rev: X.00-01/aarch64, (built  at Nov 12 2024 18:26:34 with CC 10.3.1 20210703 (ALT Sisyphus 10.3.1-alt2))
13-11-2024 13:10:58.191  12595 [E2E-FWD\s_e2e_fwd_th:245] %E2E-FWD-I:  Starting packet processing ...
13-11-2024 13:10:58.191  12595 [E2E-FWD\s_init_eth:196] %E2E-FWD-I:  Initialize <eth1> ...
13-11-2024 13:10:58.197  12595 [E2E-FWD\s_init_eth:196] %E2E-FWD-I:  Initialize <eth0> ...

```
