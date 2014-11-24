package cat.uab.senda.adtn.examples.basic;

import java.io.IOException;
import java.util.Scanner;

import cat.uab.senda.adtn.comm.Comm;
import cat.uab.senda.adtn.comm.SockAddrT;

public class Receiver {
	public static void main(String[] args) throws IOException{
		Scanner in = new Scanner(System.in);
		
		System.out.println("Introduce this platform name: ");
		String platform = in.nextLine();
		
		int appId = 1900;
		System.out.println("This application (Receiver) has id: "+ appId); 
		
		System.out.println(platform + " platform waiting for text reception.");
		byte[] data = new byte[1024];
		int socket = Comm.adtnSocket();
		System.out.println("Socket identifier: " + socket);
		Comm.adtnBind(socket, new SockAddrT(platform, appId));
		System.out.println("Bind done");
		Comm.adtnRecv(socket, data, 1024);
		System.out.println(String.valueOf(data));
		
		System.out.println("Data has been received. Press a key to exit...");
		System.out.println();
		in.close();
	}
}
