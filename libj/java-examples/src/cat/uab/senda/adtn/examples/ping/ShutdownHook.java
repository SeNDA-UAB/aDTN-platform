package cat.uab.senda.adtn.examples.ping;

import java.io.FileNotFoundException;
import java.net.SocketException;

import cat.uab.senda.adtn.comm.Comm;

public class ShutdownHook extends Thread {
	int s;

	ShutdownHook(int s) {
		this.s = s;
	}

	@Override
	public void run() {
		Ping.showStatistics();
		try {
			Comm.adtnShutdown(s);
		} catch (SocketException | FileNotFoundException | IllegalAccessException e) {
			e.printStackTrace();
		}
	}
}