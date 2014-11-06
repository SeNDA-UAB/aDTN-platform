package cat.uab.senda.adtn.adtnlibj.examples;

import java.io.IOException;
import java.util.Scanner;

import cat.uab.senda.adtn.adtnlibj.ADTNSocket;

public class Receiver {
	public static void main(String[] args) throws IOException{
		Scanner in = new Scanner(System.in);
		
		int appId = 0;
		System.out.println("This application (Receiver) has id: "+ appId); 
		
		System.out.println("Waiting for text reception.");
		
		byte[] data = new byte[1024];
		ADTNSocket socket = new ADTNSocket(appId);
		socket.receive(data, 1024);
		
		System.out.println(String.valueOf(data));
		
		System.out.println("Data has been received. Press a key to exit...");
		System.out.println();
		in.close();
	}
}
