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

import cat.uab.senda.adtn.comm.AddressInUseException;
import cat.uab.senda.adtn.comm.Comm;
import cat.uab.senda.adtn.comm.SockAddrT;

import com.beust.jcommander.JCommander;
import com.beust.jcommander.ParameterException;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileReader;
import java.io.IOException;
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
	public static int stop = 0;

	private static HashMap<Integer, Long> map = new HashMap<Integer, Long>();
	private static ArgumentHandler ah;
	public static int b_sent, b_received;
	public static long total_rtt = 0, max_rtt = 0, min_rtt = 0;

	public ArgumentHandler GetParameters(String[] args) {
		ah = new ArgumentHandler();
		JCommander jcom = new JCommander(ah);
		jcom.setProgramName("Ping");
		try {
			jcom.parse(args);

			if (ah.help) {
				jcom.usage();
				System.exit(0);

			}
			if (ah.destination_id.size() == 0 && ah.mode.equals("sender")) { // If sender mode, destination must be
																				// specified
				System.out.println("You must specify a destination");
				jcom.usage();
				System.exit(0);
			}
			if (ah.size < 14) { // Set the minimum size to 14 bytes. (1 byte for TYPE, 1 byte for PING/PONG code, 4
								// bytes for seq_number (int), 8 bytes for timestamp (long))

				System.out
						.println("Input payload size is less than the minimum required. Payload size set to 14 bytes.");
				ah.size = 14;
			}
			if (ah.interval < 1) {
				System.out
						.println("Input interval time is less thant the minimum required. Interval time set to 1 second");
				ah.interval = 1;
			}
		} catch (ParameterException ex) {
			jcom.usage();
			System.exit(0);
		}
		return ah;
	}

	public static void main(String[] args) {
		SockAddrT source, destination;

		int s;
		String line, platform_id = "";
		String[] id;

		// Extract the parameters and store all the options
		ArgumentHandler ah = new Ping().GetParameters(args);

		Scanner in = new Scanner(System.in);

		try {
			File conf_file = new File(ah.conf_file);
			BufferedReader buff = new BufferedReader(new FileReader(conf_file));
			while ((line = buff.readLine()) != null) {
				if (line.contains("id =")) {
					id = line.split("id = ");
					platform_id = id[1];
					buff.close();
					break;
				}
			}

		} catch (IOException e) {
			// Ask for the platform name
			System.out.println("File " + ah.conf_file + " not found. Introduce this platform name: ");
			platform_id = in.nextLine();
			in.close();
		}

		// adtn.ini file was found but it doesn't have id entry.
		if (platform_id.equals("")) { // If the adtn.ini file have a bad format, it would imply misbehavior.
			System.out.println("Wrong " + ah.conf_file + " format, id not found.");
			System.exit(0);
		}

		// Create a new aDTN socket that will be used to send and receive Pings
		try {
			s = Comm.adtnSocket();

			// Create the SockkAddr related to the source
			source = new SockAddrT(platform_id, Ping.PORT);

			Comm.adtnBind(s, source);
			
			Runtime.getRuntime().addShutdownHook(new ShutdownHook(s));

			// Create a thread for the receiver and starts it
			PingReceiver receiver = new PingReceiver(s, source, ah);
			receiver.start();

			// If mode is set as sender, create also a sender and starts it
			if (ah.mode.equals("sender")) {
				destination = new SockAddrT(ah.destination_id.get(0), Ping.PORT);
				PingSender sender = new PingSender(s, source, destination, ah);
				sender.start();
			}

			receiver.join();
			
		} catch (SocketException | FileNotFoundException | ParseException | IllegalAccessException
				| AddressInUseException | InterruptedException e1) {
			e1.printStackTrace();
		}

	}
	
	public static void showStatistics() {
		int precission;
		double rtt_avg;
		
		if (b_sent == 0)
			precission = 0;
		else
			precission = (1 - (b_received / b_sent)) * 100;
		
		if (b_received == 0)
			rtt_avg = 0;
		else
			rtt_avg = total_rtt / b_received;
		
		System.out.println("\n-- "+ah.destination_id+" ping statistics --");
		System.out.println(b_sent + " bundles transmitted, " + b_received 
				+ " bundles received, " + precission + "% packet loss, time "+total_rtt+"ms");
		System.out.println("rtt min/avg/max = "+min_rtt+"/"+rtt_avg+"/"+max_rtt+ "ms");
	}

	public static HashMap<Integer, Long> getMap() {
		return map;
	}

}
