
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

18:03:20: Debugging /root/Works/e2e-fwd/build/e2e-fwd /if1=eth1 /if2=eth3 /nprocs=4 ...
19-11-2024 18:03:21.301   3859 [E2E-FWD\main:696] %E2E-FWD-I:  Rev: X.00-03/x86_64, (built  at Nov 19 2024 17:52:16 with CC 12.2.0)
19-11-2024 18:03:21.301   3859 [E2E-FWD\s_config_validate:209] %E2E-FWD-I:  A number of CPU(-s): 8
19-11-2024 18:03:21.301   3862 [E2E-FWD\s_init_eth:266] %E2E-FWD-I:  Initialize <eth1> ...
19-11-2024 18:03:21.301   3863 [E2E-FWD\s_init_eth:266] %E2E-FWD-I:  Initialize <eth1> ...
19-11-2024 18:03:21.301   3864 [E2E-FWD\s_init_eth:266] %E2E-FWD-I:  Initialize <eth1> ...
19-11-2024 18:03:21.301   3865 [E2E-FWD\s_init_eth:266] %E2E-FWD-I:  Initialize <eth1> ...
19-11-2024 18:03:21.317   3864 [E2E-FWD\s_init_eth:312] %E2E-FWD-I:  Fanout is set for sd: 5 [type: 0, id: 5]
19-11-2024 18:03:21.317   3864 [E2E-FWD\s_init_eth:266] %E2E-FWD-I:  Initialize <eth3> ...
19-11-2024 18:03:21.317   3862 [E2E-FWD\s_init_eth:312] %E2E-FWD-I:  Fanout is set for sd: 3 [type: 0, id: 3]
19-11-2024 18:03:21.317   3862 [E2E-FWD\s_init_eth:266] %E2E-FWD-I:  Initialize <eth3> ...
19-11-2024 18:03:21.317   3863 [E2E-FWD\s_init_eth:312] %E2E-FWD-I:  Fanout is set for sd: 4 [type: 0, id: 4]
19-11-2024 18:03:21.317   3863 [E2E-FWD\s_init_eth:266] %E2E-FWD-I:  Initialize <eth3> ...
19-11-2024 18:03:21.328   3865 [E2E-FWD\s_init_eth:312] %E2E-FWD-I:  Fanout is set for sd: 6 [type: 0, id: 6]
19-11-2024 18:03:21.328   3865 [E2E-FWD\s_init_eth:266] %E2E-FWD-I:  Initialize <eth3> ...
19-11-2024 18:03:21.339   3864 [E2E-FWD\s_init_eth:312] %E2E-FWD-I:  Fanout is set for sd: 7 [type: 0, id: 7]
19-11-2024 18:03:21.339   3864 [E2E-FWD\s_e2e_fwd_th:472] %E2E-FWD-I:  Starting packet processing [eth1\#5, eth3\#7] on CPU#2 ...
19-11-2024 18:03:21.339   3863 [E2E-FWD\s_init_eth:312] %E2E-FWD-I:  Fanout is set for sd: 9 [type: 0, id: 9]
19-11-2024 18:03:21.339   3863 [E2E-FWD\s_e2e_fwd_th:472] %E2E-FWD-I:  Starting packet processing [eth1\#4, eth3\#9] on CPU#1 ...
19-11-2024 18:03:21.351   3862 [E2E-FWD\s_init_eth:312] %E2E-FWD-I:  Fanout is set for sd: 8 [type: 0, id: 8]
19-11-2024 18:03:21.351   3862 [E2E-FWD\s_e2e_fwd_th:472] %E2E-FWD-I:  Starting packet processing [eth1\#3, eth3\#8] on CPU#0 ...
19-11-2024 18:03:21.351   3865 [E2E-FWD\s_init_eth:312] %E2E-FWD-I:  Fanout is set for sd: 10 [type: 0, id: 10]
19-11-2024 18:03:21.351   3865 [E2E-FWD\s_e2e_fwd_th:472] %E2E-FWD-I:  Starting packet processing [eth1\#6, eth3\#10] on CPU#3 ...
19-11-2024 18:03:24.301   3859 [E2E-FWD\s_show_stat:661] %E2E-FWD-I:  -------------------------------------------------------------------------------------------------
19-11-2024 18:03:24.301   3859 [E2E-FWD\s_show_stat:663] %E2E-FWD-I:  <eth1> --- RX: [pkts: 857509, octets: 65363007, errs: 0], TX: [pkts: 318331, octets: 30543140, errs: 0]
19-11-2024 18:03:24.301   3859 [E2E-FWD\s_show_stat:674] %E2E-FWD-I:  <eth3> --- RX: [pkts: 318331, octets: 30543140, errs: 0], TX: [pkts: 857509, octets: 65363007, errs: 0]
19-11-2024 18:03:27.301   3859 [E2E-FWD\s_show_stat:661] %E2E-FWD-I:  -------------------------------------------------------------------------------------------------
19-11-2024 18:03:27.301   3859 [E2E-FWD\s_show_stat:663] %E2E-FWD-I:  <eth1> --- RX: [pkts: 1739151, octets: 132531045, errs: 0], TX: [pkts: 640166, octets: 61600034, errs: 0]
19-11-2024 18:03:27.301   3859 [E2E-FWD\s_show_stat:674] %E2E-FWD-I:  <eth3> --- RX: [pkts: 640168, octets: 61600182, errs: 0], TX: [pkts: 1739151, octets: 132531045, errs: 0]
19-11-2024 18:03:30.301   3859 [E2E-FWD\s_show_stat:661] %E2E-FWD-I:  -------------------------------------------------------------------------------------------------
19-11-2024 18:03:30.305   3859 [E2E-FWD\s_show_stat:663] %E2E-FWD-I:  <eth1> --- RX: [pkts: 2623836, octets: 199636364, errs: 0], TX: [pkts: 962038, octets: 92271711, errs: 0]
19-11-2024 18:03:30.305   3859 [E2E-FWD\s_show_stat:674] %E2E-FWD-I:  <eth3> --- RX: [pkts: 962038, octets: 92271711, errs: 0], TX: [pkts: 2623836, octets: 199636364, errs: 0]
19-11-2024 18:03:33.305   3859 [E2E-FWD\s_show_stat:661] %E2E-FWD-I:  -------------------------------------------------------------------------------------------------
19-11-2024 18:03:33.305   3859 [E2E-FWD\s_show_stat:663] %E2E-FWD-I:  <eth1> --- RX: [pkts: 3505098, octets: 266453007, errs: 0], TX: [pkts: 1284280, octets: 122560684, errs: 0]
19-11-2024 18:03:33.305   3859 [E2E-FWD\s_show_stat:674] %E2E-FWD-I:  <eth3> --- RX: [pkts: 1284280, octets: 122560684, errs: 0], TX: [pkts: 3505096, octets: 266452863, errs: 0]
19-11-2024 18:03:36.305   3859 [E2E-FWD\s_show_stat:661] %E2E-FWD-I:  -------------------------------------------------------------------------------------------------
19-11-2024 18:03:36.305   3859 [E2E-FWD\s_show_stat:663] %E2E-FWD-I:  <eth1> --- RX: [pkts: 4381098, octets: 332871585, errs: 0], TX: [pkts: 1605001, octets: 152795344, errs: 0]
19-11-2024 18:03:36.305   3859 [E2E-FWD\s_show_stat:674] %E2E-FWD-I:  <eth3> --- RX: [pkts: 1605001, octets: 152795344, errs: 0], TX: [pkts: 4381098, octets: 332871585, errs: 0]
19-11-2024 18:03:39.305   3859 [E2E-FWD\s_show_stat:661] %E2E-FWD-I:  -------------------------------------------------------------------------------------------------
19-11-2024 18:03:39.305   3859 [E2E-FWD\s_show_stat:663] %E2E-FWD-I:  <eth1> --- RX: [pkts: 5269632, octets: 400380711, errs: 0], TX: [pkts: 1932686, octets: 184186352, errs: 0]
19-11-2024 18:03:39.305   3859 [E2E-FWD\s_show_stat:674] %E2E-FWD-I:  <eth3> --- RX: [pkts: 1932686, octets: 184186352, errs: 0], TX: [pkts: 5269632, octets: 400380711, errs: 0]
19-11-2024 18:03:42.305   3859 [E2E-FWD\s_show_stat:661] %E2E-FWD-I:  -------------------------------------------------------------------------------------------------
19-11-2024 18:03:42.305   3859 [E2E-FWD\s_show_stat:663] %E2E-FWD-I:  <eth1> --- RX: [pkts: 6143398, octets: 466696557, errs: 0], TX: [pkts: 2257110, octets: 214895280, errs: 0]
19-11-2024 18:03:42.305   3859 [E2E-FWD\s_show_stat:674] %E2E-FWD-I:  <eth3> --- RX: [pkts: 2257110, octets: 214895280, errs: 0], TX: [pkts: 6143398, octets: 466696557, errs: 0]
19-11-2024 18:03:45.305   3859 [E2E-FWD\s_show_stat:661] %E2E-FWD-I:  -------------------------------------------------------------------------------------------------
19-11-2024 18:03:45.305   3859 [E2E-FWD\s_show_stat:663] %E2E-FWD-I:  <eth1> --- RX: [pkts: 7020144, octets: 533153308, errs: 0], TX: [pkts: 2576807, octets: 245207206, errs: 0]
19-11-2024 18:03:45.305   3859 [E2E-FWD\s_show_stat:674] %E2E-FWD-I:  <eth3> --- RX: [pkts: 2576807, octets: 245207206, errs: 0], TX: [pkts: 7020144, octets: 533153308, errs: 0]

```
