<h2>aDTN platform introduction</h2>

The aDTN (active Delay and Disruption Tolerant Network) platform has the following applications:

<ul>
	<li><strong>adtn</strong>: The service of the aDTN platform. Will manage all the bundles in lower layer levels.</li>
	<li><strong>adtn-lib</strong>: The API (Application Programming Interface) of the adtn.</li>
	<li><strong>adtn-tools</strong>: The provided tools to test the adtn using the adtn-lib.</li>
</ul>

On the one hand, if you simply want to be a part of the network acting as a router, i.e: forwarding, discarding and managing incoming bundles, etc. You only need the adtn service. 
On the other hand, if you want to develop your own applications or existing ones using the adtn service, you should need to install the adtn service as well as the adtn-lib. In case you want to use the provided tools, you should install the adtn-tools package too.

Is important to note that due to the structure of the packages the adtn-lib depends on adtn while the adtn-tools depends on adtn-lib as well as the adtn service.

<h2>aDTN platform installation</h2>

In order to install the aDTN platform you should include our repository into your apt-get manager. Afterwards, you will be able to install the adtn service, the adtn-library to be able to run the existing tools (adtn-tools) or even create your own applications.

<h3>Add aDTN repository</h3>

You should perform <strong>all commands as root</strong> in order to make it easier and simpler. You should log in as root using the command <code>su</code> in the command prompt and introduce your password when required. 

First, you will need to add the public key to the apt-key manager from our repository:

<code>wget -O - http://tao.uab.es/adtn/repo/conf/adtn.gpg.key | apt-key add -</code>

Next, you will need to update the sources.list files according to your SO version <code>version</code>. Note that so far, we have the following versions: <strong>wheezy</strong> (Debian 7.*), <strong>trusty</strong> (Ubuntu 14.04) and <strong>utopic</strong> (Ubuntu 14.10):

<code>wget -P /etc/apt/sources.list.d/ http://tao.uab.es/adtn/repo/conf/<strong><code>version</code></strong>/adtn.list</code>

Is important to note that you should replate the <code>version</code> by the version codename according to your SO, i.e: wheezy, trusty or utopic.

<h3>Installation process</h3>

After adding the aDTN repository to your computer, you can perform an update in order to let notice the system that there are new repositories in the system able to be installed:

<code>apt-get update</code>

After that, you should be able to install the adtn service, the adtn-lib and the adtn-tools:

<code>apt-get install (adtn | adtn-lib | adtn-tools)</code>

The required dependences should be auto-satisfied. In the adtn installation process you will be asked to enter some required information in order to run the service. The following information is the required in order to configure properly the adtn:

<ul>
	<li>Set the adtn identifier. The suggested identifier is your hostname, but keep in mind that this identifier must be unique over all local adtn platforms, so you should change it if needed. Return carrier key sets the default value between brackets.</li>
	<li>Set the adtn IP. This IP will be used to connect the current adtn with the others. All the adtns should have an unique IP and must pertain at the same local network if you want to perform the communication between them. Return carrier key sets the default value between brackets.</li>
	<li>Set the adtn port. The suggested port (4556) should be correct if you have running one instance of the adtn in the same machine and no application is using this port. Return carrier key sets the default value between brackets.</li>
</ul>

<h2>aDTN service usage</h2>

In order to start the adtn you should type the following command:

<code>sudo service adtn start</code>

There are two ways of look over the status of the adtn. The first one (<code>status</code>) checks the state of each process that the adtn service runs. The second one (<code>supervise</code>) checks the status of each process and if any of them are down, it get them up again.
<code>sudo service adtn status</code>  
<code>sudo service adtn supervise</code>

Finally, in order to stop the adtn you should perform the command below:

<code>sudo service adtn stop</code>

Is important to note that if an unexpected behavior occurs when running the adtn service. Stopping it and running it again may be fix the problem.

<h2>aDTN tools</h2>

The following tools are provided in the adtn-tools package in order to be able to test the adtn as well as the network:

<ul>
	<li><strong>adtn-ping:</strong> Tool that sends pings over the adtn. You can type <code>adtn-ping -h</code> to know how to use it.</li>
	<li><strong>adtn-neighbours:</strong> Tool that shows a list with the available neighbours in the adtn. You can type <code>adtn-neighbours -h</code> to know how to use it.</li>
	<li><strong>adtn-traceroute:</strong> Tool that gives the traceroute over the adtn. You can type <code>adtn-traceroute -h</code> to know how to use it.</li>
</ul>

<h2>aDTN Library documentation</h2>

The documentation of the libraries is located on <a href="http://senda-uab.github.io/aDTN-platform">aDTN-platform page</a>

<h2>aDTN update</h2>

In order to update the aDTN, updating to the last version from the repository, you should type the commands below as root or sudo:

<code>sudo apt-get update</code>

And next, you will update the installed version of the adtn, adtn-lib or adtn-tools pacakges:

<code>sudo apt-get install (adtn | adtn-lib | adtn-tools)</code>

<h2>aDTN platform deinstallation</h2>

In order to deinstall any (or all) of the packages, you should perform the following command as root (or sudo) according to what do you want to deinstall:

<code>sudo apt-get remove --purge (adtn | adtn-lib | adtn-tools)</code>

Note that <code>--purge</code> flag, which is recommended in use it in this case, will remove all the remaining configuration files.

<h2>How to completely remove the adtn repository</h2>

In order to remove completely the adtn repository from your computer, you should type the following command as root:

<code>apt-key del  `apt-key list | grep -i -1 developers@senda.uab.cat | grep pub | cut -d '/' -f2- | cut -d ' ' -f1`</code>

Or, if you prefer to do it manually, you can look for the key entry in the <code>apt-key list</code> and next, delete it using the <code>apt-key del key</code>, where key is the previously selected key from <code>apt-key list</code>

The command above will remove the previously-added public key from the apt-key manager. 

After that you should remove the sources.list entry regarding to the adtn repository:

<code>rm /etc/apt/sources.list.d/adtn.list</code>
