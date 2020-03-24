# ns3 Implementation for Data Center

ns3-dcn is a project for implementing and evaluating data-center algorithms in ns-3. As a derivative of ns-3, it is free, licensed under the GNU GPLv2 license, and is publicly available for research, development, and use.

ns3-dcn aims to provide an unified and standard benchmark platform for data-center research. It encourages the community contribution, and recommend all DCN researchers chose it as the benchmark for simulation.

#### Here is our project website [<u>ns3-dcn</u>](http://sing.cse.ust.hk/ns3-dcn)

<font color=red><b>[Our project is still undergoing, currently we are refactoring our code to make it meet the standard] </b></font>


## Guideline for Users

####Step1: build
However, the real quick and dirty way to get started is to
type the command
```shell
./waf configure --enable-examples
```

followed by

```shell
./waf
```

####Step2: run
On recent Linux systems, once you have built ns-3 (with examples
enabled), it should be easy to run the sample programs with the
following command, such as:

```shell
./waf --run DCTCP
```
or you can add some parameters.
```shell
./waf --run DCTCP --writeForPlot=1 --writePcap=1
```

####Note:
- You can find the example code in <code>./examples/dcn</code>, every algorithm has an independent directory. The available algorithms are listed below.
- We offer an readme for each algorithm, in the readme, you can find <u>*where is the implementation code*</u> and <u>*how to use the code*</u>.
- <font color=red>[IMPORTANT!]</font> While you are free to use the code[^1], you need to cite the related work in your paper[^2]. Since we want to bulid a community, this way can encourage the code contributors.
[^1]: Here, directly use the code as the benchmark or implement you algorithm based on the code all means "use the code"
[^2]: Here, “the related” is inheritable, for example, if you use the code of paper A, meanwhile paper A use the code of paper B, you need to cite both paper A and paper B. For your convience, we write a script in <code>./utils/dcn/gen_refence.sh</code>, you can generate all the referenced papers you need in one shot.
## Guideline for Developers

ns3-dcn aims to be a community work, and we are unable to estiablish a comprehensive and high quality platform just by us. Therefore, we hope  everyone implement your algorithms by ns3 and join the platform. You paper can be easier to reproduce and used by other people. What's more, other people can implement their work with the code your provide, and your work can get more cites.

####How to submit my code？
We heavily recommend you submit your code via Pull Request.
We offer some example <font color=orange>[to do]</font>
####Submission standard
Unlike the ns3 main project, we do not request strict code format and the test module. Our goal is, both friendly to users and developers.

- **Standard for implementation code**
  1. Name for algorithm: you need to name you algorithm first, e.g. dctcp and pfabric, please use lowercases and numbers. If name clashes happens, please add addition information, e.g. author name, public year.
  2. Name for code file: when you write a model, please name it with your alg name and the function, for example, tcp-neno.cc -> tcp-dctcp.cc, red-queue-disc.cc -> dctcp-queue-disc.cc
  3. Keep independence with other implementations: Even if one module of your algorithm is very similar with others, please add a new file with your alg name, e.g. if the switch impl in my alg(name myalg) just need modify serval lines of dctcp-queue-disc.cc, please copy from dctcp-queue-disc.cc and add a new file named myalg-queue-disc.cc. In one word, **Please add instead of modify**.
  4. Whitelist of files that can be modified：In some cases, you have the modify the files, e,g, wscript, new packet tag class defined in socket.h. Here, we offer the whitelist in <code>./utils/dcn/whitelist.txt</code>
- **Standard for example code and documents**
  1. You need to create a new directory at <code>./example/dcn/</code>, named with your algorithm name.
  2. Please create an example in your directory. You can use any code style and setting.
  3. Please add an readme in your directory, which contains:1) source code location. 2) basic usage of you algorithm. 3) code refence: if your implementation is based on others' code, please write down the names.

- **Standard for validation**
  <font color=orange>[to do]</font>


## Contact us

1. email
2. google group

## Papers and Algorithms Included
| Name | Paper | Come from | Public Year |
| :-------------: | :-------------: | :-------------: | ------------- |
| dctcp | Data Center TCP (DCTCP) | SIGCOMM | 2011 |