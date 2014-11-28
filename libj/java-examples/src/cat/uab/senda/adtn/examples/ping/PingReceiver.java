package src.cat.uab.senda.adtn.examples.ping;

import java.io.FileNotFoundException;
import java.net.SocketException;
import java.nio.ByteBuffer;
import java.util.Date;

import src.cat.uab.senda.adtn.comm.Comm;
import src.cat.uab.senda.adtn.comm.InvalidArgumentException;
import src.cat.uab.senda.adtn.comm.MessageSizeException;

public class PingReceiver extends Thread implements Runnable {

	int i, s; // Receive socket
	Configuration conf;
	byte type, option;
	int seq_num;
	long time, localTime;

	PingReceiver(int socket, Configuration conf) {
		this.s = socket;
		this.conf = conf;
	}

	@Override
	public void run() {
		i = conf.getPing_count(); // Number of Pong to receive
		while (i !=0) {
			byte[] data = new byte[conf.getPayload_size()];

			try {
				Comm.adtnRecvFrom(s, data, conf.getPayload_size(),
						conf.getDestination());
				localTime = new Date().getTime();

				ByteBuffer buff = ByteBuffer.wrap(data);

				if (buff.get() == 1) { // Ping opcode
					option = buff.get();
					seq_num = buff.getInt();
					time = buff.getLong();

					if (option == 0) { // Ping received
						// Build a new packet to response as Pong
						buff.clear();
						buff.put(Configuration.TYPE);
						buff.put(Configuration.PONG);
						buff.putInt(seq_num);
						buff.putLong(time);

						// Send the Pong
						Comm.adtnSendTo(s, conf.getDestination(), data);
					} else if (option == 1) { // Pong received
						if (Ping.getMap().get(seq_num) == time) {
							// The received Pong is from a previous Ping
							Ping.getMap().remove(seq_num);
							time = localTime - time;
							System.out.println(conf.getSrc_platform_id()
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
			} catch (SocketException | FileNotFoundException
					| InvalidArgumentException | MessageSizeException e) {
				e.printStackTrace();
			}
		}
	}
}
