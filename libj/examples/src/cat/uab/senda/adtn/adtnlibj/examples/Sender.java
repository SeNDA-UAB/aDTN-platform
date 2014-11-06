package cat.uab.senda.adtn.adtnlibj.examples;

import java.io.IOException;
import java.util.Scanner;

import cat.uab.senda.adtn.adtnlibj.ADTNAddress;
import cat.uab.senda.adtn.adtnlibj.ADTNSocket;

public class Sender {
	public static void main(String[] args) throws IOException{
		Scanner in = new Scanner(System.in);
		
		System.out.println("Introduce Receiver application platform name: ");
		String recvPlat = in.nextLine();
		
		System.out.println("Introduce Receiver application id (it is a number): ");
		int appId = in.nextInt();
		in.nextLine();
		
		System.out.println("Introduce a short text to send: ");
		String text = in.nextLine();
		
		System.out.println("Sending '"+text+"' to app "+appId+" at "+recvPlat+" platform.");
		
		ADTNSocket socket = new ADTNSocket(0);
		ADTNAddress address = new ADTNAddress(recvPlat, appId);
		socket.send(address, text.getBytes());
		
		System.out.println("Text '"+text+"' has been sent. Press a key to exit...");
		System.out.println();
		in.close();
	}
}