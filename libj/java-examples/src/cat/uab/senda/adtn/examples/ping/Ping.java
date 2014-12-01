package src.cat.uab.senda.adtn.examples.ping;

import src.cat.uab.senda.adtn.comm.AddressInUseException;
import src.cat.uab.senda.adtn.comm.Comm;
import src.cat.uab.senda.adtn.comm.SockAddrT;

import com.beust.jcommander.JCommander;
import com.beust.jcommander.ParameterException;

import java.io.FileNotFoundException;
import java.net.SocketException;
import java.text.ParseException;
import java.util.HashMap;
import java.util.Scanner;

public class Ping {
	
	// Definitions
	public static byte TYPE = 1; // Ping opcode
	// Option, specify if the message is a Ping (0) or a Pong (1)
	public static byte PING = 0;
	public static byte PONG = 1;
	public static final int PORT = 1;

	public static HashMap<Integer, Long> map = new HashMap<Integer, Long>();
	public ArgumentHandler ah;

	public ArgumentHandler GetParameters(String[] args) {
		ArgumentHandler ah = new ArgumentHandler();
		JCommander jcom = new JCommander(ah);
		jcom.setProgramName("Ping");
		try {
			jcom.parse(args);

			if (ah.help) {
				jcom.usage();
				System.exit(0);
			}
			if (ah.destination_id.size() == 0 && ah.mode.equals("sender")) {
				System.out.println("You must specify a destination");
				jcom.usage();
				System.exit(0);
			}
		} catch (ParameterException ex) {
			System.out.println(ah.destination_id.size());
			jcom.usage();
			System.exit(0);
		}
		return ah;
	}

	public static void main(String[] args) {
		SockAddrT source, destination;
	
		int s;
		Scanner in = new Scanner(System.in);

		// Ask for the platform name
		System.out.println("Introduce this platform name: ");
		String platform_id = in.nextLine();
		in.close();

		// Extract the parameters and store all the options
		ArgumentHandler ah = new Ping().GetParameters(args);
		String destination_id = ah.destination_id.get(0);

		// Create a new aDTN socket that will be used to send and receive Pings
		try {
			s = Comm.adtnSocket();

			// Create the SockkAddr objects with the source and destination
			// information
			source = new SockAddrT(platform_id, Ping.PORT);
			destination = new SockAddrT(destination_id, Ping.PORT);
	
			Comm.adtnBind(s, source);
			
			Runtime.getRuntime().addShutdownHook(new ShutdownHook(s));

			// Create a thread for the receiver and starts it
			PingReceiver receiver = new PingReceiver(s, source,
					destination, ah);
			receiver.start();
	
			// If not receiver mode, create also a sender and starts it
			if (ah.mode.equals("sender")) {
				PingSender sender = new PingSender(s, source, destination,
						ah);
				sender.start();	
			}
	
			receiver.join();
		} catch (SocketException | FileNotFoundException | ParseException
				| IllegalAccessException | AddressInUseException | InterruptedException e1) {
			e1.printStackTrace();
		}
		
	}

	public static HashMap<Integer, Long> getMap() {
		return map;
	}

}
