/*
* Copyright (c) 2014 SeNDA
* 
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
* 
*     http://www.apache.org/licenses/LICENSE-2.0
* 
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
* 
*/

package cat.uab.senda.adtn.examples.ping;

import java.nio.ByteBuffer;

import cat.uab.senda.adtn.comm.Comm;

public class PingReceiver extends Thread implements Runnable{
    
    int s; // Receive socket
    Configuration conf;
    byte type, option;
    int seq_num;
    long time;
    
    PingReceiver(int socket, Configuration conf){
        this.s = socket;
        this.conf = conf;
    }
    
     @Override
    public void run() {
		while(true) {
			byte[] data = new byte[conf.getPayload_size()];
			
	    	Comm.adtnRecvFrom(s, data, conf.getPayload_size(), conf.getDestination());
	    	ByteBuffer buff = ByteBuffer.wrap(data);
	    	
	    	if (buff.get() == 1) { // The packet type is Ping (opcode == 1)
	    		option = buff.get();
		    	seq_num = buff.getInt();
		    	time = buff.getLong();
		    	
		    	Ping.getMap().get(seq_num);
		    	System.out.println("Ping Received with seq_number "+ Integer.toString(seq_num));
	    	}else {
	    		System.out.println("Received a packet from another application. Packet is discarted");	
	    	}
		}
    }
}
