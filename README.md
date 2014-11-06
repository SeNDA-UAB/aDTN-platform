<h2>aDTN suite introduction</h2>

The aDTN (active Delay and Disruption Tolerant Network) suite has the following applications:

<ul>
<li><strong>adtnplatform</strong>: The kernel of the aDTN suite. Will manage all the bundles in lower layer levels.</li>
<li><strong>adtnlib</strong>: The API (application programming interface) of the adtnplatform.</li>
<li><strong>adtnexamples</strong>: The provided examples using the adtnlib and the adtnplatform.</li>
</ul>

On the one hand, if you want to your computer acts simply as a "router" of the network, i.e: forward, discard and manage incoming bundles you will need to install the adtnplatform. 
On the other hand, if you want to develop or use applications using the adtnplatform, you will need to install the adtnplatform, the adtnlib and, if you want to use the provided examples, the adtnexamples.

Note that due to the structure of the applications, the adtnlib depends on adtnplatform and adtnexamples depends on adtnlib as well as adtnplatform.

<h2>aDTN suite installation</h2>

In order to install any of the applications provided by aDTN suite, you should include our repository into your apt-get manager. After that you will be able to install the applications, update or remove them.

<h3>Add aDTN repository</h3>

You should perform <strong>all commands as root</strong> in order to make it easier, simpler and comfortable, so you should log in as root using the command <code>su</code> in the command prompt and introduce your password. 

First, you will need to add the public key to the apt-key manager from our repository:

<code>wget -O - http://tao.uab.es/adtn/conf/adtn.gpg.key | apt-key add -</code>

Next, you will need to update the sources.list files according to your version <code>version</code>. Note that so far, we have <strong>wheezy</strong> (Debian 7.*),  <strong>trusty</strong>(Ubuntu 14.04) and <strong>utopic</strong> (Ubuntu 14.10):

<code>wget -P /etc/apt/sources.list.d/ http://tao.uab.es/adtn/conf/<strong><code>version</code></strong>/adtn.list</code>

<h3>Installation process</h3>

After adding the aDTN repository to your computer, you can perform an update in order to let notice the system that there are new applications in the system able to be installed:

<code>apt-get update</code>

After that, you should be able to install the adtnplatform, the adtnlib and the adtnexamples:

<code>apt-get install (adtnplatform|adtnlib|adtnexamples)</code>

The required dependences should be auto-satisfied. In the adtnplatform installation process you will be asked to enter some required information in order to run the platform. This information is as follows:

<ul>
<li>Set the platform identifier. The suggested identifier is your hostname, but keep in mind that this identifier must be unique over all local adtn platforms, so you should change it if required. Return carrier key sets the default value between brackets.</li>
<li>Set the IP. This IP will be used to connect the current platform with the others. All the platforms should have an unique IP and must pertain at the same local network if you want to perform the communication between them. Return carrier key sets the default value between brackets.</li>
<li>Set the platform port. The suggested port should be correct if you have only one installed platform in the same machine and no application is this port. Return carrier key sets the default value between brackets.</li>
</ul>

<h2>aDTN platform usage</h2>

In order to start the platform you should type the following command:

<code>sudo service adtnplatform start</code>

There are two ways of checking for the status of the platform. The first one (<code>status</code>) checks the state of each process that the platform runs. The second one (<code>supervise</code>) checks for the status of each process and if any of them are off, it turn them on again.
<code>sudo service adtnplatform status</code>  
<code>sudo service adtnplatform supervise</code>

Finally in order to stop the platform you should perform the command below:

<code>sudo service adtnplatform stop</code>

Is important to note that if an unexpected behavior occurs when running the platform. Stopping it and running it again may be fix the problem.

<h2>aDTN examples applications</h2>

<li><strong>adtn-ping:</strong> Ping application over adtn platform. Simply type <code>adtn-ping -h</code> to know how to use it.</li>

<h2>aDTN suit update</h2>

In order to update the aDTN suite downloading the last version from the repository, you should type the commands below as root or sudo:

<code>sudo apt-get update</code>

And next, you update the installed version of the adtnplatform, adtnlib or adtnexamples:

 <code>sudo apt-get install (adtnplatform|adtnlib|adtnexamples)</code>

<h2>aDTN suit deinstallation</h2>

In order to deinstall any (or all) of the applications, you should perform the following command as root (or sudo), choosing the adtnplatform, adtnlib or adtnexamples according to what do you want to deinstall:

<code>sudo apt-get remove --purge (adtnplatform|adtnlib|adtnexamples)</code>

Note that <code>--purge</code> flag, which is recommended in this case to use it, will remove all the remaining configuration files.

<h2>How to completely remove the adtn repository</h2>

In order to remove completely the adtn repository from your computer, you should type the following commands as root:

<code>apt-key del  `apt-key list | grep -i -1 developers@senda.uab.cat | grep pub | cut -d '/' -f2- | cut -d ' ' -f1`</code>

The command above will remove the previously-added public key from the apt-key manager. After that you should remove the sources.list entry regarding to the adtnplatform:

<code>rm /etc/apt/sources.list.d/adtn.list</code>