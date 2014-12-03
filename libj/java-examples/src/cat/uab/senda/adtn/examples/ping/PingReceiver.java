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
import cat.uab.senda.adtn.comm.MessageSizeException;
import cat.uab.senda.adtn.comm.SockAddrT;

public class PingReceiver extends Thread implements Runnable {

	int i, s; // Receive socket
	ArgumentHandler ah;
	SockAddrT source, destination;
	byte type, option;
	int seq_num;
	long time, localTime;

	PingReceiver(int socket, SockAddrT source, ArgumentHandler ah) {
		this.s = socket;
		this.source = source;
		this.ah = ah;
	}

	@Override
	public void run() {
		i = ah.count; // Number of Pong to receive
		SockAddrT destination = new SockAddrT("", 0); // New SockAddrT variable, will be used to get the destination
														// from adtnRecvFrom method
		try {
			while (i != 0) {
				byte[] data = new byte[ah.size];
				Comm.adtnRecvFrom(s, data, ah.size, destination);
				localTime = new Date().getTime();

				ByteBuffer buff = ByteBuffer.wrap(data);

				if (buff.get() == 1) { // Ping opcode
					option = buff.get();
					seq_num = buff.getInt();
					time = buff.getLong();

					if (option == 0) { // Ping received

						if (ah.verbose)
							System.out.println("Ping received from " + destination.getId() + " at "
									+ new Date(localTime).toString() + " seq=" + seq_num + ". Building pong.");
						// Build a new packet to response as Pong
						buff.clear();
						buff.put(Ping.TYPE);
						buff.put(Ping.PONG);
						buff.putInt(seq_num);
						buff.putLong(time);

						// Send the Pong
						Comm.adtnSendTo(s, destination, data);
						if (ah.verbose)
							System.out.println("Pong sent to " + destination.getId() + " at "
									+ new Date(localTime).toString() + " seq=" + seq_num + ".");
					} else if (option == 1) { // Pong received
						if (Ping.getMap().get(seq_num) == time) {
							// The received Pong is from a previous Ping
							if (ah.verbose)
								System.out.println("Pong received from " + destination.getId() + " at "
										+ new Date(localTime).toString() + " seq=" + seq_num + ". Building pong.");
							Ping.getMap().remove(seq_num);
							time = localTime - time;

							System.out.println(destination.getId() + " received ping at "
									+ new Date(localTime).toString() + " seq=" + seq_num + " time="
									+ Long.toString(time) + "ms.");
							i--;
						}
					}
				} else {
					System.out.println("Received a packet from another application. Packet is discarted");
				}
			}
		} catch (SocketException | FileNotFoundException | InvalidArgumentException | MessageSizeException e) {
			e.printStackTrace();
		}
    }
}
