package src.cat.uab.senda.adtn.examples.ping;

import java.io.FileNotFoundException;
import java.net.SocketException;
import java.nio.ByteBuffer;
import java.util.Date;

import src.cat.uab.senda.adtn.comm.Comm;
import src.cat.uab.senda.adtn.comm.InvalidArgumentException;
import src.cat.uab.senda.adtn.comm.MessageSizeException;
import src.cat.uab.senda.adtn.comm.SockAddrT;

public class PingReceiver extends Thread implements Runnable {

	int i, s; // Receive socket
	ArgumentHandler ah;
	SockAddrT source, destination;
	byte type, option;
	int seq_num;
	long time, localTime;

	PingReceiver(int socket, SockAddrT source, SockAddrT destination, ArgumentHandler ah) {
		this.s = socket;
		this.source = source;
		this.destination = destination;
		this.ah = ah;
	}

	@Override
	public void run() {
		i = ah.count; // Number of Pong to receive
		SockAddrT destination = new SockAddrT("", 0); // New SockAddrT variable,
														// will be used to get
														// the destination from
														// adtnRecvFrom method
		String destination_id = ah.destination_id.get(0);
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
						// Build a new packet to response as Pong
						buff.clear();
						buff.put(Ping.TYPE);
						buff.put(Ping.PONG);
						buff.putInt(seq_num);
						buff.putLong(time);

						// Send the Pong
						Comm.adtnSendTo(s, destination, data);
					} else if (option == 1) { // Pong received
						if (Ping.getMap().get(seq_num) == time) {
							// The received Pong is from a previous Ping
							Ping.getMap().remove(seq_num);
							time = localTime - time;
							
							System.out.println(destination_id
									+ " received ping at "
									+ new Date(localTime).toString() + " seq="
									+ seq_num + " time=" + Long.toString(time)
									+ "ms");
							i--;
						}
					}
				} else {
					System.out
							.println("Received a packet from another application. Packet is discarted");
				}
			}
		} catch (SocketException | FileNotFoundException
				| InvalidArgumentException | MessageSizeException e) {
			e.printStackTrace();
		}
	}
}
