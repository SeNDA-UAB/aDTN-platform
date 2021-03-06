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

import java.io.FileNotFoundException;
import java.net.SocketException;
import java.nio.ByteBuffer;
import java.util.Date;

import cat.uab.senda.adtn.comm.Comm;
import cat.uab.senda.adtn.comm.InvalidArgumentException;
import cat.uab.senda.adtn.comm.JNIException;
import cat.uab.senda.adtn.comm.MessageSizeException;
import cat.uab.senda.adtn.comm.OpNotSuportedException;
import cat.uab.senda.adtn.comm.SockAddrT;

public class PingSender extends Thread implements Runnable {

	int i, s; // Send socket
	ArgumentHandler ah; // Configuration data
	SockAddrT source, destination;
	int ping_flags, seq_num;
	long time;


	PingSender(int socket, SockAddrT source, SockAddrT destination, ArgumentHandler ah) {
		this.s = socket;
		this.source = source;
		this.destination = destination;
		this.ah = ah;
		Ping.b_sent = 0;
		
	}

	@Override
	public void run() {
		seq_num = 1;
		ping_flags = Comm.H_NOTF | Comm.H_DESS;
		i = ah.count; // Number of Pings to send
		String destination_id = ah.destination_id.get(0);

		try {
			Comm.adtnSetSocketOption(s, Comm.OP_PROC_FLAGS, ping_flags);
			Comm.adtnSetSocketOption(s, Comm.OP_REPORT, source);
			Comm.adtnSetSocketOption(s, Comm.OP_LIFETIME, ah.lifetime);

			System.out.println("PING " + destination_id + " " + ah.size + " bytes of payload. " + ah.lifetime
					+ " seconds of lifetime.");
			
			while (i != 0) {
				byte[] data = new byte[ah.size];

				ByteBuffer buff = ByteBuffer.wrap(data);

				buff.put(Ping.TYPE);
				buff.put(Ping.PING);
				buff.putInt(seq_num);

				time = new Date().getTime();
				buff.putLong(time);
				Ping.getMap().put(seq_num, time);

				Comm.adtnSendTo(s, destination, data);

				Ping.b_sent++;
				seq_num++;
				i--;

				sleep(ah.interval * 1000);
			}
		} catch (SocketException | OpNotSuportedException | JNIException | FileNotFoundException
				| InvalidArgumentException | MessageSizeException | InterruptedException e1) {
			e1.printStackTrace();
		}
	}
}
